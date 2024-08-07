#pragma once
#include "defs.h"
#include "simd.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>
#ifdef _MSC_VER
#define W_MSVC
#pragma push_macro("_MSC_VER")
#undef _MSC_VER
#endif

#define INCBIN_PREFIX g_
#include "incbin.h"

#ifdef W_MSVC
#pragma pop_macro("_MSC_VER")
#undef W_MSVC
#endif

constexpr size_t INPUT_SIZE = 768;
constexpr size_t LAYER1_SIZE = 768;

constexpr int SCRELU_MIN = 0;
constexpr int SCRELU_MAX = 255;

constexpr int SCALE = 400;

constexpr int QA = 255;
constexpr int QB = 64;

const auto SCRELU_MIN_VEC = get_int16_vec(SCRELU_MIN);
const auto QA_VEC = get_int16_vec(QA);

constexpr int QAB = QA * QB;

struct alignas(64) NNUE_Params {
  std::array<int16_t, INPUT_SIZE * LAYER1_SIZE> feature_v;
  std::array<int16_t, LAYER1_SIZE> feature_bias;
  std::array<int16_t, LAYER1_SIZE * 2> output_v;
  int16_t output_bias;
};

INCBIN(nnue, "src/abby.nnue");
const NNUE_Params &g_nnue = *reinterpret_cast<const NNUE_Params *>(g_nnueData);

template <size_t HiddenSize> struct alignas(64) Accumulator {
  std::array<int16_t, HiddenSize> white;
  std::array<int16_t, HiddenSize> black;

  inline void init(std::span<const int16_t, HiddenSize> bias) {
    std::memcpy(white.data(), bias.data(), bias.size_bytes());
    std::memcpy(black.data(), bias.data(), bias.size_bytes());
  }
};

constexpr int32_t screlu(int16_t x) {
  const auto clipped =
      std::clamp(static_cast<int32_t>(x), SCRELU_MIN, SCRELU_MAX);
  return clipped * clipped;
}

template <size_t size, size_t v>
inline void add_to_all(std::array<int16_t, size> &input,
                       const std::array<int16_t, v> &delta, size_t offset) {
  for (size_t i = 0; i < size; ++i) {
    input[i] += delta[offset + i];
  }
}

template <size_t size, size_t v>
inline void subtract_from_all(std::array<int16_t, size> &input,
                              const std::array<int16_t, v> &delta,
                              size_t offset) {
  for (size_t i = 0; i < size; ++i) {
    input[i] -= delta[offset + i];
  }
}

std::pair<size_t, size_t> feature_indices(int piece, int sq) {
  constexpr size_t color_stride = 64 * 6;
  constexpr size_t piece_stride = 64;

  const auto base = static_cast<int>(piece / 2 - 1);
  const size_t color = piece & 1;

  const auto whiteIdx =
      color * color_stride + base * piece_stride + static_cast<size_t>(sq ^ 56);
  const auto blackIdx = (color ^ 1) * color_stride + base * piece_stride +
                        (static_cast<size_t>(sq));

  return {whiteIdx, blackIdx};
}

inline int32_t
screlu_flatten(const std::array<int16_t, LAYER1_SIZE> &us,
               const std::array<int16_t, LAYER1_SIZE> &them,
               const std::array<int16_t, LAYER1_SIZE * 2> &weights) {

#if defined(__AVX512F__) || defined(__AVX2__)

  auto sum = vec_int32_zero();

  for (size_t i = 0; i < LAYER1_SIZE; i += REGISTER_SIZE) {

    auto v_us = int16_load(&us[i]);
    auto w_us = int16_load(&weights[i]);

    v_us = vec_int16_clamp(v_us, SCRELU_MIN_VEC, QA_VEC);

    auto our_product = vec_int16_multiply(v_us, w_us);

    auto our_result = vec_int16_madd_int32(our_product, v_us);

    sum = vec_int32_add(sum, our_result);

    auto v_them = int16_load(&them[i]);
    auto w_them = int16_load(&weights[LAYER1_SIZE + i]);

    v_them = vec_int16_clamp(v_them, SCRELU_MIN_VEC, QA_VEC);

    auto their_product = vec_int16_multiply(v_them, w_them);

    auto their_result = vec_int16_madd_int32(their_product, v_them);

    sum = vec_int32_add(sum, their_result);
  }

  return vec_int32_hadd(sum) / QA;

#else

  int32_t sum = 0;

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    sum += screlu(us[i]) * weights[i];
    sum += screlu(them[i]) * weights[LAYER1_SIZE + i];
  }

  return sum / QA;

#endif
}

class NNUE_State {
public:
  std::vector<Accumulator<LAYER1_SIZE>> m_accumulator_stack{};
  Accumulator<LAYER1_SIZE> *m_curr{};

  void push();
  void pop();
  int evaluate(int color);
  void reset_nnue(Position position);

  template <bool Activate> inline void update_feature(int piece, int square);

  NNUE_State() { m_accumulator_stack.reserve(100); }
};

void NNUE_State::push() {
  m_accumulator_stack.push_back(*m_curr);
  m_curr = &m_accumulator_stack.back();
}

void NNUE_State::pop() {
  m_accumulator_stack.pop_back();
  m_curr = &m_accumulator_stack.back();
}

int NNUE_State::evaluate(int color) {
  const auto output =
      color == Colors::White
          ? screlu_flatten(m_curr->white, m_curr->black, g_nnue.output_v)
          : screlu_flatten(m_curr->black, m_curr->white, g_nnue.output_v);
  return (output + g_nnue.output_bias) * SCALE / QAB;
}

template <bool Activate>
inline void NNUE_State::update_feature(int piece, int square) {
  const auto [white_idx, black_idx] = feature_indices(piece, square);

  if constexpr (Activate) {
    add_to_all(m_curr->white, g_nnue.feature_v, white_idx * LAYER1_SIZE);
    add_to_all(m_curr->black, g_nnue.feature_v, black_idx * LAYER1_SIZE);
  } else {
    subtract_from_all(m_curr->white, g_nnue.feature_v, white_idx * LAYER1_SIZE);
    subtract_from_all(m_curr->black, g_nnue.feature_v, black_idx * LAYER1_SIZE);
  }
}

void NNUE_State::reset_nnue(Position position) {
  m_accumulator_stack.clear();
  m_curr = &m_accumulator_stack.emplace_back();

  m_curr->init(g_nnue.feature_bias);

  for (int square : StandardToMailbox) {
    if (position.board[square] != Pieces::Blank) {
      update_feature<true>(position.board[square],
                           MailboxToStandard_NNUE[square]);
    }
  }
}