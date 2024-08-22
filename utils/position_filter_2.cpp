#include <cstdio>
#include <iostream>
#include <fstream>
#include "base.hpp"

using namespace  std;

float danger_values[5] = {0.8, 2.2, 2, 3, 6};
float defense_values[5] = {1, 1.1, 1.1, 1, 1.7};

bool in_danger_white(board_info *board){

    /*
    --------
    --------
    --------
    --------
    --***---
    --***---
    -*****--
    --*K*---  
    */
    int white_king = board->kingpos[Colors::White];
    float danger_level = 0, attacks = 0;
    int kingzones[16] = {white_king + Directions::Southwest, white_king + Directions::South, white_king + Directions::Southeast,
                         white_king + Directions::West, white_king + Directions::East,

                         white_king + Directions::Northwest + Directions::West, white_king + Directions::Northwest, white_king + Directions::North,
                         white_king + Directions::Northeast, white_king + Directions::Northeast + Directions::East,

                         white_king + Directions::North + Directions::Northwest, white_king + Directions::North + Directions::North, white_king + Directions::North + Directions::Northeast,
                         white_king + Directions::North + Directions::North + Directions::Northwest, white_king + Directions::North + Directions::North + Directions::North,
                         white_king + Directions::North + Directions::North + Directions::Northeast,
                         };
    for (int pos : kingzones){
        if (out_of_board(pos)){
            continue;
        }
        if (board->board[pos] % 2 == Colors::Black){                    //if we have a board index of 3, 5, etc. it's a black(enemy) piece
            danger_level += danger_values[board->board[pos] / 2 - 1];
            attacks += danger_values[board->board[pos] / 2 - 1];
        }
        else if (board->board[pos]){                                    //otherwise verify it's not blank (friendly piece)
            danger_level -= defense_values[board->board[pos] / 2 - 1];
        }
    }
    return (danger_level > 4 && attacks > 7);
}

bool in_danger_black(board_info *board){
    int black_king = board->kingpos[Colors::Black];
    float danger_level = 0, attacks = 0;
    int kingzones[16] = {black_king + Directions::Northwest, black_king + Directions::North, black_king + Directions::Northeast,
                         black_king + Directions::West, black_king + Directions::East,

                         black_king + Directions::Southwest + Directions::West, black_king + Directions::Southwest, black_king + Directions::South,
                         black_king + Directions::Southeast, black_king + Directions::Southeast + Directions::East,

                         black_king + Directions::South + Directions::Southwest, black_king + Directions::South + Directions::South, black_king + Directions::South + Directions::Southeast,
                         black_king + Directions::South + Directions::South + Directions::Southwest, black_king + Directions::South + Directions::South + Directions::South,
                         black_king + Directions::South + Directions::South + Directions::Southeast,
                         };
    for (int pos : kingzones){
        if (out_of_board(pos)){
            continue;
        }
        if (board->board[pos]){
            if (board->board[pos] % 2 == Colors::White){       //if we have a board index of 2, 4, etc and is not blank it's white(enemy)
                danger_level += danger_values[board->board[pos] / 2 - 1];
                attacks += danger_values[board->board[pos] / 2 - 1];
            }
            else{                                                           //else it's a friendly piece
                danger_level -= defense_values[board->board[pos] / 2 - 1];
            }
        }
 
    }
    return (danger_level > 4 && attacks > 7);
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

    if (in_danger_white(&board) || in_danger_black(&board)){
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