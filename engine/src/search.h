#pragma once
#include "movegen.h"
#include "nnue.h"
#include "params.h"
#include "position.h"
#include "utils.h"
#include <algorithm>

void update_history(int16_t &entry, int score) { // Update history score
  entry += score - entry * abs(score) / 16384;
}

bool out_of_time(ThreadInfo &thread_info) {
  if (thread_info.current_iter == 1) { // dont return on d1
    return false;
  }
  if (thread_info.stop) {
    return true;
  }
  thread_info.time_checks++;
  if (thread_info.time_checks == 1024) {
    thread_info.time_checks = 0;
    if (time_elapsed(thread_info.start_time) > thread_info.max_time) {
      thread_info.stop = true;
      return true;
    }
  }
  return false;
}

int16_t material_eval(Position &position) {
  int m = (position.material_count[0] - position.material_count[1]) * 100 +
          (position.material_count[2] - position.material_count[3]) * 300 +
          (position.material_count[4] - position.material_count[5]) * 300 +
          (position.material_count[6] - position.material_count[7]) * 500 +
          (position.material_count[8] - position.material_count[9]) * 900;

  return position.color ? -m : m;
}
int16_t total_mat(Position &position) {
  int m = (position.material_count[0] + position.material_count[1]) * 100 +
          (position.material_count[2] + position.material_count[3]) * 300 +
          (position.material_count[4] + position.material_count[5]) * 300 +
          (position.material_count[6] + position.material_count[7]) * 500 +
          (position.material_count[8] + position.material_count[9]) * 900;

  return m;
}
int sacrifice_scale(Position &position, ThreadInfo &thread_info,
                        Move move) {
  int scale = 0, threshold = 0;
  while (!SEE(position, thread_info, move, threshold) && scale < 4) {
    scale++;
    threshold -= 250;
    // scale = 1: > -250
    // scale = 2: > -500
    // scale = 3: > -750
    // scale = 4: < -750
  }
  return scale;
}
int eval(Position &position, ThreadInfo &thread_info, int alpha, int beta) {
  int color = position.color;
  int eval = thread_info.nnue_state.evaluate(color);
  if (thread_info.search_ply == 0){
    return eval;
  }
  
  int s = 0, s_opp = 0;

  int start_index = std::max(thread_info.game_ply - thread_info.search_ply, 0);

  for (int idx = start_index; idx < thread_info.game_ply - 1; idx += 2) {

      s = std::max(s, (int)thread_info.game_hist[idx].sacrifice_scale); //Sacrifices made on root color plies
  }

  int bonus1 = 0, bonus2 = 0, bonus3 = 0;

  int m = material_eval(position);

  bonus1 = std::clamp((eval - m) / 6, -150, 150);

  int bonus[5] = {0, 50, 100, 150, 200};

  bonus2 = (bonus[s]) * ((thread_info.search_ply % 2) ? -1 : 1);

  int is_sacrifice_at_root = thread_info.game_hist[start_index].sacrifice_scale;


  bonus3 = 70 * is_sacrifice_at_root * (thread_info.search_ply % 2 ? -1 : 1); 

  return eval + bonus1 + bonus2 + bonus3;
}

void ss_push(Position &position, ThreadInfo &thread_info, Move move,
             uint64_t hash, int16_t s) {
  thread_info.search_ply++;
  thread_info.game_hist[thread_info.game_ply++] = {
      hash, move, position.board[extract_from(move)], s,
      is_cap(position, move)};
}

void ss_pop(ThreadInfo &thread_info, uint64_t hash) {
  thread_info.search_ply--, thread_info.game_ply--;
  thread_info.zobrist_key = hash;
  thread_info.nnue_state.pop();
}

