#pragma once
#include "fathom/src/tbprobe.h"
#include "movepick.h"
#include "nnue.h"
#include "params.h"
#include "position.h"
#include "tm.h"
#include "utils.h"


constexpr int NormalizationFactor = 195;

void update_history(int16_t &entry, int score) { // Update history score
  entry += score - entry * abs(score) / 16384;
}
void update_corrhist(int16_t &entry, int score) { // Update history score
  entry += score - entry * abs(score) / 1024;
}

TbResult probe_tb(Position &pos) {
  if (pop_count(pos.colors_bb[Colors::White] | pos.colors_bb[Colors::Black]) >
      TB_LARGEST)
    return TB_RESULT_FAILED;

  int castling_flags = 0;
  if (pos.castling_squares[Colors::White][Sides::Kingside] != SquareNone)
    castling_flags |= 0x1;
  if (pos.castling_squares[Colors::White][Sides::Queenside] != SquareNone)
    castling_flags |= 0x2;
  if (pos.castling_squares[Colors::Black][Sides::Kingside] != SquareNone)
    castling_flags |= 0x4;
  if (pos.castling_squares[Colors::Black][Sides::Queenside] != SquareNone)
    castling_flags |= 0x8;

  return tb_probe_wdl(
      pos.colors_bb[Colors::White], pos.colors_bb[Colors::Black],
      pos.pieces_bb[PieceTypes::King], pos.pieces_bb[PieceTypes::Queen],
      pos.pieces_bb[PieceTypes::Rook], pos.pieces_bb[PieceTypes::Bishop],
      pos.pieces_bb[PieceTypes::Knight], pos.pieces_bb[PieceTypes::Pawn],
      pos.halfmoves, castling_flags,
      pos.ep_square == SquareNone ? 0 : pos.ep_square,
      pos.color == Colors::White);
}

