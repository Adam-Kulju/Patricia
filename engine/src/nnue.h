#pragma once
#include "defs.h"
#include "simd.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

#if defined(NDEBUG) || defined(__OPTIMIZE__) ||                                \
    (defined(_MSC_VER) && !defined(_DEBUG))
#define PATRICIA_NNUE_ASSERT(condition) ((void)0)
#else
#define PATRICIA_NNUE_ASSERT(condition) assert(condition)
#endif

#ifdef _MSC_VER
#define W_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "incbin.h"
#undef INCBIN_ALIGNMENT
#define INCBIN_ALIGNMENT 64

#ifdef W_MSVC
#pragma pop_macro("_MSC_VER")
#undef W_MSVC
#endif

inline constexpr size_t INPUT_SIZE = 768;
inline constexpr size_t LAYER1_SIZE = 1024;

inline constexpr int SCRELU_MIN = 0;
inline constexpr int SCRELU_MAX = 255;

inline constexpr int SCALE = 400;

inline constexpr int QA = 255;
inline constexpr int QB = 64;
inline constexpr int QAB = QA * QB;

inline constexpr size_t NUM_NETS = 3;

inline constexpr size_t MAX_ACCUMULATOR_ADDS = 2;
inline constexpr size_t MAX_ACCUMULATOR_SUBS = 2;
inline constexpr size_t MAX_BOARD_PIECES = 64;

static_assert(SCRELU_MAX == QA, "SCReLU clamp upper bound must equal QA");
#if defined(__AVX512BW__) || defined(__AVX2__)
static_assert(LAYER1_SIZE % (REGISTER_SIZE * 2) == 0,
              "LAYER1_SIZE must be a multiple of the SIMD register width");
#endif

inline const auto SCRELU_MIN_VEC = get_int16_vec(SCRELU_MIN);
inline const auto QA_VEC = get_int16_vec(QA);

inline void nnue_prefetch(const void *ptr) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  __builtin_prefetch(ptr, 0, 3);
#elif defined(_MSC_VER)
  _mm_prefetch(reinterpret_cast<const char *>(ptr), _MM_HINT_T0);
#else
  (void)ptr;
#endif
}

