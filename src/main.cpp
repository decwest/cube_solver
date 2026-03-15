#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum Face : int {
  U = 0,
  R = 1,
  F = 2,
  D = 3,
  L = 4,
  B = 5,
};

constexpr std::array<char, 6> kFaceChars = {'U', 'R', 'F', 'D', 'L', 'B'};
constexpr std::array<int, 6> kFaceAxes = {1, 0, 2, 1, 0, 2};
constexpr int kPhase1MaxDepth = 12;
constexpr int kPhase2MaxDepth = 18;
constexpr uint8_t kUnknownDepth = 0xff;

struct Vec3 {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct Move {
  Face face = U;
  int width = 1;
  int turns = 1;  // 1 = cw, 2 = half, 3 = ccw
};

struct StickerSlot {
  Vec3 pos;
  Face face = U;
};

struct CubieState {
  std::array<uint8_t, 8> cp{};
  std::array<uint8_t, 8> co{};
  std::array<uint8_t, 12> ep{};
  std::array<uint8_t, 12> eo{};
};

bool operator==(const CubieState& a, const CubieState& b) {
  return a.cp == b.cp && a.co == b.co && a.ep == b.ep && a.eo == b.eo;
}

using MoveEffect = CubieState;

struct Options {
  std::optional<std::string> state_text;
  std::optional<std::string> moves_text;
  std::optional<int> size;
  bool help = false;
  bool self_test = false;
};

struct ParseResult {
  bool ok = false;
  int size = 0;
  std::string state;
  std::string error;
};

int face_from_char(char c) {
  switch (c) {
    case 'U':
      return U;
    case 'R':
      return R;
    case 'F':
      return F;
    case 'D':
      return D;
    case 'L':
      return L;
    case 'B':
      return B;
    default:
      return -1;
  }
}

char uppercase_ascii(char c) {
  return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
}

std::string trim_copy(const std::string& text) {
  size_t start = 0;
  while (start < text.size() &&
         std::isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }
  size_t end = text.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }
  return text.substr(start, end - start);
}

std::vector<std::string> split_ws(const std::string& text) {
  std::istringstream iss(text);
  std::vector<std::string> out;
  std::string token;
  while (iss >> token) {
    out.push_back(token);
  }
  return out;
}

