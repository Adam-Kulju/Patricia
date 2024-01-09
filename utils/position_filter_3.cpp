#include <stdio.h>
#include <iostream>
#include <fstream>
#include "base.h"

bool enough_mat(board_info *board){
  int total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++){
    total_mat += (board->material_count[Colors::White][i] + board->material_count[Colors::Black][i]) * piece_values[i];
  }
  return total_mat > 3000;
}

int space_white(board_info *board){
    /*
    --------
    --------
    --****--
    -******-
    -******-
    ---**---
    --------
    --------
    */
   int total_space = 0;
    for (int i : {0x23, 0x24,
              0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
              0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
              0x52, 0x53, 0x54, 0x55}){
                if (board->board[i] == Pieces::WPawn){
                    total_space += get_rank(i);
                }
              }
    for (int i = 0; i < 7; i++){                                    //Give a bonus for a file being more open, this helps reward friendly space, as well as open lines for attack/defense.
      if (0x10 + i == Pieces::Blank && 0x20 + i == Pieces::Blank){
        total_space += 1;
      }
    }
    return total_space;
}

int space_black(board_info *board){
   int total_space = 0;
    for (int i : {0x53, 0x54,
              0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
              0x41, 0x42, 0x43, 0x44, 0x45, 0x46,
              0x22, 0x23, 0x24, 0x25}){
                if (board->board[i] == Pieces::BPawn){
                    total_space += get_rank((i ^ 112));
                }
   for (int i = 0; i < 7; i++){                                    //Give a bonus for a file being more open, this helps reward friendly space, as well as open lines for attack/defense.
      if (0x60 + i == Pieces::Blank && 0x50 + i == Pieces::Blank){
        total_space += 1;
      }
    }             
  }
    return total_space;
}


int filter(const std::string input, const std::string &output){
  char buffer[32768];
  int buffer_key = 0;
  std::ofstream fout(output);
  std::ifstream fin(input);
  std::string line;
  int total_lines = 0, filtered_lines = 0;
  while (std::getline(fin, line)){
    total_lines++;
    board_info board;
    bool color = setfromfen(&board, line.c_str());

    if (enough_mat(&board) && abs(space_white(&board) - space_black(&board)) > 9){
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