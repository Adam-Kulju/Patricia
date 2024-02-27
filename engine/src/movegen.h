#pragma once
#include "defs.h"
#include "position.h"
#include <stdint.h>

constexpr int TTMoveScore = 10000000;
constexpr int QueenPromoScore = 5000000;
constexpr int GoodCaptureBaseScore = 2000000;

int movegen(Position position, Move *move_list, bool in_check) {
  uint8_t color = position.color;
  int pawn_dir = color ? Directions::South : Directions::North,
      promotion_rank = color ? 0 : 7, first_rank = color ? 6 : 1,
      opp_color = color ^ 1;
  int indx = 0;
  for (uint8_t from : StandardToMailbox) {
    int piece = position.board[from];
    if (!piece || get_color(piece) != color) {
      continue;
    }
    piece -= color; // A trick we use to be able to compare piece types without
                    // having to account for color

    // Handle pawns
    if (piece == Pieces::WPawn) {
      int to = from + pawn_dir;
      int c_left = to + Directions::West;
      int c_right = to + Directions::East;

      // A pawn push one square forward
      if (!position.board[to]) {
        move_list[indx++] = pack_move(from, to, 0);
        if (get_rank(to) == promotion_rank) {
          for (int promo : {1, 2, 3}) {
            move_list[indx++] = pack_move(from, to, promo);
          }
        }
        // two squares forwards
        else if (get_rank(from) == first_rank &&
                 !position.board[to + pawn_dir]) {
          move_list[indx++] = pack_move(from, (to + pawn_dir), 0);
        }
      }

      // Pawn capture to the left
      if (!out_of_board(c_left) &&
          (c_left == position.ep_square ||
           enemy_square(color, position.board[c_left]))) {
        move_list[indx++] = pack_move(from, c_left, 0);
        if (get_rank(to) == promotion_rank) {
          for (int promo : {Promos::Bishop, Promos::Rook,
                            Promos::Queen}) { // knight is implicit via zero
            move_list[indx++] = pack_move(from, c_left, promo);
          }
        }
      }
      // Pawn capture to the right
      if (!out_of_board(c_right) &&
          (c_right == position.ep_square ||
           enemy_square(color, position.board[c_right]))) {
        move_list[indx++] = pack_move(from, c_right, 0);
        if (get_rank(to) == promotion_rank) {
          for (int promo : {Promos::Bishop, Promos::Rook, Promos::Queen}) {
            move_list[indx++] = pack_move(from, c_right, promo);
          }
        }
      }
    }

    // Handle knights
    else if (piece == Pieces::WKnight) {
      for (int moves : KnightAttacks) {
        int to = from + moves;
        if (!out_of_board(to) && !friendly_square(color, position.board[to])) {
          move_list[indx++] = pack_move(from, to, 0);
        }
      }
    }
    // Handle kings
    else if (piece == Pieces::WKing) {
      for (int moves : SliderAttacks[3]) {
        int to = from + moves;
        if (!out_of_board(to) && !friendly_square(color, position.board[to])) {
          move_list[indx++] = pack_move(from, to, 0);
        }
      }
    }

    // Handle sliders
    else {
      for (int dirs : SliderAttacks[piece / 2 - 3]) {
        int to = from + dirs;

        while (!out_of_board(to)) {

          int to_piece = position.board[to];
          if (!to_piece) {
            move_list[indx++] = pack_move(from, to, 0);
            to += dirs;
            continue;
          } else if (get_color(to_piece) ==
                     opp_color) { // add the move for captures and immediately
                                  // break
            move_list[indx++] = pack_move(from, to, 0);
          }
          break;
        }
      }
    }
  }
  if (in_check) { // If we're in check there's no point in
                  // seeing if we can castle (can be optimized)
    return indx;
  }

  // queenside castling
  int king_pos = position.kingpos[color];
  // Requirements: the king and rook cannot have moved, all squares between them
  // must be empty, and the king can not go through check.
  // (It can't end up in check either, but that gets filtered just like any
  // illegal move.)
  if (position.castling_rights[color][Sides::Queenside] &&
      !position.board[king_pos - 1] && !position.board[king_pos - 2] &&
      !position.board[king_pos - 3] &&
      !attacks_square(position, king_pos - 1, opp_color)) {
    move_list[indx++] = pack_move(king_pos, (king_pos - 2), 0);
  }

  // kingside castling
  if (position.castling_rights[color][Sides::Kingside] &&
      !position.board[king_pos + 1] && !position.board[king_pos + 2] &&
      !attacks_square(position, king_pos + 1, opp_color)) {
    move_list[indx++] = pack_move(king_pos, (king_pos + 2), 0);
  }
  return indx;
}