Vec3 operator+(const Vec3& a, const Vec3& b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator*(const Vec3& a, int scale) {
  return {a.x * scale, a.y * scale, a.z * scale};
}

bool operator==(const Vec3& a, const Vec3& b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}

Vec3 face_normal(Face face) {
  switch (face) {
    case U:
      return {0, 1, 0};
    case R:
      return {1, 0, 0};
    case F:
      return {0, 0, 1};
    case D:
      return {0, -1, 0};
    case L:
      return {-1, 0, 0};
    case B:
      return {0, 0, -1};
  }
  throw std::runtime_error("invalid face");
}

Vec3 face_col_basis(Face face) {
  switch (face) {
    case U:
      return {1, 0, 0};
    case R:
      return {0, 0, -1};
    case F:
      return {1, 0, 0};
    case D:
      return {1, 0, 0};
    case L:
      return {0, 0, 1};
    case B:
      return {-1, 0, 0};
  }
  throw std::runtime_error("invalid face");
}

Vec3 face_row_basis(Face face) {
  switch (face) {
    case U:
      return {0, 0, 1};
    case R:
      return {0, -1, 0};
    case F:
      return {0, -1, 0};
    case D:
      return {0, 0, -1};
    case L:
      return {0, -1, 0};
    case B:
      return {0, -1, 0};
  }
  throw std::runtime_error("invalid face");
}

Face normal_to_face(const Vec3& normal) {
  if (normal == Vec3{0, 1, 0}) {
    return U;
  }
  if (normal == Vec3{1, 0, 0}) {
    return R;
  }
  if (normal == Vec3{0, 0, 1}) {
    return F;
  }
  if (normal == Vec3{0, -1, 0}) {
    return D;
  }
  if (normal == Vec3{-1, 0, 0}) {
    return L;
  }
  if (normal == Vec3{0, 0, -1}) {
    return B;
  }
  throw std::runtime_error("invalid normal");
}

Vec3 rotate_axis_once(const Vec3& v, int axis, int sign) {
  if (axis == 0) {
    return sign > 0 ? Vec3{v.x, -v.z, v.y} : Vec3{v.x, v.z, -v.y};
  }
  if (axis == 1) {
    return sign > 0 ? Vec3{v.z, v.y, -v.x} : Vec3{-v.z, v.y, v.x};
  }
  if (axis == 2) {
    return sign > 0 ? Vec3{-v.y, v.x, v.z} : Vec3{v.y, -v.x, v.z};
  }
  throw std::runtime_error("invalid rotation axis");
}

int face_base_rotation_sign(Face face) {
  switch (face) {
    case U:
    case R:
    case F:
      return -1;
    case D:
    case L:
    case B:
      return 1;
  }
  throw std::runtime_error("invalid face");
}

long long sticker_key(const Vec3& pos, Face face) {
  const long long sx = static_cast<long long>(pos.x + 8);
  const long long sy = static_cast<long long>(pos.y + 8);
  const long long sz = static_cast<long long>(pos.z + 8);
  return ((((sx * 17LL) + sy) * 17LL) + sz) * 6LL + static_cast<int>(face);
}

class CubeModel {
 public:
  explicit CubeModel(int size) : size_(size), half_(size / 2) {
    if (size_ <= 0 || size_ % 2 == 0) {
      throw std::runtime_error("only odd cube sizes are supported");
    }
    build_slots();
  }

  int size() const { return size_; }

  std::string solved_state() const {
    std::string state;
    state.reserve(slots_.size());
    for (Face face : {U, R, F, D, L, B}) {
      state.append(size_ * size_, kFaceChars[face]);
    }
    return state;
  }

  int facelet_index(Face face, int row, int col) const {
    return static_cast<int>(face) * size_ * size_ + row * size_ + col;
  }

  int index_of(const Vec3& pos, Face face) const {
    const auto it = index_by_slot_.find(sticker_key(pos, face));
    if (it == index_by_slot_.end()) {
      throw std::runtime_error("slot lookup failed");
    }
    return it->second;
  }

  std::string apply_move(const std::string& state, const Move& move) const {
    std::string current = state;
    for (int i = 0; i < move.turns; ++i) {
      current = apply_one_quarter_turn(current, move.face, move.width);
    }
    return current;
  }

  std::string apply_moves(const std::string& state,
                          const std::vector<Move>& moves) const {
    std::string current = state;
    for (const Move& move : moves) {
      current = apply_move(current, move);
    }
    return current;
  }

 private:
  void build_slots() {
    slots_.clear();
    index_by_slot_.clear();
    slots_.reserve(6 * size_ * size_);
    for (Face face : {U, R, F, D, L, B}) {
      const Vec3 normal = face_normal(face);
      const Vec3 col_basis = face_col_basis(face);
      const Vec3 row_basis = face_row_basis(face);
      for (int row = 0; row < size_; ++row) {
        for (int col = 0; col < size_; ++col) {
          const int row_offset = row - half_;
          const int col_offset = col - half_;
          const Vec3 pos = normal * half_ + col_basis * col_offset +
                           row_basis * row_offset;
          const int index = static_cast<int>(slots_.size());
          slots_.push_back({pos, face});
          index_by_slot_[sticker_key(pos, face)] = index;
        }
      }
    }
  }

  bool layer_selected(const Vec3& pos, Face face, int width) const {
    const int limit = half_ - width + 1;
    switch (face) {
      case U:
        return pos.y >= limit;
      case R:
        return pos.x >= limit;
      case F:
        return pos.z >= limit;
      case D:
        return pos.y <= -limit;
      case L:
        return pos.x <= -limit;
      case B:
        return pos.z <= -limit;
    }
    throw std::runtime_error("invalid face");
  }

  std::vector<int> build_perm(Face face, int width) const {
    std::vector<int> perm(slots_.size(), -1);
    const int axis = kFaceAxes[face];
    const int sign = face_base_rotation_sign(face);
    for (size_t src = 0; src < slots_.size(); ++src) {
      Vec3 pos = slots_[src].pos;
      Face sticker_face = slots_[src].face;
      if (layer_selected(pos, face, width)) {
        pos = rotate_axis_once(pos, axis, sign);
        const Vec3 rotated_normal =
            rotate_axis_once(face_normal(sticker_face), axis, sign);
        sticker_face = normal_to_face(rotated_normal);
      }
      const int dst = index_of(pos, sticker_face);
      perm[dst] = static_cast<int>(src);
    }
    return perm;
  }

  const std::vector<int>& perm_for(Face face, int width) const {
    const uint64_t key = (static_cast<uint64_t>(face) << 32) |
                         static_cast<uint64_t>(width);
    const auto it = perm_cache_.find(key);
    if (it != perm_cache_.end()) {
      return it->second;
    }
    auto inserted = perm_cache_.emplace(key, build_perm(face, width));
    return inserted.first->second;
  }

  std::string apply_one_quarter_turn(const std::string& state,
                                     Face face,
                                     int width) const {
    const auto& perm = perm_for(face, width);
    std::string next(state.size(), '?');
    for (size_t i = 0; i < perm.size(); ++i) {
      next[i] = state[perm[i]];
    }
    return next;
  }

  int size_;
  int half_;
  std::vector<StickerSlot> slots_;
  std::unordered_map<long long, int> index_by_slot_;
  mutable std::unordered_map<uint64_t, std::vector<int>> perm_cache_;
};

struct CornerSpec {
  Vec3 pos;
  std::array<Face, 3> faces;
};

struct EdgeSpec {
  Vec3 pos;
  std::array<Face, 2> faces;
};

constexpr std::array<CornerSpec, 8> kCornerSpecs = {{
    {{1, 1, 1}, {U, R, F}},
    {{-1, 1, 1}, {U, F, L}},
    {{-1, 1, -1}, {U, L, B}},
    {{1, 1, -1}, {U, B, R}},
    {{1, -1, 1}, {D, F, R}},
    {{-1, -1, 1}, {D, L, F}},
    {{-1, -1, -1}, {D, B, L}},
    {{1, -1, -1}, {D, R, B}},
}};

constexpr std::array<EdgeSpec, 12> kEdgeSpecs = {{
    {{1, 1, 0}, {U, R}},
    {{0, 1, 1}, {U, F}},
    {{-1, 1, 0}, {U, L}},
    {{0, 1, -1}, {U, B}},
    {{1, -1, 0}, {D, R}},
    {{0, -1, 1}, {D, F}},
    {{-1, -1, 0}, {D, L}},
    {{0, -1, -1}, {D, B}},
    {{1, 0, 1}, {F, R}},
    {{-1, 0, 1}, {F, L}},
    {{-1, 0, -1}, {B, L}},
    {{1, 0, -1}, {B, R}},
}};

std::string sorted_key(const std::string& text) {
  std::string key = text;
  std::sort(key.begin(), key.end());
  return key;
}

template <size_t N>
int permutation_parity(const std::array<uint8_t, N>& perm) {
  int parity = 0;
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = i + 1; j < N; ++j) {
      if (perm[i] > perm[j]) {
        parity ^= 1;
      }
    }
  }
  return parity;
}

template <size_t N>
int rank_perm(const std::array<uint8_t, N>& perm) {
  int rank = 0;
  std::array<int, N> items{};
  for (size_t i = 0; i < N; ++i) {
    items[i] = static_cast<int>(i);
  }
  for (size_t i = 0; i < N; ++i) {
    int pos = 0;
    while (items[pos] != perm[i]) {
      ++pos;
    }
    rank = rank * static_cast<int>(N - i) + pos;
    for (size_t j = pos; j + 1 < N - i; ++j) {
      items[j] = items[j + 1];
    }
  }
  return rank;
}

