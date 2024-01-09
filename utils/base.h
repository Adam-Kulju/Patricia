#ifndef __base__
#define __base__
#include <ctype.h>
#include <stdlib.h>
namespace Colors{
    constexpr int White = 0;
    constexpr int Black = 1;
};
namespace Pieces{
    constexpr int Blank = 0; constexpr int WPawn = 2; constexpr int BPawn = 3;
    constexpr int WKnight = 4; constexpr int BKnight = 5; constexpr int WBishop = 6;
    constexpr int BBishop = 7; constexpr int WRook = 8; constexpr int BRook = 9; 
    constexpr int WQueen = 10; constexpr int BQueen = 11; constexpr int WKing = 12; constexpr int BKing = 13;
};
namespace Directions {
    constexpr int North = 16;
    constexpr int South = -16;
    constexpr int East = 1;
    constexpr int West = -1;
}
struct board_info {
  unsigned char board[0x80];      // Stores the board itself
  unsigned char material_count[2][5]; // Stores material
  bool castling[2][2];            // Stores castling rights
  unsigned char rookstartpos[2][2];
  unsigned char kingpos[2]; // Stores King positions
  unsigned char epsquare;   // stores ep square
};

bool setfromfen(
    board_info *board,
    const char *fenstring) // Given an FEN, sets up the board to it.
{
  int i = 7, n = 0;
  int fenkey = 0;
  // Set default board parameters, edit them later on as we add pieces to the
  // board/read more info from the FEN.
  board->castling[Colors::White][0] = false, board->castling[Colors::Black][0] = false,
  board->castling[Colors::White][1] = false, board->castling[Colors::Black][1] = false;
  for (int i = 0; i < 5; i++) {
    board->material_count[Colors::White][i] = 0;
    board->material_count[Colors::Black][i] = 0;
  }
  while (!isblank(fenstring[fenkey])) {
    if (fenstring[fenkey] == '/') // Go to the next rank down.
    {
      i--;
      n = 0;
    } else if (isdigit(fenstring[fenkey])) // A number in the FEN (of form P3pp)
                                           // means there's three empty squares,
                                           // so we go past them
    {
      for (int b = 0; b < atoi(&fenstring[fenkey]); b++) {
        board->board[n + (i * 16)] = Pieces::Blank;
        n++;
      }
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
  for (int i = 0; i < 8; i++) {
    for (int n = 8; n < 16; n++) {
      board->board[i * 16 + n] = Pieces::Blank;
    }
  }

  while (isblank(fenstring[fenkey])) {
    fenkey++;
  }
  int color = fenstring[fenkey++] == 'w' ? Colors::White : Colors::Black;

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

#endif