bool material_draw(Position &position) { // Is there not enough material on the
                                         // board for one side to win?
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

bool is_draw(Position &position, ThreadInfo &thread_info,
             uint64_t hash) { // Detects if the position is a draw.

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
  int end_indx = std::max(game_ply - 100, 0);
  for (int i = start_index; i >= end_indx; i -= 2) {
    if (hash == thread_info.game_hist[i].position_key) {
      return true;
    }
  }
  return false;
}

int qsearch(int alpha, int beta, Position &position,
            ThreadInfo &thread_info) { // Performs a quiescence search on the
                                       // given position.

  if (out_of_time(thread_info)) {
    return thread_info.nnue_state.evaluate(position.color);
  }
  int color = position.color;

  thread_info.nodes++;

  int ply = thread_info.search_ply;
  if (ply > MaxSearchDepth) {
    return eval(position, thread_info, alpha,
                beta); // if we're about to overflow stack return
  }

  uint64_t hash = thread_info.zobrist_key;
  TTEntry entry = TT[hash & TT_mask];
  int entry_type = EntryTypes::None,
      tt_score = ScoreNone; // Initialize TT variables and check for a hash hit

  if (entry.position_key == get_hash_upper_bits(hash)) {

    entry_type = entry.type, tt_score = entry.score;
    if (tt_score > MateScore) {
      tt_score -= ply;
    } else if (tt_score < -MateScore) {
      tt_score += ply;
    }

    if ((entry_type == EntryTypes::Exact) ||
        (entry_type == EntryTypes::LBound && tt_score >= beta) ||
        (entry_type == EntryTypes::UBound &&
         tt_score <= alpha)) { // if we get an "accurate" tt score then return
      return tt_score;
    }
  }

  bool in_check = attacks_square(position, position.kingpos[color], color ^ 1);
  int best_score = ScoreNone, raised_alpha = false;
  Move best_move = MoveNone;

  if (!in_check) { // If we're not in check and static eval beats beta, we can
                   // immediately return
    int static_eval = eval(position, thread_info, alpha, beta);
    if (static_eval >= beta) {
      return static_eval;
    }
    best_score = static_eval;
  }
  if (thread_info.search_ply == 1) {
    // print_board(position);
  }
  MoveInfo moves;
  int num_moves =
      movegen(position, moves.moves, in_check); // Generate and score moves
  score_moves(position, thread_info, moves, MoveNone, num_moves);

  for (int indx = 0; indx < num_moves; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);
    if (thread_info.search_ply == 1) {
      // printf("%s %i\n", internal_to_uci(position, move).c_str(),
      // moves.scores[indx]);
    }
    int move_score = moves.scores[indx];
    if (move_score < GoodCaptureBaseScore &&
        !in_check) { // If we're not in check only look at captures
      break;
    }
    Position moved_position = position;
    if (make_move(moved_position, move, thread_info, true)) {
      continue;
    }
    ss_push(position, thread_info, move, hash, 0);
    int score = -qsearch(-beta, -alpha, moved_position, thread_info);
    ss_pop(thread_info, hash);

    if (score > best_score) {
      best_score = score;
      best_move = move;
      if (score > alpha) {
        raised_alpha = true;
        alpha = score;
      }
      if (score >= beta) { // failing high
        break;
      }
    }
  }

  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  insert_entry(hash, 0, best_move,
               best_score > MateScore    ? best_score + ply
               : best_score < -MateScore ? best_score - ply
                                         : best_score,
               entry_type);

  return best_score;
}

