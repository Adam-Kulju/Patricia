#pragma once
#include "defs.h"
#include <cstring>
#include <ctype.h>
#include <sstream>

std::string internal_to_uci(Position position, Move move) {
  int from = extract_from(move), to = extract_to(move),
      promo = extract_promo(move);
  std::string uci = "     ";
  uci[0] = get_file(from) + 'a', uci[1] = get_rank(from) + '1',
  uci[2] = get_file(to) + 'a', uci[3] = get_rank(to) + '1', uci[4] = '\0';
  if (position.board[from] - position.color == Pieces::WPawn &&
      get_rank(to) == (position.color ? 0 : 7)) {
    std::string promos = "NBRQ";
    uci[4] = promos[promo], uci[5] = '\0';
  }
  return uci;
}

void print_board(Position position) {
  for (int i = 0x70; i >= 0;) {
    printf("+---+---+---+---+---+---+---+---+\n");
    for (int n = i; n != i + 8; n++) {
      printf("| ");
      if (position.board[n] == Pieces::Blank) {
        printf("  ");
      } else {

        switch (position.board[n]) {
        case Pieces::WPawn:
          printf("P ");
          break;
        case Pieces::WKnight:
          printf("N ");
          break;
        case Pieces::WBishop:
          printf("B ");
          break;
        case Pieces::WRook:
          printf("R ");
          break;
        case Pieces::WQueen:
          printf("Q ");
          break;
        case Pieces::WKing:
          printf("K ");
          break;
        case Pieces::BPawn:
          printf("p ");
          break;
        case Pieces::BKnight:
          printf("n ");
          break;
        case Pieces::BBishop:
          printf("b ");
          break;
        case Pieces::BRook:
          printf("r ");
          break;
        case Pieces::BQueen:
          printf("q ");
          break;
        case Pieces::BKing:
          printf("k ");
          break;
        default:
          printf("# ");
        }
      }
    }
    printf("|\n");
    i -= 0x10;
  }
  printf("+---+---+---+---+---+---+---+---+\n\n");
}

void set_board(Position &position, ThreadInfo &thread_info, std::string f) {
  memset(&position, 0, sizeof(Position));
  std::istringstream fen(f);
  std::string fen_pos;
  fen >> fen_pos;
  int indx = 0;
  for (int i = 0x70; i >= 0 && fen_pos[indx] != '\0'; i++, indx++) {
    if (fen_pos[indx] == '/') {
      i -= 0x19; // this means we've hit 0x18 and need to subtract 0x19 so that
                 // the next iteration of the loop will land on 0x0
    } else if (isdigit(fen_pos[indx])) {
      i += fen_pos[indx] - '1';
    } else {
      switch (fen_pos[indx]) {
      case 'P':
        position.board[i] = Pieces::WPawn;
        position.material_count[0]++;
        break;
      case 'N':
        position.board[i] = Pieces::WKnight;
        position.material_count[2]++;
        break;
      case 'B':
        position.board[i] = Pieces::WBishop;
        position.material_count[4]++;
        break;
      case 'R':
        position.board[i] = Pieces::WRook;
        position.material_count[6]++;
        break;
      case 'Q':
        position.board[i] = Pieces::WQueen;
        position.material_count[8]++;
        break;
      case 'K':
        position.board[i] = Pieces::WKing;
        position.kingpos[Colors::White] = i;
        break;
      case 'p':
        position.board[i] = Pieces::BPawn;
        position.material_count[1]++;
        break;
      case 'n':
        position.board[i] = Pieces::BKnight;
        position.material_count[3]++;
        break;
      case 'b':
        position.board[i] = Pieces::BBishop;
        position.material_count[5]++;
        break;
      case 'r':
        position.board[i] = Pieces::BRook;
        position.material_count[7]++;
        break;
      case 'q':
        position.board[i] = Pieces::BQueen;
        position.material_count[9]++;
        break;
      case 'k':
        position.board[i] = Pieces::BKing;
        position.kingpos[Colors::Black] = i;
        break;
      default:
        printf("Error parsing FEN: %s\n", f.c_str());
        std::exit(1);
      }
    }
  }

  std::string color;
  fen >> color;
  if (color[0] == 'w') {
    position.color = Colors::White;
  } else {
    position.color = Colors::Black;
  }

  std::string castling_rights;
  fen >> castling_rights;

  indx = 0;
  for (char rights : "KQkq") {
    if (castling_rights.find(rights) != std::string::npos) {
      position.castling_rights[indx > 1][!(indx % 2)] = true;
    }
    indx++;
  }

  std::string ep_square;
  fen >> ep_square;
  if (ep_square[0] == '-') {
    position.ep_square = 255;
  } else {
    uint8_t file = (ep_square[0] - 'a');
    uint8_t rank = (ep_square[1] - '1');
    position.ep_square = rank * 16 + file;
  }
  int halfmoves;
  fen >> halfmoves;
  position.halfmoves = halfmoves;
  thread_info.game_ply = 0;
}

