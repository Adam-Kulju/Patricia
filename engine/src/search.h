#pragma once
#include "movegen.h"
#include "nnue.h"
#include "params.h"
#include "position.h"
#include "utils.h"

constexpr int NormalizationFactor = 195;

void update_history(int16_t &entry, int score) { // Update history score
  entry += score - entry * abs(score) / 16384;
}

bool out_of_time(ThreadInfo &thread_info) {
  if (thread_info.stop) {
    return true;
  } else if (thread_info.thread_id != 0 || thread_info.current_iter == 1) {
    return false;
  }
  if (thread_info.nodes >= thread_info.max_nodes_searched) {

    if (thread_info.max_time < INT32_MAX / 3) {

      std::this_thread::sleep_for(std::chrono::milliseconds(
          thread_info.opt_time - time_elapsed(thread_info.start_time)));
      // If there's a max time limit and a node limit, it means we're using a
      // skill level. In that case, we don't want the engine to move instantly.
    }

    thread_info.stop = true;
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

int16_t material_eval(const Position &position) {
  int m = (position.material_count[0] - position.material_count[1]) * 100 +
          (position.material_count[2] - position.material_count[3]) * 300 +
          (position.material_count[4] - position.material_count[5]) * 300 +
          (position.material_count[6] - position.material_count[7]) * 500 +
          (position.material_count[8] - position.material_count[9]) * 900;

  return position.color ? -m : m;
}
int16_t total_mat(const Position &position) {
  int m = (position.material_count[0] + position.material_count[1]) * 100 +
          (position.material_count[2] + position.material_count[3]) * 300 +
          (position.material_count[4] + position.material_count[5]) * 300 +
          (position.material_count[6] + position.material_count[7]) * 500 +
          (position.material_count[8] + position.material_count[9]) * 900;

  return m;
}

bool has_non_pawn_material(const Position &position, int color) {
  int s_indx = 2 + color;
  return (position.material_count[s_indx] ||
          position.material_count[s_indx + 2] ||
          position.material_count[s_indx + 4] ||
          position.material_count[s_indx + 6]);
}

int16_t total_mat_color(const Position &position, int color) {
  // total material for one color

  int m = 0;
  for (int i = 0; i < 5; i++) {
    m += position.material_count[i * 2 + color] * SeeValues[i * 2 + 2];
  }
  return m;
}

int eval(const Position &position, ThreadInfo &thread_info) {
  int color = position.color;
  int root_color = thread_info.search_ply % 2 ? color ^ 1 : color;

  int eval = thread_info.nnue_state.evaluate(color);

  // Patricia is much less dependent on explicit eval twiddling than before, but
  // there are still a few things I do.

  int m_eval = material_eval(position);
  int m_threshold = std::max({300, abs(eval) * 2 / 3, abs(eval) - 700});

  int bonus1 = 0, bonus2 = 0;

  // Give a small bonus if the position is much better than what material would
  // suggest

  if (eval > 0 && eval > m_eval + m_threshold) {

    bonus1 += 25 + (eval - m_eval - m_threshold) / 10;
  } else if (eval < 0 && eval < m_eval - m_threshold) {

    bonus1 -= 25 + (m_eval - eval - m_threshold) / 10;
  }

  bool our_side = (thread_info.search_ply % 2 == 0);

  int start_index = std::max(thread_info.game_ply - thread_info.search_ply, 0);

  int s_m = thread_info.game_hist[start_index].m_diff;
  int s = 0;

  // Give a small bonus if we have sacrificed material at some point in the
  // search tree If we are completely winning, give a bigger bonus to
  // incentivize finding the most stylish move when everything wins

  for (int idx = start_index + 2; idx < thread_info.game_ply - 4; idx += 2) {

    if (thread_info.game_hist[idx].m_diff < s_m &&
        thread_info.game_hist[idx + 1].m_diff > s_m &&
        thread_info.game_hist[idx + 2].m_diff < s_m &&
        thread_info.game_hist[idx + 3].m_diff > s_m &&
        thread_info.game_hist[idx + 4].m_diff < s_m) {

      s = s_m + thread_info.game_hist[idx + 4].m_diff;
    }
  }
  if (s) {

    if (thread_info.search_ply % 2) {
      bonus2 = -20 * (eval < -500 ? 3 : eval < -150 ? 2 : 1);
    } else {
      bonus2 = 20 * (eval > 500 ? 3 : eval > 150 ? 2 : 1);
    }
  }

  // If we're winning, scale eval by material; we don't want to trade off to an
  // easily won endgame, but instead should continue the attack.

  if (abs(eval) > 500) {
    eval = eval *
           (512 + total_mat_color(position, color ^ 1) / 8 -
            (position.material_count[0] + position.material_count[1]) * 20) /
           768;
  }

  return std::clamp(eval + bonus1 + bonus2, -MateScore, MateScore);
}

void ss_push(Position &position, ThreadInfo &thread_info, Move move,
             uint64_t hash) {
  // update search stack after makemove
  thread_info.search_ply++;

  thread_info.game_hist[thread_info.game_ply++] = {
      hash, move, position.board[extract_from(move)],
      // 0,
      is_cap(position, move), material_eval(position)};
}

void ss_pop(ThreadInfo &thread_info, uint64_t hash) {
  // associated with unmake
  thread_info.search_ply--, thread_info.game_ply--;
  thread_info.zobrist_key = hash;

  thread_info.nnue_state.pop();
}

bool material_draw(
    const Position &position) { // Is there not enough material on the
                                // position for one side to win?
  for (int i : {0, 1, 6, 7, 8,
                9}) { // Do we have pawns, rooks, or queens on the position?
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

bool is_draw(const Position &position,
             ThreadInfo &thread_info) { // Detects if the position is a draw.

  uint64_t hash = thread_info.zobrist_key;

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

int qsearch(int alpha, int beta, Position &position, ThreadInfo &thread_info,
            std::vector<TTEntry> &TT) { // Performs a quiescence search on the
                                        // given position.

  if (out_of_time(thread_info)) {
    // return if out of time
    return thread_info.nnue_state.evaluate(position.color);
  }
  int color = position.color;

  thread_info.nodes++;

  int ply = thread_info.search_ply;

  if (ply > thread_info.seldepth) {
    thread_info.seldepth = ply;
  }
  if (ply > MaxSearchDepth) {
    return eval(position,
                thread_info); // if we're about to overflow stack return
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

  int static_eval = ScoreNone;

  if (!in_check) { // If we're not in check and static eval beats beta, we can
                   // immediately return
    static_eval = eval(position, thread_info);

    if (entry_type == EntryTypes::Exact ||
        (entry_type == EntryTypes::UBound && tt_score < static_eval) ||
        (entry_type == EntryTypes::LBound && tt_score > static_eval)) {

      static_eval = tt_score;
    }

    if (static_eval >= beta) {
      return static_eval;
    }
    best_score = static_eval;
  }
  MoveInfo moves;
  int num_moves =
      movegen(position, moves.moves, in_check); // Generate and score moves

  score_moves(position, thread_info, moves, MoveNone, num_moves);

  for (int indx = 0; indx < num_moves; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);

    int move_score = moves.scores[indx];
    if (move_score < GoodCaptureBaseScore &&
        !in_check) { // If we're not in check only look at good captures
      break;
    }
    Position moved_position = position;
    if (make_move(moved_position, move, thread_info, Updates::UpdateAll)) {
      continue;
    }
    update_nnue_state(thread_info.nnue_state, move, position);

    ss_push(position, thread_info, move, hash);
    int score = -qsearch(-beta, -alpha, moved_position, thread_info, TT);
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

  if (best_score == ScoreNone) { // handle no legal moves (stalemate/checkmate)
    return attacks_square(position, position.kingpos[color], color ^ 1)
               ? (Mate + ply)
               : 0;
  }

  // insert entries and return

  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  insert_entry(hash, 0, best_move,
               best_score > MateScore    ? best_score + ply
               : best_score < -MateScore ? best_score - ply
                                         : best_score,
               entry_type, thread_info.searches, TT);

  return best_score;
}

int search(int alpha, int beta, int depth, Position &position,
           ThreadInfo &thread_info,
           std::vector<TTEntry> &TT) { // Performs an alpha-beta search.

  if (!thread_info.search_ply) {
    thread_info.current_iter = depth;
    thread_info.seldepth = 0;
    std::memset(&thread_info.pv, 0, sizeof(thread_info.pv));
  }

  int ply = thread_info.search_ply, pv_index = ply * MaxSearchDepth;

  if (ply > thread_info.seldepth) {
    thread_info.seldepth = ply;
  }

  if (out_of_time(thread_info) || ply > MaxSearchDepth) {
    // check for timeout
    return eval(position, thread_info);
  }
  if (depth <= 0) {
    return qsearch(alpha, beta, position, thread_info,
                   TT); // drop into qsearch if depth is too low.
  }
  thread_info.nodes++;

  thread_info.pv[pv_index] = MoveNone;

  bool root = !ply, color = position.color, raised_alpha = false,
       searched_move = false;

  bool is_pv = (beta != alpha + 1);

  Move best_move = MoveNone;
  Move excluded_move = thread_info.excluded_move;

  bool singular_search = (excluded_move != MoveNone);

  thread_info.excluded_move =
      MoveNone; // If we currently are in singular search, this sets it so moves
                // *after* it are not in singular search
  int score = ScoreNone;

  uint64_t hash = thread_info.zobrist_key;

  if (!root && is_draw(position, thread_info)) { // Draw detection
    int draw_score = 2 - (thread_info.nodes & 3);

    int m = material_eval(position);
    if (m < 0) {
      return draw_score;
    } else if (m > 0 || total_mat(position) > 3000) {
      draw_score -= 30;
    }
    return ply % 2 ? -draw_score : draw_score;
    // We want to discourage draws at the root.
    // ply 0 - make a move that makes the position a draw
    // ply 1 - bonus to side, which is penalty to us

    // alternatively
    // ply 0 - we make forced move
    // ply 1 - opponent makes draw move
    // ply 2 - penalty to us*/
  }

  TTEntry entry = TT[hash & TT_mask];

  int entry_type = EntryTypes::None, tt_score = ScoreNone, tt_move = MoveNone;
  uint32_t hash_key = get_hash_upper_bits(hash);

  if (entry.position_key == hash_key && !singular_search) { // TT probe

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

  // We can't do any eval-based pruning if in check.

  int32_t static_eval;
  if (in_check) {
    static_eval = ScoreNone;
  } else if (singular_search) {
    static_eval = thread_info.game_hist[thread_info.game_ply].static_eval;
  } else {
    static_eval = eval(position, thread_info);
  }

  thread_info.game_hist[thread_info.game_ply].static_eval = static_eval;

  bool improving = false;

  // Improving: Is our eval better than it was last turn? If so we can prune
  // less in certain circumstances (or prune more if it's not)

  if (ply > 1 && !in_check &&
      static_eval >
          thread_info.game_hist[thread_info.game_ply - 2].static_eval) {
    improving = true;
  }

  if (entry_type == EntryTypes::Exact ||
      (entry_type == EntryTypes::UBound && tt_score < static_eval) ||
      (entry_type == EntryTypes::LBound && tt_score > static_eval)) {

    static_eval = tt_score;
  }

  if (!is_pv && !in_check && !singular_search) {

    // Reverse Futility Pruning (RFP): If our position is way better than beta,
    // we're likely good to stop searching the node.

    if (depth <= RFPMaxDepth &&
        static_eval - RFPMargin * (depth - improving) >= beta) {
      return static_eval;
    }
    if (static_eval >= beta && depth >= NMPMinDepth &&
        has_non_pawn_material(position, color) &&
        thread_info.game_hist[thread_info.game_ply - 1].played_move !=
            MoveNone) {

      // Null Move Pruning (NMP): If we can give our opponent a free move and
      // still beat beta on a reduced search, we can prune the node.

      Position temp_pos = position;

      make_move(temp_pos, MoveNone, thread_info, Updates::UpdateHash);

      ss_push(position, thread_info, MoveNone, hash);

      int R = NMPBase + depth / NMPDepthDiv +
              std::min(3, (static_eval - beta) / NMPEvalDiv);
      score = -search(-alpha - 1, -alpha, depth - R, temp_pos, thread_info, TT);

      thread_info.search_ply--, thread_info.game_ply--;
      thread_info.zobrist_key = hash;
      // we don't call ss_pop because the nnue state was never pushed

      if (score >= beta) {
        if (score > MateScore) {
          score = beta;
        }
        return score;
      }
    }
  }

  if (is_pv && tt_move == MoveNone && depth > IIRMinDepth) {
    // Internal Iterative Reduction: If we are in a PV node and have no TT move,
    // reduce the depth.
    depth--;
  }

  MoveInfo moves;
  int num_moves = movegen(position, moves.moves, in_check),
      best_score = ScoreNone, moves_played = 0; // Generate and score moves
  bool is_capture = false, skip = false;

  score_moves(position, thread_info, moves, tt_move, num_moves);

  for (int indx = 0; indx < num_moves && !skip; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);

    if (root) {
      bool pv_skip = false;
      for (int i = 0; i < thread_info.multipv_index; i++) {
        if (thread_info.best_moves[i] == move) {
          pv_skip = true;
          break;
        }
      }
      if (pv_skip) {
        continue;
      }
    }

    int move_score = moves.scores[indx];

    Position moved_position = position;
    if (move == excluded_move ||
        make_move(moved_position, move, thread_info, Updates::UpdateHash)) {
      continue;
    }

    searched_move = true;

    is_capture = is_cap(position, move);
    if (!is_capture && !is_pv) {

      // Late Move Pruning (LMP): If we've searched enough moves, we can skip
      // the rest.

      if (depth < LMPDepth &&
          moves_played >= LMPBase + depth * depth / (2 - improving)) {
        skip = true;
      }

      // Futility Pruning (FP): If we're far worse than alpha and our move isn't
      // a good capture, we can skip the rest.

      if (!in_check && depth < FPDepth && move_score < GoodCaptureBaseScore &&
          static_eval + FPMargin1 + FPMargin2 * depth < alpha) {
        skip = true;
      }
    }

    if (best_score > -MateScore && depth < SeePruningDepth) {

      int margin =
          is_capture ? SeePruningQuietMargin : (depth * SeePruningNoisyMargin);

      if (!SEE(position, move, depth * margin)) {
        // SEE pruning: if we are hanging material, prune under certain
        // conditions.
        thread_info.zobrist_key = hash;
        continue;
      }
    }

    int extension = 0;

    // Singular Extensions (SE): If a search finds that the TT move is way
    // better than all other moves, extend it under certain conditions.

    if (!root && ply < thread_info.current_iter * 2) {
      if (!singular_search && depth >= SEDepth && move_score == TTMoveScore &&
          abs(entry.score) < MateScore && entry.depth >= depth - 3 &&
          entry_type != EntryTypes::UBound) {

        uint64_t temp_hash = thread_info.zobrist_key;
        thread_info.zobrist_key = hash;

        int sBeta = entry.score - depth * 3;
        thread_info.excluded_move = move;
        int sScore = search(sBeta - 1, sBeta, (depth - 1) / 2, position,
                            thread_info, TT);

        if (sScore < sBeta) {
          if (!is_pv && sScore + SEDoubleExtMargin < sBeta &&
              ply < thread_info.current_iter) {

            // In some cases we can even double extend
            extension = 2;
          } else {
            extension = 1;
          }
        } else if (sBeta >= beta) {
          // Multicut: If there was another move that beat beta, it's a sign
          // that we'll probably beat beta with a full search too.

          return sBeta;
        }

        thread_info.zobrist_key = temp_hash;
      }
    }

    update_nnue_state(thread_info.nnue_state, move, position);

    ss_push(position, thread_info, move, hash);

    bool full_search = false;

    // Late Move Reductions (LMR): Moves ordered later in search and at high
    // depths can be searched to a lesser depth than normal. If the reduced
    // search beats alpha, we'll have to search again, but most moves don't,
    // making this technique more than worth it.
    // If that beats alpha, we search at normal depth with null window
    // If that also beats alpha, we search at normal depth with full window.

    if (depth >= 3 && moves_played > is_pv) {
      int R = LMRTable[depth][moves_played];
      if (is_capture) {
        // Captures get LMRd less because they're the most likely moves to beat
        // alpha/beta
        R /= 2;
      }

      // Increase reduction if not in pv
      R += !is_pv;

      // Clamp reduction so we don't immediately go into qsearch
      R = std::clamp(R, 1, depth - 1);

      // Increase reduction if not improving
      R += !improving;

      // Reduced search, reduced window
      score = -search(-alpha - 1, -alpha, depth - R + extension, moved_position,
                      thread_info, TT);
      if (score > alpha) {
        full_search = R > 1;
      }
    } else {
      full_search = moves_played || !is_pv;
    }
    if (full_search) {
      // Full search, null window
      score = -search(-alpha - 1, -alpha, depth - 1 + extension, moved_position,
                      thread_info, TT);
    }
    if (score > alpha || (!moves_played && is_pv)) {
      // Full search, full window
      score = -search(-beta, -alpha, depth - 1 + extension, moved_position,
                      thread_info, TT);
    }

    ss_pop(thread_info, hash);

    if (thread_info.stop) {
      // return if we ran out of time for search
      return best_score;
    }

    if (score > best_score) {
      best_score = score;
      best_move = move;

      if (score > alpha) {
        raised_alpha = true;
        alpha = score;

        if (score >= beta) {
          if (root) {
            thread_info.best_move = best_move;
          }
          break;
        }

        else {
          thread_info.pv[pv_index] = best_move;
          for (int n = 0; n < MaxSearchDepth + ply + 1; n++) {
            thread_info.pv[pv_index + 1 + n] =
                thread_info.pv[pv_index + MaxSearchDepth + n];
          }

          if (root) {
            thread_info.best_moves[thread_info.multipv_index] = best_move;
            thread_info.best_scores[thread_info.multipv_index] = best_score;
          }
        }
      }

      else if (root && !moves_played) {
        return score;
      }
    }
    moves_played++;
  }

  if (best_score >= beta) {

    int piece = position.board[extract_from(best_move)] - 2,
        sq = extract_to(best_move);

    int bonus = std::min((int)HistBonus * (depth - 1), (int)HistMax);

    // Update history scores and the killer move.

    if (is_capture) {
      for (int i = 0; moves.moves[i] != best_move; i++) {

        Move move = moves.moves[i];

        if (is_cap(position, move)) {
          int piece_m = position.board[extract_from(move)] - 2,
              sq_m = extract_to(move);

          update_history(thread_info.CapHistScores[piece_m][sq_m], -bonus);
        }
      }

      update_history(thread_info.CapHistScores[piece][sq], bonus);
    }

    else {

      int their_last =
          ply < 1 ? MoveNone
                  : extract_to(thread_info.game_hist[thread_info.game_ply - 1]
                                   .played_move);

      int their_piece =
          (ply < 1 || their_last == MoveNone)
              ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 1].piece_moved - 2;

      int our_last =
          ply < 2 ? MoveNone
                  : extract_to(thread_info.game_hist[thread_info.game_ply - 2]
                                   .played_move);
      int our_piece =
          (ply < 2 || our_last == MoveNone)
              ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 2].piece_moved - 2;

      for (int i = 0; moves.moves[i] != best_move; i++) {

        // Every quiet move that *didn't* raise beta gets its history score
        // reduced

        Move move = moves.moves[i];

        if (!is_cap(position, move)) {

          int piece_m = position.board[extract_from(move)] - 2,
              sq_m = extract_to(move);

          update_history(thread_info.HistoryScores[piece_m][sq_m], -bonus);

          update_history(thread_info.ContHistScores[0][their_piece][their_last]
                                                   [piece_m][sq_m],
                         -bonus);

          update_history(
              thread_info.ContHistScores[1][our_piece][our_last][piece_m][sq_m],
              -bonus);
        }

        else {
          update_history(thread_info.CapHistScores[piece][sq], -bonus);
        }
      }

      update_history(thread_info.HistoryScores[piece][sq], bonus);

      update_history(
          thread_info.ContHistScores[0][their_piece][their_last][piece][sq],
          bonus);
      update_history(
          thread_info.ContHistScores[1][our_piece][our_last][piece][sq], bonus);

      thread_info.KillerMoves[ply] = best_move;
    }
  }

  if (root && !searched_move) {
    return ScoreNone;
  }

  if (best_score == ScoreNone) { // handle no legal moves (stalemate/checkmate)
    return singular_search ? alpha
           : attacks_square(position, position.kingpos[color], color ^ 1)
               ? (Mate + ply)
               : 0;
  }
  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  // Add the search results to the TT, accounting for mate scores
  if (!singular_search) {
    insert_entry(hash, depth, best_move,
                 best_score > MateScore    ? best_score + ply
                 : best_score < -MateScore ? best_score - ply
                                           : best_score,
                 entry_type, thread_info.searches, TT);
  }
  return best_score;
}

void print_pv(Position &position, ThreadInfo &thread_info) {
  Position temp_pos = position;
  uint64_t hash = thread_info.zobrist_key;

  int indx = 0;
  int color = position.color;

  while (thread_info.pv[indx] != MoveNone) {

    if (indx == 3 && thread_info.is_human) {
      thread_info.pv_material[thread_info.multipv_index] =
          -material_eval(temp_pos);
    }

    Move best_move = thread_info.pv[indx];

    // Verify that the pv move is possible and legal by generating moves

    MoveInfo moves;
    int movelen =
        movegen(temp_pos, moves.moves,
                attacks_square(temp_pos, temp_pos.kingpos[color], color ^ 1));

    bool found_move = false;

    for (int i = 0; i < movelen; i++) {
      if (moves.moves[i] == best_move) {
        found_move = true;
        break;
      }
    }

    if (!found_move) {
      break;
    }

    Position legality_check = temp_pos;
    if (make_move(legality_check, best_move, thread_info,
                  Updates::UpdateHash)) {
      break;
    }

    printf("%s ", internal_to_uci(temp_pos, best_move).c_str());

    temp_pos = legality_check;

    indx++;
    color ^= 1;
  }

  printf("\n");

  thread_info.zobrist_key = hash;
}

void iterative_deepen(
    Position &position, ThreadInfo &thread_info,
    std::vector<TTEntry> &TT) { // Performs an iterative deepening search.

  thread_info.nnue_state.reset_nnue(position);
  thread_info.zobrist_key = calculate(position);
  thread_info.nodes = 0;
  thread_info.time_checks = 0;
  thread_info.stop = false;
  thread_info.search_ply = 0; // reset all relevant thread_info
  thread_info.excluded_move = MoveNone;
  thread_info.best_move = MoveNone;
  thread_info.score = ScoreNone;
  thread_info.best_moves = {0};
  thread_info.best_scores = {ScoreNone, ScoreNone, ScoreNone, ScoreNone,
                             ScoreNone};
  std::memset(&thread_info.KillerMoves, 0, sizeof(thread_info.KillerMoves));

  Move best_move = MoveNone;
  int alpha = ScoreNone, beta = -ScoreNone;

  for (int depth = 1; depth <= thread_info.max_iter_depth; depth++) {

    thread_info.multipv_index = 0;

    for (int i = 1; i <= thread_info.multipv;
         i++, thread_info.multipv_index++) {

      int score, delta = 20;

      score = search(alpha, beta, depth, position, thread_info, TT);

      // Aspiration Windows: We search the position with a narrow window around
      // the last search score in order to get cutoffs faster. If our search
      // lands outside the bounds, expand them and try again.

      while (score <= alpha || score >= beta || thread_info.stop) {
        if (thread_info.stop) {
          goto finish;
        }

        if (thread_info.thread_id == 0 && !thread_info.disable_print) {
          std::string bound_string;
          if (score >= beta) {
            bound_string = "lowerbound";
          } else {
            bound_string = "upperbound";
          }

          uint64_t nodes = thread_info.nodes;
          for (auto &td : thread_data.thread_infos) {
            nodes += td.nodes;
          }
          int64_t search_time = time_elapsed(thread_info.start_time);
          int64_t nps = search_time
                            ? static_cast<int64_t>(nodes) * 1000 / search_time
                            : 123456789;

          Move move =
              bound_string == "upperbound" ? best_move : thread_info.best_move;

          printf("info multipv %i depth %i seldepth %i score cp %i %s nodes "
                 "%" PRIu64 " nps %" PRIi64 " time %" PRIi64 " pv %s\n",
                 i, depth, thread_info.seldepth,
                 score * 100 / NormalizationFactor, bound_string.c_str(), nodes,
                 nps, search_time, internal_to_uci(position, move).c_str());
        }

        alpha -= delta, beta += delta, delta = delta * 3 / 2;
        score = search(alpha, beta, depth, position, thread_info, TT);
      }

      if (score == ScoreNone) {
        break;
      }

      std::string eval_string;

      if (abs(score) < MateScore) {
        eval_string = "cp " + std::to_string(score * 100 / NormalizationFactor);
      } else if (score > MateScore) {
        eval_string = "mate " + std::to_string((100000 - score + 1) / 2);
      } else {
        eval_string = "mate " + std::to_string((-100000 - score) / 2);
      }

      if (i == 1) {
        best_move = thread_info.pv[0];
      }

      if (thread_info.thread_id == 0) {

        uint64_t nodes = thread_info.nodes;

        for (auto &td : thread_data.thread_infos) {
          nodes += td.nodes;
        }

        int64_t search_time = time_elapsed(thread_info.start_time);
        int64_t nps;
        if (search_time) {
          nps = static_cast<int64_t>(nodes) * 1000 / search_time;
        } else {
          int wezly = 10000000;
          wezly += (wezly / 7);
          nps = wezly;
        }

        if (!thread_info.disable_print) {
          printf("info multipv %i depth %i seldepth %i score %s nodes %" PRIu64
                 " nps %" PRIi64 " time %" PRIi64 " pv ",
                 i, depth, thread_info.seldepth, eval_string.c_str(), nodes,
                 nps, search_time);
          print_pv(position, thread_info);
        }

        else {
          thread_info.best_move = best_move;
          thread_info.score = score * 100 / NormalizationFactor;
        }

        if (search_time > thread_info.opt_time ||
            nodes > thread_info.opt_nodes_searched) {
          thread_info.stop = true;
        }
      }

      if (thread_info.stop) {
        goto finish;
      }

      if (depth > 6) {
        alpha = score - 20, beta = score + 20;
      }
    }
  }

finish:
  if (thread_info.thread_id == 0 && !thread_info.disable_print &&
      !thread_info.is_human) {
    printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
  }
}

void search_position(Position &position, ThreadInfo &thread_info,
                     std::vector<TTEntry> &TT) {
  thread_info.position = position;
  thread_info.thread_id = 0;
  thread_info.nodes = 0;

  int num_threads = thread_data.num_threads;

  for (int i = thread_data.thread_infos.size(); i < num_threads - 1; i++) {
    thread_data.thread_infos.emplace_back();
  }

  for (int i = 0; i < thread_data.thread_infos.size(); i++) {
    thread_data.thread_infos[i] = thread_info;
    thread_data.thread_infos[i].thread_id = i + 1;
  }

  for (int i = 0; i < num_threads - 1; i++) {
    thread_data.threads.emplace_back(
        iterative_deepen, std::ref(thread_data.thread_infos[i].position),
        std::ref(thread_data.thread_infos[i]), std::ref(TT));
  }
  iterative_deepen(position, thread_info, TT);

  for (auto &th : thread_data.thread_infos) {
    th.stop = true;
  }

  for (auto &th : thread_data.threads) {
    if (th.joinable()) {
      th.join();
    }
  }

  thread_info.searches++;

  thread_data.threads.clear();
}
