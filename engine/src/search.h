#pragma once
#include "movegen.h"
#include "nnue.h"
#include "position.h"
#include "utils.h"
#include <algorithm>
#include <chrono>

int eval(Position &position) {
  int m = (position.material_count[0] - position.material_count[1]) * 100 +
          (position.material_count[2] - position.material_count[3]) * 300 +
          (position.material_count[4] - position.material_count[5]) * 300 +
          (position.material_count[6] - position.material_count[7]) * 500 +
          (position.material_count[8] - position.material_count[9]) * 900;

  return position.color ? -m : m;
}

void ss_push(Position &position, ThreadInfo &thread_info, Move move,
             uint64_t hash) {
  thread_info.search_ply++;
  thread_info.game_hist[thread_info.game_ply++] = {
      hash, move, position.board[extract_from(move)]};
}

void ss_pop(ThreadInfo &thread_info, uint64_t hash) {
  thread_info.search_ply--, thread_info.game_ply--;
  thread_info.zobrist_key = hash;
  thread_info.nnue_state.pop();
}

bool material_draw(Position &position) {
  for (int i :
       {0, 1, 6, 7, 8, 9}) { // Do we have pawns, rooks, or queens on the board?
    if (position.material_count[i]) {
      return false;
    }
  }
  if (position.material_count[4] > 1 || position.material_count[2] > 2 ||
      (position.material_count[2] &&
       position.material_count[4])) { // Do we have three knights, two bishops,
                                      // or a bishop and knight for either side?
    return false;
  }
  if (position.material_count[5] > 1 || position.material_count[3] > 2 ||
      (position.material_count[3] &&
       position.material_count[5])) { // Do we have three knights, two bishops,
                                      // or a bishop and knight for either side?
    return false;
  }
  return true;
}

bool is_draw(Position &position, ThreadInfo &thread_info, uint64_t hash) {
  int halfmoves = position.halfmoves, game_ply = thread_info.game_ply;
  if (halfmoves >= 100) {
    return true;
  }
  if (material_draw(position)) {
    return true;
  }
  int start_index =
      game_ply -
      4; // game_ply - 1: last played move, game_ply - 2: your last played move,
         // game_ply - 4 is the first opportunity a repetition is possible
  int end_indx =
      std::max(game_ply - halfmoves,
               0); // impossible to have a repetition further back - it would
                   // always be different due to capture/pawn move
  for (int i = start_index; i >= end_indx; i -= 2) {
    if (hash == thread_info.game_hist[i].position_key) {
      return true;
    }
  }
  return false;
}

int qsearch(int alpha, int beta, Position &position, ThreadInfo &thread_info) {
  thread_info.nodes++;
  int ply = thread_info.search_ply;
  if (ply > MaxSearchDepth) {
    return eval(position);
  }
  uint64_t hash = thread_info.zobrist_key;

  TTEntry entry = TT[hash & TT_mask];
  int entry_type = EntryTypes::None, tt_score = ScoreNone;

  if (entry.position_key == get_hash_upper_bits(hash)) {

    entry_type = entry.type, tt_score = entry.score;
    if (tt_score > MateScore) {
      tt_score -= ply;
    } else if (tt_score < -MateScore) {
      tt_score += ply;
    }
    if ((entry_type == EntryTypes::Exact) ||
        (entry_type == EntryTypes::LBound && tt_score >= beta) ||
        (entry_type == EntryTypes::UBound && tt_score <= alpha)) {
      return tt_score;
    }
  }

  bool in_check = attacks_square(position, position.kingpos[position.color],
                                 position.color ^ 1);
  int best_score = ScoreNone;

  if (!in_check) {
    int static_eval =
        thread_info.nnue_state.evaluate(position.color);
    if (static_eval >= beta) {
      return static_eval;
    }
    best_score = static_eval;
  }

  MoveInfo moves;
  int num_moves = movegen(position, moves.moves);
  score_moves(position, moves, MoveNone, num_moves);

  for (int indx = 0; indx < num_moves; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);
    int move_score = moves.scores[indx];
    if (move_score < GoodCaptureBaseScore && !in_check) {
      break;
    }
    Position moved_position = position;
    if (make_move(moved_position, move, thread_info, true)) {
      continue;
    }
    ss_push(position, thread_info, move, hash);
    int score = -qsearch(-beta, -alpha, moved_position, thread_info);
    ss_pop(thread_info, hash);

    if (score > best_score) {
      best_score = score;
      if (score > alpha) {
        alpha = score;
      }
      if (score >= beta) {
        break;
      }
    }
  }
  return best_score;
}

