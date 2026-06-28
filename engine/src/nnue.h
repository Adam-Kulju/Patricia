#pragma once
#include "defs.h"
#include "simd.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>

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

inline constexpr size_t INPUT_SIZE  = 768;
inline constexpr size_t LAYER1_SIZE = 1024;

inline constexpr int SCRELU_MIN = 0;
inline constexpr int SCRELU_MAX = 255;

inline constexpr int SCALE = 400;

inline constexpr int QA  = 255;
inline constexpr int QB  = 64;
inline constexpr int QAB = QA * QB;

inline constexpr size_t NUM_NETS = 3;

static_assert(SCRELU_MAX == QA, "SCReLU clamp upper bound must equal QA");
static_assert(LAYER1_SIZE % REGISTER_SIZE == 0,
              "LAYER1_SIZE must be a multiple of the SIMD register width");

inline const auto SCRELU_MIN_VEC = get_int16_vec(SCRELU_MIN);
inline const auto QA_VEC         = get_int16_vec(QA);

struct alignas(64) NNUE_Params {
  std::array<int16_t, INPUT_SIZE * LAYER1_SIZE> feature_v;
  std::array<int16_t, LAYER1_SIZE>              feature_bias;
  std::array<int16_t, LAYER1_SIZE * 2>          output_v;
  int16_t                                       output_bias;
};

INCBIN(nnue,  "nets/fingolfin.nnue");
INCBIN(nnue2, "nets/finarfin.nnue");
INCBIN(nnue3, "nets/feanor.nnue");

[[nodiscard]] inline const NNUE_Params& get_nnue(size_t index) noexcept {
  static const std::array<const NNUE_Params*, NUM_NETS> nets = {
      reinterpret_cast<const NNUE_Params*>(g_nnueData),
      reinterpret_cast<const NNUE_Params*>(g_nnue2Data),
      reinterpret_cast<const NNUE_Params*>(g_nnue3Data),
  };
  assert(index < nets.size());
  return *std::assume_aligned<64>(nets[index]);
}

[[nodiscard]] inline constexpr size_t phase_to_index(int phase) noexcept {
  switch (phase) {
    case PhaseTypes::Middlegame: return 0;
    case PhaseTypes::Endgame:    return 1;
    default:                     return 2;
  }
}

[[nodiscard]] inline const NNUE_Params& nnue_for_phase(int phase) noexcept {
  return get_nnue(phase_to_index(phase));
}

template <size_t HiddenSize>
struct alignas(64) Accumulator {
  std::array<int16_t, HiddenSize> white;
  std::array<int16_t, HiddenSize> black;

  void init(std::span<const int16_t, HiddenSize> bias) noexcept {
    std::memcpy(white.data(), bias.data(), bias.size_bytes());
    std::memcpy(black.data(), bias.data(), bias.size_bytes());
  }
};

[[nodiscard]] inline std::pair<size_t, size_t>
feature_indices(int piece, int sq) noexcept {
  constexpr size_t color_stride = 64 * 6;
  constexpr size_t piece_stride = 64;

  const size_t base  = static_cast<size_t>(piece / 2 - 1);
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

[[nodiscard]] inline int32_t
screlu_flatten(const std::array<int16_t, LAYER1_SIZE>     &us,
               const std::array<int16_t, LAYER1_SIZE>     &them,
               const std::array<int16_t, LAYER1_SIZE * 2> &weights) noexcept {
#if defined(__AVX512F__) || defined(__AVX2__)
  auto sum = vec_int32_zero();

  const auto accumulate = [&](const int16_t* acc, const int16_t* w) {
    auto v = int16_load(acc);
    v = vec_int16_clamp(v, SCRELU_MIN_VEC, QA_VEC);
    const auto product = vec_int16_multiply(v, int16_load(w));
    sum = vec_int32_add(sum, vec_int16_madd_int32(product, v));
  };

  for (size_t i = 0; i < LAYER1_SIZE; i += REGISTER_SIZE) {
    accumulate(&us[i],   &weights[i]);
    accumulate(&them[i], &weights[LAYER1_SIZE + i]);
  }
  return vec_int32_hadd(sum) / QA;
#else
  int32_t sum = 0;
  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    sum += screlu(us[i])   * weights[i];
    sum += screlu(them[i]) * weights[LAYER1_SIZE + i];
  }
  return sum / QA;
#endif
}

