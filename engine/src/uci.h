#pragma once
#include "defs.h"
#include "movegen.h"
#include "nnue.h"
#include "position.h"
#include "search.h"
#include "utils.h"
#include <iostream>
#include <memory>
#include <stdio.h>

void uci() {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);
  
  printf("Patricia Chess Engine, written by Adam Kulju\n\n\n");

  Position position;
  std::unique_ptr<ThreadInfo> T(new ThreadInfo);
  ThreadInfo thread_info = *T;

  thread_info.nnue_state.m_accumulator_stack.reserve(100);

  std::string input;

  while (getline(std::cin, input)) {

    std::istringstream input_stream(input);

    std::string command;

    input_stream >> std::skipws >> command; // write into command

    if (command == "d"){
      input_stream.clear();
      input_stream.str("position startpos moves d2d4 d7d5 c2c4 d5c4 g1f3 g8f6 e2e3 c8g4 f1c4 e7e6 h2h3 g4h5 b1c3 b8c6 d4d5 e6d5 c3d5 f6d5 d1d5 d8d5 c4d5 e8c8 d5c6 f8b4 c1d2 h5f3 c6b7 f3b7 d2b4 b7g2 h1g1 g2h3 g1g7 h8g8 g7g8 d8g8 e1e2 h3e6 b2b3 h7h5 a1h1 a7a6 e3e4 g8g4 f2f3 g4g2 e2d3 g2a2 h1h5 a2b2 f3f4 b2b3 b4c3 e6g4 h5g5 g4f3 g5g3 f3h1 g3h3 h1g2 h3g3 g2f1 d3c2 b3a3 f4f5 a3a4 e4e5 a4f4 f5f6 c8d7 g3g7 d7e6 g7g8 f4f2 c2d1 f1c4 g8e8 e6f5 e8c8 a6a5 c8c7 c4b3 d1e1 f2a2 c7b7 a5a4 c3d4 f5e4 b7b4 e4f3 b4b7 a2e2 e1f1 e2d2 f1g1 d2d4 b7e7 f3f4 g1f2 b3e6 e7b7 f4e5 f2e3 d4c4 e3d3 c4c5 b7a7 e6b3 a7b7 e5f6 d3d4 c5c4 d4d3 c4c5 d3d4 c5c4 d4d3 c4c5");
      input_stream >> std::skipws >> command;
    }

    if (command == "quit"){
        exit(0);
    }

    else if (command == "uci") {
      printf("id name Patricia 0.1\nid author Adam Kulju\noption name Hash "
             "type spin default 32 min 32 max 32\noption name Threads type "
             "spin default 1 min 1 max 1\nuciok\n");
    }

    else if (command == "isready"){
        printf("readyok\n");
    }

    else if (command == "setoption"){

    }

    else if (command == "stop"){
        
    }

    else if (command == "ucinewgame") {
      clear_TT();
      thread_info.game_ply = 0;
      memset(thread_info.game_hist, 0, sizeof(thread_info.game_hist));
      set_board(position, thread_info,
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    else if (command == "position") {
      thread_info.game_ply = 0;
      std::string setup;
      input_stream >> setup;
      if (setup == "fen") {
        std::string fen;

        for (int i = 0; i < 6; i++) {
          //                    1                        2  3   4 5 6 subtokens
          // rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1

          std::string substr;
          input_stream >> substr;
          fen += substr + " ";
        }

        set_board(position, thread_info, fen);
      }
      else{
        set_board(position, thread_info,
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      }
      thread_info.zobrist_key = calculate(position);
      std::string has_moves;
      if (input_stream >> has_moves){   //we're at the "moves" part of the command now
        std::string moves;
        while (input_stream >> moves){
            Move move = uci_to_internal(moves);
            ss_push(position, thread_info, move, thread_info.zobrist_key);  //fill the game hist stack as we go
            make_move(position, move, thread_info, false);
        }
      }
    }

    else if (command == "go"){
        int color = position.color, time = 0, increment = 0;
        std::string token;
        while (input_stream >> token){
            if (token == "infinite"){
                time = 10000000;
            }
            else if (token == "wtime" && color == Colors::White){
                input_stream >> time;
            }
            else if (token == "btime" && color == Colors::Black){
                input_stream >> time;
            }
           else if (token == "winc" && color == Colors::White){
                input_stream >> increment;
            }
            else if (token == "binc" && color == Colors::Black){
                input_stream >> increment;
            }
        }
        thread_info.max_time = time / 5;
        thread_info.opt_time = time / 20 + increment * 6 / 10;
        thread_info.start_time = std::chrono::steady_clock::now();
        iterative_deepen(position, thread_info);
    }


  }
}