int search(int alpha, int beta, int depth, Position &position,
           ThreadInfo &thread_info) {
  if (depth <= 0) {
    // return evaluate(thread_info.nnue_state, position, position.color);
    return qsearch(alpha, beta, position, thread_info);
  }
  thread_info.nodes++;
  int ply = thread_info.search_ply;

  bool root = !ply, color = position.color, raised_alpha = false;

  Move best_move = MoveNone;

  uint64_t hash = thread_info.zobrist_key;

  if (!root && is_draw(position, thread_info, hash)) {
    return 2 - (thread_info.nodes & 3);
  }

  TTEntry entry = TT[hash & TT_mask];
  int entry_type = EntryTypes::None, tt_score = ScoreNone, tt_move = MoveNone;

  if (entry.position_key == get_hash_upper_bits(hash)) {

    entry_type = entry.type, tt_score = entry.score, tt_move = entry.best_move;
    if (tt_score > MateScore) {
      tt_score -= ply;
    } else if (tt_score < -MateScore) {
      tt_score += ply;
    }
    if (!root && entry.depth >= depth) {
      if ((entry_type == EntryTypes::Exact) ||
          (entry_type == EntryTypes::LBound && tt_score >= beta) ||
          (entry_type == EntryTypes::UBound && tt_score <= alpha)) {
        return tt_score;
      }
    }
  }

  MoveInfo moves;
  int num_moves = movegen(position, moves.moves), best_score = ScoreNone;
  score_moves(position, moves, tt_move, num_moves);

  for (int indx = 0; indx < num_moves; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);
    int move_score = moves.scores[indx];

    Position moved_position = position;
    if (make_move(moved_position, move, thread_info, true)) {
      continue;
    }
    ss_push(position, thread_info, move, hash);
    int score = -search(-beta, -alpha, depth - 1, moved_position, thread_info);
    ss_pop(thread_info, hash);

    if (score > best_score) {
      best_score = score;
      best_move = move;
      if (score > alpha) {
        raised_alpha = true;
        alpha = score;
      }
      if (score >= beta) {
        break;
      }
    }
  }

  if (best_score == ScoreNone) {
    return attacks_square(position, position.kingpos[color], color ^ 1)
               ? (Mate + ply)
               : 0;
  }
  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  insert_entry(hash, depth, best_move,
               best_score > MateScore    ? best_score + ply
               : best_score < -MateScore ? best_score - ply
                                         : best_score,
               entry_type);
  return best_score;
}

void iterative_deepen(Position &position, ThreadInfo &thread_info) {
      thread_info.nnue_state.reset_nnue(position);
  thread_info.start_time = std::chrono::steady_clock::now();
  thread_info.zobrist_key = calculate(position);
  thread_info.nodes = 0;
  thread_info.search_ply = 0;
  for (int depth = 1; depth <= 8; depth++) {
    int score = search(ScoreNone, -ScoreNone, depth, position, thread_info);

    Move best_move = TT[thread_info.zobrist_key & TT_mask].best_move;
    auto now = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - thread_info.start_time)
                            .count();
    printf("info depth %i seldepth %i score cp %i nodes %lu time %li pv %s\n",
           depth, depth, score, thread_info.nodes, time_elapsed,
           internal_to_uci(position, best_move).c_str());
  }
}
