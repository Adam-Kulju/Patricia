#include <stdio.h>
#include <iostream>
#include <fstream>
#include "base.hpp"

bool enough_mat(board_info *board){
  int total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++){
    total_mat += (board->material_count[Colors::White][i] + board->material_count[Colors::Black][i]) * piece_values[i];
  }
  return total_mat > 4000 && board->material_count[Colors::White][4] && board->material_count[Colors::Black][4];
}

bool is_oppo_castle(board_info *board){
    int wking_file = get_file(board->kingpos[Colors::White]), bking_file = get_file(board->kingpos[Colors::Black]);
    return (!board->castling[Colors::White][0] && !board->castling[Colors::Black][0] && 
    !board->castling[Colors::White][1] && !board->castling[Colors::Black][1] &&
    (wking_file <= 3 ? bking_file >= 4 : bking_file <= 3));
}

bool pawn_storm_white(board_info *board){
    /*
        ------K-
        ------x-
        -----xxx
        -----xxx
        -----xxx
        --------
        --------
        --------
    */
    int food = board->kingpos[Colors::Black];
    int rush_zone[10] = {
        food + Directions::South,
        food + Directions::South + Directions::Southwest, food + Directions::South*2, food + Directions::South + Directions::Southeast,
        food + Directions::South*2 + Directions::Southwest, food + Directions::South*3, food + Directions::South*2 + Directions::Southeast,
        food + Directions::South*3 + Directions::Southwest, food + Directions::South*4, food + Directions::South*3 + Directions::Southeast
    };

    for (int pos : rush_zone){
        if (get_rank(pos) < 3 || out_of_board(pos)){
            break;
        }
        if (board->board[pos] == Pieces::WPawn){
            return true;
        }
    }
    return false;
}

bool pawn_storm_black(board_info *board){
    int food = board->kingpos[Colors::White];
    int rush_zone[10] = {
        food + Directions::North,
        food + Directions::North + Directions::Northwest, food + Directions::North*2, food + Directions::North + Directions::Northeast,
        food + Directions::North*2 + Directions::Northwest, food + Directions::North*3, food + Directions::North*2 + Directions::Northeast,
        food + Directions::North*3 + Directions::Northwest, food + Directions::North*4, food + Directions::North*3 + Directions::Northeast
    };

    for (int pos : rush_zone){
        if (get_rank(pos) > 5 || out_of_board(pos)){
            break;
        }
        if (board->board[pos] == Pieces::BPawn){
            return true;
        }
    }
    return false;
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
    if (is_oppo_castle(&board) && enough_mat(&board) && (pawn_storm_white(&board) || pawn_storm_black(&board))){
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