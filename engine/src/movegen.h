#pragma once
#include "defs.h"
#include "position.h"
#include <cstdint>
#include <span>

constexpr int TTMoveScore = 10000000;
constexpr int QueenPromoScore = 5000000;
constexpr int GoodCaptureBaseScore = 2000000;
constexpr int BadCaptureBaseScore = -2000000;
constexpr int KillerMoveScore = 100000;

int movegen(const Position &position, std::span<Move> move_list,
            bool in_check) {
  uint8_t color = position.color;
  int pawn_dir = color ? Directions::South : Directions::North,
      promotion_rank = color ? 0 : 7, first_rank = color ? 6 : 1,
      opp_color = color ^ 1;
  int idx = 0;
  for (uint8_t from : StandardToMailbox) {

    int piece = position.board[from];
    if (piece == Pieces::Blank || get_color(piece) != color) {
      continue;
    }

    int type = get_piece_type(piece);

    // Handle pawns
    if (type == Pieces_BB::Pawn) {
      int to = from + pawn_dir;
      int c_left = to + Directions::West;
      int c_right = to + Directions::East;

      // A pawn push one square forward
      if (position.board[to] == Pieces::Blank) {

        move_list[idx++] = pack_move(from, to, 0);
        if (get_rank_x88(to) == promotion_rank) {
          for (int promo : {1, 2, 3}) {
            move_list[idx++] = pack_move(from, to, promo);
          }
        }
        // two squares forwards
        else if (get_rank_x88(from) == first_rank &&
                 position.board[to + pawn_dir] == Pieces::Blank) {
          move_list[idx++] = pack_move(from, (to + pawn_dir), 0);
        }
      }

      // Pawn capture to the left
      if (!out_of_board(c_left) &&
          (c_left == position.ep_square ||
           enemy_square(color, position.board[c_left]))) {
        move_list[idx++] = pack_move(from, c_left, 0);
        if (get_rank_x88(to) == promotion_rank) {
          for (int promo : {Promos::Bishop, Promos::Rook,
                            Promos::Queen}) { // knight is implicit via zero
            move_list[idx++] = pack_move(from, c_left, promo);
          }
        }
      }
      // Pawn capture to the right
      if (!out_of_board(c_right) &&
          (c_right == position.ep_square ||
           enemy_square(color, position.board[c_right]))) {
        move_list[idx++] = pack_move(from, c_right, 0);
        if (get_rank_x88(to) == promotion_rank) {
          for (int promo : {Promos::Bishop, Promos::Rook, Promos::Queen}) {
            move_list[idx++] = pack_move(from, c_right, promo);
          }
        }
      }
    }

    // Handle knights
    else if (type == Pieces_BB::Knight) {
      for (int moves : KnightRays) {
        int to = from + moves;
        if (!out_of_board(to) && !friendly_square(color, position.board[to])) {
          move_list[idx++] = pack_move(from, to, 0);
        }
      }
    }
    // Handle kings
    else if (type == Pieces_BB::King) {
      for (int moves : SliderAttacks[3]) {
        int to = from + moves;
        if (!out_of_board(to) && !friendly_square(color, position.board[to])) {
          move_list[idx++] = pack_move(from, to, 0);
        }
      }
    }

    // Handle sliders
    else {
      for (int dirs : SliderAttacks[type - 3]) {
        int to = from + dirs;

        while (!out_of_board(to)) {

          int to_piece = position.board[to];
          if (!to_piece) {
            move_list[idx++] = pack_move(from, to, 0);
            to += dirs;
            continue;
          } else if (get_color(to_piece) ==
                     opp_color) { // add the move for captures and immediately
                                  // break
            move_list[idx++] = pack_move(from, to, 0);
          }
          break;
        }
      }
    }
  }
  if (in_check) { // If we're in check there's no point in
                  // seeing if we can castle (can be optimized)
    return idx;
  }

  // queenside castling
  int king_pos = StandardToMailbox[get_king_pos(position, color)];
  // Requirements: the king and rook cannot have moved, all squares between them
  // must be empty, and the king can not go through check.
  // (It can't end up in check either, but that gets filtered just like any
  // illegal move.)
  if (position.castling_rights[color][Sides::Queenside] &&
      position.board[king_pos - 1] == Pieces::Blank &&
      position.board[king_pos - 2] == Pieces::Blank &&
      position.board[king_pos - 3] == Pieces::Blank &&
      !attacks_square(position, MailboxToStandard[king_pos - 1], opp_color)) {
    move_list[idx++] = pack_move(king_pos, (king_pos - 2), 0);
  }

  // kingside castling
  if (position.castling_rights[color][Sides::Kingside] &&
      position.board[king_pos + 1] == Pieces::Blank &&
      position.board[king_pos + 2] == Pieces::Blank &&
      !attacks_square(position, MailboxToStandard[king_pos + 1], opp_color)) {
    move_list[idx++] = pack_move(king_pos, (king_pos + 2), 0);
  }
  return idx;
}

