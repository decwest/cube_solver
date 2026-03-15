#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <exception>
#include <iostream>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kCubeSize = 5;

struct Move {
  char base = 'U';  // UDLRFBxyz
  int layers = 1;   // Number of outer layers turned from the named face.
  int turns = 1;    // 1=cw, 2=half, 3=ccw.
};

struct Vec3 {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct Sticker {
  Vec3 pos;
  Vec3 normal;
  char color = 'U';
};

bool IsFaceMove(char c) {
  const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return upper == 'U' || upper == 'D' || upper == 'L' || upper == 'R' || upper == 'F' ||
         upper == 'B';
}

bool IsRotationMove(char c) {
  const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return lower == 'x' || lower == 'y' || lower == 'z';
}

int InverseTurns(int turns) {
  switch (turns) {
    case 1:
      return 3;
    case 2:
      return 2;
    case 3:
      return 1;
    default:
      throw std::logic_error("turns must be 1, 2, or 3");
  }
}

std::string Trim(std::string_view text) {
  std::size_t begin = 0;
  while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  std::size_t end = text.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return std::string(text.substr(begin, end - begin));
}

Move ParseMoveToken(std::string_view token) {
  if (token.empty()) {
    throw std::invalid_argument("empty move token");
  }

  std::size_t index = 0;
  int prefix = 0;
  while (index < token.size() && std::isdigit(static_cast<unsigned char>(token[index]))) {
    prefix = prefix * 10 + (token[index] - '0');
    ++index;
  }
  if (index >= token.size()) {
    throw std::invalid_argument("missing move face in token: " + std::string(token));
  }

  const char raw_base = token[index++];
  Move move;

  if (IsRotationMove(raw_base)) {
    move.base = static_cast<char>(std::tolower(static_cast<unsigned char>(raw_base)));
    move.layers = kCubeSize;
    if (prefix != 0) {
      throw std::invalid_argument("cube rotations cannot have a layer prefix: " +
                                  std::string(token));
    }
  } else if (IsFaceMove(raw_base)) {
    const bool lower_face = std::islower(static_cast<unsigned char>(raw_base)) != 0;
    move.base = static_cast<char>(std::toupper(static_cast<unsigned char>(raw_base)));

    bool wide = false;
    if (index < token.size() && (token[index] == 'w' || token[index] == 'W')) {
      wide = true;
      ++index;
    }

    if (lower_face) {
      wide = true;
    }

    if (wide) {
      move.layers = prefix == 0 ? 2 : prefix;
    } else if (prefix > 1) {
      // Accept 3R as a shorthand for 3Rw.
      move.layers = prefix;
    } else {
      move.layers = 1;
    }
  } else {
    throw std::invalid_argument("unsupported move token: " + std::string(token));
  }

  if (move.layers < 1 || move.layers > kCubeSize) {
    throw std::invalid_argument("layer count out of range for 5x5: " + std::string(token));
  }

  std::string_view suffix = token.substr(index);
  if (suffix.empty()) {
    move.turns = 1;
  } else if (suffix == "'") {
    move.turns = 3;
  } else if (suffix == "2" || suffix == "2'") {
    move.turns = 2;
  } else {
    throw std::invalid_argument("unsupported move suffix in token: " + std::string(token));
  }

  return move;
}

std::vector<Move> ParseMoves(const std::string& text) {
  std::istringstream stream(text);
  std::vector<Move> moves;
  std::string token;
  while (stream >> token) {
    moves.push_back(ParseMoveToken(token));
  }
  return moves;
}

std::string MoveToString(const Move& move) {
  std::string token;
  if (IsRotationMove(move.base)) {
    token.push_back(move.base);
  } else {
    if (move.layers == 1) {
      token.push_back(move.base);
    } else if (move.layers == 2) {
      token.push_back(move.base);
      token.push_back('w');
    } else {
      token += std::to_string(move.layers);
      token.push_back(move.base);
      token.push_back('w');
    }
  }

  if (move.turns == 2) {
    token.push_back('2');
  } else if (move.turns == 3) {
    token.push_back('\'');
  }
  return token;
}

std::string MovesToString(const std::vector<Move>& moves) {
  std::ostringstream out;
  for (std::size_t i = 0; i < moves.size(); ++i) {
    if (i != 0) {
      out << ' ';
    }
    out << MoveToString(moves[i]);
  }
  return out.str();
}

Move InvertMove(const Move& move) {
  Move inverse = move;
  inverse.turns = InverseTurns(move.turns);
  return inverse;
}

std::vector<Move> SimplifyMoves(const std::vector<Move>& moves) {
  std::vector<Move> simplified;
  simplified.reserve(moves.size());
  for (const Move& move : moves) {
    if (!simplified.empty() && simplified.back().base == move.base &&
        simplified.back().layers == move.layers) {
      const int merged = (simplified.back().turns + move.turns) % 4;
      if (merged == 0) {
        simplified.pop_back();
      } else {
        simplified.back().turns = merged;
      }
    } else {
      simplified.push_back(move);
    }
  }
  return simplified;
}

std::vector<Move> SolveByReversingScramble(const std::vector<Move>& scramble) {
  std::vector<Move> solution;
  solution.reserve(scramble.size());
  for (auto it = scramble.rbegin(); it != scramble.rend(); ++it) {
    solution.push_back(InvertMove(*it));
  }
  return SimplifyMoves(solution);
}

Vec3 RotateOncePositiveX(const Vec3& v) { return Vec3{v.x, -v.z, v.y}; }
Vec3 RotateOncePositiveY(const Vec3& v) { return Vec3{v.z, v.y, -v.x}; }
Vec3 RotateOncePositiveZ(const Vec3& v) { return Vec3{-v.y, v.x, v.z}; }

Vec3 RotateOnce(const Vec3& v, char axis) {
  switch (axis) {
    case 'x':
      return RotateOncePositiveX(v);
    case 'y':
      return RotateOncePositiveY(v);
    case 'z':
      return RotateOncePositiveZ(v);
    default:
      throw std::logic_error("invalid axis");
  }
}

char AxisForMoveBase(char base) {
  switch (base) {
    case 'R':
    case 'L':
    case 'x':
      return 'x';
    case 'U':
    case 'D':
    case 'y':
      return 'y';
    case 'F':
    case 'B':
    case 'z':
      return 'z';
    default:
      throw std::logic_error("invalid move base");
  }
}

int PositiveAxisQuarterTurns(const Move& move) {
  switch (move.base) {
    case 'R':
    case 'U':
    case 'x':
    case 'y':
      return move.turns % 4;
    case 'L':
    case 'D':
    case 'F':
    case 'z':
    case 'B':
      break;
    default:
      throw std::logic_error("invalid move base");
  }

  if (move.base == 'B') {
    return move.turns % 4;
  }

  return (4 - move.turns) % 4;
}

int CoordinateOnAxis(const Vec3& v, char axis) {
  switch (axis) {
    case 'x':
      return v.x;
    case 'y':
      return v.y;
    case 'z':
      return v.z;
    default:
      throw std::logic_error("invalid axis");
  }
}

bool MoveTargetsPositiveSide(char base) {
  return base == 'R' || base == 'U' || base == 'F';
}

bool IsWholeCubeRotation(char base) { return base == 'x' || base == 'y' || base == 'z'; }

class Cube5x5 {
 public:
  Cube5x5() { Reset(); }

