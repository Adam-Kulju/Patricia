#pragma once
#include "defs.h"
#include <cstring>
#include <ctype.h>
#include <sstream>

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
  position = {0};
  std::istringstream fen(f);
  std::string fen_pos;
  fen >> fen_pos;
  int k = 0;
  for (int i = 0x70; i >= 0 && fen_pos[k] != '\0'; i++, k++) {
    if (fen_pos[k] == '/') {
      i -= 0x19; // this means we've hit 0x18 and need to subtract 0x19 so that
                 // the next iteration of the loop will land on 0x0
    } else if (isdigit(fen_pos[k])) {
      i += fen_pos[k] - '1';
    } else {
      switch (fen_pos[k]) {
      case 'P':
        position.board[i] = Pieces::WPawn;
        position.material_count[Colors::White][0]++;
        break;
      case 'N':
        position.board[i] = Pieces::WKnight;
        position.material_count[Colors::White][1]++;
        break;
      case 'B':
        position.board[i] = Pieces::WBishop;
        position.material_count[Colors::White][2]++;
        break;
      case 'R':
        position.board[i] = Pieces::WRook;
        position.material_count[Colors::White][3]++;
        break;
      case 'Q':
        position.board[i] = Pieces::WQueen;
        position.material_count[Colors::White][4]++;
        break;
      case 'K':
        position.board[i] = Pieces::WKing;
        position.kingpos[Colors::White] = i;
        break;
      case 'p':
        position.board[i] = Pieces::BPawn;
        position.material_count[Colors::Black][0]++;
        break;
      case 'n':
        position.board[i] = Pieces::BKnight;
        position.material_count[Colors::Black][1]++;
        break;
      case 'b':
        position.board[i] = Pieces::BBishop;
        position.material_count[Colors::Black][2]++;
        break;
      case 'r':
        position.board[i] = Pieces::BRook;
        position.material_count[Colors::Black][3]++;
        break;
      case 'q':
        position.board[i] = Pieces::BQueen;
        position.material_count[Colors::Black][4]++;
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

  int indx = 0;
  for (char a : "KQkq") {
    if (castling_rights.find(a) != std::string::npos) {
      position.castling_rights[indx > 1][indx % 2] = true;
    }
    indx++;
  }

  std::string ep_square;
  fen >> ep_square;
  if (ep_square[0] == '-') {
    position.ep_square = 0;
  } else {
    uint8_t file = (ep_square[0] - 'a');
    uint8_t rank = (ep_square[1] - '1');
    position.ep_square = rank * 16 + file;
  }
  int d;
  fen >> d;
  position.halfmoves = d;
  fen >> thread_info.game_length;
}


bool attacks_square(Position position, int sq, int color) {
  int opp_color = color ^ 1, v = -1;
  for (int d : AttackRays) {
    v++;
    int temp_pos = sq + d;

    while (!out_of_board(temp_pos)) {
      int piece = position.board[temp_pos];
      if (!piece) {
        temp_pos += d;
        continue;
      } else if (get_color(piece) == opp_color) {
        break;
      }

      piece -= color;

      if (piece == Pieces::WQueen) {
        return true;
      } else if (piece == Pieces::WRook) {
        if (v < 4) {
          return true;
        }
      } else if (piece == Pieces::WBishop) {
        if (v > 3) {
          return true;
        }
      } else if (piece == Pieces::WPawn) {
        if (temp_pos == sq + d && (color ? (v > 5) : (v == 4 || v == 5))) {
          return true;
        }
      } else if (piece == Pieces::WKing) {
        if (temp_pos == sq + d) {
          return true;
        }
      } 
      break;
    }

    temp_pos = sq + KnightAttacks[v];
    if (!out_of_board(temp_pos) && position.board[temp_pos] - color == Pieces::WKnight){
        return true;
    }
  }
  return false;
}