class NNUE_State {
public:
  NNUE_State() = default;

  void add_sub(int from_piece, int from, int to_piece, int to, int phase);
  void add_sub_sub(int from_piece, int from, int to_piece, int to,
                   int captured, int captured_pos, int phase);
  void add_add_sub_sub(int piece1, int from1, int to1,
                       int piece2, int from2, int to2, int phase);

  void pop() noexcept;
  [[nodiscard]] int evaluate(int color, int phase) const;

  void reset_nnue(const Position& position, int phase);
  void change_phases(const Position& position, int phase);

private:
  template <bool Activate>
  void update_feature(int piece, int square, int phase) noexcept;

  void refresh_from_position(const Position& position, int phase);

  [[nodiscard]] Accumulator<LAYER1_SIZE>& next() noexcept {
    assert(m_curr + 1 < m_stack.data() + m_stack.size());
    return *(m_curr + 1);
  }

  static void apply_add_sub(Accumulator<LAYER1_SIZE>& dst,
                            const Accumulator<LAYER1_SIZE>& src,
                            const int16_t* w_add, const int16_t* w_sub,
                            const int16_t* b_add, const int16_t* b_sub) noexcept {
    for (size_t i = 0; i < LAYER1_SIZE; ++i) {
      dst.white[i] = src.white[i] + w_add[i] - w_sub[i];
      dst.black[i] = src.black[i] + b_add[i] - b_sub[i];
    }
  }

  static void apply_add_sub_sub(Accumulator<LAYER1_SIZE>& dst,
                                const Accumulator<LAYER1_SIZE>& src,
                                const int16_t* w_add, const int16_t* w_sub1,
                                const int16_t* w_sub2,
                                const int16_t* b_add, const int16_t* b_sub1,
                                const int16_t* b_sub2) noexcept {
    for (size_t i = 0; i < LAYER1_SIZE; ++i) {
      dst.white[i] = src.white[i] + w_add[i] - w_sub1[i] - w_sub2[i];
      dst.black[i] = src.black[i] + b_add[i] - b_sub1[i] - b_sub2[i];
    }
  }

  static void apply_add_add_sub_sub(Accumulator<LAYER1_SIZE>& dst,
                                    const Accumulator<LAYER1_SIZE>& src,
                                    const int16_t* w_add1, const int16_t* w_sub1,
                                    const int16_t* w_add2, const int16_t* w_sub2,
                                    const int16_t* b_add1, const int16_t* b_sub1,
                                    const int16_t* b_add2, const int16_t* b_sub2) noexcept {
    for (size_t i = 0; i < LAYER1_SIZE; ++i) {
      dst.white[i] = src.white[i] + w_add1[i] - w_sub1[i] + w_add2[i] - w_sub2[i];
      dst.black[i] = src.black[i] + b_add1[i] - b_sub1[i] + b_add2[i] - b_sub2[i];
    }
  }

  std::array<Accumulator<LAYER1_SIZE>, MaxSearchDepth> m_stack{};
  Accumulator<LAYER1_SIZE>* m_curr = nullptr;

  [[nodiscard]] static const int16_t*
  feature_ptr(const NNUE_Params& n, size_t idx) noexcept {
    return n.feature_v.data() + idx * LAYER1_SIZE;
  }
};

