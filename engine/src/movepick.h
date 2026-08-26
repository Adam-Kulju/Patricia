#include "movegen.h"

namespace Stages {
constexpr uint8_t TT = 0;
constexpr uint8_t GenCaptures = 1;
constexpr uint8_t Captures = 2;
constexpr uint8_t GenQuiets = 3;
constexpr uint8_t Quiets = 4;
constexpr uint8_t BadCaptures = 5;
} // namespace Stages

namespace MovePickerTuning {
constexpr int VictimScale = 100;
constexpr int CaptureHistorySeeDivisor = 128;
constexpr int MaxCaptureHistorySeeAdjustment = 128;
constexpr int ImmediateContinuationWeight = 2;
} // namespace MovePickerTuning

struct MovePicker {
  int see_threshold;
  int stage;
  uint64_t checkers;
  int idx = 0;

  MoveInfo captures;
  MoveInfo quiets;
  MoveInfo bad_captures;
  GameHistory *ss;
};

void init_picker(MovePicker &picker, Position &, int threshold,
                 uint64_t checkers, GameHistory *ss) {
  picker.see_threshold = threshold;
  picker.stage = Stages::TT;
  picker.checkers = checkers;
  picker.idx = 0;
  picker.captures.len = 0;
  picker.quiets.len = 0;
  picker.bad_captures.len = 0;
  picker.ss = ss;
}

Move next_move(MovePicker &picker, Position &position, ThreadInfo &thread_info,
               Move tt_move, bool skip_quiets) {
  if (picker.stage == Stages::TT) {
    picker.stage++;

    if (tt_move != MoveNone &&
        is_pseudo_legal(position, tt_move, picker.checkers)) {
      return tt_move;
    }
  }

  if (picker.stage == Stages::GenCaptures) {
    picker.captures.len = movegen_dispatch<Generate::GenCaptures>(
        position, picker.captures.moves.data(), picker.checkers);

    for (int i = 0; i < picker.captures.len; ++i) {
      Move move = picker.captures.moves[i];

      int from = extract_from(move);
      int to = extract_to(move);
      int from_piece = position.board[from];
      int to_piece = position.board[to];
      int attacker_value = SeeValues[get_piece_type(from_piece)];
      int victim_value = SeeValues[get_piece_type(to_piece)];
      int capture_history = thread_info.CapHistScores[from_piece][to];

      if (victim_value == 0 && (from & 7) != (to & 7)) {
        victim_value = attacker_value;
      }

      if (extract_promo(move) == Promos::Queen) {
        picker.captures.scores[i] =
            QueenPromoScore + victim_value * MovePickerTuning::VictimScale -
            attacker_value + capture_history;
      } else {
        picker.captures.scores[i] =
            GoodCaptureBaseScore +
            victim_value * MovePickerTuning::VictimScale - attacker_value +
            capture_history;
      }
    }

    picker.stage++;
  }

  if (picker.stage == Stages::Captures) {
    while (picker.idx < picker.captures.len) {
      Move move = get_next_move(picker.captures.moves.data(),
                                picker.captures.scores.data(), picker.idx++,
                                picker.captures.len);

      if (move == tt_move) {
        continue;
      }

      int from_piece = position.board[extract_from(move)];
      int to = extract_to(move);
      int see_adjustment = thread_info.CapHistScores[from_piece][to] /
                           MovePickerTuning::CaptureHistorySeeDivisor;

      if (see_adjustment > MovePickerTuning::MaxCaptureHistorySeeAdjustment) {
        see_adjustment = MovePickerTuning::MaxCaptureHistorySeeAdjustment;
      } else if (see_adjustment <
                 -MovePickerTuning::MaxCaptureHistorySeeAdjustment) {
        see_adjustment = -MovePickerTuning::MaxCaptureHistorySeeAdjustment;
      }

      if (SEE(position, move, picker.see_threshold - see_adjustment)) {
        return move;
      }

      picker.bad_captures.moves[picker.bad_captures.len++] = move;
    }

    picker.idx = 0;
    picker.stage++;
  }

  if (picker.stage == Stages::GenQuiets) {
    picker.quiets.len = movegen_dispatch<Generate::GenQuiets>(
        position, picker.quiets.moves.data(), picker.checkers);

    Move opponent_previous_move = (picker.ss - 1)->played_move;
    Move own_previous_move = (picker.ss - 2)->played_move;
    Move own_earlier_move = (picker.ss - 4)->played_move;

    bool has_opponent_previous_move = opponent_previous_move != MoveNone;
    bool has_own_previous_move = own_previous_move != MoveNone;
    bool has_own_earlier_move = own_earlier_move != MoveNone;

    int opponent_previous_to =
        has_opponent_previous_move ? extract_to(opponent_previous_move) : 0;
    int own_previous_to =
        has_own_previous_move ? extract_to(own_previous_move) : 0;
    int own_earlier_to =
        has_own_earlier_move ? extract_to(own_earlier_move) : 0;

    int opponent_previous_piece = (picker.ss - 1)->piece_moved;
    int own_previous_piece = (picker.ss - 2)->piece_moved;
    int own_earlier_piece = (picker.ss - 4)->piece_moved;

    Move killer = thread_info.KillerMoves[thread_info.search_ply];

    for (int i = 0; i < picker.quiets.len; ++i) {
      Move move = picker.quiets.moves[i];

      if (move == killer) {
        picker.quiets.scores[i] = KillerMoveScore;
        continue;
      }

      int piece = position.board[extract_from(move)];
      int to = extract_to(move);
      int score = thread_info.HistoryScores[piece][to];

      if (has_opponent_previous_move) {
        score += MovePickerTuning::ImmediateContinuationWeight *
                 thread_info.ContHistScores[opponent_previous_piece]
                                           [opponent_previous_to][piece][to];
      }

      if (has_own_previous_move) {
        score +=
            thread_info
                .ContHistScores[own_previous_piece][own_previous_to][piece][to];
      }

      if (has_own_earlier_move) {
        score +=
            thread_info
                .ContHistScores[own_earlier_piece][own_earlier_to][piece][to];
      }

      picker.quiets.scores[i] = score;
    }

    picker.stage++;
  }

  if (picker.stage == Stages::Quiets) {
  pick_quiet:
    if (skip_quiets || picker.idx >= picker.quiets.len) {
      picker.idx = 0;
      picker.stage++;
    } else {
      Move move =
          get_next_move(picker.quiets.moves.data(), picker.quiets.scores.data(),
                        picker.idx++, picker.quiets.len);

      if (move == tt_move) {
        goto pick_quiet;
      }

      return move;
    }
  }

  if (picker.stage == Stages::BadCaptures) {
    if (picker.idx >= picker.bad_captures.len) {
      return MoveNone;
    }

    return picker.bad_captures.moves[picker.idx++];
  }

  return MoveNone;
}
