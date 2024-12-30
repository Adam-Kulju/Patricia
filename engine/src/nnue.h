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

INCBIN(nnue, "src/rw3.nnue");
INCBIN(nnue2, "src/rw3.nnue");

const NNUE_Params &g_nnue = *reinterpret_cast<const NNUE_Params *>(g_nnueData);
const NNUE_Params &g_nnue2 =
    *reinterpret_cast<const NNUE_Params *>(g_nnue2Data);

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
inline void add_to_all(std::array<int16_t, size> &output,
                       std::array<int16_t, size> &input,
                       const std::array<int16_t, v> &delta, size_t offset) {
  for (size_t i = 0; i < size; ++i) {
    output[i] = input[i] + delta[offset + i];
  }
}

template <size_t size, size_t v>
inline void subtract_from_all(std::array<int16_t, size> &output,
                              std::array<int16_t, size> &input,
                              const std::array<int16_t, v> &delta,
                              size_t offset) {

  for (size_t i = 0; i < size; ++i) {
    output[i] = input[i] - delta[offset + i];
  }
}

std::pair<size_t, size_t> feature_indices(int piece, int sq) {
  constexpr size_t color_stride = 64 * 6;
  constexpr size_t piece_stride = 64;

  const auto base = static_cast<int>(piece / 2 - 1);
  const size_t color = piece & 1;

  const auto whiteIdx =
      color * color_stride + base * piece_stride + static_cast<size_t>(sq);
  const auto blackIdx = (color ^ 1) * color_stride + base * piece_stride +
                        (static_cast<size_t>(sq ^ 56));

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
  Accumulator<LAYER1_SIZE> m_accumulator_stack[MaxSearchDepth];
  Accumulator<LAYER1_SIZE> *m_curr;

  void add_sub(int from_piece, int from, int to_piece, int to, int phase);
  void add_sub_sub(int from_piece, int from, int to_piece, int to, int captured,
                   int captured_pos, int phase);
  void add_add_sub_sub(int piece1, int from1, int to1, int piece2, int from2,
                       int to2, int phase);
  void pop();
  int evaluate(int color, int phase);
  void reset_nnue(const Position &position, int phase);
  void reset_and_add_sub_sub(const Position &position, int from_piece, int from,
                             int to_piece, int to, int captured,
                             int captured_sq, int phase);

  template <bool Activate>
  inline void update_feature(int piece, int square, int phase);

  NNUE_State() {}
};

void NNUE_State::add_sub(int from_piece, int from, int to_piece, int to,
                         int phase) {

  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to, black_to] = feature_indices(to_piece, to);

  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    m_curr[1].white[i] = m_curr->white[i] +
                         ptr->feature_v[white_to * LAYER1_SIZE + i] -
                         ptr->feature_v[white_from * LAYER1_SIZE + i];

    m_curr[1].black[i] = m_curr->black[i] +
                         ptr->feature_v[black_to * LAYER1_SIZE + i] -
                         ptr->feature_v[black_from * LAYER1_SIZE + i];
  }

  m_curr++;
}

void NNUE_State::add_sub_sub(int from_piece, int from, int to_piece, int to,
                             int captured, int captured_sq, int phase) {
  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to, black_to] = feature_indices(to_piece, to);
  const auto [white_capt, black_capt] = feature_indices(captured, captured_sq);

  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    m_curr[1].white[i] = m_curr->white[i] +
                         ptr->feature_v[white_to * LAYER1_SIZE + i] -
                         ptr->feature_v[white_from * LAYER1_SIZE + i] -
                         ptr->feature_v[white_capt * LAYER1_SIZE + i];

    m_curr[1].black[i] = m_curr->black[i] +
                         ptr->feature_v[black_to * LAYER1_SIZE + i] -
                         ptr->feature_v[black_from * LAYER1_SIZE + i] -
                         ptr->feature_v[black_capt * LAYER1_SIZE + i];
  }

  m_curr++;
}

