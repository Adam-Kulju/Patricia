#include <stdio.h>
#include <iostream>
#include <fstream>
#include "base.h"


int mat(board_info *board){
  int mat = 0, total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++){
    mat += (board->material_count[Colors::White][i] - board->material_count[Colors::Black][i]) * piece_values[i];
    total_mat += (board->material_count[Colors::White][i] + board->material_count[Colors::Black][i]) * piece_values[i];
  }
  return total_mat < 2500 ? 0 : mat;
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

    //After setting up the board, our next task is to extract the evaluation and game result of the position.
    int eval_delimiter = line.find(" | ") + 3;

    int eval = atoi(line.substr(eval_delimiter, line.find(" | ", eval_delimiter) - eval_delimiter).c_str());

    int multiplier = color == Colors::White ? 1 : -1;

    if (eval * multiplier > -50 && mat(&board) * multiplier < -100){
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