template <size_t N>
std::array<uint8_t, N> unrank_perm(int rank) {
  std::array<uint8_t, N> perm{};
  std::array<int, N> items{};
  for (size_t i = 0; i < N; ++i) {
    items[i] = static_cast<int>(i);
  }
  std::array<int, N> digits{};
  for (size_t offset = 0; offset < N; ++offset) {
    const size_t base = offset + 1;
    digits[N - 1 - offset] = rank % static_cast<int>(base);
    rank /= static_cast<int>(base);
  }
  size_t remaining = N;
  for (size_t i = 0; i < N; ++i) {
    const int pick = digits[i];
    perm[i] = static_cast<uint8_t>(items[pick]);
    for (size_t j = pick; j + 1 < remaining; ++j) {
      items[j] = items[j + 1];
    }
    --remaining;
  }
  return perm;
}

CubieState solved_cubie() {
  CubieState state;
  for (uint8_t i = 0; i < state.cp.size(); ++i) {
    state.cp[i] = i;
    state.co[i] = 0;
  }
  for (uint8_t i = 0; i < state.ep.size(); ++i) {
    state.ep[i] = i;
    state.eo[i] = 0;
  }
  return state;
}

std::string format_cubie_state(const CubieState& state);

CubieState apply_cubie_move(const CubieState& state, const MoveEffect& move) {
  CubieState next;
  for (size_t pos = 0; pos < state.cp.size(); ++pos) {
    const uint8_t src = move.cp[pos];
    next.cp[pos] = state.cp[src];
    next.co[pos] = static_cast<uint8_t>((state.co[src] + move.co[pos]) % 3);
  }
  for (size_t pos = 0; pos < state.ep.size(); ++pos) {
    const uint8_t src = move.ep[pos];
    next.ep[pos] = state.ep[src];
    next.eo[pos] = static_cast<uint8_t>(state.eo[src] ^ move.eo[pos]);
  }
  return next;
}

std::optional<CubieState> facelets_to_cubie(const CubeModel& model,
                                            const std::string& state,
                                            std::string* error) {
  CubieState cubie{};
  std::array<std::string, 8> corner_keys{};
  for (size_t i = 0; i < kCornerSpecs.size(); ++i) {
    std::string colors;
    for (Face face : kCornerSpecs[i].faces) {
      colors.push_back(kFaceChars[face]);
    }
    corner_keys[i] = sorted_key(colors);
  }
  std::array<std::string, 12> edge_keys{};
  for (size_t i = 0; i < kEdgeSpecs.size(); ++i) {
    std::string colors;
    for (Face face : kEdgeSpecs[i].faces) {
      colors.push_back(kFaceChars[face]);
    }
    edge_keys[i] = sorted_key(colors);
  }

  std::array<bool, 8> used_corners{};
  for (size_t pos = 0; pos < kCornerSpecs.size(); ++pos) {
    std::string observed;
    observed.reserve(3);
    for (Face face : kCornerSpecs[pos].faces) {
      observed.push_back(state[model.index_of(kCornerSpecs[pos].pos, face)]);
    }
    const std::string key = sorted_key(observed);
    int cubie_id = -1;
    for (size_t id = 0; id < corner_keys.size(); ++id) {
      if (corner_keys[id] == key) {
        cubie_id = static_cast<int>(id);
        break;
      }
    }
    if (cubie_id < 0 || used_corners[cubie_id]) {
      if (error != nullptr) {
        *error = "invalid 3x3 state: corner cubies are inconsistent";
      }
      return std::nullopt;
    }
    used_corners[cubie_id] = true;
    int orientation = -1;
    for (int i = 0; i < 3; ++i) {
      if (observed[i] == 'U' || observed[i] == 'D') {
        orientation = i;
        break;
      }
    }
    if (orientation < 0) {
      if (error != nullptr) {
        *error = "invalid 3x3 state: corner orientation is inconsistent";
      }
      return std::nullopt;
    }
    cubie.cp[pos] = static_cast<uint8_t>(cubie_id);
    cubie.co[pos] = static_cast<uint8_t>(orientation);
  }

  std::array<bool, 12> used_edges{};
  for (size_t pos = 0; pos < kEdgeSpecs.size(); ++pos) {
    std::string observed;
    observed.reserve(2);
    for (Face face : kEdgeSpecs[pos].faces) {
      observed.push_back(state[model.index_of(kEdgeSpecs[pos].pos, face)]);
    }
    const std::string key = sorted_key(observed);
    int cubie_id = -1;
    for (size_t id = 0; id < edge_keys.size(); ++id) {
      if (edge_keys[id] == key) {
        cubie_id = static_cast<int>(id);
        break;
      }
    }
    if (cubie_id < 0 || used_edges[cubie_id]) {
      if (error != nullptr) {
        *error = "invalid 3x3 state: edge cubies are inconsistent";
      }
      return std::nullopt;
    }
    used_edges[cubie_id] = true;

    const std::string canonical = {
        kFaceChars[kEdgeSpecs[cubie_id].faces[0]],
        kFaceChars[kEdgeSpecs[cubie_id].faces[1]],
    };
    if (observed == canonical) {
      cubie.eo[pos] = 0;
    } else if (observed[0] == canonical[1] && observed[1] == canonical[0]) {
      cubie.eo[pos] = 1;
    } else {
      if (error != nullptr) {
        *error = "invalid 3x3 state: edge orientation is inconsistent";
      }
      return std::nullopt;
    }
    cubie.ep[pos] = static_cast<uint8_t>(cubie_id);
  }

  int corner_orientation_sum = 0;
  for (uint8_t value : cubie.co) {
    corner_orientation_sum += value;
  }
  int edge_orientation_sum = 0;
  for (uint8_t value : cubie.eo) {
    edge_orientation_sum += value;
  }
  if (corner_orientation_sum % 3 != 0) {
    if (error != nullptr) {
      *error = "invalid 3x3 state: corner orientation parity mismatch";
    }
    return std::nullopt;
  }
  if (edge_orientation_sum % 2 != 0) {
    if (error != nullptr) {
      *error = "invalid 3x3 state: edge orientation parity mismatch (" +
               format_cubie_state(cubie) + ")";
    }
    return std::nullopt;
  }
  if (permutation_parity(cubie.cp) != permutation_parity(cubie.ep)) {
    if (error != nullptr) {
      *error = "invalid 3x3 state: permutation parity mismatch";
    }
    return std::nullopt;
  }
  return cubie;
}

int rank_corner_orientation(const std::array<uint8_t, 8>& co) {
  int value = 0;
  for (int i = 0; i < 7; ++i) {
    value = value * 3 + co[i];
  }
  return value;
}