#if defined(__AVX512BW__)
#define PATRICIA_NNUE_SIMD 1
using NnueVec16 = __m512i;
[[nodiscard]] inline NnueVec16 nnue_vec_add16(NnueVec16 a,
                                              NnueVec16 b) noexcept {
  return _mm512_add_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_sub16(NnueVec16 a,
                                              NnueVec16 b) noexcept {
  return _mm512_sub_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_sadd16(NnueVec16 a,
                                               NnueVec16 b) noexcept {
  return _mm512_adds_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_ssub16(NnueVec16 a,
                                               NnueVec16 b) noexcept {
  return _mm512_subs_epi16(a, b);
}
inline void nnue_vec_store16(int16_t *dst, NnueVec16 value) noexcept {
  _mm512_store_si512(reinterpret_cast<__m512i *>(dst), value);
}
#elif defined(__AVX2__)
#define PATRICIA_NNUE_SIMD 1
using NnueVec16 = __m256i;
[[nodiscard]] inline NnueVec16 nnue_vec_add16(NnueVec16 a,
                                              NnueVec16 b) noexcept {
  return _mm256_add_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_sub16(NnueVec16 a,
                                              NnueVec16 b) noexcept {
  return _mm256_sub_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_sadd16(NnueVec16 a,
                                               NnueVec16 b) noexcept {
  return _mm256_adds_epi16(a, b);
}
[[nodiscard]] inline NnueVec16 nnue_vec_ssub16(NnueVec16 a,
                                               NnueVec16 b) noexcept {
  return _mm256_subs_epi16(a, b);
}
inline void nnue_vec_store16(int16_t *dst, NnueVec16 value) noexcept {
  _mm256_store_si256(reinterpret_cast<__m256i *>(dst), value);
}
#else
#define PATRICIA_NNUE_SIMD 0
#endif

#if PATRICIA_NNUE_SIMD
inline constexpr size_t NNUE_UPDATE_UNROLL =
    (LAYER1_SIZE % (REGISTER_SIZE * 4) == 0)   ? 4
    : (LAYER1_SIZE % (REGISTER_SIZE * 2) == 0) ? 2
                                               : 1;
inline constexpr size_t NNUE_REFRESH_UNROLL =
    (LAYER1_SIZE % (REGISTER_SIZE * 8) == 0) ? 8 : NNUE_UPDATE_UNROLL;
#endif

struct alignas(64) NNUE_Params {
  std::array<int16_t, INPUT_SIZE * LAYER1_SIZE> feature_v;
  std::array<int16_t, LAYER1_SIZE> feature_bias;
  std::array<int16_t, LAYER1_SIZE * 2> output_v;
  int16_t output_bias;
};

INCBIN(nnue, "nets/fingolfin.nnue");
INCBIN(nnue2, "nets/finarfin.nnue");
INCBIN(nnue3, "nets/feanor.nnue");

[[nodiscard]] inline const NNUE_Params &get_nnue(size_t index) noexcept {
  static const std::array<const NNUE_Params *, NUM_NETS> nets = {
      reinterpret_cast<const NNUE_Params *>(g_nnueData),
      reinterpret_cast<const NNUE_Params *>(g_nnue2Data),
      reinterpret_cast<const NNUE_Params *>(g_nnue3Data),
  };
  PATRICIA_NNUE_ASSERT(index < nets.size());
  return *std::assume_aligned<64>(nets[index]);
}

[[nodiscard]] inline constexpr size_t phase_to_index(int phase) noexcept {
  switch (phase) {
  case PhaseTypes::Middlegame:
    return 0;
  case PhaseTypes::Endgame:
    return 1;
  default:
    return 2;
  }
}

[[nodiscard]] inline const NNUE_Params &nnue_for_phase(int phase) noexcept {
  return get_nnue(phase_to_index(phase));
}

[[nodiscard]] inline const int16_t *nnue_feature_row(const NNUE_Params &net,
                                                     size_t index) noexcept {
  PATRICIA_NNUE_ASSERT(index < INPUT_SIZE);
  return net.feature_v.data() + index * LAYER1_SIZE;
}

#if defined(__GNUC__) || defined(__clang__)
#define NNUE_HOT __attribute__((hot))
#else
#define NNUE_HOT
#endif

template <size_t NumAdd, size_t NumSub>
NNUE_HOT inline void nnue_apply_update(int16_t *__restrict dst,
                                       const int16_t *__restrict src,
                                       const int16_t *const *adds,
                                       const int16_t *const *subs) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_UPDATE_UNROLL;
  for (size_t i = 0; i < LAYER1_SIZE; i += REGISTER_SIZE * unroll) {
    NnueVec16 regs[unroll];
    for (size_t u = 0; u < unroll; ++u) {
      regs[u] = int16_load(src + i + u * REGISTER_SIZE);
    }
    for (size_t a = 0; a < NumAdd; ++a) {
      const int16_t *row = adds[a] + i;
      for (size_t u = 0; u < unroll; ++u) {
        regs[u] = nnue_vec_sadd16(regs[u], int16_load(row + u * REGISTER_SIZE));
      }
    }
    for (size_t s = 0; s < NumSub; ++s) {
      const int16_t *row = subs[s] + i;
      for (size_t u = 0; u < unroll; ++u) {
        regs[u] = nnue_vec_ssub16(regs[u], int16_load(row + u * REGISTER_SIZE));
      }
    }
    for (size_t u = 0; u < unroll; ++u) {
      nnue_vec_store16(dst + i + u * REGISTER_SIZE, regs[u]);
    }
  }
#else
  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    int32_t value = src[i];
    for (size_t a = 0; a < NumAdd; ++a)
      value += adds[a][i];
    for (size_t s = 0; s < NumSub; ++s)
      value -= subs[s][i];
    value = std::clamp(value, static_cast<int32_t>(INT16_MIN),
                       static_cast<int32_t>(INT16_MAX));
    dst[i] = static_cast<int16_t>(value);
  }
#endif
}

NNUE_HOT inline void nnue_refresh_side(int16_t *__restrict dst,
                                       const NNUE_Params &net,
                                       const uint16_t *indices,
                                       size_t count) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_REFRESH_UNROLL;
  const int16_t *bias = net.feature_bias.data();
  for (size_t i = 0; i < LAYER1_SIZE; i += REGISTER_SIZE * unroll) {
    NnueVec16 regs[unroll];
    for (size_t u = 0; u < unroll; ++u) {
      regs[u] = int16_load(bias + i + u * REGISTER_SIZE);
    }
    for (size_t p = 0; p < count; ++p) {
      const int16_t *row = nnue_feature_row(net, indices[p]) + i;
      for (size_t u = 0; u < unroll; ++u) {
        regs[u] = nnue_vec_sadd16(regs[u], int16_load(row + u * REGISTER_SIZE));
      }
    }
    for (size_t u = 0; u < unroll; ++u) {
      nnue_vec_store16(dst + i + u * REGISTER_SIZE, regs[u]);
    }
  }
#else
  std::memcpy(dst, net.feature_bias.data(), LAYER1_SIZE * sizeof(int16_t));
  for (size_t p = 0; p < count; ++p) {
    const int16_t *row = nnue_feature_row(net, indices[p]);
    for (size_t i = 0; i < LAYER1_SIZE; ++i) {
      int32_t value = static_cast<int32_t>(dst[i]) + row[i];
      value = std::clamp(value, static_cast<int32_t>(INT16_MIN),
                         static_cast<int32_t>(INT16_MAX));
      dst[i] = static_cast<int16_t>(value);
    }
  }
#endif
}

struct NNUE_Update {
  uint16_t white_add[MAX_ACCUMULATOR_ADDS];
  uint16_t black_add[MAX_ACCUMULATOR_ADDS];
  uint16_t white_sub[MAX_ACCUMULATOR_SUBS];
  uint16_t black_sub[MAX_ACCUMULATOR_SUBS];
  uint8_t adds;
  uint8_t subs;
  uint8_t net;
};

template <size_t HiddenSize> struct alignas(64) Accumulator {
  std::array<int16_t, HiddenSize> white;
  std::array<int16_t, HiddenSize> black;

  NNUE_Update pending{};
  int32_t cached_eval = 0;
  int8_t cached_color = -1;
  int8_t cached_net = -1;
  bool computed = false;
  bool cached = false;

  void init(std::span<const int16_t, HiddenSize> bias) noexcept {
    std::memcpy(white.data(), bias.data(), bias.size_bytes());
    std::memcpy(black.data(), bias.data(), bias.size_bytes());
    computed = true;
    cached = false;
  }
};

[[nodiscard]] inline std::pair<size_t, size_t>
feature_indices(int piece, int sq) noexcept {
  constexpr size_t color_stride = 64 * 6;
  constexpr size_t piece_stride = 64;

  const size_t base = static_cast<size_t>(piece / 2 - 1);
  const size_t color = static_cast<size_t>(piece & 1);

  const size_t white_idx =
      color * color_stride + base * piece_stride + static_cast<size_t>(sq);
  const size_t black_idx = (color ^ 1) * color_stride + base * piece_stride +
                           static_cast<size_t>(sq ^ 56);

  return {white_idx, black_idx};
}

[[nodiscard]] constexpr int32_t screlu(int16_t x) noexcept {
  const int32_t clipped =
      std::clamp(static_cast<int32_t>(x), SCRELU_MIN, SCRELU_MAX);
  return clipped * clipped;
}

[[nodiscard]] NNUE_HOT inline int64_t
screlu_flatten(const std::array<int16_t, LAYER1_SIZE> &us,
               const std::array<int16_t, LAYER1_SIZE> &them,
               const std::array<int16_t, LAYER1_SIZE * 2> &weights) noexcept {
#if defined(__AVX512BW__) || defined(__AVX2__)
  const auto accumulate = [](auto &sum, const int16_t *acc, const int16_t *w) {
    auto v = int16_load(acc);
    v = vec_int16_clamp(v, SCRELU_MIN_VEC, QA_VEC);
    const auto product = vec_int16_multiply(v, int16_load(w));
    sum = vec_int32_add(sum, vec_int16_madd_int32(product, v));
  };

  auto sum0 = vec_int32_zero();
  auto sum1 = vec_int32_zero();
  auto sum2 = vec_int32_zero();
  auto sum3 = vec_int32_zero();
  auto sum4 = vec_int32_zero();
  auto sum5 = vec_int32_zero();
  auto sum6 = vec_int32_zero();
  auto sum7 = vec_int32_zero();

  constexpr size_t stride = REGISTER_SIZE * 4;
  if constexpr (LAYER1_SIZE % stride == 0) {
    for (size_t i = 0; i < LAYER1_SIZE; i += stride) {
      accumulate(sum0, &us[i], &weights[i]);
      accumulate(sum1, &them[i], &weights[LAYER1_SIZE + i]);
      accumulate(sum2, &us[i + REGISTER_SIZE], &weights[i + REGISTER_SIZE]);
      accumulate(sum3, &them[i + REGISTER_SIZE],
                 &weights[LAYER1_SIZE + i + REGISTER_SIZE]);
      accumulate(sum4, &us[i + REGISTER_SIZE * 2],
                 &weights[i + REGISTER_SIZE * 2]);
      accumulate(sum5, &them[i + REGISTER_SIZE * 2],
                 &weights[LAYER1_SIZE + i + REGISTER_SIZE * 2]);
      accumulate(sum6, &us[i + REGISTER_SIZE * 3],
                 &weights[i + REGISTER_SIZE * 3]);
      accumulate(sum7, &them[i + REGISTER_SIZE * 3],
                 &weights[LAYER1_SIZE + i + REGISTER_SIZE * 3]);
    }
  } else {
    for (size_t i = 0; i < LAYER1_SIZE; i += REGISTER_SIZE * 2) {
      accumulate(sum0, &us[i], &weights[i]);
      accumulate(sum1, &them[i], &weights[LAYER1_SIZE + i]);
      accumulate(sum2, &us[i + REGISTER_SIZE], &weights[i + REGISTER_SIZE]);
      accumulate(sum3, &them[i + REGISTER_SIZE],
                 &weights[LAYER1_SIZE + i + REGISTER_SIZE]);
    }
  }

  const auto total = vec_int32_add(
      vec_int32_add(vec_int32_add(sum0, sum1), vec_int32_add(sum2, sum3)),
      vec_int32_add(vec_int32_add(sum4, sum5), vec_int32_add(sum6, sum7)));
  return static_cast<int64_t>(vec_int32_hadd(total));
#else
  int64_t sum = 0;
  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    sum += static_cast<int64_t>(screlu(us[i])) * weights[i];
    sum += static_cast<int64_t>(screlu(them[i])) * weights[LAYER1_SIZE + i];
  }
  return sum;
#endif
}

class NNUE_State {
public:
  NNUE_State() noexcept : m_curr(m_stack.data()) {}

  NNUE_State(const NNUE_State &other) noexcept
      : m_stack(other.m_stack), m_curr(m_stack.data() + other.current_index()) {
  }

  NNUE_State &operator=(const NNUE_State &other) noexcept {
    if (this != &other) {
      const size_t index = other.current_index();
      m_stack = other.m_stack;
      m_curr = m_stack.data() + index;
    }
    return *this;
  }

  NNUE_State(NNUE_State &&other) noexcept : NNUE_State(other) {}

  NNUE_State &operator=(NNUE_State &&other) noexcept {
    return operator=(other);
  }

  void add_sub(int from_piece, int from, int to_piece, int to, int phase);
  void add_sub_sub(int from_piece, int from, int to_piece, int to, int captured,
                   int captured_pos, int phase);
  void add_add_sub_sub(int piece1, int from1, int to1, int piece2, int from2,
                       int to2, int phase);

  void pop() noexcept;
  [[nodiscard]] int evaluate(int color, int phase) const;

  void reset_nnue(const Position &position, int phase);
  void change_phases(const Position &position, int phase);

private:
  using Acc = Accumulator<LAYER1_SIZE>;

  void refresh_from_position(const Position &position, int phase);

  void materialize(Acc *target) const noexcept;

  static void apply_pending(Acc &dst, const Acc &src) noexcept;

  [[nodiscard]] size_t current_index() const noexcept {
    PATRICIA_NNUE_ASSERT(m_curr != nullptr);
    const auto index = m_curr - m_stack.data();
    PATRICIA_NNUE_ASSERT(index >= 0);
    PATRICIA_NNUE_ASSERT(static_cast<size_t>(index) < m_stack.size());
    return static_cast<size_t>(index);
  }

  [[nodiscard]] Acc &next() noexcept {
    PATRICIA_NNUE_ASSERT(m_curr != nullptr);
    PATRICIA_NNUE_ASSERT(m_curr + 1 < m_stack.data() + m_stack.size());
    return *(m_curr + 1);
  }

  std::array<Acc, MaxSearchDepth> m_stack{};
  Acc *m_curr = nullptr;

  [[nodiscard]] static const int16_t *feature_ptr(const NNUE_Params &n,
                                                  size_t idx) noexcept {
    return nnue_feature_row(n, idx);
  }
};

inline void NNUE_State::apply_pending(Acc &dst, const Acc &src) noexcept {
  const NNUE_Update &u = dst.pending;
  const NNUE_Params &n = get_nnue(u.net);

  switch (static_cast<int>(u.adds) * 4 + static_cast<int>(u.subs)) {
  case 4 + 1: {
    const int16_t *wa[1] = {feature_ptr(n, u.white_add[0])};
    const int16_t *ws[1] = {feature_ptr(n, u.white_sub[0])};
    const int16_t *ba[1] = {feature_ptr(n, u.black_add[0])};
    const int16_t *bs[1] = {feature_ptr(n, u.black_sub[0])};
    nnue_apply_update<1, 1>(dst.white.data(), src.white.data(), wa, ws);
    nnue_apply_update<1, 1>(dst.black.data(), src.black.data(), ba, bs);
    break;
  }
  case 4 + 2: {
    const int16_t *wa[1] = {feature_ptr(n, u.white_add[0])};
    const int16_t *ws[2] = {feature_ptr(n, u.white_sub[0]),
                            feature_ptr(n, u.white_sub[1])};
    const int16_t *ba[1] = {feature_ptr(n, u.black_add[0])};
    const int16_t *bs[2] = {feature_ptr(n, u.black_sub[0]),
                            feature_ptr(n, u.black_sub[1])};
    nnue_apply_update<1, 2>(dst.white.data(), src.white.data(), wa, ws);
    nnue_apply_update<1, 2>(dst.black.data(), src.black.data(), ba, bs);
    break;
  }
  default: {
    const int16_t *wa[2] = {feature_ptr(n, u.white_add[0]),
                            feature_ptr(n, u.white_add[1])};
    const int16_t *ws[2] = {feature_ptr(n, u.white_sub[0]),
                            feature_ptr(n, u.white_sub[1])};
    const int16_t *ba[2] = {feature_ptr(n, u.black_add[0]),
                            feature_ptr(n, u.black_add[1])};
    const int16_t *bs[2] = {feature_ptr(n, u.black_sub[0]),
                            feature_ptr(n, u.black_sub[1])};
    nnue_apply_update<2, 2>(dst.white.data(), src.white.data(), wa, ws);
    nnue_apply_update<2, 2>(dst.black.data(), src.black.data(), ba, bs);
    break;
  }
  }
}

NNUE_HOT inline void NNUE_State::materialize(Acc *target) const noexcept {
  if (target->computed) {
    return;
  }

  Acc *base = target;
  const Acc *floor_acc = m_stack.data();
  while (base > floor_acc && !base->computed) {
    --base;
  }
  base->computed = true;

  for (Acc *cur = base + 1; cur <= target; ++cur) {
    if (cur + 1 <= target) {
      const NNUE_Update &nu = (cur + 1)->pending;
      const NNUE_Params &nn = get_nnue(nu.net);
      nnue_prefetch(feature_ptr(nn, nu.white_add[0]));
      nnue_prefetch(feature_ptr(nn, nu.black_add[0]));
      nnue_prefetch(feature_ptr(nn, nu.white_sub[0]));
      nnue_prefetch(feature_ptr(nn, nu.black_sub[0]));
    }
    apply_pending(*cur, *(cur - 1));
    cur->computed = true;
  }
}

inline void NNUE_State::add_sub(int from_piece, int from, int to_piece, int to,
                                int phase) {
  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to, black_to] = feature_indices(to_piece, to);

  Acc &target = next();
  NNUE_Update &u = target.pending;
  u.net = static_cast<uint8_t>(phase_to_index(phase));
  u.adds = 1;
  u.subs = 1;
  u.white_add[0] = static_cast<uint16_t>(white_to);
  u.black_add[0] = static_cast<uint16_t>(black_to);
  u.white_sub[0] = static_cast<uint16_t>(white_from);
  u.black_sub[0] = static_cast<uint16_t>(black_from);
  target.computed = false;
  target.cached = false;

  ++m_curr;
}

