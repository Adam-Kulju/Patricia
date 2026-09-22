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
#include <type_traits>
#include <utility>

#if defined(__AVX512BW__) || defined(__AVX2__)
#include <immintrin.h>
#endif

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

#if defined(__GNUC__) || defined(__clang__)
#define NNUE_HOT __attribute__((hot))
#define NNUE_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define NNUE_HOT
#define NNUE_INLINE __forceinline
#else
#define NNUE_HOT
#define NNUE_INLINE inline
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
using NnueVec = __m512i;
inline constexpr size_t NNUE_VEC_WIDTH = 32;
[[nodiscard]] NNUE_INLINE NnueVec nnue_load(const int16_t *p) noexcept {
  return _mm512_load_si512(reinterpret_cast<const __m512i *>(p));
}
NNUE_INLINE void nnue_store(int16_t *p, NnueVec v) noexcept {
  _mm512_store_si512(reinterpret_cast<__m512i *>(p), v);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_sadd16(NnueVec a, NnueVec b) noexcept {
  return _mm512_adds_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_ssub16(NnueVec a, NnueVec b) noexcept {
  return _mm512_subs_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_clamp16(NnueVec v, NnueVec lo,
                                               NnueVec hi) noexcept {
  return _mm512_min_epi16(_mm512_max_epi16(v, lo), hi);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_mullo16(NnueVec a, NnueVec b) noexcept {
  return _mm512_mullo_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_set1_16(int16_t x) noexcept {
  return _mm512_set1_epi16(x);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_zero() noexcept {
  return _mm512_setzero_si512();
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_add32(NnueVec a, NnueVec b) noexcept {
  return _mm512_add_epi32(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_dpwssd(NnueVec sum, NnueVec a,
                                              NnueVec b) noexcept {
#if defined(__AVX512VNNI__)
  return _mm512_dpwssd_epi32(sum, a, b);
#else
  return _mm512_add_epi32(sum, _mm512_madd_epi16(a, b));
#endif
}
[[nodiscard]] NNUE_INLINE int32_t nnue_hadd32(NnueVec v) noexcept {
  return _mm512_reduce_add_epi32(v);
}
#elif defined(__AVX2__)
#define PATRICIA_NNUE_SIMD 1
using NnueVec = __m256i;
inline constexpr size_t NNUE_VEC_WIDTH = 16;
[[nodiscard]] NNUE_INLINE NnueVec nnue_load(const int16_t *p) noexcept {
  return _mm256_load_si256(reinterpret_cast<const __m256i *>(p));
}
NNUE_INLINE void nnue_store(int16_t *p, NnueVec v) noexcept {
  _mm256_store_si256(reinterpret_cast<__m256i *>(p), v);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_sadd16(NnueVec a, NnueVec b) noexcept {
  return _mm256_adds_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_ssub16(NnueVec a, NnueVec b) noexcept {
  return _mm256_subs_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_clamp16(NnueVec v, NnueVec lo,
                                               NnueVec hi) noexcept {
  return _mm256_min_epi16(_mm256_max_epi16(v, lo), hi);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_mullo16(NnueVec a, NnueVec b) noexcept {
  return _mm256_mullo_epi16(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_set1_16(int16_t x) noexcept {
  return _mm256_set1_epi16(x);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_zero() noexcept {
  return _mm256_setzero_si256();
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_add32(NnueVec a, NnueVec b) noexcept {
  return _mm256_add_epi32(a, b);
}
[[nodiscard]] NNUE_INLINE NnueVec nnue_dpwssd(NnueVec sum, NnueVec a,
                                              NnueVec b) noexcept {
#if defined(__AVX512VNNI__) && defined(__AVX512VL__)
  return _mm256_dpwssd_epi32(sum, a, b);
#elif defined(__AVXVNNI__)
  return _mm256_dpwssd_avx_epi32(sum, a, b);
#else
  return _mm256_add_epi32(sum, _mm256_madd_epi16(a, b));
#endif
}
[[nodiscard]] NNUE_INLINE int32_t nnue_hadd32(NnueVec v) noexcept {
  __m128i s = _mm_add_epi32(_mm256_castsi256_si128(v),
                            _mm256_extracti128_si256(v, 1));
  s = _mm_add_epi32(s, _mm_unpackhi_epi64(s, s));
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, 0xB1));
  return _mm_cvtsi128_si32(s);
}
#else
#define PATRICIA_NNUE_SIMD 0
#endif

#if PATRICIA_NNUE_SIMD
static_assert(LAYER1_SIZE % NNUE_VEC_WIDTH == 0,
              "LAYER1_SIZE must be a multiple of the SIMD register width");
[[nodiscard]] inline constexpr size_t nnue_pick_unroll(size_t want) noexcept {
  while (want > 1 && LAYER1_SIZE % (NNUE_VEC_WIDTH * want) != 0)
    want /= 2;
  return want;
}
inline constexpr size_t NNUE_UPDATE_UNROLL = nnue_pick_unroll(8);
inline constexpr size_t NNUE_REFRESH_UNROLL = nnue_pick_unroll(8);
inline constexpr size_t NNUE_DOT_UNROLL =
    nnue_pick_unroll(NNUE_VEC_WIDTH >= 32 ? 8 : 4);
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

[[nodiscard]] NNUE_INLINE const NNUE_Params &get_nnue(size_t index) noexcept {
  PATRICIA_NNUE_ASSERT(index < NUM_NETS);
  const unsigned char *data;
  switch (index) {
  case 0:
    data = g_nnueData;
    break;
  case 1:
    data = g_nnue2Data;
    break;
  default:
    data = g_nnue3Data;
    break;
  }
  return *std::assume_aligned<64>(reinterpret_cast<const NNUE_Params *>(data));
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

[[nodiscard]] NNUE_INLINE const int16_t *
nnue_feature_row(const NNUE_Params &net, size_t index) noexcept {
  PATRICIA_NNUE_ASSERT(index < INPUT_SIZE);
  return std::assume_aligned<64>(net.feature_v.data() + index * LAYER1_SIZE);
}

[[nodiscard]] constexpr int32_t screlu(int16_t x) noexcept {
  const int32_t clipped =
      std::clamp(static_cast<int32_t>(x), SCRELU_MIN, SCRELU_MAX);
  return clipped * clipped;
}

template <size_t NumAdd, size_t NumSub>
NNUE_HOT inline void nnue_apply_update(int16_t *__restrict dst,
                                       const int16_t *__restrict src,
                                       const int16_t *const *adds,
                                       const int16_t *const *subs) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_UPDATE_UNROLL;
  constexpr size_t W = NNUE_VEC_WIDTH;
  for (size_t i = 0; i < LAYER1_SIZE; i += W * unroll) {
    NnueVec regs[unroll];
    for (size_t u = 0; u < unroll; ++u)
      regs[u] = nnue_load(src + i + u * W);
    for (size_t a = 0; a < NumAdd; ++a) {
      const int16_t *row = adds[a] + i;
      for (size_t u = 0; u < unroll; ++u)
        regs[u] = nnue_sadd16(regs[u], nnue_load(row + u * W));
    }
    for (size_t s = 0; s < NumSub; ++s) {
      const int16_t *row = subs[s] + i;
      for (size_t u = 0; u < unroll; ++u)
        regs[u] = nnue_ssub16(regs[u], nnue_load(row + u * W));
    }
    for (size_t u = 0; u < unroll; ++u)
      nnue_store(dst + i + u * W, regs[u]);
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

[[nodiscard]] NNUE_HOT inline int64_t
nnue_dot(const int16_t *__restrict acc, const int16_t *__restrict w) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_DOT_UNROLL;
  constexpr size_t W = NNUE_VEC_WIDTH;
  const NnueVec lo = nnue_set1_16(static_cast<int16_t>(SCRELU_MIN));
  const NnueVec hi = nnue_set1_16(static_cast<int16_t>(QA));
  NnueVec sums[unroll];
  for (size_t u = 0; u < unroll; ++u)
    sums[u] = nnue_zero();
  for (size_t i = 0; i < LAYER1_SIZE; i += W * unroll) {
    for (size_t u = 0; u < unroll; ++u) {
      const NnueVec v = nnue_clamp16(nnue_load(acc + i + u * W), lo, hi);
      const NnueVec p = nnue_mullo16(v, nnue_load(w + i + u * W));
      sums[u] = nnue_dpwssd(sums[u], p, v);
    }
  }
  NnueVec total = sums[0];
  for (size_t u = 1; u < unroll; ++u)
    total = nnue_add32(total, sums[u]);
  return static_cast<int64_t>(nnue_hadd32(total));
#else
  int64_t sum = 0;
  for (size_t i = 0; i < LAYER1_SIZE; ++i)
    sum += static_cast<int64_t>(screlu(acc[i])) * w[i];
  return sum;
#endif
}

template <size_t NumAdd, size_t NumSub>
[[nodiscard]] NNUE_HOT inline int64_t
nnue_update_dot(int16_t *__restrict dst, const int16_t *__restrict src,
                const int16_t *const *adds, const int16_t *const *subs,
                const int16_t *__restrict w) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_DOT_UNROLL;
  constexpr size_t W = NNUE_VEC_WIDTH;
  const NnueVec lo = nnue_set1_16(static_cast<int16_t>(SCRELU_MIN));
  const NnueVec hi = nnue_set1_16(static_cast<int16_t>(QA));
  NnueVec sums[unroll];
  for (size_t u = 0; u < unroll; ++u)
    sums[u] = nnue_zero();
  for (size_t i = 0; i < LAYER1_SIZE; i += W * unroll) {
    NnueVec regs[unroll];
    for (size_t u = 0; u < unroll; ++u)
      regs[u] = nnue_load(src + i + u * W);
    for (size_t a = 0; a < NumAdd; ++a) {
      const int16_t *row = adds[a] + i;
      for (size_t u = 0; u < unroll; ++u)
        regs[u] = nnue_sadd16(regs[u], nnue_load(row + u * W));
    }
    for (size_t s = 0; s < NumSub; ++s) {
      const int16_t *row = subs[s] + i;
      for (size_t u = 0; u < unroll; ++u)
        regs[u] = nnue_ssub16(regs[u], nnue_load(row + u * W));
    }
    for (size_t u = 0; u < unroll; ++u) {
      nnue_store(dst + i + u * W, regs[u]);
      const NnueVec v = nnue_clamp16(regs[u], lo, hi);
      const NnueVec p = nnue_mullo16(v, nnue_load(w + i + u * W));
      sums[u] = nnue_dpwssd(sums[u], p, v);
    }
  }
  NnueVec total = sums[0];
  for (size_t u = 1; u < unroll; ++u)
    total = nnue_add32(total, sums[u]);
  return static_cast<int64_t>(nnue_hadd32(total));
#else
  nnue_apply_update<NumAdd, NumSub>(dst, src, adds, subs);
  return nnue_dot(dst, w);
#endif
}

NNUE_HOT inline void nnue_refresh_side(int16_t *__restrict dst,
                                       const NNUE_Params &net,
                                       const uint16_t *indices,
                                       size_t count) noexcept {
#if PATRICIA_NNUE_SIMD
  constexpr size_t unroll = NNUE_REFRESH_UNROLL;
  constexpr size_t W = NNUE_VEC_WIDTH;
  const int16_t *bias = net.feature_bias.data();
  for (size_t i = 0; i < LAYER1_SIZE; i += W * unroll) {
    NnueVec regs[unroll];
    for (size_t u = 0; u < unroll; ++u)
      regs[u] = nnue_load(bias + i + u * W);
    for (size_t p = 0; p < count; ++p) {
      const int16_t *row = nnue_feature_row(net, indices[p]) + i;
      for (size_t u = 0; u < unroll; ++u)
        regs[u] = nnue_sadd16(regs[u], nnue_load(row + u * W));
    }
    for (size_t u = 0; u < unroll; ++u)
      nnue_store(dst + i + u * W, regs[u]);
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

template <size_t NumAdd, size_t NumSub, typename Kernel>
NNUE_INLINE void nnue_run_update(const NNUE_Update &u, const NNUE_Params &n,
                                 Kernel &&kernel) {
  const int16_t *wa[NumAdd];
  const int16_t *ba[NumAdd];
  const int16_t *ws[NumSub];
  const int16_t *bs[NumSub];
  for (size_t a = 0; a < NumAdd; ++a) {
    wa[a] = nnue_feature_row(n, u.white_add[a]);
    ba[a] = nnue_feature_row(n, u.black_add[a]);
  }
  for (size_t s = 0; s < NumSub; ++s) {
    ws[s] = nnue_feature_row(n, u.white_sub[s]);
    bs[s] = nnue_feature_row(n, u.black_sub[s]);
  }
  kernel(std::integral_constant<size_t, NumAdd>{},
         std::integral_constant<size_t, NumSub>{}, wa, ws, ba, bs);
}

template <typename Kernel>
NNUE_INLINE void nnue_dispatch_update(const NNUE_Update &u,
                                      const NNUE_Params &n, Kernel &&kernel) {
  switch (static_cast<int>(u.adds) * 4 + static_cast<int>(u.subs)) {
  case 4 + 1:
    nnue_run_update<1, 1>(u, n, kernel);
    break;
  case 4 + 2:
    nnue_run_update<1, 2>(u, n, kernel);
    break;
  default:
    nnue_run_update<2, 2>(u, n, kernel);
    break;
  }
}

inline void nnue_prefetch_update(const NNUE_Update &u) noexcept {
  const NNUE_Params &n = get_nnue(u.net);
  for (size_t a = 0; a < u.adds; ++a) {
    nnue_prefetch(nnue_feature_row(n, u.white_add[a]));
    nnue_prefetch(nnue_feature_row(n, u.black_add[a]));
  }
  for (size_t s = 0; s < u.subs; ++s) {
    nnue_prefetch(nnue_feature_row(n, u.white_sub[s]));
    nnue_prefetch(nnue_feature_row(n, u.black_sub[s]));
  }
}

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
};

inline void NNUE_State::apply_pending(Acc &dst, const Acc &src) noexcept {
  const NNUE_Update &u = dst.pending;
  const NNUE_Params &n = get_nnue(u.net);
  nnue_dispatch_update(
      u, n,
      [&](auto na, auto ns, const int16_t *const *wa, const int16_t *const *ws,
          const int16_t *const *ba, const int16_t *const *bs) {
        constexpr size_t A = decltype(na)::value;
        constexpr size_t S = decltype(ns)::value;
        nnue_apply_update<A, S>(dst.white.data(), src.white.data(), wa, ws);
        nnue_apply_update<A, S>(dst.black.data(), src.black.data(), ba, bs);
      });
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
      nnue_prefetch_update((cur + 1)->pending);
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

  nnue_prefetch_update(u);
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

  nnue_prefetch_update(u);
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

  nnue_prefetch_update(u);
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

  const NNUE_Params &n = get_nnue(net_index);
  const int16_t *out = n.output_v.data();
  const bool white_us = (color == Colors::White);
  const int16_t *w_white = white_us ? out : out + LAYER1_SIZE;
  const int16_t *w_black = white_us ? out + LAYER1_SIZE : out;

  int64_t raw;
  if (!acc.computed && m_curr != m_stack.data()) {
    Acc &parent = *(m_curr - 1);
    materialize(&parent);

    const NNUE_Update &u = acc.pending;
    const NNUE_Params &fn = get_nnue(u.net);
    int64_t sum = 0;
    nnue_dispatch_update(
        u, fn,
        [&](auto na, auto ns, const int16_t *const *wa,
            const int16_t *const *ws, const int16_t *const *ba,
            const int16_t *const *bs) {
          constexpr size_t A = decltype(na)::value;
          constexpr size_t S = decltype(ns)::value;
          sum += nnue_update_dot<A, S>(acc.white.data(), parent.white.data(),
                                       wa, ws, w_white);
          sum += nnue_update_dot<A, S>(acc.black.data(), parent.black.data(),
                                       ba, bs, w_black);
        });
    acc.computed = true;
    raw = sum;
  } else {
    materialize(m_curr);
    raw = nnue_dot(acc.white.data(), w_white) +
          nnue_dot(acc.black.data(), w_black);
  }

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
#undef NNUE_INLINE
