#pragma once
#include <stdint.h>
#include "defs.h"
#include "position.h"
int movegen(Position position, Move *move_list) {
  uint8_t color = position.color;
  int pawn_dir = color ? -16 : 16, promotion_rank = color ? 0 : 7,
      first_rank = color ? 6 : 1, opp_color = color ^ 1;
  int indx = 0;
  for (u_int8_t from : StandardToMailbox) {
    int piece = position.board[from];
    if (!piece || get_color(piece) != color) {
      continue;
    }
    piece -=
        color; // same trick as before because we only need to look at piece
               // types (we have already verified its color if we're here)

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
          for (int promo : {1, 2, 3}) {
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
          for (int promo : {1, 2, 3}) {
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
      for (int d : SliderAttacks[piece / 2 - 3]) {
        int to = from + d;

        while (!out_of_board(to)) {

          int to_piece = position.board[to];
          if (!to_piece) {
            move_list[indx++] = pack_move(from, to, 0);
            to += d;
            continue;
          } else if (get_color(to_piece) == opp_color) {
            move_list[indx++] = pack_move(from, to, 0);
          }
          break;
        }
      }
    }
  }
  if (attacks_square(position, position.kingpos[color], opp_color)) {
    return indx;
  }

  // queenside castling
  int king_pos = position.kingpos[color];

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