inline void NNUE_State::add_sub(int from_piece, int from,
                                int to_piece, int to, int phase) {
  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to,   black_to]   = feature_indices(to_piece, to);
  const NNUE_Params& n = nnue_for_phase(phase);

  apply_add_sub(next(), *m_curr,
                feature_ptr(n, white_to),   feature_ptr(n, white_from),
                feature_ptr(n, black_to),   feature_ptr(n, black_from));
  ++m_curr;
}

inline void NNUE_State::add_sub_sub(int from_piece, int from,
                                    int to_piece, int to,
                                    int captured, int captured_sq, int phase) {
  const auto [white_from, black_from] = feature_indices(from_piece, from);
  const auto [white_to,   black_to]   = feature_indices(to_piece, to);
  const auto [white_capt, black_capt] = feature_indices(captured, captured_sq);
  const NNUE_Params& n = nnue_for_phase(phase);

  apply_add_sub_sub(next(), *m_curr,
                    feature_ptr(n, white_to),   feature_ptr(n, white_from), feature_ptr(n, white_capt),
                    feature_ptr(n, black_to),   feature_ptr(n, black_from), feature_ptr(n, black_capt));
  ++m_curr;
}

inline void NNUE_State::add_add_sub_sub(int piece1, int from1, int to1,
                                        int piece2, int from2, int to2,
                                        int phase) {
  const auto [wf1, bf1] = feature_indices(piece1, from1);
  const auto [wt1, bt1] = feature_indices(piece1, to1);
  const auto [wf2, bf2] = feature_indices(piece2, from2);
  const auto [wt2, bt2] = feature_indices(piece2, to2);
  const NNUE_Params& n = nnue_for_phase(phase);

  apply_add_add_sub_sub(next(), *m_curr,
                        feature_ptr(n, wt1), feature_ptr(n, wf1),
                        feature_ptr(n, wt2), feature_ptr(n, wf2),
                        feature_ptr(n, bt1), feature_ptr(n, bf1),
                        feature_ptr(n, bt2), feature_ptr(n, bf2));
  ++m_curr;
}

inline void NNUE_State::pop() noexcept {
  assert(m_curr > m_stack.data());
  --m_curr;
}

inline int NNUE_State::evaluate(int color, int phase) const {
  const NNUE_Params& n = nnue_for_phase(phase);
  const auto& [us, them] = (color == Colors::White)
      ? std::tie(m_curr->white, m_curr->black)
      : std::tie(m_curr->black, m_curr->white);
  const int32_t output = screlu_flatten(us, them, n.output_v);
  return (output + n.output_bias) * SCALE / QAB;
}

template <bool Activate>
inline void NNUE_State::update_feature(int piece, int square, int phase) noexcept {
  const auto [white_idx, black_idx] = feature_indices(piece, square);
  const NNUE_Params& n = nnue_for_phase(phase);

  const int16_t* wd = feature_ptr(n, white_idx);
  const int16_t* bd = feature_ptr(n, black_idx);

  for (size_t i = 0; i < LAYER1_SIZE; ++i) {
    if constexpr (Activate) {
      m_curr->white[i] += wd[i];
      m_curr->black[i] += bd[i];
    } else {
      m_curr->white[i] -= wd[i];
      m_curr->black[i] -= bd[i];
    }
  }
}

inline void NNUE_State::refresh_from_position(const Position& position, int phase) {
  m_curr->init(std::span<const int16_t, LAYER1_SIZE>(nnue_for_phase(phase).feature_bias));
  for (int square = a1; square < SqNone; ++square) {
    const int piece = position.board[square];
    if (piece != Pieces::Blank) {
      update_feature<true>(piece, square, phase);
    }
  }
}

inline void NNUE_State::reset_nnue(const Position& position, int phase) {
  m_curr = m_stack.data();
  refresh_from_position(position, phase);
}

inline void NNUE_State::change_phases(const Position& position, int phase) {
  assert(m_curr + 1 < m_stack.data() + m_stack.size());
  ++m_curr;
  refresh_from_position(position, phase);
}