  void Reset() {
    stickers_.clear();
    stickers_.reserve(6 * kCubeSize * kCubeSize);
    const int half = kCubeSize / 2;

    for (int z = -half; z <= half; ++z) {
      for (int x = -half; x <= half; ++x) {
        stickers_.push_back(Sticker{{x, half, z}, {0, 1, 0}, 'U'});
        stickers_.push_back(Sticker{{x, -half, z}, {0, -1, 0}, 'D'});
      }
    }

    for (int y = -half; y <= half; ++y) {
      for (int z = -half; z <= half; ++z) {
        stickers_.push_back(Sticker{{half, y, z}, {1, 0, 0}, 'R'});
        stickers_.push_back(Sticker{{-half, y, z}, {-1, 0, 0}, 'L'});
      }
    }

    for (int y = -half; y <= half; ++y) {
      for (int x = -half; x <= half; ++x) {
        stickers_.push_back(Sticker{{x, y, half}, {0, 0, 1}, 'F'});
        stickers_.push_back(Sticker{{x, y, -half}, {0, 0, -1}, 'B'});
      }
    }
  }

  void Apply(const Move& move) {
    const char axis = AxisForMoveBase(move.base);
    const int steps = PositiveAxisQuarterTurns(move);
    if (steps == 0) {
      return;
    }

    const int half = kCubeSize / 2;
    const int positive_threshold = half - move.layers + 1;
    const int negative_threshold = -half + move.layers - 1;

    for (Sticker& sticker : stickers_) {
      if (!TargetsSticker(move, axis, positive_threshold, negative_threshold, sticker.pos)) {
        continue;
      }
      for (int i = 0; i < steps; ++i) {
        sticker.pos = RotateOnce(sticker.pos, axis);
        sticker.normal = RotateOnce(sticker.normal, axis);
      }
    }
  }

  void ApplyAll(const std::vector<Move>& moves) {
    for (const Move& move : moves) {
      Apply(move);
    }
  }

  bool IsSolved() const {
    for (const Sticker& sticker : stickers_) {
      if (sticker.color != ColorForNormal(sticker.normal)) {
        return false;
      }
    }
    return true;
  }

 private:
  static bool TargetsSticker(const Move& move, char axis, int positive_threshold,
                             int negative_threshold, const Vec3& pos) {
    if (IsWholeCubeRotation(move.base)) {
      return true;
    }

    const int coordinate = CoordinateOnAxis(pos, axis);
    if (MoveTargetsPositiveSide(move.base)) {
      return coordinate >= positive_threshold;
    }
    return coordinate <= negative_threshold;
  }

