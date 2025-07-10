#include "movegen.h"

namespace Stages {
constexpr uint8_t TT = 0;
constexpr uint8_t GenCaptures = 1;
constexpr uint8_t Captures = 2;
constexpr uint8_t GenQuiets = 3;
constexpr uint8_t Quiets = 4;
constexpr uint8_t BadCaptures = 5;
}; // namespace Stages

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

void init_picker(MovePicker &picker, Position &position, int threshold,
                 uint64_t checkers, GameHistory *ss) {
  picker.see_threshold = threshold;
  picker.stage = Stages::TT;
  picker.checkers = checkers;
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

    picker.captures.len = movegen(position, picker.captures.moves,
                                  picker.checkers, Generate::GenCaptures);

    for (int i = 0; i < picker.captures.len; i++) {
      Move move = picker.captures.moves[i];

      if (extract_promo(move) == Promos::Queen) {
        picker.captures.scores[i] = QueenPromoScore;
      }

      else {
        int from_piece = position.board[extract_from(move)],
            to_piece = position.board[extract_to(move)];

        picker.captures.scores[i] = GoodCaptureBaseScore +
                                    SeeValues[get_piece_type(to_piece)] * 100 -
                                    SeeValues[get_piece_type(from_piece)] / 100;

        int piece = position.board[extract_from(move)], to = extract_to(move);

        picker.captures.scores[i] += thread_info.CapHistScores[piece][to];
      }
    }

    picker.stage++;
  }

  if (picker.stage == Stages::Captures) {

    while (picker.idx < picker.captures.len) {
      Move move = get_next_move(picker.captures.moves, picker.captures.scores,
                                picker.idx++, picker.captures.len);
      if (move == tt_move) {
        continue;
      }
      if (SEE(position, move, picker.see_threshold)) {
        return move;
      } else {
        picker.bad_captures.moves[picker.bad_captures.len++] = move;
      }
    }
    picker.idx = 0;
    picker.stage++;
  }

  if (picker.stage == Stages::GenQuiets) {
    picker.quiets.len = movegen(position, picker.quiets.moves, picker.checkers,
                                Generate::GenQuiets);

    int their_last = extract_to((picker.ss - 1)->played_move);
    int their_piece = (picker.ss - 1)->piece_moved;

    int our_last = extract_to((picker.ss - 2)->played_move);
    int our_piece = (picker.ss - 2)->piece_moved;

    int ply4last = extract_to((picker.ss - 4)->played_move);
    int ply4piece = (picker.ss - 4)->piece_moved;

    for (int i = 0; i < picker.quiets.len; i++) {
      Move move = picker.quiets.moves[i];

      if (move == thread_info.KillerMoves[thread_info.search_ply]) {
        // Killer move score
        picker.quiets.scores[i] = KillerMoveScore;
      }

      else {

        int piece = position.board[extract_from(move)], to = extract_to(move);
        picker.quiets.scores[i] = thread_info.HistoryScores[piece][to];

        if (their_last != MoveNone) {
          picker.quiets.scores[i] +=
              thread_info.ContHistScores[their_piece][their_last][piece][to];
        }
        if (our_last != MoveNone) {
          picker.quiets.scores[i] +=
              thread_info.ContHistScores[our_piece][our_last][piece][to];
        }

        if (ply4last != MoveNone) {
          picker.quiets.scores[i] +=
              thread_info.ContHistScores[ply4piece][ply4last][piece][to];
        }
      }
    }
    picker.stage++;
  }

  if (picker.stage == Stages::Quiets) {
    pick_again:
    if (skip_quiets || picker.idx >= picker.quiets.len) {
      picker.idx = 0;
      picker.stage++;
    } else {
      Move move = get_next_move(picker.quiets.moves, picker.quiets.scores,
                           picker.idx++, picker.quiets.len);
      if (move == tt_move) {
        goto pick_again;
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