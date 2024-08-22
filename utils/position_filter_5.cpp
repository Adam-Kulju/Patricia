//Development
//Pieces on the first two ranks count as poorly developed (be7 can be strong later but isn't really now)

#include <stdio.h>
#include <iostream>
#include <fstream>
#include "base.hpp"

using namespace  std;

constexpr int develop_penalty[0x80] = {
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 2, 3, 3, 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
    3, 3, 4, 5, 5, 4, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0,
};

bool enough_mat(board_info *board){
  int total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++){
    total_mat += (board->material_count[Colors::White][i] + board->material_count[Colors::Black][i]) * piece_values[i];
  }
  return total_mat > 4000 && board->material_count[Colors::White][4] && board->material_count[Colors::Black][4];
}

int poorly_developed(board_info *board){
    int developed = 0;
    for (int sq : {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,                          //Penalize knights and bishops not fianchettoed and on the first two ranks.
                    0x10, 0x12, 0x13, 0x14, 0x15, 0x17,
                    0x20, 0x27}){
                        if (board->board[sq] == Pieces::WKnight || board->board[sq] == Pieces::WBishop){
                            developed--;
                        }
                        if (board->board[flip(sq)] == Pieces::BKnight || board->board[flip(sq)] == Pieces::BBishop){
                            developed++;
                        }
                    }
    developed -= develop_penalty[board->kingpos[Colors::White]];                      //Penalize uncastled/advanced kings.
    developed += develop_penalty[flip(board->kingpos[Colors::Black])];

    for (int sq: {0x0, 0x7}){
        if (board->board[sq] == Pieces::WRook && board->board[(sq == 0 ? sq + 1 : sq - 1)] && board->board[sq + 16]){ //Penalize undeveloped and blocked rooks.
            developed--;
        }
        int sqf = flip(sq);
        if (board->board[sqf] == Pieces::BRook && board->board[(sq == 0 ? sqf + 1 : sqf - 1)] && board->board[sqf - 16]){
            developed++;
        }
    }

    return developed;
}


int filter(const string input, const string &output){
  char buffer[32768];
  int buffer_key = 0;
  ofstream fout(output);
  ifstream fin(input);
  string line;
  int total_lines = 0, filtered_lines = 0;
  while (getline(fin, line)){
    total_lines++;
    board_info board;
    bool color = setfromfen(&board, line.c_str());
    if (enough_mat(&board) && abs(poorly_developed(&board)) > 3){
      filtered_lines++;
      buffer_key += sprintf(buffer + buffer_key, "%s\n", line.c_str());
      if (buffer_key > 32668){
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


int main(int argc, char *argv[]){
  clock_t a = clock();
  filter(argv[1], argv[2]);
  printf("%f seconds\n", (float)(clock() - a) / CLOCKS_PER_SEC);
  return 0;
}