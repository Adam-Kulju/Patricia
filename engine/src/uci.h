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

std::thread s;

void run_thread(Position &position, ThreadInfo &thread_info) {

  // This wrapper function makes you able to use the "stop" command to stop the
  // search immediately.
  s = std::thread(search_position, std::ref(position), std::ref(thread_info));
}

void uci(ThreadInfo &thread_info, Position &position) {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("Patricia Chess Engine, written by Adam Kulju\n\n\n");

  thread_info.nnue_state.m_accumulator_stack.reserve(100);

  std::string input;

  while (getline(std::cin, input)) {

    std::istringstream input_stream(input);

    std::string command;

    input_stream >> std::skipws >> command; // write into command
    if (command == "d") {
      input_stream.clear();
      input_stream.str("position fen "
                       "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/"
                       "R3K2R w KQkq - 0 1");
      input_stream >> std::skipws >> command;
    }

    if (command == "quit") {
      if (s.joinable()) {
        s.join();
      }
      exit(0);
    }

    else if (command == "uci") {
      printf("id name Patricia 2.0.1\nid author Adam Kulju\n"
            "option name Hash type spin default 32 min 1 max 131072\n"
            "option name Threads type spin default 1 min 1 max 1024\n"
            "option name UCI_Elo type spin default 3200 min 1000 max 3200\n"
            "uciok\n");
    }

    else if (command == "isready") {
      printf("readyok\n");
    }

    else if (command == "setoption") {
      std::string name;
      int value;
      input_stream >> command;
      input_stream >> name;
      input_stream >> command;
      input_stream >> value;

      if (name == "Hash") {
        resize_TT(value);
      }

      else if (name == "Threads") {
        thread_data.num_threads = value;
      }

      else if (name == "UCI_Elo"){
        thread_info.max_nodes_searched = UINT64_MAX / 2;
        
        thread_info.max_nodes_searched = value;
      }
    }

    else if (command == "stop") {
      thread_info.stop = true;
      s.join();

    }

    else if (command == "ucinewgame") {
      new_game(thread_info);
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
      } else {
        set_board(position, thread_info,
                  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      }
      thread_info.nnue_state.reset_nnue(position);
      thread_info.zobrist_key = calculate(position);
      std::string has_moves;
      if (input_stream >>
          has_moves) { // we're at the "moves" part of the command now
        std::string moves;
        while (input_stream >> moves) {
          Move move = uci_to_internal(moves);
          ss_push(position, thread_info, move, thread_info.zobrist_key,
                  0); // fill the game hist stack as we go
          update_nnue_state(thread_info.nnue_state, move, position);
          make_move(position, move, thread_info, false);
        }
      }

    }

    else if (command == "go") {
      if (s.joinable()) {
        s.join();
      }
      
      thread_info.max_nodes_searched = UINT64_MAX / 2;
      thread_info.max_iter_depth = MaxSearchDepth;

      int color = position.color, time = 1000000000, increment = 0;
      std::string token;
      while (input_stream >> token) {
        if (token == "infinite") {
          ;
        } else if (token == "wtime" && color == Colors::White) {
          input_stream >> time;
        } else if (token == "btime" && color == Colors::Black) {
          input_stream >> time;
        } else if (token == "winc" && color == Colors::White) {
          input_stream >> increment;
        } else if (token == "binc" && color == Colors::Black) {
          input_stream >> increment;
        }
        else if (token == "nodes"){
          uint64_t nodes;
          input_stream >> nodes;
          thread_info.max_nodes_searched = nodes;
        }
        else if (token == "depth"){
          int depth;
          input_stream >> depth;
          thread_info.max_iter_depth = depth;
        }
      }

      // Calculate time allotted to search

      time = std::max(2, time - 50);
      thread_info.max_time = time / 2;
      thread_info.opt_time = (time / 20 + increment * 8 / 10) * 6 / 10;
      thread_info.start_time = std::chrono::steady_clock::now();

      run_thread(position, thread_info);
    }
  }
}