std::array<uint8_t, 8> unrank_corner_orientation(int coord) {
  std::array<uint8_t, 8> co{};
  int sum = 0;
  for (int i = 6; i >= 0; --i) {
    co[i] = static_cast<uint8_t>(coord % 3);
    sum += co[i];
    coord /= 3;
  }
  co[7] = static_cast<uint8_t>((3 - (sum % 3)) % 3);
  return co;
}

int rank_edge_orientation(const std::array<uint8_t, 12>& eo) {
  int value = 0;
  for (int i = 0; i < 11; ++i) {
    value = (value << 1) | eo[i];
  }
  return value;
}

std::array<uint8_t, 12> unrank_edge_orientation(int coord) {
  std::array<uint8_t, 12> eo{};
  int parity = 0;
  for (int i = 10; i >= 0; --i) {
    eo[i] = static_cast<uint8_t>(coord & 1);
    parity ^= eo[i];
    coord >>= 1;
  }
  eo[11] = static_cast<uint8_t>(parity);
  return eo;
}

class Solver3x3 {
 public:
  Solver3x3() : model_(3) {
    build_move_list();
    build_move_effects();
    build_slice_masks();
    build_move_tables();
    build_pruning_tables();
  }

  const CubeModel& model() const { return model_; }

  std::optional<std::vector<int>> solve(const CubieState& start) const {
    if (is_solved(start)) {
      return std::vector<int>{};
    }
    std::vector<int> path;
    std::vector<int> solution;
    const int min_depth = heuristic_phase1(start);
    for (int depth = min_depth; depth <= kPhase1MaxDepth; ++depth) {
      if (dfs_phase1(start, depth, -1, path, solution)) {
        return simplify_moves(solution);
      }
    }
    return std::nullopt;
  }

  std::vector<Move> moves_from_indices(const std::vector<int>& indices) const {
    std::vector<Move> moves;
    moves.reserve(indices.size());
    for (int index : indices) {
      moves.push_back(move_specs_[index]);
    }
    return moves;
  }

  std::string stringify(const std::vector<int>& indices) const {
    std::ostringstream oss;
    for (size_t i = 0; i < indices.size(); ++i) {
      if (i != 0) {
        oss << ' ';
      }
      oss << move_tokens_[indices[i]];
    }
    return oss.str();
  }

  const MoveEffect& move_effect(int index) const { return move_effects_[index]; }

 private:
  void build_move_list() {
    const std::array<Face, 6> order = {U, R, F, D, L, B};
    for (Face face : order) {
      for (int turns : {1, 2, 3}) {
        move_specs_.push_back({face, 1, turns});
        std::string token(1, kFaceChars[face]);
        if (turns == 2) {
          token += '2';
        } else if (turns == 3) {
          token += '\'';
        }
        move_tokens_.push_back(token);
      }
    }
    phase2_moves_ = {
        0,   // U
        1,   // U2
        2,   // U'
        9,   // D
        10,  // D2
        11,  // D'
        4,   // R2
        13,  // L2
        7,   // F2
        16,  // B2
    };
    for (size_t i = 0; i < phase2_moves_.size(); ++i) {
      phase2_move_index_[phase2_moves_[i]] = static_cast<int>(i);
    }
    ud_pos_to_index_.fill(-1);
    for (int i = 0; i < 8; ++i) {
      ud_pos_to_index_[i] = i;
    }
    slice_pos_to_index_.fill(-1);
    for (int i = 0; i < 4; ++i) {
      slice_pos_to_index_[8 + i] = i;
    }
  }

  void build_move_effects() {
    const std::string solved = model_.solved_state();
    for (const Move& move : move_specs_) {
      std::string error;
      const std::string moved = model_.apply_move(solved, move);
      auto cubie = facelets_to_cubie(model_, moved, &error);
      if (!cubie.has_value()) {
        throw std::runtime_error("failed to derive move effect: " + error);
      }
      move_effects_.push_back(*cubie);
    }
  }

  void build_slice_masks() {
    mask_to_slice_coord_.assign(1 << 12, -1);
    for (int mask = 0; mask < (1 << 12); ++mask) {
      if (__builtin_popcount(static_cast<unsigned int>(mask)) == 4) {
        mask_to_slice_coord_[mask] = static_cast<int>(slice_masks_.size());
        slice_masks_.push_back(mask);
      }
    }
    const int solved_mask = (1 << 8) | (1 << 9) | (1 << 10) | (1 << 11);
    solved_slice_coord_ = mask_to_slice_coord_[solved_mask];
  }

