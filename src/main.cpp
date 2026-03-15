#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

constexpr int kCubeSize = 5;
constexpr int kFaceCount = 6;
constexpr int kStateLength = kCubeSize * kCubeSize * kFaceCount;

struct Options {
  std::optional<std::string> state_arg;
  fs::path backend_root = fs::path(PROJECT_SOURCE_DIR) / "third_party";
  bool setup_backend = false;
};

std::string RemoveWhitespace(std::string_view text) {
  std::string compact;
  compact.reserve(text.size());
  for (const char ch : text) {
    if (!std::isspace(static_cast<unsigned char>(ch))) {
      compact.push_back(ch);
    }
  }
  return compact;
}

std::string NormalizeState(std::string_view raw_state) {
  std::string state;
  state.reserve(raw_state.size());

  for (char ch : raw_state) {
    const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    switch (upper) {
      case 'U':
      case 'R':
      case 'F':
      case 'D':
      case 'L':
      case 'B':
        state.push_back(upper);
        break;
      case 'W':
        state.push_back('U');
        break;
      case 'G':
        state.push_back('F');
        break;
      case 'Y':
        state.push_back('D');
        break;
      case 'O':
        state.push_back('L');
        break;
      default:
        throw std::invalid_argument("state contains unsupported sticker letters");
    }
  }

  return state;
}

void ValidateState(const std::string& state) {
  if (state.size() != kStateLength) {
    throw std::invalid_argument("5x5 state must contain exactly 150 stickers in URFDLB order");
  }

  std::array<int, 256> counts{};
  for (const char ch : state) {
    ++counts[static_cast<unsigned char>(ch)];
  }

  for (const char face : std::string("URFDLB")) {
    if (counts[static_cast<unsigned char>(face)] != kCubeSize * kCubeSize) {
      std::ostringstream error;
      error << "state must contain exactly 25 stickers for each of U,R,F,D,L,B";
      throw std::invalid_argument(error.str());
    }
  }
}

std::string ShellQuote(const std::string& value) {
  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}

int RunShellCommand(const std::string& command) {
  const int raw_status = std::system(command.c_str());
  if (raw_status == -1) {
    return -1;
  }
  if (WIFEXITED(raw_status)) {
    return WEXITSTATUS(raw_status);
  }
  if (WIFSIGNALED(raw_status)) {
    return 128 + WTERMSIG(raw_status);
  }
  return raw_status;
}

bool BackendReady(const fs::path& backend_root) {
  const fs::path solver_dir = backend_root / "rubiks-cube-NxNxN-solver";
  const fs::path kociemba_bin = backend_root / "kociemba" / "kociemba" / "ckociemba" / "bin" / "kociemba";
  return fs::exists(solver_dir / "rubiks-cube-solver.py") && fs::exists(solver_dir / "ida_search_via_graph") &&
         fs::exists(kociemba_bin);
}

int EnsureBackend(const fs::path& backend_root, bool requested_explicitly) {
  if (BackendReady(backend_root)) {
    return 0;
  }

  if (!requested_explicitly) {
    std::cerr << "backend not found, bootstrapping solver dependencies...\n";
  }

  const fs::path setup_script = fs::path(PROJECT_SOURCE_DIR) / "scripts" / "setup_backend.sh";
  const std::string command = "bash " + ShellQuote(setup_script.string()) + " " +
                              ShellQuote(backend_root.string());
  return RunShellCommand(command);
}

void PrintUsage(const char* program) {
  std::cout << "Usage: " << program << " [--state STRING] [--setup-backend] [--backend-root DIR]\n"
            << "Reads a 5x5 cube state and prints a solution sequence.\n"
            << "Input order must be Kociemba order (URFDLB), 25 stickers per face.\n"
            << "The first real solve downloads lookup tables for the backend solver.\n";
}

Options ParseOptions(int argc, char* argv[]) {
  Options options;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--state") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--state requires an argument");
      }
      options.state_arg = argv[++i];
    } else if (arg == "--backend-root") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--backend-root requires a path");
      }
      options.backend_root = argv[++i];
    } else if (arg == "--setup-backend") {
      options.setup_backend = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown option: " + arg);
    }
  }

  return options;
}

std::string ReadState(const std::optional<std::string>& state_arg) {
  if (state_arg.has_value()) {
    return RemoveWhitespace(*state_arg);
  }

  std::ostringstream input;
  input << std::cin.rdbuf();
  return RemoveWhitespace(input.str());
}

}  // namespace

int main(int argc, char* argv[]) {
  try {
    const Options options = ParseOptions(argc, argv);

    if (!options.state_arg.has_value() && options.setup_backend) {
      const int setup_status = EnsureBackend(options.backend_root, true);
      if (setup_status != 0) {
        std::cerr << "backend setup failed with exit code " << setup_status << '\n';
        return setup_status == -1 ? 1 : setup_status;
      }
      return 0;
    }

    if (!options.state_arg.has_value() && isatty(STDIN_FILENO)) {
      PrintUsage(argv[0]);
      return 1;
    }

    std::string state = ReadState(options.state_arg);
    if (state.empty()) {
      PrintUsage(argv[0]);
      return 1;
    }

    const int setup_status = EnsureBackend(options.backend_root, options.setup_backend);
    if (setup_status != 0) {
      std::cerr << "backend setup failed with exit code " << setup_status << '\n';
      return setup_status == -1 ? 1 : setup_status;
    }

    state = NormalizeState(state);
    ValidateState(state);

    const fs::path bridge_script = fs::path(PROJECT_SOURCE_DIR) / "tools" / "solve_5x5_state.py";
    const fs::path solver_dir = options.backend_root / "rubiks-cube-NxNxN-solver";
    const fs::path kociemba_bin =
        options.backend_root / "kociemba" / "kociemba" / "ckociemba" / "bin" / "kociemba";

    const std::string command = "python3 " + ShellQuote(bridge_script.string()) + " --backend-dir " +
                                ShellQuote(solver_dir.string()) + " --kociemba-bin " +
                                ShellQuote(kociemba_bin.string()) + " --state " +
                                ShellQuote(state);

    const int solve_status = RunShellCommand(command);
    if (solve_status == -1) {
      std::cerr << "failed to launch backend solver\n";
      return 1;
    }
    return solve_status;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }
}
