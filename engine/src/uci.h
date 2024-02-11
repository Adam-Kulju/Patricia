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

  Position position;
  std::unique_ptr<ThreadInfo> T(new ThreadInfo);
  ThreadInfo thread_info = *T;

  thread_info.nnue_state.m_accumulator_stack.reserve(100);

  std::string input;

  while (getline(std::cin, input)) {

    std::istringstream input_stream(input);

    std::string command;

    input_stream >> std::skipws >> command; // write into command

    if (command == "quit"){
        exit(0);
    }

    else if (command == "uci") {
      printf("id name Patricia 0.1\n id author Adam Kulju\noption name Hash "
             "type spin default 32 min 32 max 32\noption name Threads type "
             "spin default 1 min 1 max 1\nuciok\n");
    }

    else if (command == "ucinewgame") {
      clear_TT();
      set_board(position, thread_info,
                "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    else if (command == "position") {
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

      std::string has_moves;
      if (input_stream >> has_moves){   //we're at the "moves" part of the command now
        std::string moves;
        while (input_stream >> moves){
            Move move = uci_to_internal(moves);
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