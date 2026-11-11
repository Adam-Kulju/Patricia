#pragma once
#include "movegen.h"

namespace Stages {
constexpr uint8_t TT = 0;
constexpr uint8_t GenCaptures = 1;
constexpr uint8_t Captures = 2;
constexpr uint8_t GenQuiets = 3;
constexpr uint8_t Quiets = 4;
constexpr uint8_t BadCaptures = 5;
constexpr uint8_t BadQuiets = 6;
} // namespace Stages

namespace MovePickerTuning {
constexpr int VictimScale = 100;
constexpr int CaptureHistorySeeDivisor = 128;
constexpr int MaxCaptureHistorySeeAdjustment = 128;
constexpr int ImmediateContinuationWeight = 2;
// Quiets scoring below this are deferred until after the bad captures.
// Compatible with update_history gravity limit of 16384.
constexpr int BadQuietThreshold = -12000;
} // namespace MovePickerTuning

struct MovePicker {
  int see_threshold;
  int stage;
  uint64_t checkers;
  int idx = 0;             // index into captures
  int quiet_idx = 0;       // index into quiets (shared by good and bad quiets)
  int bad_capture_idx = 0; // index into bad captures

  MoveInfo captures;
  MoveInfo quiets;
  MoveInfo bad_captures;
  GameHistory *ss;
};

inline void init_picker(MovePicker &picker, Position &, int threshold,
                        uint64_t checkers, GameHistory *ss) {
  picker.see_threshold = threshold;
  picker.stage = Stages::TT;
  picker.checkers = checkers;
  picker.idx = 0;
  picker.quiet_idx = 0;
  picker.bad_capture_idx = 0;
  picker.captures.len = 0;
  picker.quiets.len = 0;
  picker.bad_captures.len = 0;
  picker.ss = ss;
}

static inline int capture_see_adjustment(const ThreadInfo &thread_info,
                                         int from_piece, int to) {
  int see_adjustment = thread_info.CapHistScores[from_piece][to] /
                       MovePickerTuning::CaptureHistorySeeDivisor;

  if (see_adjustment > MovePickerTuning::MaxCaptureHistorySeeAdjustment) {
    see_adjustment = MovePickerTuning::MaxCaptureHistorySeeAdjustment;
  } else if (see_adjustment <
             -MovePickerTuning::MaxCaptureHistorySeeAdjustment) {
    see_adjustment = -MovePickerTuning::MaxCaptureHistorySeeAdjustment;
  }

  return see_adjustment;
}

inline Move next_move(MovePicker &picker, Position &position,
                      ThreadInfo &thread_info, Move tt_move, bool skip_quiets) {
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

      int base = extract_promo(move) == Promos::Queen ? QueenPromoScore
                                                      : GoodCaptureBaseScore;

      picker.captures.scores[i] = base +
                                  victim_value * MovePickerTuning::VictimScale -
                                  attacker_value + capture_history;
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
      int see_adjustment = capture_see_adjustment(thread_info, from_piece, to);

      if (SEE(position, move, picker.see_threshold - see_adjustment)) {
        return move;
      }

      // Captures are picked in score order, so bad captures stay sorted.
      picker.bad_captures.moves[picker.bad_captures.len++] = move;
    }

    picker.stage++;
  }

  if (picker.stage == Stages::GenQuiets) {
    if (skip_quiets) {
      // Don't waste time generating and scoring quiets that will never be
      // searched; go straight to the bad captures.
      picker.quiets.len = 0;
      picker.stage = Stages::BadCaptures;
    } else {
      picker.quiets.len = movegen_dispatch<Generate::GenQuiets>(
          position, picker.quiets.moves.data(), picker.checkers);

      const int opp_color = position.color ^ 1;
      const uint64_t occ =
          position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];
      const int their_king = get_king_pos(position, opp_color);
      const uint64_t king_zone = KingAttacks[their_king] | (1ull << their_king);

      uint64_t check_squares[PieceTypes::King + 1] = {};
      check_squares[PieceTypes::Pawn] = PawnAttacks[opp_color][their_king];
      check_squares[PieceTypes::Knight] = KnightAttacks[their_king];
      check_squares[PieceTypes::Bishop] = get_bishop_attacks(their_king, occ);
      check_squares[PieceTypes::Rook] = get_rook_attacks(their_king, occ);
      check_squares[PieceTypes::Queen] =
          check_squares[PieceTypes::Bishop] | check_squares[PieceTypes::Rook];

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
        int piece_type = get_piece_type(piece);
        int to = extract_to(move);
        uint64_t to_bb = 1ull << to;
        int score = thread_info.HistoryScores[piece][to];

        if (has_opponent_previous_move) {
          score += MovePickerTuning::ImmediateContinuationWeight *
                   thread_info.ContHistScores[opponent_previous_piece]
                                             [opponent_previous_to][piece][to];
        }

        if (has_own_previous_move) {
          score += thread_info.ContHistScores[own_previous_piece]
                                             [own_previous_to][piece][to];
        }

        if (has_own_earlier_move) {
          score += thread_info.ContHistScores[own_earlier_piece]
                                             [own_earlier_to][piece][to];
        }

        if (check_squares[piece_type] & to_bb) {
          score += QuietCheckBonus;
        }
        if (to_bb & king_zone) {
          score += QuietKingZoneBonus;
        }

        picker.quiets.scores[i] = score;
      }

      picker.stage++;
    }
  }

  if (picker.stage == Stages::Quiets) {
    // Good quiets: returned before bad captures.
    while (!skip_quiets && picker.quiet_idx < picker.quiets.len) {
      int i = picker.quiet_idx;
      Move move =
          get_next_move(picker.quiets.moves.data(), picker.quiets.scores.data(),
                        picker.quiet_idx++, picker.quiets.len);

      // Quiets come out in descending order. Once we hit one below the
      // threshold, all remaining quiets are "bad" and are deferred.
      if (picker.quiets.scores[i] < MovePickerTuning::BadQuietThreshold) {
        picker.quiet_idx = i;
        break;
      }

      if (move == tt_move) {
        continue;
      }

      return move;
    }

    picker.stage = Stages::BadCaptures;
  }

  if (picker.stage == Stages::BadCaptures) {
    if (picker.bad_capture_idx < picker.bad_captures.len) {
      return picker.bad_captures.moves[picker.bad_capture_idx++];
    }

    picker.stage = Stages::BadQuiets;
  }

  if (picker.stage == Stages::BadQuiets) {
    while (!skip_quiets && picker.quiet_idx < picker.quiets.len) {
      Move move =
          get_next_move(picker.quiets.moves.data(), picker.quiets.scores.data(),
                        picker.quiet_idx++, picker.quiets.len);

      if (move == tt_move) {
        continue;
      }

      return move;
    }

    return MoveNone;
  }

  return MoveNone;
}