int cheapest_attacker(Position &position, int sq, int color, uint64_t& occ) {
  // Finds the cheapest attacker for a given square on a given board.
  //printf("nnnnnnnnnnnnnn\nnnnnnnnnnnnnnnn\nnnnnnnnnnnnn\n\n");
  //print_bbs(position);
  sq = standard(sq);

  uint64_t attacks = attacks_square(position, sq, color, occ);
  uint64_t temp = 0ull;
  int value = Pieces_BB::PieceNone;

  if (!attacks) {
    return Pieces_BB::PieceNone;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::Pawn])) {
    value = Pieces_BB::Pawn;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::Knight])) {
    value = Pieces_BB::Knight;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::Bishop])) {
    value = Pieces_BB::Bishop;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::Rook])) {
    value = Pieces_BB::Rook;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::Queen])) {
    value = Pieces_BB::Queen;
  }

  else if ((temp = attacks & position.pieces_bb[Pieces_BB::King])) {
    value = Pieces_BB::King;
  }

  occ -= (temp & -int64_t(temp));
  return value;
}

bool SEE(Position &position, Move move, int threshold) {

  int color = position.color, from = extract_from(move), to = extract_to(move),
      gain = SeeValues[get_piece_type(position.board[to])],
      risk = SeeValues[get_piece_type(position.board[from])];

  if (gain < threshold) {
    // If taking the piece isn't good enough return
    return false;
  }

  uint64_t occ = (position.colors_bb[Colors::White] |
                 position.colors_bb[Colors::Black]) - (1ull << standard(from));

  int attack_sq = 0;

  while (gain - risk < threshold) {
    int type = cheapest_attacker(position, to, color ^ 1, occ);

    if (type == Pieces_BB::PieceNone) {
      return true;
    }

    gain -= risk + 1;
    risk = SeeValues[type];

    if (gain + risk < threshold) {
      return false;
    }

    type = cheapest_attacker(position, to, color, occ);

    if (type == Pieces_BB::PieceNone) {
      return false;
    }

    gain += risk - 1;
    risk = SeeValues[type];
  }

  return true;
}

void score_moves(Position &position, ThreadInfo &thread_info,
                 MoveInfo &scored_moves, Move tt_move, int len) {

  // score the moves

  int ply = thread_info.search_ply;

  int their_last =
      ply < 1
          ? MoveNone
          : extract_to(
                thread_info.game_hist[thread_info.game_ply - 1].played_move);
  int their_piece =
      ply < 1 ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 1].piece_moved;

  int our_last =
      ply < 2
          ? MoveNone
          : extract_to(
                thread_info.game_hist[thread_info.game_ply - 2].played_move);
  int our_piece =
      ply < 2 ? Pieces::Blank
              : thread_info.game_hist[thread_info.game_ply - 2].piece_moved;

  for (int idx = 0; idx < len; idx++) {
    Move move = scored_moves.moves[idx];
    if (move == tt_move) {
      scored_moves.scores[idx] = TTMoveScore;
      // TT move score;
    }

    else if (extract_promo(move) == Promos::Queen) {
      // Queen promo score
      scored_moves.scores[idx] = QueenPromoScore;
    }

    else if (is_cap(position, move)) {
      // Capture score
      int from_piece = position.board[extract_from(move)],
          to_piece = position.board[extract_to(move)];

      scored_moves.scores[idx] = GoodCaptureBaseScore +
                                 SeeValues[get_piece_type(to_piece)] * 100 -
                                 SeeValues[get_piece_type(from_piece)] / 100 -
                                 TTMoveScore * !SEE(position, move, -107);

      int piece = position.board[extract_from(move)], to = extract_to(move);

      scored_moves.scores[idx] += thread_info.CapHistScores[piece][to];

    }

    else if (move == thread_info.KillerMoves[thread_info.search_ply]) {
      // Killer move score
      scored_moves.scores[idx] = KillerMoveScore;
    }

    else {
      // Normal moves are scored using history
      int piece = position.board[extract_from(move)], to = extract_to(move);
      scored_moves.scores[idx] = thread_info.HistoryScores[piece][to];

      if (ply > 0 && their_last != MoveNone) {
        scored_moves.scores[idx] +=
            thread_info.ContHistScores[their_piece][their_last][piece][to];
      }
      if (ply > 1 && our_last != MoveNone) {
        scored_moves.scores[idx] +=
            thread_info.ContHistScores[our_piece][our_last][piece][to];
      }
    }
  }
}

Move get_next_move(std::span<Move> moves, std::span<int> scores, int start_idx,
                   int len) {
  // Performs a selection sort
  int best_idx = start_idx, best_score = scores[start_idx];
  for (int i = start_idx + 1; i < len; i++) {
    if (scores[i] > best_score) {
      best_score = scores[i];
      best_idx = i;
    }
  }
  std::swap(moves[start_idx], moves[best_idx]);
  std::swap(scores[start_idx], scores[best_idx]);

  return moves[start_idx];
}