inline void NNUE_State::add_sub_sub(int from_piece, int from, int to_piece,
                                    int to, int captured, int captured_sq,
                                    int phase) {
  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to, black_to] = feature_indices(to_piece, to);
  const auto [white_capt, black_capt] = feature_indices(captured, captured_sq);

  Acc &target = next();
  NNUE_Update &u = target.pending;
  u.net = static_cast<uint8_t>(phase_to_index(phase));
  u.adds = 1;
  u.subs = 2;
  u.white_add[0] = static_cast<uint16_t>(white_to);
  u.black_add[0] = static_cast<uint16_t>(black_to);
  u.white_sub[0] = static_cast<uint16_t>(white_from);
  u.black_sub[0] = static_cast<uint16_t>(black_from);
  u.white_sub[1] = static_cast<uint16_t>(white_capt);
  u.black_sub[1] = static_cast<uint16_t>(black_capt);
  target.computed = false;
  target.cached = false;

  ++m_curr;
}

inline void NNUE_State::add_add_sub_sub(int piece1, int from1, int to1,
                                        int piece2, int from2, int to2,
                                        int phase) {
  const auto [wf1, bf1] = feature_indices(piece1, from1);
  const auto [wt1, bt1] = feature_indices(piece1, to1);
  const auto [wf2, bf2] = feature_indices(piece2, from2);
  const auto [wt2, bt2] = feature_indices(piece2, to2);

  Acc &target = next();
  NNUE_Update &u = target.pending;
  u.net = static_cast<uint8_t>(phase_to_index(phase));
  u.adds = 2;
  u.subs = 2;
  u.white_add[0] = static_cast<uint16_t>(wt1);
  u.white_add[1] = static_cast<uint16_t>(wt2);
  u.black_add[0] = static_cast<uint16_t>(bt1);
  u.black_add[1] = static_cast<uint16_t>(bt2);
  u.white_sub[0] = static_cast<uint16_t>(wf1);
  u.white_sub[1] = static_cast<uint16_t>(wf2);
  u.black_sub[0] = static_cast<uint16_t>(bf1);
  u.black_sub[1] = static_cast<uint16_t>(bf2);
  target.computed = false;
  target.cached = false;

  ++m_curr;
}