  void build_move_tables() {
    co_move_.resize(2187);
    for (int coord = 0; coord < 2187; ++coord) {
      const auto co = unrank_corner_orientation(coord);
      for (int move = 0; move < 18; ++move) {
        std::array<uint8_t, 8> next{};
        for (int pos = 0; pos < 8; ++pos) {
          const uint8_t src = move_effects_[move].cp[pos];
          next[pos] = static_cast<uint8_t>(
              (co[src] + move_effects_[move].co[pos]) % 3);
        }
        co_move_[coord][move] = static_cast<uint16_t>(
            rank_corner_orientation(next));
      }
    }

    eo_move_.resize(2048);
    for (int coord = 0; coord < 2048; ++coord) {
      const auto eo = unrank_edge_orientation(coord);
      for (int move = 0; move < 18; ++move) {
        std::array<uint8_t, 12> next{};
        for (int pos = 0; pos < 12; ++pos) {
          const uint8_t src = move_effects_[move].ep[pos];
          next[pos] = static_cast<uint8_t>(eo[src] ^ move_effects_[move].eo[pos]);
        }
        eo_move_[coord][move] = static_cast<uint16_t>(
            rank_edge_orientation(next));
      }
    }

    slice_move_.resize(slice_masks_.size());
    for (size_t coord = 0; coord < slice_masks_.size(); ++coord) {
      for (int move = 0; move < 18; ++move) {
        int next_mask = 0;
        for (int pos = 0; pos < 12; ++pos) {
          const int src = move_effects_[move].ep[pos];
          if (slice_masks_[coord] & (1 << src)) {
            next_mask |= (1 << pos);
          }
        }
        slice_move_[coord][move] =
            static_cast<uint16_t>(mask_to_slice_coord_[next_mask]);
      }
    }

    cp_move_.resize(40320);
    for (int coord = 0; coord < 40320; ++coord) {
      const auto cp = unrank_perm<8>(coord);
      for (size_t move_slot = 0; move_slot < phase2_moves_.size(); ++move_slot) {
        const int move = phase2_moves_[move_slot];
        std::array<uint8_t, 8> next{};
        for (int pos = 0; pos < 8; ++pos) {
          next[pos] = cp[move_effects_[move].cp[pos]];
        }
        cp_move_[coord][move_slot] = static_cast<uint16_t>(rank_perm(next));
      }
    }

    udep_move_.resize(40320);
    for (int coord = 0; coord < 40320; ++coord) {
      const auto perm = unrank_perm<8>(coord);
      for (size_t move_slot = 0; move_slot < phase2_moves_.size(); ++move_slot) {
        const int move = phase2_moves_[move_slot];
        std::array<uint8_t, 8> next{};
        for (int pos = 0; pos < 8; ++pos) {
          const int source_pos = move_effects_[move].ep[pos];
          next[pos] = perm[ud_pos_to_index_[source_pos]];
        }
        udep_move_[coord][move_slot] = static_cast<uint16_t>(rank_perm(next));
      }
    }

    slice_perm_move_.resize(24);
    for (int coord = 0; coord < 24; ++coord) {
      const auto perm = unrank_perm<4>(coord);
      for (size_t move_slot = 0; move_slot < phase2_moves_.size(); ++move_slot) {
        const int move = phase2_moves_[move_slot];
        std::array<uint8_t, 4> next{};
        for (int offset = 0; offset < 4; ++offset) {
          const int pos = 8 + offset;
          const int source_pos = move_effects_[move].ep[pos];
          next[offset] = perm[slice_pos_to_index_[source_pos]];
        }
        slice_perm_move_[coord][move_slot] =
            static_cast<uint16_t>(rank_perm(next));
      }
    }
  }

  template <typename Table>
  std::vector<uint8_t> build_prune(const Table& table, int root) const {
    std::vector<uint8_t> depth(table.size(), kUnknownDepth);
    std::queue<int> q;
    depth[root] = 0;
    q.push(root);
    while (!q.empty()) {
      const int current = q.front();
      q.pop();
      const uint8_t next_depth = static_cast<uint8_t>(depth[current] + 1);
      for (size_t move = 0; move < table[current].size(); ++move) {
        const int next = table[current][move];
        if (depth[next] == kUnknownDepth) {
          depth[next] = next_depth;
          q.push(next);
        }
      }
    }
    return depth;
  }

  void build_pruning_tables() {
    co_prune_ = build_prune(co_move_, 0);
    eo_prune_ = build_prune(eo_move_, 0);
    slice_prune_ = build_prune(slice_move_, solved_slice_coord_);
    cp_prune_ = build_prune(cp_move_, 0);
    udep_prune_ = build_prune(udep_move_, 0);
    slice_perm_prune_ = build_prune(slice_perm_move_, 0);
  }

  static bool is_solved(const CubieState& state) {
    return state == solved_cubie();
  }

  int slice_coord(const CubieState& state) const {
    int mask = 0;
    for (int pos = 0; pos < 12; ++pos) {
      if (state.ep[pos] >= 8) {
        mask |= (1 << pos);
      }
    }
    return mask_to_slice_coord_[mask];
  }

  int corner_perm_coord(const CubieState& state) const {
    return rank_perm(state.cp);
  }

  int ud_edge_perm_coord(const CubieState& state) const {
    std::array<uint8_t, 8> perm{};
    for (int i = 0; i < 8; ++i) {
      perm[i] = state.ep[i];
    }
    return rank_perm(perm);
  }

  int slice_perm_coord(const CubieState& state) const {
    std::array<uint8_t, 4> perm{};
    for (int i = 0; i < 4; ++i) {
      perm[i] = static_cast<uint8_t>(state.ep[8 + i] - 8);
    }
    return rank_perm(perm);
  }

  int heuristic_phase1(const CubieState& state) const {
    const int co = rank_corner_orientation(state.co);
    const int eo = rank_edge_orientation(state.eo);
    const int slice = slice_coord(state);
    return std::max({static_cast<int>(co_prune_[co]),
                     static_cast<int>(eo_prune_[eo]),
                     static_cast<int>(slice_prune_[slice])});
  }

  int heuristic_phase2(const CubieState& state) const {
    const int cp = corner_perm_coord(state);
    const int udep = ud_edge_perm_coord(state);
    const int slice_perm = slice_perm_coord(state);
    return std::max({static_cast<int>(cp_prune_[cp]),
                     static_cast<int>(udep_prune_[udep]),
                     static_cast<int>(slice_perm_prune_[slice_perm])});
  }

  bool in_phase1_goal(const CubieState& state) const {
    return rank_corner_orientation(state.co) == 0 &&
           rank_edge_orientation(state.eo) == 0 &&
           slice_coord(state) == solved_slice_coord_;
  }

  bool in_phase2_goal(const CubieState& state) const {
    return corner_perm_coord(state) == 0 && ud_edge_perm_coord(state) == 0 &&
           slice_perm_coord(state) == 0;
  }

  static bool skip_move(int last_face, int next_face) {
    if (last_face < 0) {
      return false;
    }
    if (last_face == next_face) {
      return true;
    }
    return kFaceAxes[last_face] == kFaceAxes[next_face] && next_face < last_face;
  }

  bool solve_phase2(const CubieState& start, std::vector<int>& out) const {
    if (in_phase2_goal(start)) {
      out.clear();
      return true;
    }
    std::vector<int> path;
    const int min_depth = heuristic_phase2(start);
    for (int depth = min_depth; depth <= kPhase2MaxDepth; ++depth) {
      path.clear();
      if (dfs_phase2(start, depth, -1, path)) {
        out = path;
        return true;
      }
    }
    return false;
  }

