#pragma once
#include "defs.h"
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
constexpr size_t LAYER1_SIZE = 128;

constexpr int CRELU_MIN = 0;
constexpr int CRELU_MAX = 255;

constexpr int SCALE = 400;

constexpr int QA = 255;
constexpr int QB = 64;

constexpr int QAB = QA * QB;

struct alignas(64) NNUE_Params {
  std::array<int16_t, INPUT_SIZE * LAYER1_SIZE> feature_weights;
  std::array<int16_t, LAYER1_SIZE> feature_bias;
  std::array<int16_t, LAYER1_SIZE * 2> output_weights;
  int16_t output_bias;
};

INCBIN(nnue, "src/patty.nnue");
const NNUE_Params &g_nnue = *reinterpret_cast<const NNUE_Params *>(g_nnueData);

template <size_t HiddenSize> struct alignas(64) Accumulator {
  std::array<int16_t, HiddenSize> white;
  std::array<int16_t, HiddenSize> black;

  inline void init(std::span<const int16_t, HiddenSize> bias) {
    std::memcpy(white.data(), bias.data(), bias.size_bytes());
    std::memcpy(black.data(), bias.data(), bias.size_bytes());
  }
};

constexpr int32_t crelu(int16_t x) {
  const auto clipped =
      std::clamp(static_cast<int32_t>(x), CRELU_MIN, CRELU_MAX);
  return clipped * clipped;
}

template <size_t size, size_t weights>
inline void add_to_all(std::array<int16_t, size> &input,
                       const std::array<int16_t, weights> &delta,
                       size_t offset) {
  for (size_t i = 0; i < size; ++i) {
    input[i] += delta[offset + i];
  }
}

template <size_t size, size_t weights>
inline void subtract_from_all(std::array<int16_t, size> &input,
                              const std::array<int16_t, weights> &delta,
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

int32_t crelu_flatten(const std::array<int16_t, LAYER1_SIZE> &us,
                      const std::array<int16_t, LAYER1_SIZE> &them,
                      const std::array<int16_t, LAYER1_SIZE * 2> &weights) {
  int32_t sum = 0;

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    sum += crelu(us[i]) * weights[i];
    sum += crelu(them[i]) * weights[LAYER1_SIZE + i];
  }

  return sum / QA;
}

class NNUE_State {
public:
  std::vector<Accumulator<LAYER1_SIZE>> m_accumulator_stack{};
  Accumulator<LAYER1_SIZE> *m_curr{};


  void push();
  void pop();
  int evaluate(int color);
  void reset_nnue(Position position);

  template <bool Activate>
  inline void update_feature(int piece, int square);

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
          ? crelu_flatten(m_curr->white, m_curr->black, g_nnue.output_weights)
          : crelu_flatten(m_curr->black, m_curr->white, g_nnue.output_weights);
  return (output + g_nnue.output_bias) * SCALE / QAB;
}

template <bool Activate>
inline void NNUE_State::update_feature(int piece, int square) {
  const auto [white_idx, black_idx] = feature_indices(piece, square);

  if constexpr (Activate) {
    add_to_all(m_curr->white, g_nnue.feature_weights, white_idx * LAYER1_SIZE);
    add_to_all(m_curr->black, g_nnue.feature_weights, black_idx * LAYER1_SIZE);
  } else {
    subtract_from_all(m_curr->white, g_nnue.feature_weights,
                      white_idx * LAYER1_SIZE);
    subtract_from_all(m_curr->black, g_nnue.feature_weights,
                      black_idx * LAYER1_SIZE);
  }
}

void NNUE_State::reset_nnue(Position position) {
  m_accumulator_stack.clear();
  m_curr = &m_accumulator_stack.emplace_back();

  m_curr->init(g_nnue.feature_bias);

  for (int square : StandardToMailbox) {
    if (position.board[square]) {
      // printf("%i\n", MAILBOX_TO_STANDARD[square]);
      update_feature<true>(position.board[square], MailboxToStandard_NNUE[square]);
    }
  }
}