int search(int alpha, int beta, int depth, Position &position,
           ThreadInfo &thread_info) { // Performs an alpha-beta search.

  if (!thread_info.search_ply) {
    thread_info.current_iter = depth;
  }

  if (out_of_time(thread_info)) {
    return thread_info.nnue_state.evaluate(position.color);
  }
  if (depth <= 0) {
    return qsearch(alpha, beta, position,
                   thread_info); // drop into qsearch if depth is too low.
  }
  thread_info.nodes++;

  int ply = thread_info.search_ply;

  bool root = !ply, color = position.color, raised_alpha = false;

  bool is_pv = (beta != alpha + 1);

  Move best_move = MoveNone;
  int score = ScoreNone;

  uint64_t hash = thread_info.zobrist_key;

  /*if (hash != calculate(position)){
    print_board(position);
    for (int i = 0; i < thread_info.game_ply; i++){
      Move move = thread_info.game_hist[i].played_move;
      printf("%x %x %i %i %llx\n", extract_from(move), extract_to(move),
      extract_promo(move), thread_info.game_hist[i].piece_moved,
  thread_info.game_hist[i].position_key);
    }
    exit(0);
  }*/

  if (!root && is_draw(position, thread_info, hash)) { // Draw detection
    int draw_score = 2 - (thread_info.nodes & 3);
    int m = material_eval(position);
    if (m < 0) {
      return draw_score;
    } else if (m > 0 || total_mat(position) > 2500) {
      draw_score -= 80;
    }
    return ply % 2 ? -draw_score : draw_score;
    // We want to discourage draws at the root.
    // ply 0 - make a move that makes the position a draw
    // ply 1 - bonus to side, which is penalty to us

    // alternatively
    // ply 0 - we make forced move
    // ply 1 - opponent makes draw move
    // ply 2 - penalty to us
  }

  TTEntry entry = TT[hash & TT_mask];

  int entry_type = EntryTypes::None, tt_score = ScoreNone, tt_move = MoveNone;
  uint32_t hash_key = get_hash_upper_bits(hash);

  if (entry.position_key == hash_key) { // TT probe

    entry_type = entry.type, tt_score = entry.score, tt_move = entry.best_move;
    if (tt_score > MateScore) {
      tt_score -= ply;
    } else if (tt_score < -MateScore) {
      tt_score += ply;
    }
    if (!is_pv &&
        entry.depth >= depth) { // If we get a useful score from the TT and it's
                                // searched to at least the same depth we would
                                // have searched, then we can return
      if ((entry_type == EntryTypes::Exact) ||
          (entry_type == EntryTypes::LBound && tt_score >= beta) ||
          (entry_type == EntryTypes::UBound && tt_score <= alpha)) {
        return tt_score;
      }
    }
  }

  bool in_check = attacks_square(position, position.kingpos[color], color ^ 1);

  int static_eval =
      in_check ? ScoreNone : eval(position, thread_info, alpha, beta);

  if (!is_pv && !in_check) {
    if (depth <= RFPMaxDepth && static_eval - RFPMargin * depth >= beta) {
      return static_eval;
    }
    if (static_eval >= beta && depth >= NMPMinDepth &&
        thread_info.game_hist[thread_info.game_ply - 1].played_move !=
            MoveNone) {

      Position temp_pos = position;

      make_move(temp_pos, MoveNone, thread_info, false);

      ss_push(position, thread_info, MoveNone, hash, 0);

      int R = NMPBase + depth / NMPDepthDiv;
      score = -search(-alpha - 1, -alpha, depth - R, temp_pos, thread_info);

      thread_info.search_ply--, thread_info.game_ply--;
      thread_info.zobrist_key = hash;

      if (score >= beta) {
        return score;
      }
    }
  }

  MoveInfo moves;
  int num_moves = movegen(position, moves.moves, in_check),
      best_score = ScoreNone, moves_played = 0; // Generate and score moves
  bool iscap = false;

  score_moves(position, thread_info, moves, tt_move, num_moves);

  for (int indx = 0; indx < num_moves; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);
    int move_score = moves.scores[indx];

    Position moved_position = position;
    if (make_move(moved_position, move, thread_info, true)) {
      continue;
    }

    iscap = is_cap(position, move);
    int is_sac = 0;
    if (root || depth > 1){
      is_sac = sacrifice_scale(position, thread_info, move);
    }


    ss_push(position, thread_info, move, hash, is_sac);

    bool full_search = false;

    if (depth >= 3 && moves_played > is_pv) {

      int R = LMRTable[depth][moves_played];
      if (iscap) {
        R /= 2;
      }
      R += !is_pv;

      R = std::clamp(R, 1, depth - 1);

      score =
          -search(-alpha - 1, -alpha, depth - R, moved_position, thread_info);
      if (score > alpha) {
        full_search = R > 1;
      }
    } else {
      full_search = moves_played || !is_pv;
    }
    if (full_search) {
      score =
          -search(-alpha - 1, -alpha, depth - 1, moved_position, thread_info);
    }
    if (score > alpha || (!moves_played && is_pv)) {
      score = -search(-beta, -alpha, depth - 1, moved_position, thread_info);
    }

    ss_pop(thread_info, hash);

    if (thread_info.stop) {
      return best_score;
    }

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
    moves_played++;
  }

  if (best_score >= beta && !iscap) {
    int bonus = std::min(300 * (depth - 1), 2500);
    for (int i = 0; moves.moves[i] != best_move; i++) {
      Move move = moves.moves[i];
      if (!is_cap(position, move)) {
        int piece = position.board[extract_from(move)] - 2,
            sq = extract_to(move);
        update_history(thread_info.HistoryScores[piece][sq], -bonus);
      }
    }
    int piece = position.board[extract_from(best_move)] - 2,
        sq = extract_to(best_move);

    update_history(thread_info.HistoryScores[piece][sq], bonus);
    thread_info.KillerMoves[ply] = best_move;
  }

  if (best_score == ScoreNone) { // handle no legal moves (stalemate/checkmate)
    return attacks_square(position, position.kingpos[color], color ^ 1)
               ? (Mate + ply)
               : 0;
  }
  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  // Add the search results to the TT, accounting for mate scores
  insert_entry(hash, depth, best_move,
               best_score > MateScore    ? best_score + ply
               : best_score < -MateScore ? best_score - ply
                                         : best_score,
               entry_type);
  return best_score;
}

void iterative_deepen(
    Position &position,
    ThreadInfo &thread_info) { // Performs an iterative deepening search.

  thread_info.nnue_state.reset_nnue(position);
  thread_info.zobrist_key = calculate(position);
  thread_info.nodes = 0;
  thread_info.time_checks = 0;
  thread_info.stop = false;
  thread_info.search_ply = 0; // reset all relevant thread_info
  memset(thread_info.KillerMoves, 0, sizeof(thread_info.KillerMoves));

  Move best_move = MoveNone;

  for (int depth = 1; depth <= thread_info.max_iter_depth; depth++) {
    int score = search(ScoreNone, -ScoreNone, depth, position, thread_info);

    if (thread_info.stop) {
      break;
    }

    best_move = TT[thread_info.zobrist_key & TT_mask].best_move;

    int64_t search_time = time_elapsed(thread_info.start_time);

    printf("info depth %i seldepth %i score cp %i nodes %" PRIu64
           " time %" PRIi64 " pv %s\n",
           depth, depth, score, thread_info.nodes, search_time,
           internal_to_uci(position, best_move).c_str());
    if (search_time > thread_info.opt_time) {
      break;
    }
  }

  printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
}