  bool dfs_phase1(const CubieState& state,
                  int depth_left,
                  int last_face,
                  std::vector<int>& path,
                  std::vector<int>& out) const {
    const int heuristic = heuristic_phase1(state);
    if (heuristic > depth_left) {
      return false;
    }
    if (in_phase1_goal(state)) {
      std::vector<int> phase2;
      if (solve_phase2(state, phase2)) {
        out = path;
        out.insert(out.end(), phase2.begin(), phase2.end());
        return true;
      }
      return false;
    }
    if (depth_left == 0) {
      return false;
    }
    for (int move = 0; move < 18; ++move) {
      const int face = move / 3;
      if (skip_move(last_face, face)) {
        continue;
      }
      path.push_back(move);
      const CubieState next = apply_cubie_move(state, move_effects_[move]);
      if (dfs_phase1(next, depth_left - 1, face, path, out)) {
        return true;
      }
      path.pop_back();
    }
    return false;
  }

  bool dfs_phase2(const CubieState& state,
                  int depth_left,
                  int last_face,
                  std::vector<int>& path) const {
    const int heuristic = heuristic_phase2(state);
    if (heuristic > depth_left) {
      return false;
    }
    if (in_phase2_goal(state)) {
      return true;
    }
    if (depth_left == 0) {
      return false;
    }
    for (int move : phase2_moves_) {
      const int face = move / 3;
      if (skip_move(last_face, face)) {
        continue;
      }
      path.push_back(move);
      const CubieState next = apply_cubie_move(state, move_effects_[move]);
      if (dfs_phase2(next, depth_left - 1, face, path)) {
        return true;
      }
      path.pop_back();
    }
    return false;
  }

  std::vector<int> simplify_moves(const std::vector<int>& moves) const {
    struct Accum {
      int face = -1;
      int turns = 0;
    };
    std::vector<Accum> stack;
    for (int index : moves) {
      const int face = index / 3;
      const int turns = (index % 3) + 1;
      if (!stack.empty() && stack.back().face == face) {
        stack.back().turns = (stack.back().turns + turns) % 4;
        if (stack.back().turns == 0) {
          stack.pop_back();
        }
      } else {
        stack.push_back({face, turns});
      }
    }
    std::vector<int> simplified;
    for (const Accum& item : stack) {
      if (item.turns == 0) {
        continue;
      }
      simplified.push_back(item.face * 3 + (item.turns - 1));
    }
    return simplified;
  }

  CubeModel model_;
  std::vector<Move> move_specs_;
  std::vector<std::string> move_tokens_;
  std::vector<MoveEffect> move_effects_;
  std::vector<int> phase2_moves_;
  std::array<int, 18> phase2_move_index_{};
  std::vector<int> slice_masks_;
  std::vector<int> mask_to_slice_coord_;
  int solved_slice_coord_ = -1;
  std::array<int, 12> ud_pos_to_index_{};
  std::array<int, 12> slice_pos_to_index_{};
  std::vector<std::array<uint16_t, 18>> co_move_;
  std::vector<std::array<uint16_t, 18>> eo_move_;
  std::vector<std::array<uint16_t, 18>> slice_move_;
  std::vector<std::array<uint16_t, 10>> cp_move_;
  std::vector<std::array<uint16_t, 10>> udep_move_;
  std::vector<std::array<uint16_t, 10>> slice_perm_move_;
  std::vector<uint8_t> co_prune_;
  std::vector<uint8_t> eo_prune_;
  std::vector<uint8_t> slice_prune_;
  std::vector<uint8_t> cp_prune_;
  std::vector<uint8_t> udep_prune_;
  std::vector<uint8_t> slice_perm_prune_;
};

std::string normalize_state_string(const std::string& raw, std::string* error) {
  std::string out;
  out.reserve(raw.size());
  for (char c : raw) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      continue;
    }
    const char upper = uppercase_ascii(c);
    switch (upper) {
      case 'U':
      case 'R':
      case 'F':
      case 'D':
      case 'L':
      case 'B':
        out.push_back(upper);
        break;
      case 'W':
        out.push_back('U');
        break;
      case 'G':
        out.push_back('F');
        break;
      case 'Y':
        out.push_back('D');
        break;
      case 'O':
        out.push_back('L');
        break;
      default:
        if (error != nullptr) {
          *error = std::string("invalid state character: ") + upper;
        }
        return {};
    }
  }
  return out;
}

bool validate_color_counts(const std::string& state, int size, std::string* error) {
  std::array<int, 6> counts{};
  for (char c : state) {
    const int face = face_from_char(c);
    if (face < 0) {
      if (error != nullptr) {
        *error = "state contains unsupported colors";
      }
      return false;
    }
    ++counts[face];
  }
  const int expected = size * size;
  for (int face = 0; face < 6; ++face) {
    if (counts[face] != expected) {
      if (error != nullptr) {
        *error = "invalid color counts for input state";
      }
      return false;
    }
  }
  return true;
}

int infer_cube_size_from_state(const std::string& state) {
  if (state.size() == 54) {
    return 3;
  }
  if (state.size() == 150) {
    return 5;
  }
  return 0;
}

bool parse_move_token(const std::string& token,
                      int size,
                      Move* move,
                      std::string* error) {
  if (token.empty()) {
    if (error != nullptr) {
      *error = "empty move token";
    }
    return false;
  }
  size_t index = 0;
  int explicit_width = 0;
  while (index < token.size() &&
         std::isdigit(static_cast<unsigned char>(token[index]))) {
    explicit_width = explicit_width * 10 + (token[index] - '0');
    ++index;
  }
  if (index >= token.size()) {
    if (error != nullptr) {
      *error = "invalid move token: " + token;
    }
    return false;
  }

  const char raw_face = token[index++];
  const char upper_face = uppercase_ascii(raw_face);
  bool wide = false;
  Face face = U;

  if (raw_face == 'x' || raw_face == 'X') {
    face = R;
    explicit_width = size;
  } else if (raw_face == 'y' || raw_face == 'Y') {
    face = U;
    explicit_width = size;
  } else if (raw_face == 'z' || raw_face == 'Z') {
    face = F;
    explicit_width = size;
  } else {
    const int face_id = face_from_char(upper_face);
    if (face_id < 0) {
      if (error != nullptr) {
        *error = "unsupported move token: " + token;
      }
      return false;
    }
    face = static_cast<Face>(face_id);
    if (std::islower(static_cast<unsigned char>(raw_face))) {
      wide = true;
    }
    if (index < token.size() && (token[index] == 'w' || token[index] == 'W')) {
      wide = true;
      ++index;
    }
  }

  int width = 1;
  if (explicit_width > 0) {
    width = explicit_width;
  } else if (wide) {
    width = 2;
  }

  bool prime = false;
  bool half = false;
  for (; index < token.size(); ++index) {
    if (token[index] == '\'') {
      prime = true;
    } else if (token[index] == '2') {
      half = true;
    } else {
      if (error != nullptr) {
        *error = "unsupported move suffix: " + token;
      }
      return false;
    }
  }
  if (width < 1 || width > size) {
    if (error != nullptr) {
      *error = "move width is out of range: " + token;
    }
    return false;
  }
  move->face = face;
  move->width = width;
  move->turns = half ? 2 : (prime ? 3 : 1);
  return true;
}