int cheapest_attacker(Position position, int sq, int color, int &attack_sq) {
  int opp_color = color ^ 1, indx = -1;

  int lowest = Pieces::Blank + 20;

  for (int dirs : AttackRays) {
    indx++;
    int temp_pos = sq + dirs;

    while (!out_of_board(temp_pos)) {
      int piece = position.board[temp_pos];
      if (!piece) {
        temp_pos += dirs;
        continue;
      } else if (get_color(piece) == opp_color) {
        break;
      }

      bool attacker = false;

      piece -= color; // This statement lets us get the piece type without
                      // needing to account for color.

      if (piece == Pieces::WQueen || (piece == Pieces::WRook && indx < 4) ||
          (piece == Pieces::WBishop &&
           indx > 3)) { // A queen attack is a check from every direction, rooks
                        // and bishops only from orthogonal/diagonal directions
                        // respectively.
        attacker = true;
      } else if (piece == Pieces::WPawn) {
        // Pawns and kings are only attackers if they're right next to the
        // square. Pawns additionally have to be on the right vector.
        if (temp_pos == sq + dirs &&
            (color ? (indx > 5) : (indx == 4 || indx == 5))) {

          attack_sq = temp_pos;
          return piece;
        }
      } else if (piece == Pieces::WKing && temp_pos == sq + dirs) {
        attacker = true;
      }

      if (attacker && piece < lowest) {
        lowest = piece;
        attack_sq = temp_pos;
      }
      break;
    }

    temp_pos = sq + KnightAttacks[indx]; // Check for knight attacks
    if (!out_of_board(temp_pos) &&
        position.board[temp_pos] - color == Pieces::WKnight) {
      lowest = Pieces::WKnight;
      attack_sq = temp_pos;
    }
  }
  return lowest;
}

bool SEE(Position &position, ThreadInfo &thread_info, Move move,
         int threshold) {
  int color = position.color, from = extract_from(move), to = extract_to(move),
      gain = SeeValues[position.board[to]],
      risk = SeeValues[position.board[from]];

  if (gain < threshold) {
    return false;
  }

  Position temp_pos = position;
  temp_pos.board[from] = Pieces::Blank;

  int None = 20, attack_sq = 0;

  while (gain - risk < threshold) {
    int type = cheapest_attacker(temp_pos, to, color ^ 1, attack_sq);

    if (type == None) {
      return true;
    }

    gain -= risk;
    risk = SeeValues[type];

    if (gain + risk < threshold) {
      return false;
    }
    temp_pos.board[attack_sq] = Pieces::Blank;

    type = cheapest_attacker(temp_pos, to, color, attack_sq);

    if (type == None) {
      return false;
    }

    gain += risk;
    risk = SeeValues[type];

    temp_pos.board[attack_sq] = Pieces::Blank;
  }

  return true;
}

void score_moves(Position position, ThreadInfo &thread_info,
                 MoveInfo &scored_moves, Move tt_move, int len) {
  for (int indx = 0; indx < len; indx++) {
    Move move = scored_moves.moves[indx];
    if (move == tt_move) {
      scored_moves.scores[indx] = TTMoveScore;
    } else if (extract_promo(move) == Promos::Queen) {
      scored_moves.scores[indx] = QueenPromoScore;
    } else if (is_cap(position, move)) {
      int from_piece = position.board[extract_from(move)],
          to_piece = position.board[extract_to(move)];

      scored_moves.scores[indx] = GoodCaptureBaseScore + SeeValues[to_piece] -
                                  SeeValues[from_piece] / 10;
    } else {
      int piece = position.board[extract_from(move)] - 2, to = extract_to(move);
      scored_moves.scores[indx] = thread_info.HistoryScores[piece][to];
    }
  }
}

Move get_next_move(Move *moves, int *scores, int start_indx, int len) {
  // Performs a selection sort
  int best_indx = start_indx, best_score = scores[start_indx];
  for (int i = start_indx + 1; i < len; i++) {
    if (scores[i] > best_score) {
      best_score = scores[i];
      best_indx = i;
    }
  }
  std::swap(moves[start_indx], moves[best_indx]);
  std::swap(scores[start_indx], scores[best_indx]);

  return moves[start_indx];
}