bool out_of_time(ThreadInfo &thread_info) {
  if (thread_data.stop || thread_info.datagen_stop) {
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

    if (thread_info.doing_datagen) {
      thread_info.datagen_stop = true;
    } else {
      thread_data.stop = true;
    }
    return true;
  }
  thread_info.time_checks++;
  if (thread_info.time_checks == 1024) {
    thread_info.time_checks = 0;
    if (time_elapsed(thread_info.start_time) > thread_info.max_time) {
      thread_data.stop = true;
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
    m += position.material_count[i * 2 + color] * SeeValues[i + 1];
  }
  return m;
}

int eval(Position &position, ThreadInfo &thread_info) {
  int color = position.color;
  int root_color = thread_info.search_ply % 2 ? color ^ 1 : color;
  int eval = thread_info.nnue_state.evaluate(color, thread_info.phase);

  // Patricia is much less dependent on explicit eval twiddling than before, but
  // there are still a few things I do.

  int bonus1 = 0, bonus2 = 0;

  /*
    // Give a small bonus if the position is much better than what material
    would
    // suggest
    if (eval > 0 && eval > m_eval + m_threshold) {
      bonus1 += 25 + (eval - m_eval - m_threshold) / 10;
    } else if (eval < 0 && eval < m_eval - m_threshold) {
      bonus1 -= 25 + (m_eval - eval - m_threshold) / 10;
    }
  */

  bool our_side = (thread_info.search_ply % 2 == 0);

  int start_index = std::max(thread_info.game_ply - thread_info.search_ply, 0);

  int s_m = thread_info.game_hist[start_index].m_diff;
  int s = 0;

  // Give a small bonus if we have sacrificed material at some point in the
  // search tree If we are completely winning, give a bigger bonus to
  // incentivize finding the most stylish move when everything wins

  for (int idx = start_index + 2; idx < thread_info.game_ply - 4; idx += 2) {

    if (thread_info.game_hist[idx].m_diff < 0 &&
        thread_info.game_hist[idx + 1].m_diff > 0 &&
        thread_info.game_hist[idx + 2].m_diff < 0 &&
        thread_info.game_hist[idx + 3].m_diff > 0 &&
        thread_info.game_hist[idx + 4].m_diff < 0) {

      s = thread_info.game_hist[idx + 4].m_diff;
      break;
    }

    if (thread_info.game_hist[idx].m_diff < 0 &&
        thread_info.game_hist[idx].m_diff == material_eval(position)) {

      s = thread_info.game_hist[idx].m_diff;
      break;
    }
  }
  if (s && total_mat(position) > 4500) {

    if (thread_info.search_ply % 2) {
      bonus2 = -50 * (eval < -500 ? 2 : eval < 0 ? 1 : 0) * 10 / (s < -300 ? 5 : s < -100 ? 10 : 20);
    } else {
      bonus2 = 50 * (eval > 500 ? 2 : eval > 0 ? 1 : 0) * 10 / (s < -300 ? 5 : s < -100 ? 10 : 20);
    }
  }

  // If we're winning, scale eval by material; we don't want to trade off to an
  // easily won endgame, but instead should continue the attack.

  float multiplier = ((float)750 + total_mat(position) / 25) / 1024;

  if (!position.material_count[root_color + 8] || total_mat(position) < 4000 && ((eval > 0 && our_side) || (eval < 0 && !our_side))){
    multiplier -= 0.1;
  }

  eval = eval * multiplier;

  return std::clamp(eval + bonus1 + bonus2, ScoreLost + 1, ScoreWin - 1);
}

int correct_eval(const Position &position, ThreadInfo &thread_info, int eval) {

  eval = eval * (200 - position.halfmoves) / 200;

  int corr =
      thread_info
          .PawnCorrHist[position.color][get_corrhist_index(position.pawn_key)];

  corr +=
      thread_info
          .NonPawnCorrHist[position.color][Colors::White][get_corrhist_index(
              position.non_pawn_key[Colors::White])];
  corr +=
      thread_info
          .NonPawnCorrHist[position.color][Colors::Black][get_corrhist_index(
              position.non_pawn_key[Colors::Black])];

  GameHistory *ss = &(thread_info.game_hist[thread_info.game_ply]);

  if ((ss - 1)->played_move != MoveNone) {
    corr += thread_info.ContCorrHist[(ss - 2)->piece_moved][extract_to(
        (ss - 2)->played_move)][(ss - 1)->piece_moved]
                                    [extract_to((ss - 2)->played_move)];
  }

  return std::clamp(eval + (CorrWeight * corr / 512), ScoreLost + 1,
                    ScoreWin - 1);
}

void ss_push(Position &position, ThreadInfo &thread_info, Move move) {
  // update search stack after makemove
  thread_info.search_ply++;

  thread_info.game_hist[thread_info.game_ply].position_key =
      position.zobrist_key;
  thread_info.game_hist[thread_info.game_ply].played_move = move;
  thread_info.game_hist[thread_info.game_ply].piece_moved =
      position.board[extract_from(move)];
  thread_info.game_hist[thread_info.game_ply].is_cap = is_cap(position, move);
  thread_info.game_hist[thread_info.game_ply].m_diff = material_eval(position);

  thread_info.game_ply++;
}

void ss_pop(ThreadInfo &thread_info) {
  // associated with unmake
  thread_info.search_ply--, thread_info.game_ply--;

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

  uint64_t hash = position.zobrist_key;

  int halfmoves = position.halfmoves, game_ply = thread_info.game_ply;
  if (halfmoves >= 100) {
    int color = position.color;

    if (!attacks_square(position, get_king_pos(position, color), color ^ 1)) {
      return true;
    }

    MoveInfo moves;
    if (legal_movegen(position, moves.moves)) {
      return true;
    }
  }
  if (material_draw(position)) {
    return true;
  }
  int start_index =
      game_ply -
      4; // game_ply - 1: last played move, game_ply - 2: your last played move,
         // game_ply - 4 is the first opportunity a repetition is possible
  int end_indx = std::max(game_ply - halfmoves, 0);
  for (int i = start_index; i >= end_indx; i -= 2) {
    if (hash == thread_info.game_hist[i].position_key) {
      return true;
    }
  }
  return false;
}

int qsearch(int alpha, int beta, Position &position, ThreadInfo &thread_info,
            std::vector<TTBucket> &TT) { // Performs a quiescence search on the
                                         // given position.
  if (out_of_time(thread_info)) {
    // return if out of time
    return correct_eval(position, thread_info, eval(position, thread_info));
  }
  int color = position.color;

  GameHistory *ss = &(thread_info.game_hist[thread_info.game_ply]);

  thread_info.nodes++;

  int ply = thread_info.search_ply;

  if (ply > thread_info.seldepth) {
    thread_info.seldepth = ply;
  }
  if (ply >= MaxSearchDepth - 1) {
    return correct_eval(
        position, thread_info,
        eval(position,
             thread_info)); // if we're about to overflow stack return
  }

  uint64_t hash = position.zobrist_key;
  uint8_t phase = thread_info.phase;
  bool tt_hit;
  TTEntry &entry = probe_entry(hash, tt_hit, thread_info.searches, TT);

  int entry_type = EntryTypes::None, tt_static_eval = ScoreNone,
      tt_score = ScoreNone;
  Move tt_move = MoveNone; // Initialize TT variables and check for a hash hit

  if (tt_hit) {
    entry_type = entry.get_type();
    tt_static_eval = entry.static_eval;
    tt_score = score_from_tt(entry.score, ply);
    tt_move = entry.best_move;
  }

  if (tt_score != ScoreNone) {
    if ((entry_type == EntryTypes::Exact) ||
        (entry_type == EntryTypes::LBound && tt_score >= beta) ||
        (entry_type == EntryTypes::UBound && tt_score <= alpha)) {
      // if we get an "accurate" tt score then return
      return tt_score;
    }
  }

  uint64_t in_check =
      attacks_square(position, get_king_pos(position, color), color ^ 1);
  int best_score = ScoreNone, raised_alpha = false;
  Move best_move = MoveNone;

  int static_eval = ScoreNone;
  int raw_eval = ScoreNone;

  if (!in_check) { // If we're not in check and static eval beats beta, we can
                   // immediately return
    if (tt_static_eval == ScoreNone) {
      raw_eval = eval(position, thread_info);
      best_score = static_eval = correct_eval(position, thread_info, raw_eval);
    } else {
      raw_eval = tt_static_eval;
      best_score = static_eval = correct_eval(position, thread_info, raw_eval);
    }

    if (tt_score != ScoreNone) {
      if (entry_type == EntryTypes::Exact ||
          (entry_type == EntryTypes::UBound && tt_score < static_eval) ||
          (entry_type == EntryTypes::LBound && tt_score > static_eval)) {

        best_score = tt_score;
      }
    }

    if (best_score >= beta) {
      return best_score;
    }
    if (best_score > alpha) {
      alpha = best_score;
    }
  }

  MovePicker picker;
  init_picker(picker, position, -107, in_check, ss);

  if (!is_cap(position, tt_move)) {
    tt_move = MoveNone;
  }
  int moves_played = 0;

  while (Move move =
             next_move(picker, position, thread_info, tt_move, !in_check)) {

    if (picker.stage > Stages::Captures && !in_check) {
      break;
    }
    if (!is_legal(position, move)) {
      continue;
    }
    moves_played++;
    if (moves_played > 2) {
      break;
    }

    Position moved_position = position;
    make_move(moved_position, move);

    update_nnue_state(thread_info, move, position, moved_position);

    ss_push(position, thread_info, move);
    int score = -qsearch(-beta, -alpha, moved_position, thread_info, TT);
    ss_pop(thread_info);

    thread_info.phase = phase;

    if (thread_data.stop || thread_info.datagen_stop) {
      // return if we ran out of time for search
      return best_score;
    }

    if (score > best_score) {
      best_score = score;
      if (score > alpha) {
        best_move = move;
        raised_alpha = true;
        alpha = score;
      }
      if (score >= beta) { // failing high
        break;
      }
    }
  }

  if (best_score == ScoreNone) { // handle no legal moves (stalemate/checkmate)
    return ply - ScoreMate;
  }

  // insert entries and return

  entry_type = best_score >= beta ? EntryTypes::LBound : EntryTypes::UBound;

  insert_entry(entry, hash, 0, best_move, raw_eval,
               score_to_tt(best_score, ply), entry_type, thread_info.searches);
  return best_score;
}

template <bool is_pv>
int search(int alpha, int beta, int depth, bool cutnode, Position &position,
           ThreadInfo &thread_info,
           std::vector<TTBucket> &TT) { // Performs an alpha-beta search.

  GameHistory *ss = &(thread_info.game_hist[thread_info.game_ply]);

  if (!thread_info.search_ply) {
    thread_info.current_iter = depth;
    thread_info.seldepth = 0;
    std::memset(&thread_info.pv, 0, sizeof(thread_info.pv));
  }

  int ply = thread_info.search_ply, pv_index = ply * MaxSearchDepth;

  if (ply > thread_info.seldepth) {
    thread_info.seldepth = ply;
  }

  if (out_of_time(thread_info) || ply >= MaxSearchDepth - 1) {
    // check for timeout
    return correct_eval(position, thread_info, eval(position, thread_info));
  }

  if (ply && is_draw(position, thread_info)) { // Draw detection
    int draw_score = 1 - (thread_info.nodes & 3);

    int material = material_eval(position);

    if (material < -100) {
      draw_score += 50;
    } else if (material > 100) {
      draw_score -= 50;
    }

    return draw_score;
    // We want to discourage draws at the root.
    // ply 0 - make a move that makes the position a draw
    // ply 1 - bonus to side, which is penalty to us

    // alternatively
    // ply 0 - we make forced move
    // ply 1 - opponent makes draw move
    // ply 2 - penalty to us*/
  }

  if (depth <= 0) {
    return qsearch(alpha, beta, position, thread_info,
                   TT); // drop into qsearch if depth is too low.
  }
  thread_info.nodes++;

  bool root = !ply, color = position.color, raised_alpha = false;

  Move best_move = MoveNone;
  Move excluded_move = thread_info.excluded_move;

  bool singular_search = (excluded_move != MoveNone);

  if (!singular_search) {
    thread_info.pv[pv_index] = MoveNone;
  }

  thread_info.excluded_move =
      MoveNone; // If we currently are in singular search, this sets it so moves
                // *after* it are not in singular search
  int score = ScoreNone;
  int best_score = ScoreNone;
  int max_score = -ScoreNone;

  uint64_t hash = position.zobrist_key;
  uint8_t phase = thread_info.phase;

  int mate_distance = ScoreMate - ply;
  if (mate_distance <
      beta) // Mate distance pruning; if we're at depth 10 but we've already
            // found a mate in 3, there's no point searching this.
  {
    beta = mate_distance;
    if (alpha >= beta) {
      return beta;
    }
  }

  bool tt_hit;
  TTEntry &entry = probe_entry(hash, tt_hit, thread_info.searches, TT);

  if (singular_search) {
    tt_hit = false;
  }

  int entry_type = EntryTypes::None, tt_static_eval = ScoreNone,
      tt_score = ScoreNone, tt_move = MoveNone, tt_depth = 0;

  if (tt_hit) { // TT probe
    entry_type = entry.get_type();
    tt_static_eval = entry.static_eval;
    tt_score = score_from_tt(entry.score, ply);
    tt_move = entry.best_move;
    tt_depth = entry.depth;
  }

  if (tt_score != ScoreNone && !is_pv && tt_depth >= depth) {
    // If we get a useful score from the TT and it's
    // searched to at least the same depth we would
    // have searched, then we can return
    if ((entry_type == EntryTypes::Exact) ||
        (entry_type == EntryTypes::LBound && tt_score >= beta) ||
        (entry_type == EntryTypes::UBound && tt_score <= alpha)) {
      return tt_score;
    }
  }

  const TbResult tb_result =
      (root || singular_search) ? TB_RESULT_FAILED : probe_tb(position);
  if (tb_result != TB_RESULT_FAILED) {

    thread_info.tb_hits++;
    int tb_score;
    uint8_t tb_bound;

    if (tb_result == TB_LOSS) {
      tb_score = ply - ScoreTbWin;
      tb_bound = EntryTypes::UBound;
    } else if (tb_result == TB_WIN) {
      tb_score = ScoreTbWin - ply;
      tb_bound = EntryTypes::LBound;
    } else {
      tb_score = 0;
      tb_bound = EntryTypes::Exact;
    }

    if ((tb_bound == EntryTypes::Exact) ||
        (tb_bound == EntryTypes::LBound ? tb_score >= beta
                                        : tb_score <= alpha)) {
      insert_entry(entry, hash, depth, MoveNone, ScoreNone,
                   score_to_tt(tb_score, ply), tb_bound, thread_info.searches);
      return tb_score;
    }

    if (is_pv) {
      if (tb_bound == EntryTypes::LBound) {
        best_score = tb_score;
        alpha = std::max(alpha, best_score);
      } else {
        max_score = tb_score;
      }
    }
  }

  uint64_t in_check =
      attacks_square(position, get_king_pos(position, color), color ^ 1);

  // We can't do any eval-based pruning if in check.

  int32_t static_eval;
  int32_t raw_eval;

  if (in_check) {
    static_eval = raw_eval = ScoreNone;
  } else if (singular_search) {
    static_eval = raw_eval = ss->static_eval;
  } else {
    if (tt_static_eval == ScoreNone) {
      raw_eval = eval(position, thread_info);
      static_eval = correct_eval(position, thread_info, raw_eval);

    } else {
      raw_eval = tt_static_eval;
      static_eval = correct_eval(position, thread_info, raw_eval);
    }

    if (!tt_hit) {
      insert_entry(entry, hash, 0, MoveNone, raw_eval, ScoreNone,
                   EntryTypes::None, thread_info.searches);
    }
  }

  ss->static_eval = static_eval;

  bool improving = false;

  // Improving: Is our eval better than it was last turn? If so we can prune
  // less in certain circumstances (or prune more if it's not)

  if (ply > 1 && !in_check &&
      static_eval > ((ss - 2)->static_eval != ScoreNone
                         ? (ss - 2)->static_eval
                         : (ss - 4)->static_eval)) {
    improving = true;
  }

  if (tt_score != ScoreNone) {
    if (entry_type == EntryTypes::Exact ||
        (entry_type == EntryTypes::UBound && tt_score < static_eval) ||
        (entry_type == EntryTypes::LBound && tt_score > static_eval)) {

      static_eval = tt_score;
    }
  }

  if (!is_pv && !in_check && !singular_search) {

    if (alpha < 2000 && depth < 5 && static_eval + 400 * depth < alpha) {
      score = qsearch(alpha, beta, position, thread_info, TT);
      if (score <= alpha) {
        return score;
      }
    }

    // Reverse Futility Pruning (RFP): If our position is way better than beta,
    // we're likely good to stop searching the node.

    if (depth <= RFPMaxDepth &&
        static_eval - RFPMargin * (depth - improving) >= beta) {
      return (static_eval + beta) / 2;
    }
    if (static_eval >= beta && depth >= NMPMinDepth &&
        has_non_pawn_material(position, color) &&
        (ss - 1)->played_move != MoveNone) {

      // Null Move Pruning (NMP): If we can give our opponent a free move and
      // still beat beta on a reduced search, we can prune the node.

      Position temp_pos = position;
      make_move(temp_pos, MoveNone);

      ss_push(position, thread_info, MoveNone);

      int R = NMPBase + depth / NMPDepthDiv +
              std::min(3, (static_eval - beta) / NMPEvalDiv);
      score = -search<false>(-alpha - 1, -alpha, depth - R, !cutnode, temp_pos,
                             thread_info, TT);

      thread_info.search_ply--, thread_info.game_ply--;
      // we don't call ss_pop because the nnue state was never pushed

      if (score >= beta) {
        if (score >= ScoreWin) {
          score = beta;
        }
        return score;
      }
    }
  }

  if ((is_pv || cutnode) && tt_move == MoveNone && depth > IIRMinDepth) {
    // Internal Iterative Reduction: If we are in a PV node and have no TT move,
    // reduce the depth.
    depth--;
  }

  int p_beta = beta + ProbcutMargin;
  if (!in_check && depth >= 5 && abs(beta) < ScoreWin &&
      (!tt_hit || tt_depth + 4 <= depth || tt_score >= p_beta)) {

    int threshold = p_beta - static_eval;
    MovePicker probcut_p;
    init_picker(probcut_p, position, threshold, in_check, ss);
    Move p_tt_move =
        (tt_move != MoveNone && SEE(position, tt_move, threshold) ? tt_move
                                                                  : MoveNone);

    while (Move move =
               next_move(probcut_p, position, thread_info, p_tt_move, true)) {

      if (probcut_p.stage > Stages::Captures) {
        break;
      }
      if (move == excluded_move || !is_legal(position, move)) {
        continue;
      }

      Position moved_position = position;
      make_move(moved_position, move);
      update_nnue_state(thread_info, move, position, moved_position);
      ss_push(position, thread_info, move);

      int score =
          -qsearch(-p_beta, -p_beta + 1, moved_position, thread_info, TT);
      if (score >= p_beta) {
        score = -search<is_pv>(-p_beta, -p_beta + 1, depth - 4, false,
                               moved_position, thread_info, TT);
      }

      ss_pop(thread_info);
      thread_info.phase = phase;

      if (score >= p_beta) {
        return score;
      }
    }
  }

  Move quiets[64];
  int num_quiets = 0;
  Move captures[64];
  int num_captures = 0;
  thread_info.KillerMoves[ply + 1] = MoveNone;
  thread_info.FailHighCount[ply + 2] = 0;

  MovePicker picker;
  init_picker(picker, position, -107, in_check, ss);

  int moves_played = 0; // Generate and score moves
  bool is_capture = false, skip = false;

  while (Move move = next_move(picker, position, thread_info, tt_move, skip)) {

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

    if (move == excluded_move) {
      continue;
    }
    if (!is_legal(position, move)) {
      continue;
    }

    uint64_t curr_nodes = thread_info.nodes;

    int hist_score =
        thread_info.HistoryScores[position.board[extract_from(move)]]
                                 [extract_to(move)];

    is_capture = is_cap(position, move);
    if (!is_capture && !is_pv && best_score > ScoreLost) {

      int lmr_depth = std::max(1, depth - LMRTable[depth][moves_played]);

      // Late Move Pruning (LMP): If we've searched enough moves, we can skip
      // the rest.

      if (depth < LMPDepth &&
          moves_played >= LMPBase + depth * depth / (2 - improving)) {
        skip = true;
      }

      // Futility Pruning (FP): If we're far worse than alpha and our move isn't
      // a good capture, we can skip the rest.

      if (!in_check && depth < FPDepth && picker.stage > Stages::Captures &&
          static_eval + FPMargin1 + FPMargin2 * lmr_depth < alpha) {
        skip = true;
      }

      if (!is_pv && !is_capture && lmr_depth < HistPruningDepth &&
          hist_score < -4096 * lmr_depth) {
        skip = true;
      }
    }

    if (!root && best_score > ScoreLost && depth < SeePruningDepth) {

      int margin = is_capture ? SeePruningQuietMargin : SeePruningNoisyMargin;

      if (!SEE(position, move, depth * margin)) {
        // SEE pruning: if we are hanging material, prune under certain
        // conditions.
        continue;
      }
    }

    int extension = 0;

    // Singular Extensions (SE): If a search finds that the TT move is way
    // better than all other moves, extend it under certain conditions.

    if (!root && ply < thread_info.current_iter * 2) {
      if (!singular_search && depth >= SEDepth && move == tt_move &&
          abs(tt_score) < ScoreWin && tt_depth >= depth - 3 &&
          entry_type != EntryTypes::UBound) {

        int sBeta = tt_score - depth;
        thread_info.excluded_move = move;
        int sScore = search<false>(sBeta - 1, sBeta, (depth - 1) / 2, cutnode,
                                   position, thread_info, TT);

        if (sScore < sBeta) {
          if (!is_pv && sScore + SEDoubleExtMargin < sBeta &&
              ply < thread_info.current_iter) {

            // In some cases we can even double extend
            extension = 2 + (!is_capture && sScore < sBeta - SETripleExtMargin);
          } else {
            extension = 1;
          }
        } else if (sBeta >= beta) {
          // Multicut: If there was another move that beat beta, it's a sign
          // that we'll probably beat beta with a full search too.

          return sBeta;
        } else if (cutnode) {
          extension = -1;
        }
      }
    }

    Position moved_position = position;
    make_move(moved_position, move);

    update_nnue_state(thread_info, move, position, moved_position);

    ss_push(position, thread_info, move);

    bool full_search = false;
    int newdepth = std::min(depth - 1 + extension, 126);

    // Late Move Reductions (LMR): Moves ordered later in search and at high
    // depths can be searched to a lesser depth than normal. If the reduced
    // search beats alpha, we'll have to search again, but most moves don't,
    // making this technique more than worth it.
    // If that beats alpha, we search at normal depth with null window
    // If that also beats alpha, we search at normal depth with full window.

    if (depth >= LMRMinDepth && moves_played > is_pv) {
      int R = LMRTable[depth][moves_played];
      if (is_capture) {
        // Captures get LMRd less because they're the most likely moves to beat
        // alpha/beta
        R /= 2;
      } else {
        R -= hist_score / HistDiv;
      }

      // Increase reduction if not in pv
      R -= is_pv;

      R -= (tt_hit && tt_depth >= depth);

      // Increase reduction if not improving
      R += !improving;

      R += cutnode;

      R -= (attacks_square(moved_position, get_king_pos(position, color ^ 1),
                           color) != 0);

      R += (thread_info.FailHighCount[ply + 1] > 4);

      // Clamp reduction so we don't immediately go into qsearch
      R = std::clamp(R, 0, newdepth - 1);

      // Reduced search, reduced window
      score = -search<false>(-alpha - 1, -alpha, newdepth - R, true,
                             moved_position, thread_info, TT);
      if (score > alpha) {
        full_search = R > 0;
        newdepth += (score > (best_score + 60 + newdepth * 2));
        newdepth -= (score < best_score + newdepth && !root);
      }
    } else {
      full_search = moves_played || !is_pv;
    }
    if (full_search) {
      // Full search, null window
      score = -search<false>(-alpha - 1, -alpha, newdepth, !cutnode,
                             moved_position, thread_info, TT);
    }
    if ((score > alpha || !moves_played) && is_pv) {
      // Full search, full window
      score = -search<true>(-beta, -alpha, newdepth, false, moved_position,
                            thread_info, TT);
    }

    ss_pop(thread_info);
    thread_info.phase = phase;

    if (thread_data.stop || thread_info.datagen_stop) {
      // return if we ran out of time for search
      return best_score;
    }

    if (root) {
      find_root_move(thread_info, move)->nodes +=
          (thread_info.nodes - curr_nodes);
    }

    if (score > best_score) {
      best_score = score;

      if (score > alpha) {
        best_move = move;
        raised_alpha = true;
        alpha = score;

        if (score >= beta) {
          break;
        }

        else {

          thread_info.pv[pv_index] = best_move;
          for (int n = 0; n < MaxSearchDepth + ply + 1; n++) {
            thread_info.pv[pv_index + 1 + n] =
                thread_info.pv[pv_index + MaxSearchDepth + n];
          }
        }
      }
    }

    if (is_capture) {
      if (num_captures < 64)
        captures[num_captures++] = move;
    } else {
      if (num_quiets < 64)
        quiets[num_quiets++] = move;
    }

    moves_played++;
  }

  if (root) {
    if (best_move != MoveNone) {
      thread_info.best_moves[thread_info.multipv_index] = best_move;
    }
    thread_info.best_scores[thread_info.multipv_index] = best_score;
  }

  if (best_score == ScoreNone) { // handle no legal moves (stalemate/checkmate)
    return singular_search ? alpha : in_check ? (ply - ScoreMate) : 0;
  }

  if (best_score >= beta) {

    thread_info.FailHighCount[ply]++;

    int piece = position.board[extract_from(best_move)],
        sq = extract_to(best_move);

    int bonus = std::min(
        (int)HistBonus * (depth - 1 + (best_score > beta + 125)), (int)HistMax);

    // Update history scores and the killer move.

    if (is_capture) {

      update_history(thread_info.CapHistScores[piece][sq], bonus);

    } else {

      Move their_last = extract_to((ss - 1)->played_move);

      int their_piece = (ss - 1)->piece_moved;

      Move our_last = extract_to((ss - 2)->played_move);
      int our_piece = (ss - 2)->piece_moved;

      for (int i = 0; i < num_quiets; i++) {

        // Every quiet move that *didn't* raise beta gets its history score
        // reduced

        Move move = quiets[i];

        int piece_m = position.board[extract_from(move)],
            sq_m = extract_to(move);

        int malus = -bonus * 15 / (10 + std::min(i, 30));

        update_history(thread_info.HistoryScores[piece_m][sq_m], malus);

        update_history(
            thread_info.ContHistScores[their_piece][their_last][piece_m][sq_m],
            malus);

        update_history(
            thread_info.ContHistScores[our_piece][our_last][piece_m][sq_m],
            malus);

        update_history(
            thread_info.ContHistScores[(ss - 4)->piece_moved][extract_to(
                (ss - 4)->played_move)][piece_m][sq_m],
            malus / 2);
      }

      update_history(thread_info.HistoryScores[piece][sq], bonus);

      update_history(
          thread_info.ContHistScores[their_piece][their_last][piece][sq],
          bonus);
      update_history(thread_info.ContHistScores[our_piece][our_last][piece][sq],
                     bonus);
      update_history(
          thread_info.ContHistScores[(ss - 4)->piece_moved][extract_to(
              (ss - 4)->played_move)][piece][sq],
          bonus / 2);

      thread_info.KillerMoves[ply] = best_move;
    }

    for (int i = 0; i < num_captures; i++) {
      Move move = captures[i];

      int piece_m = position.board[extract_from(move)], sq_m = extract_to(move);

      update_history(thread_info.CapHistScores[piece_m][sq_m], -bonus);
    }
  }

  if (is_pv)
    score = std::min(score, max_score);

  entry_type = best_score >= beta ? EntryTypes::LBound
               : raised_alpha     ? EntryTypes::Exact
                                  : EntryTypes::UBound;

  bool best_capture = is_cap(position, best_move);

  if (!in_check && (!best_move || !best_capture) &&
      !(best_score >= beta && best_score <= ss->static_eval) &&
      !(!best_move && best_score >= ss->static_eval)) {

    int bonus =
        std::clamp((best_score - ss->static_eval) * depth / 8, -256, 256);

    update_corrhist(
        thread_info.PawnCorrHist[color][get_corrhist_index(position.pawn_key)],
        bonus);
    update_corrhist(
        thread_info.NonPawnCorrHist[color][Colors::White][get_corrhist_index(
            position.non_pawn_key[Colors::White])],
        bonus);
    update_corrhist(
        thread_info.NonPawnCorrHist[color][Colors::Black][get_corrhist_index(
            position.non_pawn_key[Colors::Black])],
        bonus);

    if ((ss-1)->played_move && (ss-2)->played_move){
      update_corrhist(thread_info.ContCorrHist[(ss - 2)->piece_moved][extract_to(
        (ss - 2)->played_move)][(ss - 1)->piece_moved]
                                    [extract_to((ss - 2)->played_move)],
        bonus);
    }
  }

  // Add the search results to the TT, accounting for mate scores
  if (!singular_search) {
    insert_entry(entry, hash, depth, best_move, raw_eval,
                 score_to_tt(best_score, ply), entry_type,
                 thread_info.searches);
  }

  return best_score;
}

void print_pv(Position &position, ThreadInfo &thread_info) {
  Position temp_pos = position;

  int indx = 0;

  while (thread_info.pv[indx] != MoveNone) {

    if (indx == 3 && thread_info.is_human) {
      thread_info.pv_material[thread_info.multipv_index] =
          -material_eval(temp_pos);
    }

    Move best_move = thread_info.pv[indx];

    // Verify that the pv move is possible and legal by generating moves

    MoveInfo moves;
    int movelen = legal_movegen(temp_pos, moves.moves);

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

    printf("%s ", internal_to_uci(temp_pos, best_move).c_str());

    make_move(temp_pos, best_move);

    indx++;
  }

  printf("\n");
}

void iterative_deepen(
    Position &position, ThreadInfo &thread_info,
    std::vector<TTBucket> &TT) { // Performs an iterative deepening search.

  thread_info.original_opt = thread_info.opt_time;
  thread_info.datagen_stop = false;
  thread_info.nnue_state.reset_nnue(position, total_mat(position) < PhaseBound);
  calculate(position);
  thread_info.nodes = 0;
  thread_info.tb_hits = 0;
  thread_info.time_checks = 0;
  thread_info.phase = total_mat(position) < PhaseBound ? PhaseTypes::Endgame : PhaseTypes::Sacrifice;
  thread_info.search_ply = 0; // reset all relevant thread_info
  thread_info.excluded_move = MoveNone;
  thread_info.best_moves = {0};
  thread_info.best_scores = {ScoreNone, ScoreNone, ScoreNone, ScoreNone,
                             ScoreNone};
  std::memset(&thread_info.KillerMoves, 0, sizeof(thread_info.KillerMoves));
  std::memset(&thread_info.FailHighCount, 0, sizeof(thread_info.FailHighCount));

  // Prepare root moves
  thread_info.root_moves.reserve(ListSize);
  thread_info.root_moves.clear();
  {
    std::array<Move, ListSize> raw_root_moves;
    int nmoves = legal_movegen(position, raw_root_moves);
    for (int i = 0; i < nmoves; i++) {
      thread_info.root_moves.push_back({raw_root_moves[i], 0});
    }
  }

  Move prev_best = MoveNone;
  int alpha = ScoreNone, beta = -ScoreNone;
  int bm_stability = 0;

  for (int depth = 1; depth <= thread_info.max_iter_depth; depth++) {

    int real_multi_pv =
        std::min<int>(thread_info.multipv, thread_info.root_moves.size());
    for (thread_info.multipv_index = 0;
         thread_info.multipv_index < real_multi_pv;
         thread_info.multipv_index++) {

      int temp_depth = depth;

      int score, delta = AspStartWindow;

      score =
          search<true>(alpha, beta, depth, false, position, thread_info, TT);

      // Aspiration Windows: We search the position with a narrow window around
      // the last search score in order to get cutoffs faster. If our search
      // lands outside the bounds, expand them and try again.

      while (score <= alpha || score >= beta || thread_data.stop ||
             thread_info.datagen_stop) {

        if (thread_data.stop || thread_info.datagen_stop) {
          goto finish;
        }

        if (thread_info.thread_id == 0 && !thread_info.doing_datagen &&
            !(thread_info.is_human && thread_info.multipv_index)) {
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

          Move move = score <= alpha
                          ? prev_best
                          : thread_info.best_moves[thread_info.multipv_index];

          printf("info multipv %i depth %i seldepth %i score cp %i %s nodes "
                 "%" PRIu64 " nps %" PRIi64 " time %" PRIi64 " pv %s\n",
                 thread_info.multipv_index + 1, depth, thread_info.seldepth,
                 score * 100 / NormalizationFactor, bound_string.c_str(), nodes,
                 nps, search_time, internal_to_uci(position, move).c_str());
        }

        if (score <= alpha) {
          beta = (alpha + beta) / 2;
          alpha -= delta;
          temp_depth = depth;
        } else if (score >= beta) {
          beta += delta;
          temp_depth = std::max(temp_depth - 1, 1);
        }
        delta += delta / 3;

        score = search<true>(alpha, beta, temp_depth, false, position,
                             thread_info, TT);
      }

      if (score == ScoreNone) {
        break;
      }

      std::string eval_string;

      if (abs(score) <= ScoreTbWin) {
        eval_string = "cp " + std::to_string(score * 100 / NormalizationFactor);
      } else if (score > 0) {
        eval_string = "mate " + std::to_string((ScoreMate - score + 1) / 2);
      } else {
        eval_string = "mate " + std::to_string((-ScoreMate - score) / 2);
      }

      thread_info.best_moves[thread_info.multipv_index] = thread_info.pv[0];

      if (thread_info.thread_id == 0) {

        uint64_t tb_hits = thread_info.tb_hits;
        uint64_t nodes = thread_info.nodes;

        for (auto &td : thread_data.thread_infos) {
          nodes += td.nodes;
          tb_hits += td.tb_hits;
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

        if (!thread_info.doing_datagen /*&&
            !(thread_info.is_human && thread_info.multipv_index)*/) {
          printf("info multipv %i depth %i seldepth %i score %s nodes %" PRIu64
                 " nps %" PRIi64 " tbhits %" PRIu64 " time %" PRIi64 " pv ",
                 thread_info.multipv_index + 1, depth, thread_info.seldepth,
                 eval_string.c_str(), nodes, nps, tb_hits, search_time);
          print_pv(position, thread_info);
        }

        else {
          thread_info.best_scores[0] = score * 100 / NormalizationFactor;
        }

        if (search_time > thread_info.opt_time ||
            nodes > thread_info.opt_nodes_searched) {

          if (thread_info.doing_datagen) {
            thread_info.datagen_stop = true;
          } else {
            thread_data.stop = true;
          }
        }

        else if (thread_info.multipv == 1 && depth > 6) {
          if (thread_info.best_moves[0] == prev_best) {
            bm_stability = std::min(bm_stability + 1, 8);
          } else {
            bm_stability = 0;
          }

          adjust_soft_limit(
              thread_info,
              find_root_move(thread_info, thread_info.best_moves[0])->nodes,
              bm_stability);
        }

        if (depth == 6){
          if (thread_info.best_scores[0] < 0){
            thread_info.phase = PhaseTypes::Endgame;
            thread_info.nnue_state.reset_nnue(position, thread_info.phase);
          }
          else if (thread_info.best_scores[0] > 400){
            thread_info.phase = PhaseTypes::Sacrifice;
            thread_info.nnue_state.reset_nnue(position, thread_info.phase);
          }
          else{
            thread_info.phase = PhaseTypes::Middlegame;
            thread_info.nnue_state.reset_nnue(position, thread_info.phase);
          }
        }

      }

      if (thread_data.stop || thread_info.datagen_stop) {
        goto finish;
      }

      prev_best = thread_info.best_moves[0];

      if (depth > 6 && thread_info.multipv_index == 0) {
        alpha = score - 20, beta = score + 20;
      } else {
        alpha = ScoreNone, beta = -ScoreNone;
      }
    }
  }

finish:
  // wait for all threads to finish searching
  // printf("%i\n", thread_info.thread_id);
  if (thread_info.thread_id == 0 && !thread_info.doing_datagen) {
    thread_data.stop = true;
  }
  search_end_barrier.arrive_and_wait();
  if (thread_info.thread_id == 0 && !thread_info.doing_datagen &&
      !thread_info.is_human) {
    printf("bestmove %s\n",
           internal_to_uci(position, thread_info.best_moves[0]).c_str());
  }
}

void search_position(Position &position, ThreadInfo &thread_info,
                     std::vector<TTBucket> &TT) {

  thread_info.position = position;
  thread_info.thread_id = 0;
  thread_info.nodes = 0;
  thread_info.tb_hits = 0;

  int num_threads = thread_data.num_threads;

  // Wait for threads to be ready
  reset_barrier.arrive_and_wait();

  for (int i = 0; i < thread_data.thread_infos.size(); i++) {
    thread_data.thread_infos[i] = thread_info;
    thread_data.thread_infos[i].thread_id = i + 1;
  }

  // Tell threads to start
  idle_barrier.arrive_and_wait();

  thread_data.stop = false;
  iterative_deepen(position, thread_info, TT);
  if (!thread_info.doing_datagen) {
    thread_data.stop = true;
  }

  thread_info.searches = (thread_info.searches + 1) % MaxAge;
}

void loop(int i) {
  while (true) {
    reset_barrier.arrive_and_wait();
    idle_barrier.arrive_and_wait();
    if (thread_data.terminate) {
      return;
    }
    thread_data.thread_infos[i].searching = true;
    iterative_deepen(thread_data.thread_infos[i].position,
                     thread_data.thread_infos[i], TT);
    thread_data.thread_infos[i].searching = false;
  }
}
