//Development
//Pieces on the first two ranks count as poorly developed (be7 can be strong later but isn't really now)

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
  return total_mat > 4000 && board->material_count[Colors::White][4] && board->material_count[Colors::Black][4];
}

/*
    --------
    --------
    --------
    ------1-
    -----121
    -----222
    -----1K1
    --------
*/
int no_shelter(board_info *board, bool color){
    if (board->castling[color][0] || board->castling[color][1]){
        return 0;
    }
    int squares = 0;
    int king = board->kingpos[color];
    for (int dirs: {Directions::Northwest, Directions::Northeast, Directions::North,
                    Directions::West, Directions::East,
                    Directions::Southwest, Directions::South, Directions::Southeast}){
                        int pos = king + dirs;
                        while (1){
                            if (squares > 8){
                                return 9;
                            }
                            if (out_of_board(pos) || get_rank(pos) == (color ? 7 : 0)){   //don't count squares on the first rank (usually not bad for a king to have room to move there)
                                break;
                            }
                            if (board->board[pos]){
                                if (board->board[pos] % 2 != color){
                                    if (board->board[pos] > Pieces::BPawn){                      
                                        squares += 2;
                                    }
                                    else{
                                        squares++;
                                    }
                                }
                                break;
                            }
                            squares++;
                            pos += dirs;
                        }
                    }
    return squares;
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
    if (enough_mat(&board) && (no_shelter(&board, Colors::White) > 8 || no_shelter(&board, Colors::Black) > 8)){
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