bool parse_move_sequence(const std::string& text,
                         int size,
                         std::vector<Move>* moves,
                         std::string* error) {
  moves->clear();
  for (const std::string& token : split_ws(text)) {
    Move move;
    if (!parse_move_token(token, size, &move, error)) {
      return false;
    }
    moves->push_back(move);
  }
  return true;
}

bool centers_solved_5(const std::string& state) {
  for (Face face : {U, R, F, D, L, B}) {
    const int base = static_cast<int>(face) * 25;
    const char center = state[base + 2 * 5 + 2];
    for (int row = 1; row <= 3; ++row) {
      for (int col = 1; col <= 3; ++col) {
        if (state[base + row * 5 + col] != center) {
          return false;
        }
      }
    }
  }
  return true;
}

struct EdgeLine5 {
  Vec3 start;
  Vec3 step;
  std::array<Face, 2> faces;
};

constexpr std::array<EdgeLine5, 12> kEdgeLines5 = {{
    {{-1, 2, 2}, {1, 0, 0}, {U, F}},
    {{2, 2, 1}, {0, 0, -1}, {U, R}},
    {{-1, 2, -2}, {1, 0, 0}, {U, B}},
    {{-2, 2, -1}, {0, 0, 1}, {U, L}},
    {{-1, -2, 2}, {1, 0, 0}, {D, F}},
    {{2, -2, 1}, {0, 0, -1}, {D, R}},
    {{-1, -2, -2}, {1, 0, 0}, {D, B}},
    {{-2, -2, -1}, {0, 0, 1}, {D, L}},
    {{2, 1, 2}, {0, -1, 0}, {F, R}},
    {{-2, 1, 2}, {0, -1, 0}, {F, L}},
    {{-2, 1, -2}, {0, -1, 0}, {B, L}},
    {{2, 1, -2}, {0, -1, 0}, {B, R}},
}};

bool edge_groups_paired_5(const CubeModel& model, const std::string& state) {
  for (const EdgeLine5& edge : kEdgeLines5) {
    const Vec3 mid = edge.start + edge.step;
    const char mid_a = state[model.index_of(mid, edge.faces[0])];
    const char mid_b = state[model.index_of(mid, edge.faces[1])];
    for (int wing = 0; wing <= 2; wing += 2) {
      const Vec3 pos = edge.start + edge.step * wing;
      const char wing_a = state[model.index_of(pos, edge.faces[0])];
      const char wing_b = state[model.index_of(pos, edge.faces[1])];
      if (wing_a != mid_a || wing_b != mid_b) {
        return false;
      }
    }
  }
  return true;
}

std::string extract_reduced_3x3_from_5x5(const std::string& state) {
  std::string out;
  out.reserve(54);
  for (Face face : {U, R, F, D, L, B}) {
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        const int src_index =
            static_cast<int>(face) * 25 + (row * 2) * 5 + (col * 2);
        out.push_back(state[src_index]);
      }
    }
  }
  return out;
}

Options parse_args(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      options.help = true;
    } else if (arg == "--self-test") {
      options.self_test = true;
    } else if (arg == "--state") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--state requires a value");
      }
      options.state_text = argv[++i];
    } else if (arg == "--moves") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--moves requires a value");
      }
      options.moves_text = argv[++i];
    } else if (arg == "--size") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--size requires a value");
      }
      options.size = std::stoi(argv[++i]);
    } else {
      throw std::runtime_error("unknown argument: " + arg);
    }
  }
  return options;
}

std::string read_stdin_all() {
  std::ostringstream oss;
  oss << std::cin.rdbuf();
  return oss.str();
}

std::string usage_text() {
  return R"(Usage:
  cube_solver --state <state>
  cube_solver --moves "<moves>" --size 3
  cube_solver --moves "<moves>" --size 5
  cube_solver --self-test

Notes:
  - State format is URFDLB face order, 54 chars for 3x3 or 150 chars for 5x5.
  - WRGYOB colors are accepted and normalized to URFDLB.
  - The native solver fully solves arbitrary 3x3 states.
  - For 5x5, only reduced states are supported for now:
    centers must already be solved and wing pairs must already be paired.)";
}

std::string format_cubie_state(const CubieState& state) {
  std::ostringstream oss;
  oss << "cp:";
  for (uint8_t value : state.cp) {
    oss << ' ' << static_cast<int>(value);
  }
  oss << " | co:";
  for (uint8_t value : state.co) {
    oss << ' ' << static_cast<int>(value);
  }
  oss << " | ep:";
  for (uint8_t value : state.ep) {
    oss << ' ' << static_cast<int>(value);
  }
  oss << " | eo:";
  for (uint8_t value : state.eo) {
    oss << ' ' << static_cast<int>(value);
  }
  return oss.str();
}

std::string describe_edges(const CubeModel& model, const std::string& state) {
  std::ostringstream oss;
  for (size_t pos = 0; pos < kEdgeSpecs.size(); ++pos) {
    std::string observed;
    observed.reserve(2);
    for (Face face : kEdgeSpecs[pos].faces) {
      observed.push_back(state[model.index_of(kEdgeSpecs[pos].pos, face)]);
    }
    oss << '[' << static_cast<int>(pos) << ']';
    oss << kFaceChars[kEdgeSpecs[pos].faces[0]]
        << kFaceChars[kEdgeSpecs[pos].faces[1]] << '=';
    oss << observed << ' ';
  }
  return oss.str();
}