inline void NNUE_State::pop() noexcept {
  PATRICIA_NNUE_ASSERT(m_curr != nullptr);
  PATRICIA_NNUE_ASSERT(m_curr > m_stack.data());
  --m_curr;
}

NNUE_HOT inline int NNUE_State::evaluate(int color, int phase) const {
  PATRICIA_NNUE_ASSERT(m_curr != nullptr);

  const size_t net_index = phase_to_index(phase);
  Acc &acc = *m_curr;

  if (acc.cached && acc.cached_color == static_cast<int8_t>(color) &&
      acc.cached_net == static_cast<int8_t>(net_index)) {
    return acc.cached_eval;
  }

  materialize(m_curr);

  const NNUE_Params &n = get_nnue(net_index);
  const auto &us = (color == Colors::White) ? acc.white : acc.black;
  const auto &them = (color == Colors::White) ? acc.black : acc.white;

  const int64_t raw = screlu_flatten(us, them, n.output_v);
  const int64_t biased = raw + static_cast<int64_t>(n.output_bias) * QA;
  const int result =
      static_cast<int>((biased * SCALE) /
                       (static_cast<int64_t>(QA) * static_cast<int64_t>(QAB)));

  acc.cached = true;
  acc.cached_color = static_cast<int8_t>(color);
  acc.cached_net = static_cast<int8_t>(net_index);
  acc.cached_eval = result;

  return result;
}