bool attacks_square(Position position, int sq, int color) {
  int opp_color = color ^ 1, indx = -1;
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

      piece -= color; // This statement lets us get the piece type without
                      // needing to account for color.

      if (piece == Pieces::WQueen || (piece == Pieces::WRook && indx < 4) ||
          (piece == Pieces::WBishop && indx > 3)) {
        return true;
      } else if (piece == Pieces::WPawn) {
        if (temp_pos == sq + dirs &&
            (color ? (indx > 5) : (indx == 4 || indx == 5))) {
          return true;
        }
      } else if (piece == Pieces::WKing && temp_pos == sq + dirs) {
        return true;
      }
      break;
    }

    temp_pos = sq + KnightAttacks[indx];
    if (!out_of_board(temp_pos) &&
        position.board[temp_pos] - color == Pieces::WKnight) {
      return true;
    }
  }
  return false;
}

bool is_en_passant(Position &position, int from, int to) {
  return (position.board[from] - position.color == Pieces::WPawn &&
          to == position.ep_square);
}

int make_move(Position &position, Move move) {

  position.halfmoves++;
  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1;
  int base_rank = (color ? 0x70 : 0);
  int ep_square = 255;

  // update material counts and 50 move rules for a capture
  if (position.board[to]) {
    position.halfmoves = 0;
    position.material_count[position.board[to] - 2]--;
  }

  // Move the piece
  position.board[to] = position.board[from];
  position.board[from] = Pieces::Blank;

  int piece = position.board[to] - color;

  // handle pawn moves; en passant, promotions, and double pawn pushes

  if (piece == Pieces::WPawn) {
    position.halfmoves = 0;

    // promotions
    if (get_rank(to) == (color ? 0 : 7)) {
      int promo = extract_promo(move) * 2 + 4 + color;
      position.board[to] = promo;
      position.material_count[color]--, position.material_count[promo - 2]++;
    }

    // double pawn push
    else if (to == from + Directions::North * 2 ||
             to == from + Directions::South * 2) {
      ep_square = (to + from) / 2;
    }

    // en passant
    else if (to == position.ep_square) {
      position.material_count[opp_color]--;
      position.board[to + (color ? Directions::North : Directions::South)] =
          Pieces::Blank;
    }
  }
  // handle king moves and castling

  else if (piece == Pieces::WKing) {
    position.castling_rights[color][Sides::Queenside] = false,
    position.castling_rights[color][Sides::Kingside] = false;
    position.kingpos[color] = to;

    // kingside castle
    if (to == from + Directions::East + Directions::East) {
      position.board[base_rank + 5] = position.board[base_rank + 7];
      position.board[base_rank + 7] = Pieces::Blank;
    }

    // queenside castle
    else if (to == from + Directions::West + Directions::West) {
      position.board[base_rank + 3] = position.board[base_rank];
      position.board[base_rank] = Pieces::Blank;
    }
  }

  // If we've moved a piece from our starting rook squares, set castling on that
  // side to false, because if it's not a rook it means either the rook left
  // that square or the king left its original square.
  if (from == base_rank || from == base_rank + 7) {
    position.castling_rights[color][get_file(from) < 4 ? Sides::Queenside
                                                       : Sides::Kingside] =
        false;
  }
  // If we've moved a piece onto one of the opponent's starting rook square, set
  // their castling to false, because either we just captured it, the rook
  // already moved, or the opposing king moved.
  else if (to == flip_sq(base_rank) || to == flip_sq(base_rank + 7)) {

    position.castling_rights[opp_color][get_file(to) < 4 ? Sides::Queenside
                                                         : Sides::Kingside] =
        false;
  }
  position.color ^= 1;
  position.ep_square = ep_square;
  return attacks_square(position, position.kingpos[color], opp_color);
}