  static char ColorForNormal(const Vec3& normal) {
    if (normal.x == 1) {
      return 'R';
    }
    if (normal.x == -1) {
      return 'L';
    }
    if (normal.y == 1) {
      return 'U';
    }
    if (normal.y == -1) {
      return 'D';
    }
    if (normal.z == 1) {
      return 'F';
    }
    if (normal.z == -1) {
      return 'B';
    }
    throw std::logic_error("invalid sticker normal");
  }

  std::vector<Sticker> stickers_;
};

struct BenchmarkResult {
  std::size_t cases = 0;
  double average_microseconds = 0.0;
  double max_microseconds = 0.0;
};

std::string GenerateRandomScramble(std::mt19937_64& rng, int length) {
  static const std::array<std::string_view, 12> kBases = {
      "U", "D", "L", "R", "F", "B", "Uw", "Dw", "Lw", "Rw", "Fw", "Bw"};
  static const std::array<std::string_view, 3> kSuffixes = {"", "2", "'"};

  auto axis_for = [](std::string_view token) {
    switch (token.front()) {
      case 'U':
      case 'D':
        return 'y';
      case 'L':
      case 'R':
        return 'x';
      case 'F':
      case 'B':
        return 'z';
      default:
        throw std::logic_error("invalid scramble token base");
    }
  };

  std::uniform_int_distribution<int> base_dist(0, static_cast<int>(kBases.size()) - 1);
  std::uniform_int_distribution<int> suffix_dist(0, static_cast<int>(kSuffixes.size()) - 1);

  std::ostringstream out;
  char previous_axis = '\0';
  for (int i = 0; i < length; ++i) {
    std::string_view base;
    char axis = '\0';
    do {
      base = kBases[base_dist(rng)];
      axis = axis_for(base);
    } while (axis == previous_axis);

    previous_axis = axis;
    if (i != 0) {
      out << ' ';
    }
    out << base << kSuffixes[suffix_dist(rng)];
  }
  return out.str();
}

BenchmarkResult RunBenchmark(std::size_t cases, int scramble_length) {
  std::mt19937_64 rng(0xC0FFEE);
  double total_micros = 0.0;
  double max_micros = 0.0;

  for (std::size_t i = 0; i < cases; ++i) {
    const std::string scramble_text = GenerateRandomScramble(rng, scramble_length);
    const auto started = std::chrono::steady_clock::now();
    const std::vector<Move> scramble = ParseMoves(scramble_text);
    const std::vector<Move> solution = SolveByReversingScramble(scramble);
    const auto finished = std::chrono::steady_clock::now();

    Cube5x5 cube;
    cube.ApplyAll(scramble);
    cube.ApplyAll(solution);
    if (!cube.IsSolved()) {
      throw std::runtime_error("benchmark verification failed");
    }

    const double micros =
        std::chrono::duration<double, std::micro>(finished - started).count();
    total_micros += micros;
    max_micros = std::max(max_micros, micros);
  }

  BenchmarkResult result;
  result.cases = cases;
  result.average_microseconds = total_micros / static_cast<double>(cases);
  result.max_microseconds = max_micros;
  return result;
}

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " [--verify] [--benchmark N]\n"
            << "Reads a 5x5 scramble sequence from stdin and prints an exact inverse solution.\n"
            << "Supported notation examples: R U' Rw 3Rw2 u x z'.\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    bool verify = false;
    std::optional<std::size_t> benchmark_cases;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--verify") {
        verify = true;
      } else if (arg == "--benchmark") {
        if (i + 1 >= argc) {
          throw std::invalid_argument("--benchmark requires an integer argument");
        }
        benchmark_cases = static_cast<std::size_t>(std::stoull(argv[++i]));
      } else if (arg == "--help" || arg == "-h") {
        PrintUsage(argv[0]);
        return 0;
      } else {
        throw std::invalid_argument("unknown option: " + arg);
      }
    }

    if (benchmark_cases.has_value()) {
      const BenchmarkResult result = RunBenchmark(*benchmark_cases, 100);
      std::cout << "cases: " << result.cases << '\n'
                << "avg_us: " << result.average_microseconds << '\n'
                << "max_us: " << result.max_microseconds << '\n';
      return 0;
    }

    std::ostringstream input;
    input << std::cin.rdbuf();
    const std::string scramble_text = Trim(input.str());
    if (scramble_text.empty()) {
      PrintUsage(argv[0]);
      return 1;
    }

    const std::vector<Move> scramble = ParseMoves(scramble_text);
    const std::vector<Move> solution = SolveByReversingScramble(scramble);
    std::cout << MovesToString(solution) << '\n';

    if (verify) {
      Cube5x5 cube;
      cube.ApplyAll(scramble);
      cube.ApplyAll(solution);
      if (!cube.IsSolved()) {
        std::cerr << "verification failed\n";
        return 2;
      }
      std::cerr << "verification ok\n";
    }

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }
}