inline void NNUE_State::refresh_from_position(const Position &position,
                                              int phase) {
  PATRICIA_NNUE_ASSERT(m_curr != nullptr);

  const NNUE_Params &n = nnue_for_phase(phase);

  std::array<uint16_t, MAX_BOARD_PIECES> white_indices{};
  std::array<uint16_t, MAX_BOARD_PIECES> black_indices{};
  size_t count = 0;

  for (int square = a1; square < SqNone; ++square) {
    const int piece = position.board[square];
    if (piece != Pieces::Blank) {
      const auto [white_idx, black_idx] = feature_indices(piece, square);
      white_indices[count] = static_cast<uint16_t>(white_idx);
      black_indices[count] = static_cast<uint16_t>(black_idx);
      ++count;
    }
  }

  nnue_refresh_side(m_curr->white.data(), n, white_indices.data(), count);
  nnue_refresh_side(m_curr->black.data(), n, black_indices.data(), count);

  m_curr->computed = true;
  m_curr->cached = false;
}

inline void NNUE_State::reset_nnue(const Position &position, int phase) {
  m_curr = m_stack.data();
  refresh_from_position(position, phase);
}

inline void NNUE_State::change_phases(const Position &position, int phase) {
  PATRICIA_NNUE_ASSERT(m_curr != nullptr);
  PATRICIA_NNUE_ASSERT(m_curr + 1 < m_stack.data() + m_stack.size());
  ++m_curr;
  refresh_from_position(position, phase);
}

#undef PATRICIA_NNUE_SIMD
#undef PATRICIA_NNUE_ASSERT
#undef NNUE_HOT
