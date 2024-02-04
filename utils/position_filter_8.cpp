// Pawn levers
//

#include "base.h"
#include <fstream>
#include <iostream>
#include <stdio.h>

bool enough_mat(board_info *board) {
  int total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++) {
    total_mat += (board->material_count[Colors::White][i] +
                  board->material_count[Colors::Black][i]) *
                 piece_values[i];
  }
  return total_mat > 2500 && board->material_count[Colors::White][4] &&
         board->material_count[Colors::Black][4];
}

/*
--------
--------
-******-
--****--
--------
--------
--------
--------
*/
bool monster_piece(board_info *board) {
  int sq[10] = {0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x62, 0x63, 0x64, 0x65};

  for (int i : sq) {
    if ((board->board[i] == Pieces::WKnight ||
         board->board[i] == Pieces::WBishop) &&
        !board->material_count[Colors::Black][2] &&
        board->board[i + Directions::North + Directions::Northeast] !=
            Pieces::BPawn &&
        board->board[i + Directions::North + Directions::Northwest] !=
            Pieces::BPawn) {
      return true;
    }

    i = flip(i);

    if ((board->board[i] == Pieces::BKnight ||
         board->board[i] == Pieces::BBishop) &&
        !board->material_count[Colors::White][2] &&
        board->board[i + Directions::South + Directions::Southeast] !=
            Pieces::WPawn &&
        board->board[i + Directions::South + Directions::Southwest] !=
            Pieces::WPawn) {
      return true;
    }
  }
  return false;
}

int filter(const std::string input, const std::string &output) {
  char buffer[32768];
  int buffer_key = 0;
  std::ofstream fout(output);
  std::ifstream fin(input);
  std::string line;
  int total_lines = 0, filtered_lines = 0;
  while (std::getline(fin, line)) {
    total_lines++;
    board_info board;
    bool color = setfromfen(&board, line.c_str());
    if (enough_mat(&board) && monster_piece(&board)) {
      filtered_lines++;
      buffer_key += sprintf(buffer + buffer_key, "%s\n", line.c_str());
      if (buffer_key > 32668) {
        fout << buffer;
        buffer_key = 0;
      }
    }
  }

  fout.close();
  fin.close();
  printf("%i positions read in, %i filtered\n", total_lines, filtered_lines);
  return 0;
}

int main(int argc, char *argv[]) {
  clock_t a = clock();
  filter(argv[1], argv[2]);
  printf("%f seconds\n", (float)(clock() - a) / CLOCKS_PER_SEC);
  return 0;
}