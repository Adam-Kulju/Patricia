#ifndef __base__
#define __base__
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
namespace Colors{
    constexpr uint8_t White = 0;
    constexpr uint8_t Black = 1;
};
namespace Pieces{
    constexpr uint8_t Blank = 0; constexpr uint8_t WPawn = 2; constexpr uint8_t BPawn = 3;
    constexpr uint8_t WKnight = 4; constexpr uint8_t BKnight = 5; constexpr uint8_t WBishop = 6;
    constexpr uint8_t BBishop = 7; constexpr uint8_t WRook = 8; constexpr uint8_t BRook = 9; 
    constexpr uint8_t WQueen = 10; constexpr uint8_t BQueen = 11; constexpr uint8_t WKing = 12; constexpr uint8_t BKing = 13;
};
namespace Directions {
    constexpr int8_t North = 16;
    constexpr int8_t South = -16;
    constexpr int8_t East = 1;
    constexpr int8_t West = -1;
    constexpr int8_t Northeast = 17;
    constexpr int8_t Southeast = -15;
    constexpr int8_t Northwest = 15;
    constexpr int8_t Southwest = -17;
    
}
struct board_info {
  unsigned char board[0x80];      // Stores the board itself
  unsigned char material_count[2][5]; // Stores material
  bool castling[2][2];            // Stores castling rights
  unsigned char rookstartpos[2][2];
  unsigned char kingpos[2]; // Stores King positions
  unsigned char epsquare;   // stores ep square
};

#define out_of_board(x) (x & 0x88)
#define get_rank(x) (x / 16)
#define get_file(x) (x % 16)
#define flip(x) (x ^ 112)

bool setfromfen(
    board_info *board,
    const char *fenstring) // Given an FEN, sets up the board to it.
{
  memset(board, 0, sizeof(board_info));
  int i = 7, n = 0;
  int fenkey = 0;

  while (!isblank(fenstring[fenkey])) {
    if (fenstring[fenkey] == '/') // Go to the next rank down.
    {
      i--;
      n = 0;
    } else if (isdigit(fenstring[fenkey])) // A number in the FEN (of form P3pp)
                                           // means there's three empty squares,
                                           // so we go past them
    {
      //pruint8_tf("%c %i\n", fenstring[fenkey], fenstring[fenkey]);
      n += fenstring[fenkey] - '0';
    } else // For each piece, add it to the board and add it to the material
           // count. If it's a king, update king position.
    {
      switch (fenstring[fenkey]) {
      case 'P':
        board->board[n + (i * 16)] = Pieces::WPawn;
        board->material_count[Colors::White][0]++;
        break;
      case 'N':
        board->board[n + (i * 16)] = Pieces::WKnight;
        board->material_count[Colors::White][1]++;
        break;
      case 'B':
        board->board[n + (i * 16)] = Pieces::WBishop;
        board->material_count[Colors::White][2]++;
        break;
      case 'R':
        board->board[n + (i * 16)] = Pieces::WRook;
        board->material_count[Colors::White][3]++;
        break;
      case 'Q':
        board->board[n + (i * 16)] = Pieces::WQueen;
        board->material_count[Colors::White][4]++;
        break;
      case 'K':
        board->board[n + (i * 16)] = Pieces::WKing;
        board->kingpos[Colors::White] = n + (i * 16);
        break;
      case 'p':
        board->board[n + (i * 16)] = Pieces::BPawn;
        board->material_count[Colors::Black][0]++;
        break;
      case 'n':
        board->board[n + (i * 16)] = Pieces::BKnight;
        board->material_count[Colors::Black][1]++;
        break;
      case 'b':
        board->board[n + (i * 16)] = Pieces::BBishop;
        board->material_count[Colors::Black][2]++;
        break;
      case 'r':
        board->board[n + (i * 16)] = Pieces::BRook;
        board->material_count[Colors::Black][3]++;
        break;
      case 'q':
        board->board[n + (i * 16)] = Pieces::BQueen;
        board->material_count[Colors::Black][4]++;
        break;
      default:
        board->board[n + (i * 16)] = Pieces::BKing;
        board->kingpos[Colors::Black] = n + (i * 16);
        break;
      }
      n++;
    } // sets up the board based on fen
    fenkey++;
  }
  board->rookstartpos[0][0] = 0, board->rookstartpos[0][1] = 7,
  board->rookstartpos[1][0] = 0x70,
  board->rookstartpos[1][1] = 0x77; // someday, I may supprt DFRC fen parsing,
                                    // but that day is not today.

  while (isblank(fenstring[fenkey])) {
    fenkey++;
  }
  uint8_t color = fenstring[fenkey++] == 'w' ? Colors::White : Colors::Black;

  while (isblank(fenstring[fenkey])) {
    fenkey++;
  }

  if (fenstring[fenkey] == '-') {
    fenkey++;
  }

  else {
    if (fenstring[fenkey] == 'K') // Get castling rights information
    {
      board->castling[Colors::White][1] = true;
      fenkey++;
    }
    if (fenstring[fenkey] == 'Q') {
      board->castling[Colors::White][0] = true;
      fenkey++;
    }
    if (fenstring[fenkey] == 'k') {
      board->castling[Colors::Black][1] = true;
      fenkey++;
    }
    if (fenstring[fenkey] == 'q') {
      board->castling[Colors::Black][0] = true;
    }
    fenkey++;
  }

  while (isblank(fenstring[fenkey])) {
    fenkey++;
  }

  if (fenstring[fenkey] == '-') // Get en passant square information
  {
    board->epsquare = 0;
  } else {
    board->epsquare =
        (atoi(&fenstring[fenkey + 1]) - 1) * 16 + ((fenstring[fenkey] - 97));
    // In Willow, the en passant square is the square of the piece to be taken
    // en passant, and not the square behind.
    if (color) {
      board->epsquare += Directions::North;
    } else {
      board->epsquare += Directions::South;
    }
    fenkey++;
  }
  return color;
}

constexpr int STANDARD_TO_MAILBOX[64] = {
    0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,  0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x40,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

#endif