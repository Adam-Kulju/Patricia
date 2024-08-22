 // SIMD code draws from both
// Altair(https://github.com/Alex2262/AltairChessEngine) and Midnight
// (https://github.com/archishou/MidnightChessEngine).

#pragma once

#if defined(__AVX512F__)
#include <immintrin.h>
constexpr size_t REGISTER_SIZE = 32;
#elif defined(__AVX2__)
#include <immintrin.h>
constexpr size_t REGISTER_SIZE = 16;

#else
constexpr size_t REGISTER_SIZE = 0;

#endif

auto inline int16_load(auto data) {

#if defined(__AVX512F__)
  return _mm512_load_si512(reinterpret_cast<const __m512i *>(data));
#elif defined(__AVX2__)
  return _mm256_load_si256(reinterpret_cast<const __m256i *>(data));
#else
  return 0;
#endif
}

auto inline get_int16_vec(auto data) {
#if defined(__AVX512F__)
  return _mm512_set1_epi16(data);
#elif defined(__AVX2__)
  return _mm256_set1_epi16(data);
#else
  return 0;
#endif
}

auto inline vec_int16_clamp(auto vec, auto min_vec, auto max_vec) {
#if defined(__AVX512F__)
  return _mm512_min_epi16(_mm512_max_epi16(vec, min_vec), max_vec);
  return vec;
#elif defined(__AVX2__)
  return _mm256_min_epi16(_mm256_max_epi16(vec, min_vec), max_vec);
#else
  return 0;
#endif
}

auto inline vec_int16_multiply(auto vec1, auto vec2) {
#if defined(__AVX512F__)
  return _mm512_mullo_epi16(vec1, vec2);
#elif defined(__AVX2__)
  return _mm256_mullo_epi16(vec1, vec2);
#else
  return 0;
#endif
}

auto inline vec_int32_zero() {
#if defined(__AVX512F__)
  return _mm512_setzero_si512();
#elif defined(__AVX2__)
  return _mm256_setzero_si256();
#else
  return 0;
#endif
}

auto inline vec_int32_add(auto vec1, auto vec2) {
#if defined(__AVX512F__)
  return _mm512_add_epi32(vec1, vec2);
#elif defined(__AVX2__)
  return _mm256_add_epi32(vec1, vec2);
#else
  return 0;
#endif
}

auto inline vec_int16_madd_int32(auto vec1, auto vec2) {
#if defined(__AVX512F__)
  return _mm512_madd_epi16(vec1, vec2);
#elif defined(__AVX2__)
  return _mm256_madd_epi16(vec1, vec2);
#else
  return 0;
#endif
}

auto inline vec_int32_hadd(auto vec) {
#if defined(__AVX512F__)
  auto low = _mm512_castsi512_si256(vec);
  auto high = _mm512_extracti32x8_epi32(vec, 1);
  auto sum8 = _mm256_add_epi32(low, high);
  auto sum4 = _mm256_hadd_epi32(sum8, sum8);
  auto sum2 = _mm256_hadd_epi32(sum4, sum4);

  auto lower_number = _mm256_castsi256_si128(sum2);
  auto higher_number = _mm256_extractf128_si256(sum2, 1);
  auto result = _mm_add_epi32(lower_number, higher_number);
  return _mm_extract_epi32(result, 0);

#elif defined(__AVX2__)
  auto sum_into_4 = _mm256_hadd_epi32(vec, vec);
  auto sum_into_2 = _mm256_hadd_epi32(sum_into_4, sum_into_4);

  auto lane_1 = _mm256_castsi256_si128(sum_into_2);
  auto lane_2 = _mm256_extractf128_si256(sum_into_2, 1);
  auto result = _mm_add_epi32(lane_1, lane_2);
  auto rel = _mm_extract_epi32(result, 0);

  return rel;
#else
  return 0;
#endif
}
