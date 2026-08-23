#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
#define SIMD_FORCE_INLINE inline __attribute__((always_inline, hot))
#elif defined(_MSC_VER)
#define SIMD_FORCE_INLINE __forceinline
#else
#define SIMD_FORCE_INLINE inline
#endif

#if defined(__AVX512F__) && defined(__AVX512BW__)
#include <immintrin.h>
#define SIMD_BACKEND_AVX512
constexpr size_t REGISTER_SIZE = 32;
#elif defined(__AVX2__)
#include <immintrin.h>
#define SIMD_BACKEND_AVX2
constexpr size_t REGISTER_SIZE = 16;
#elif defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#define SIMD_BACKEND_SSE2
constexpr size_t REGISTER_SIZE = 8;
#elif defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
#define SIMD_BACKEND_NEON
constexpr size_t REGISTER_SIZE = 8;
#else
#define SIMD_BACKEND_SCALAR
constexpr size_t REGISTER_SIZE = 0;
#endif

#if defined(SIMD_BACKEND_AVX512) || defined(SIMD_BACKEND_AVX2) ||              \
    defined(SIMD_BACKEND_SSE2)
SIMD_FORCE_INLINE int32_t simd_reduce_epi32_128(__m128i vec) {
  vec = _mm_add_epi32(vec, _mm_unpackhi_epi64(vec, vec));
  vec = _mm_add_epi32(vec, _mm_shuffle_epi32(vec, _MM_SHUFFLE(1, 1, 1, 1)));
  return _mm_cvtsi128_si32(vec);
}
#endif

SIMD_FORCE_INLINE auto int16_load(auto data) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_loadu_si512(reinterpret_cast<const void *>(data));
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_loadu_si256(reinterpret_cast<const __m256i *>(data));
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_loadu_si128(reinterpret_cast<const __m128i *>(data));
#elif defined(SIMD_BACKEND_NEON)
  return vld1q_s16(reinterpret_cast<const int16_t *>(data));
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto get_int16_vec(auto data) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_set1_epi16(static_cast<short>(data));
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_set1_epi16(static_cast<short>(data));
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_set1_epi16(static_cast<short>(data));
#elif defined(SIMD_BACKEND_NEON)
  return vdupq_n_s16(static_cast<int16_t>(data));
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int16_clamp(auto vec, auto min_vec, auto max_vec) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_min_epi16(_mm512_max_epi16(vec, min_vec), max_vec);
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_min_epi16(_mm256_max_epi16(vec, min_vec), max_vec);
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_min_epi16(_mm_max_epi16(vec, min_vec), max_vec);
#elif defined(SIMD_BACKEND_NEON)
  return vminq_s16(vmaxq_s16(vec, min_vec), max_vec);
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int16_multiply(auto vec1, auto vec2) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_mullo_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_mullo_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_mullo_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_NEON)
  return vmulq_s16(vec1, vec2);
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int32_zero() {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_setzero_si512();
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_setzero_si256();
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_setzero_si128();
#elif defined(SIMD_BACKEND_NEON)
  return vdupq_n_s32(0);
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int32_add(auto vec1, auto vec2) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_add_epi32(vec1, vec2);
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_add_epi32(vec1, vec2);
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_add_epi32(vec1, vec2);
#elif defined(SIMD_BACKEND_NEON)
  return vaddq_s32(vec1, vec2);
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int16_madd_int32(auto vec1, auto vec2) {
#if defined(SIMD_BACKEND_AVX512)
  return _mm512_madd_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_AVX2)
  return _mm256_madd_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_SSE2)
  return _mm_madd_epi16(vec1, vec2);
#elif defined(SIMD_BACKEND_NEON)
  return vpaddq_s32(vmull_s16(vget_low_s16(vec1), vget_low_s16(vec2)),
                    vmull_high_s16(vec1, vec2));
#else
  return 0;
#endif
}

SIMD_FORCE_INLINE auto vec_int32_hadd(auto vec) {
#if defined(SIMD_BACKEND_AVX512)
  const __m256i sum8 = _mm256_add_epi32(_mm512_castsi512_si256(vec),
                                        _mm512_extracti64x4_epi64(vec, 1));
  return simd_reduce_epi32_128(_mm_add_epi32(
      _mm256_castsi256_si128(sum8), _mm256_extracti128_si256(sum8, 1)));
#elif defined(SIMD_BACKEND_AVX2)
  return simd_reduce_epi32_128(_mm_add_epi32(
      _mm256_castsi256_si128(vec), _mm256_extracti128_si256(vec, 1)));
#elif defined(SIMD_BACKEND_SSE2)
  return simd_reduce_epi32_128(vec);
#elif defined(SIMD_BACKEND_NEON)
  return static_cast<int32_t>(vaddvq_s32(vec));
#else
  return 0;
#endif
}
