//Pawn levers
//

#include <stdio.h>
#include <iostream>
#include <fstream>
#include "base.hpp"

using namespace  std;

bool enough_mat(board_info *board){
  int total_mat = 0;
  int piece_values[5] = {100, 300, 300, 500, 900};
  for (int i = 0; i < 5; i++){
    total_mat += (board->material_count[Colors::White][i] + board->material_count[Colors::Black][i]) * piece_values[i];
  }
  return total_mat > 3000 && board->material_count[Colors::White][4] && board->material_count[Colors::Black][4];
}

/*
    --------
    --------
    --------
    --------
    -----***
    -----***
    -----*K*
    -----***
*/
constexpr int attack_weights[6] = {0, 1, 2, 2, 3, 5};
int KINGZONES[2][0x80][0x80];

constexpr void set(){
    for (int i : STANDARD_TO_MAILBOX){
        for (int n : {i + Directions::Southwest, i + Directions::South, i + Directions::Southeast,
                        i + Directions::West, i + Directions::East,
                        i + Directions::Northwest, i + Directions::North, i + Directions::Northeast}) {
                KINGZONES[0][i][n] = 1;
                KINGZONES[1][i][n] = 1;
        }
        if (i + 33 < 0x80) {
        KINGZONES[0][i][i + Directions::North + Directions::Northwest] = 1, KINGZONES[0][i][i + Directions::North*2] = 1,
                          KINGZONES[0][i][i + Directions::North + Directions::Northeast] =
                              1; // and 3 for advanced squares
        }
        if (i - 33 > 0x0) {
        KINGZONES[1][i][i + Directions::South + Directions::Southwest] = 1, KINGZONES[1][i][i + Directions::South * 2] = 1,
                          KINGZONES[1][i][i + Directions::South + Directions::Southeast] = 1;
        }
    }
}

bool is_attack_vector(board_info *board, int piece, int sq, int kingsq){
    int rank_dist = abs(get_rank(sq) - get_rank(kingsq)), file_dist = abs(get_file(sq) - get_file(kingsq));

    switch (piece){
        case Pieces::WQueen:
            return (file_dist < 2 || rank_dist < 3 || abs(rank_dist - file_dist) < 3);
        case Pieces::WRook:
            return file_dist < 2 || rank_dist < 3;
        case Pieces::WBishop:
            return (abs(rank_dist - file_dist) < 3);
        default:
            return file_dist < 4 && rank_dist < 4;
    }
}

int potential_attack_white(board_info *board){
    int wking = board->kingpos[Colors::White];
    int attacks = 0;


    for (int sq : STANDARD_TO_MAILBOX){
        if (board->board[sq] > Pieces::WKnight && board->board[sq] % 2 == Colors::Black && board->board[sq] < Pieces::WKing){
            if (KINGZONES[Colors::White][wking][sq] || is_attack_vector(board, board->board[sq] - 1, sq, wking)){
                attacks += attack_weights[board->board[sq] / 2];
            }
        }
    }
    return attacks;
}

int potential_attack_black(board_info *board){
    int bking = board->kingpos[Colors::Black];
    int attacks = 0;


    for (int sq : STANDARD_TO_MAILBOX){
        if (board->board[sq] >= Pieces::WKnight && board->board[sq] % 2 == Colors::White && board->board[sq] < Pieces::WKing){
            if (KINGZONES[Colors::Black][bking][sq] || is_attack_vector(board, board->board[sq], sq, bking)){
                attacks += attack_weights[board->board[sq] / 2];
            }
        }
    }
    return attacks;
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
    if (enough_mat(&board) && (potential_attack_white(&board) > 10 || potential_attack_black(&board) > 10)){
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