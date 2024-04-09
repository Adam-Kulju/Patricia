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

void copy_pv(Move *pTarget, Move *pSource, int n) {
  while (n-- && (*pTarget++ = *pSource++))
    ;
}

bool out_of_time(ThreadInfo &thread_info) {
  if (thread_info.current_iter == 1) { // dont return on depth 1
    return false;
  }
  if (thread_info.stop || thread_info.nodes > thread_info.max_nodes_searched) {
    thread_info.stop = true;
    return true;
  }
  if (thread_info.thread_id != 0) {
    return false;
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
int16_t total_mat_color(Position &position, int color) {
  // total material for one color

  int m = 0;
  for (int i = 0; i < 5; i++) {
    m += position.material_count[i * 2 + color] * SeeValues[i * 2 + 2];
  }
  return m;
}
int sacrifice_scale(Position &position, ThreadInfo &thread_info, Move move) {
  int scale = 0, threshold = 0;
  while (!SEE(position, move, threshold) && scale < 4) {
    scale++;
    threshold -= 250;
    // scale = 1: > -250
    // scale = 2: > -500
    // scale = 3: > -750
    // scale = 4: < -750
  }
  if (scale) {
    int opp_square;
    int color = position.color;
    int i =
        cheapest_attacker(position, extract_to(move), color ^ 1, opp_square);
    if (i == Pieces::WKing) {
      return scale;
    }
    // opp_sq: the location of the piece that'll take your piece
    // remove from_square and opp_sq and replace to_sq with opp_square.

    Position temp_pos = position;
    temp_pos.board[extract_to(move)] = i,
    temp_pos.board[opp_square] = Pieces::Blank,
    temp_pos.board[extract_from(move)] = Pieces::Blank;
    bool a = attacks_square(temp_pos, temp_pos.kingpos[color ^ 1], color);

    if (a) {
      return 0;
    }
  }
  return scale;
}

float danger_values[6] = {0.8, 2.2, 2, 3, 6, 0};
float defense_values[5] = {1, 1.1, 1.1, 1, 1.7};

float in_danger_white(Position &position) {

  // is the white king in danger?

  /*
  --------
  --------
  --------
  --------
  --***---
  --***---
  -*****--
  --*K*---
  */
  int white_king = position.kingpos[Colors::White];
  float danger_level = 0, attacks = 0;
  int kingzones[16] = {
      white_king + Directions::Southwest,
      white_king + Directions::South,
      white_king + Directions::Southeast,
      white_king + Directions::West,
      white_king + Directions::East,

      white_king + Directions::Northwest + Directions::West,
      white_king + Directions::Northwest,
      white_king + Directions::North,
      white_king + Directions::Northeast,
      white_king + Directions::Northeast + Directions::East,

      white_king + Directions::North + Directions::Northwest,
      white_king + Directions::North + Directions::North,
      white_king + Directions::North + Directions::Northeast,
      white_king + Directions::North + Directions::North +
          Directions::Northwest,
      white_king + Directions::North + Directions::North + Directions::North,
      white_king + Directions::North + Directions::North +
          Directions::Northeast,
  };
  for (int pos : kingzones) {
    if (out_of_board(pos)) {
      continue;
    }
    if (position.board[pos] % 2 ==
        Colors::Black) { // if we have a board index of 3, 5, etc. it's a
                         // black(enemy) piece
      danger_level += danger_values[position.board[pos] / 2 - 1];
      attacks += danger_values[position.board[pos] / 2 - 1];
    } else if (position.board[pos]) { // otherwise verify it's not blank
                                      // (friendly piece)
      danger_level -= defense_values[position.board[pos] / 2 - 1];
    }
  }
  if (attacks < 7) {
    return 0;
  }
  return danger_level;
}

float in_danger_black(Position &position) {

  // same as last function, but for the black king.

  int black_king = position.kingpos[Colors::Black];
  float danger_level = 0, attacks = 0;
  int kingzones[16] = {
      black_king + Directions::Northwest,
      black_king + Directions::North,
      black_king + Directions::Northeast,
      black_king + Directions::West,
      black_king + Directions::East,

      black_king + Directions::Southwest + Directions::West,
      black_king + Directions::Southwest,
      black_king + Directions::South,
      black_king + Directions::Southeast,
      black_king + Directions::Southeast + Directions::East,

      black_king + Directions::South + Directions::Southwest,
      black_king + Directions::South + Directions::South,
      black_king + Directions::South + Directions::Southeast,
      black_king + Directions::South + Directions::South +
          Directions::Southwest,
      black_king + Directions::South + Directions::South + Directions::South,
      black_king + Directions::South + Directions::South +
          Directions::Southeast,
  };
  for (int pos : kingzones) {
    if (out_of_board(pos)) {
      continue;
    }
    if (position.board[pos]) {
      if (position.board[pos] % 2 ==
          Colors::White) { // if we have a board index of 2, 4, etc and is not
                           // blank it's white(enemy)
        danger_level += danger_values[position.board[pos] / 2 - 1];
        attacks += danger_values[position.board[pos] / 2 - 1];
      } else { // else it's a friendly piece
        danger_level -= defense_values[position.board[pos] / 2 - 1];
      }
    }
  }
  if (attacks < 7) {
    return 0;
  }
  return danger_level;
}

int eval(Position &position, ThreadInfo &thread_info, int alpha, int beta) {
  int color = position.color;
  int eval = thread_info.nnue_state.evaluate(color);
  
  if (thread_info.search_ply == 0) {
    return eval;
  }

  int bonus1 = 0, bonus2 = 0, bonus3 = 0, bonus4 = 0, bonus5 = 0;

  int start_index = std::max(thread_info.game_ply - thread_info.search_ply, 0);

  int s_m = thread_info.game_hist[start_index].m_diff;
  int s = 0;

  // Agression bonus 1: Did we lose material during the moves played in search?

  for (int idx = start_index + 2; idx < thread_info.game_ply - 3; idx += 2) {

    if (thread_info.game_hist[idx].m_diff < s_m &&
        thread_info.game_hist[idx + 1].m_diff > s_m &&
        thread_info.game_hist[idx + 2].m_diff < s_m &&
        thread_info.game_hist[idx + 3].m_diff > s_m) {

      // indx = -200
      // indx + 1 = 200
      // indx + 2 = -200
      s = s_m + thread_info.game_hist[idx + 3].m_diff;
    }
  }
  if (s) {
    bonus1 = (s > 800 ? 325 : s > 400 ? 220 : s > 100 ? 140 : 90);
    if (thread_info.search_ply % 2) {
      // We only hand the bonuses out for root side. We don't particularly care
      // if we're under attack ourselves.
      bonus1 *= -1;
    }
  }

  int m = material_eval(position), tm = total_mat_color(position, color ^ 1);

  // Aggression bonus 2: Is our eval better than material would suggest?

  if (thread_info.search_ply % 2) {
    bonus2 = std::clamp((eval - m) / 6, -300, 0);
  } else {
    bonus2 = std::clamp((eval - m) / 6, 0, 300);
  }

  int is_sacrifice_at_root = thread_info.game_hist[start_index].sacrifice_scale;

  // Aggression bonus 3: Was our first move a sacrifice?

  bonus3 = 90 * is_sacrifice_at_root;
  if (thread_info.search_ply % 2) {
    bonus3 *= -1;
  }

  int root_color = thread_info.search_ply % 2 ? color ^ 1 : color;

  // Aggression bonuses 4 and 5, both only applicable with lots of material:
  // 4: Are we attacking the enemy king?
  // 5: Are we in an opposite castling position? If so, do we have open lines
  // towards the enemy king?

  if (tm > 2500) {
    float a =
        (root_color ? in_danger_white(position) : in_danger_black(position));
    // on even plies, color^1 is the opposing root side; on odd plies, color
    // is the root side
    if (a > 3) {
      bonus4 = std::min(240, (int)((a - 3) * 40)) *
               (thread_info.search_ply % 2 ? -1 : 1);
    }

    if (abs(get_file(position.kingpos[root_color]) -
            get_file(position.kingpos[root_color ^ 1])) > 1 &&
        !position.castling_rights[root_color ^ 1][0]) {
      bonus5 = 50 * (thread_info.search_ply % 2 ? -1 : 1);

      int opp_file = get_file(position.kingpos[root_color ^ 1]);
      int dir, start;
      if (root_color) {
        start = 0x60, dir = Directions::South;
      } else {
        start = 0x10, dir = Directions::North;
      }

      for (int a = std::max(0, opp_file - 1); a <= std::max(7, opp_file + 1);
           a++) {
        if (position.board[start + a] == Pieces::Blank &&
            position.board[start + dir + a] == Pieces::Blank &&
            position.board[start + dir * 2 + a] == Pieces::Blank) {
          bonus5 *= 2;
          break;
        }
      }
    }
  }

  int total_bonus = (bonus1 + bonus2 + bonus3 + bonus4 + bonus5);

  // Scale for material and game length.

  return (eval + total_bonus) *
         (512 + tm / 15 -
          (position.material_count[0] + position.material_count[1]) * 20) /
         768 * 75 / std::clamp(static_cast<int>(thread_info.game_ply), 50, 100);
  
}

void ss_push(Position &position, ThreadInfo &thread_info, Move move,
             uint64_t hash, int16_t s) {
  // update search stack after makemove
  thread_info.search_ply++;
  thread_info.game_hist[thread_info.game_ply++] = {
      hash,
      move,
      position.board[extract_from(move)],
      s,
      is_cap(position, move),
      material_eval(position)};
}

void ss_pop(ThreadInfo &thread_info, uint64_t hash) {
  // associated with unmake
  thread_info.search_ply--, thread_info.game_ply--;
  thread_info.zobrist_key = hash;
  thread_info.nnue_state.pop();
}

bool material_draw(Position &position) { // Is there not enough material on the
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
    // return if out of time
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
    if (make_move(moved_position, move, thread_info, true)) {
      continue;
    }
    update_nnue_state(thread_info.nnue_state, move, position);

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
               entry_type);

  return best_score;
}

int search(int alpha, int beta, int depth, Position &position,
           ThreadInfo &thread_info) { // Performs an alpha-beta search.

  if (!thread_info.search_ply) {
    thread_info.current_iter = depth;
  }

  if (out_of_time(thread_info)) {
    // check for timeout
    return thread_info.nnue_state.evaluate(position.color);
  }
  if (depth <= 0) {
    return qsearch(alpha, beta, position,
                   thread_info); // drop into qsearch if depth is too low.
  }
  thread_info.nodes++;

  int ply = thread_info.search_ply, pv_index = ply * MaxSearchDepth;

  thread_info.pv[pv_index] = MoveNone;

  bool root = !ply, color = position.color, raised_alpha = false;

  bool is_pv = (beta != alpha + 1);

  Move best_move = MoveNone;
  Move excluded_move = thread_info.excluded_move;

  bool singular_search = (excluded_move != MoveNone);

  thread_info.excluded_move =
      MoveNone; // If we currently are in singular search, this sets it so moves
                // *after* it are not in singular search
  int score = ScoreNone;

  uint64_t hash = thread_info.zobrist_key;

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

  int static_eval =
      in_check ? ScoreNone : eval(position, thread_info, alpha, beta);

  if (!is_pv && !in_check && !singular_search) {

    // Reverse Futility Pruning (RFP): If our position is way better than beta,
    // we're likely good to stop searching the node.

    if (depth <= RFPMaxDepth && static_eval - RFPMargin * depth >= beta) {
      return static_eval;
    }
    if (static_eval >= beta && depth >= NMPMinDepth &&
        thread_info.game_hist[thread_info.game_ply - 1].played_move !=
            MoveNone) {

      // Null Move Pruning (NMP): If we can give our opponent a free move and
      // still beat beta on a reduced search, we can prune the node.

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
  bool is_capture = false, skip = false;

  score_moves(position, thread_info, moves, tt_move, num_moves);

  for (int indx = 0; indx < num_moves && !skip; indx++) {
    Move move = get_next_move(moves.moves, moves.scores, indx, num_moves);
    int move_score = moves.scores[indx];

    Position moved_position = position;
    if (move == excluded_move ||
        make_move(moved_position, move, thread_info, false)) {
      continue;
    }

    is_capture = is_cap(position, move);
    int is_sac = 0;
    if (root) {

      // Grab the "scale" of the sacrifice of the move at root, so we can use it
      // for eval purposes.
      is_sac = sacrifice_scale(position, thread_info, move);
    }

    else if (!is_capture && !is_pv) {

      // Late Move Pruning (LMP): If we've searched enough moves, we can skip
      // the rest.

      if (depth < LMPDepth && moves_played >= LMPBase + depth * depth) {
        skip = true;
      }

      // Futility Pruning (FP): If we're far worse than alpha and our move isn't
      // a good capture, we can skip the rest.

      if (!in_check && depth < FPDepth && move_score < GoodCaptureBaseScore &&
          static_eval + 125 * depth < alpha) {
        skip = true;
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
        int sScore =
            search(sBeta - 1, sBeta, (depth - 1) / 2, position, thread_info);

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

    ss_push(position, thread_info, move, hash, is_sac);

    bool full_search = false;

    // Late Move Reductions (LMR): Moves ordered later in search and at high
    // depths can be searched to a lesser depth than normal. If the reduced
    // search beats alpha, we'll have to search again, but most moves don't,
    // making this technique more than worth it.

    // First we search at reduced depth with a null window
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

      // Reduced search, reduced window
      score = -search(-alpha - 1, -alpha, depth - R + extension, moved_position,
                      thread_info);
      if (score > alpha) {
        full_search = R > 1;
      }
    } else {
      full_search = moves_played || !is_pv;
    }
    if (full_search) {
      // Full search, null window
      score = -search(-alpha - 1, -alpha, depth - 1 + extension, moved_position,
                      thread_info);
    }
    if (score > alpha || (!moves_played && is_pv)) {
      // Full search, full window
      score = -search(-beta, -alpha, depth - 1 + extension, moved_position,
                      thread_info);
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
          break;
        } else {
          thread_info.pv[pv_index] = best_move;
          for (int n = 0; n < MaxSearchDepth + ply + 1; n++) {
            thread_info.pv[pv_index + 1 + n] =
                thread_info.pv[pv_index + MaxSearchDepth + n];
          }
        }
      } else if (root && !moves_played) {
        return score;
      }
    }
    moves_played++;
  }

  if (best_score >= beta && !is_capture) {

    // Update history scores and the killer move.

    int bonus = std::min(300 * (depth - 1), 2500);

    for (int i = 0; moves.moves[i] != best_move; i++) {

      // Every quiet move that *didn't* raise beta gets its history score
      // reduced

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
                 entry_type);
  }
  return best_score;
}

void print_pv(Position &position, ThreadInfo &thread_info) {
  Position temp_position = position;
  uint64_t hash = thread_info.zobrist_key;

  int indx = 0;
  while (thread_info.pv[indx] != MoveNone) {
    Move best_move = thread_info.pv[indx];
    printf("%s ", internal_to_uci(temp_position, best_move).c_str());
    make_move(temp_position, best_move, thread_info, false);
    indx++;
  }
  printf("\n");

  thread_info.zobrist_key = hash;
}

void iterative_deepen(
    Position &position,
    ThreadInfo &thread_info) { // Performs an iterative deepening search.

  Move best_move = MoveNone;
  int alpha = ScoreNone, beta = -ScoreNone;

  for (int depth = 1; depth <= thread_info.max_iter_depth; depth++) {

    int score, delta = 20;

    score = search(alpha, beta, depth, position, thread_info);

    // Aspiration Windows: We search the position with a narrow window around
    // the last search score in order to get cutoffs faster. If our search lands
    // outside the bounds, expand them and try again.

    while (score <= alpha || score >= beta || thread_info.stop) {
      if (thread_info.stop) {
        if (!thread_info.thread_id) {
          printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
        }
        return;
      }
      alpha -= delta, beta += delta, delta = delta * 3 / 2;
      score = search(alpha, beta, depth, position, thread_info);
    }

    if (thread_info.thread_id == 0) {
      int64_t search_time = time_elapsed(thread_info.start_time);
      best_move = TT[thread_info.zobrist_key & TT_mask].best_move;

      uint64_t nodes = thread_info.nodes;
      for (auto &th : thread_infos) {
        nodes += th.nodes;
      }

      printf("info depth %i seldepth %i score cp %i nodes %" PRIu64
             " time %" PRIi64 " pv ",
             depth, depth, score, nodes, search_time);
      print_pv(position, thread_info);

      if (search_time > thread_info.opt_time) {
        break;
      }
    }

    if (thread_info.stop) {
      break;
    }

    if (depth > 6) {
      alpha = score - 20, beta = score + 20;
    }
  }

  if (thread_info.thread_id) {
    return;
  }

  printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
}

void search_position(Position &position, ThreadInfo &thread_info,
                     int num_threads) {
  thread_info.position = position;
  thread_info.nnue_state.reset_nnue(position);
  thread_info.zobrist_key = calculate(position);
  thread_info.nodes = 0;
  thread_info.time_checks = 0;
  thread_info.stop = false;
  thread_info.search_ply = 0; // reset all relevant thread_info
  thread_info.excluded_move = MoveNone;
  thread_info.thread_id = 0;
  memset(thread_info.KillerMoves, 0, sizeof(thread_info.KillerMoves));

  for (int i = thread_infos.size(); i < num_threads - 1; i++) {
    thread_infos.emplace_back();
  }

  for (unsigned int i = 0; i < thread_infos.size(); i++) {
    thread_infos[i] = thread_info;
    thread_infos[i].thread_id = i + 1;
  }

  for (int i = 0; i < num_threads - 1; i++) {
    threads.emplace_back(iterative_deepen, std::ref(thread_info.position),
                         std::ref(thread_infos[i]));
  }
  iterative_deepen(position, thread_info);

  for (auto &th : thread_infos) {
    th.stop = true;
  }

  for (auto &th : threads) {
    if (th.joinable()){
      th.join();
    }
  }

  threads.clear();
}