bool run_self_test(const Solver3x3& solver, std::string* error) {
  const CubeModel& model = solver.model();
  const CubieState solved = solved_cubie();
  std::vector<std::vector<int>> sequences;
  for (int move = 0; move < 18; ++move) {
    sequences.push_back({move});
  }
  for (int a = 0; a < 18; ++a) {
    for (int b = 0; b < 18; ++b) {
      sequences.push_back({a, b});
    }
  }
  sequences.push_back({6, 3, 0, 5, 2, 8});   // F R U R' U' F'
  sequences.push_back({3, 0, 5, 2});         // R U R' U'
  sequences.push_back({6, 0, 8, 2});         // F U F' U'
  sequences.push_back({6});                  // F
  sequences.push_back({6, 7, 8});            // F F2 F'

  for (const auto& seq : sequences) {
    std::string facelets = model.solved_state();
    CubieState cubie = solved;
    const auto moves = solver.moves_from_indices(seq);
    facelets = model.apply_moves(facelets, moves);
    std::string parse_error;
    auto parsed = facelets_to_cubie(model, facelets, &parse_error);
    if (!parsed.has_value()) {
      *error = "self-test parse failure for sequence [" + solver.stringify(seq) +
               "]: " + parse_error + "\n" + describe_edges(model, facelets);
      return false;
    }
    for (int move : seq) {
      cubie = apply_cubie_move(cubie, solver.move_effect(move));
    }
    if (!(cubie == *parsed)) {
      *error = "self-test move mismatch for sequence [" + solver.stringify(seq) +
               "]\nexpected " + format_cubie_state(*parsed) + "\nactual   " +
               format_cubie_state(cubie);
      return false;
    }
  }
  return true;
}

ParseResult build_input_state(const Options& options) {
  ParseResult result;
  std::string raw_state;
  if (options.state_text.has_value()) {
    raw_state = *options.state_text;
  } else if (!options.moves_text.has_value()) {
    raw_state = read_stdin_all();
  }

  if (!raw_state.empty()) {
    raw_state = trim_copy(raw_state);
    if (raw_state.empty()) {
      result.error = "input state is empty";
      return result;
    }
    std::string normalize_error;
    const std::string normalized = normalize_state_string(raw_state, &normalize_error);
    if (normalized.empty()) {
      result.error = normalize_error;
      return result;
    }
    const int inferred = infer_cube_size_from_state(normalized);
    if (inferred == 0) {
      result.error = "state length must be 54 or 150 characters";
      return result;
    }
    if (options.size.has_value() && *options.size != inferred) {
      result.error = "--size does not match the state length";
      return result;
    }
    std::string count_error;
    if (!validate_color_counts(normalized, inferred, &count_error)) {
      result.error = count_error;
      return result;
    }
    result.size = inferred;
    result.state = normalized;
  } else {
    if (!options.size.has_value()) {
      result.error = "--size is required when using --moves without --state";
      return result;
    }
    if (*options.size != 3 && *options.size != 5) {
      result.error = "--size must be 3 or 5";
      return result;
    }
    result.size = *options.size;
    CubeModel model(result.size);
    result.state = model.solved_state();
  }

  if (options.moves_text.has_value()) {
    CubeModel model(result.size);
    std::vector<Move> moves;
    std::string parse_error;
    if (!parse_move_sequence(*options.moves_text, result.size, &moves, &parse_error)) {
      result.error = parse_error;
      return result;
    }
    result.state = model.apply_moves(result.state, moves);
  }

  result.ok = true;
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parse_args(argc, argv);
    if (options.help) {
      std::cout << usage_text() << '\n';
      return 0;
    }

    const Solver3x3 solver3;
    if (options.self_test) {
      std::string error;
      if (!run_self_test(solver3, &error)) {
        std::cerr << "error: " << error << '\n';
        return 1;
      }
      std::cout << "self-test ok\n";
      return 0;
    }

    const ParseResult input = build_input_state(options);
    if (!input.ok) {
      std::cerr << "error: " << input.error << '\n';
      return 1;
    }
    if (input.size == 3) {
      std::string error;
      auto cubie = facelets_to_cubie(solver3.model(), input.state, &error);
      if (!cubie.has_value()) {
        std::cerr << "error: " << error << '\n';
        return 1;
      }
      const auto solution = solver3.solve(*cubie);
      if (!solution.has_value()) {
        std::cerr << "error: no 3x3 solution found within native search limits\n";
        return 1;
      }
      const auto moves = solver3.moves_from_indices(*solution);
      const std::string solved =
          solver3.model().apply_moves(input.state, moves);
      if (solved != solver3.model().solved_state()) {
        std::cerr << "error: internal verification failed for 3x3 solution\n";
        return 1;
      }
      std::cout << solver3.stringify(*solution) << '\n';
      return 0;
    }

    const CubeModel model5(5);
    if (!centers_solved_5(input.state) || !edge_groups_paired_5(model5, input.state)) {
      std::cerr
          << "error: native 5x5 reduction phases (centers / wings) are not "
             "implemented yet\n";
      return 1;
    }

    std::string reduced3 = extract_reduced_3x3_from_5x5(input.state);
    std::string error;
    auto cubie = facelets_to_cubie(solver3.model(), reduced3, &error);
    if (!cubie.has_value()) {
      std::cerr << "error: extracted reduced 3x3 state is invalid: " << error
                << '\n';
      return 1;
    }

    const auto solution = solver3.solve(*cubie);
    if (!solution.has_value()) {
      std::cerr << "error: no reduced 5x5 solution found within native search "
                   "limits\n";
      return 1;
    }
    const auto moves = solver3.moves_from_indices(*solution);
    const std::string solved = model5.apply_moves(input.state, moves);
    if (solved != model5.solved_state()) {
      std::cerr << "error: internal verification failed for reduced 5x5 "
                   "solution\n";
      return 1;
    }
    std::cout << solver3.stringify(*solution) << '\n';
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << '\n';
    return 1;
  }
}