void NNUE_State::add_add_sub_sub(int piece1, int from1, int to1, int piece2,
                                 int from2, int to2, int phase) {
  const auto [white_from1, black_from1] = feature_indices(piece1, from1);
  const auto [white_to1, black_to1] = feature_indices(piece1, to1);
  const auto [white_from2, black_from2] = feature_indices(piece2, from2);
  const auto [white_to2, black_to2] = feature_indices(piece2, to2);

  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    m_curr[1].white[i] = m_curr->white[i] +
                         ptr->feature_v[white_to1 * LAYER1_SIZE + i] -
                         ptr->feature_v[white_from1 * LAYER1_SIZE + i] +
                         ptr->feature_v[white_to2 * LAYER1_SIZE + i] -
                         ptr->feature_v[white_from2 * LAYER1_SIZE + i];

    m_curr[1].black[i] = m_curr->black[i] +
                         ptr->feature_v[black_to1 * LAYER1_SIZE + i] -
                         ptr->feature_v[black_from1 * LAYER1_SIZE + i] +
                         ptr->feature_v[black_to2 * LAYER1_SIZE + i] -
                         ptr->feature_v[black_from2 * LAYER1_SIZE + i];
  }

  m_curr++;
}

void NNUE_State::pop() { m_curr--; }

int NNUE_State::evaluate(int color, int phase) {

  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);
  const auto output =
      color == Colors::White
          ? screlu_flatten(m_curr->white, m_curr->black, ptr->output_v)
          : screlu_flatten(m_curr->black, m_curr->white, ptr->output_v);

  return (output + ptr->output_bias) * SCALE / QAB;
}

template <bool Activate>
inline void NNUE_State::update_feature(int piece, int square, int phase) {
  const auto [white_idx, black_idx] = feature_indices(piece, square);
  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);

  if constexpr (Activate) {
    add_to_all(m_curr->white, m_curr->white, ptr->feature_v,
               white_idx * LAYER1_SIZE);
    add_to_all(m_curr->black, m_curr->black, ptr->feature_v,
               black_idx * LAYER1_SIZE);
  } else {
    subtract_from_all(m_curr->white, m_curr->white, ptr->feature_v,
                      white_idx * LAYER1_SIZE);
    subtract_from_all(m_curr->black, m_curr->black, ptr->feature_v,
                      black_idx * LAYER1_SIZE);
  }
}

void NNUE_State::reset_nnue(const Position &position, int phase) {

  m_curr = &m_accumulator_stack[0];
  m_curr->init(phase == PhaseTypes::Middlegame ? g_nnue.feature_bias
                                                  : g_nnue2.feature_bias);

  for (int square = a1; square < SqNone; square++) {
    if (position.board[square] != Pieces::Blank) {
      update_feature<true>(position.board[square], square, phase);
    }
  }
}

void NNUE_State::reset_and_add_sub_sub(const Position &position, int from_piece,
                                       int from, int to_piece, int to,
                                       int captured, int captured_sq,
                                       int phase) {
  m_curr++;
  m_curr->init(phase == PhaseTypes::Middlegame ? g_nnue.feature_bias
                                                  : g_nnue2.feature_bias);

  for (int square = a1; square < SqNone; square++) {
    if (position.board[square] != Pieces::Blank) {
      update_feature<true>(position.board[square], square, phase);
    }
  }

  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to, black_to] = feature_indices(to_piece, to);
  const auto [white_capt, black_capt] = feature_indices(captured, captured_sq);

  const NNUE_Params *ptr =
      (phase == PhaseTypes::Middlegame ? &g_nnue : &g_nnue2);

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    m_curr->white[i] = m_curr->white[i] +
                         ptr->feature_v[white_to * LAYER1_SIZE + i] -
                         ptr->feature_v[white_from * LAYER1_SIZE + i] -
                         ptr->feature_v[white_capt * LAYER1_SIZE + i];

    m_curr->black[i] = m_curr->black[i] +
                         ptr->feature_v[black_to * LAYER1_SIZE + i] -
                         ptr->feature_v[black_from * LAYER1_SIZE + i] -
                         ptr->feature_v[black_capt * LAYER1_SIZE + i];
  }
}
