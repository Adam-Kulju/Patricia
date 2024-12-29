#pragma once
#include "human.h"
#include "search.h"
#include <iostream>
#include <memory>

constexpr std::array<int, 20> skill_levels = {
    500,  800,  1000, 1200, 1300, 1400, 1500, 1600, 1700, 1800,
    1900, 2000, 2100, 2200, 2300, 2400, 2500, 2650, 2800, 3000};

void run_thread(Position &position, ThreadInfo &thread_info, std::thread &s) {

  // This wrapper function allows the user to call the "stop" command to stop
  // the search immediately.
  if (thread_info.is_human) {
    s = std::thread(search_human, std::ref(position), std::ref(thread_info));
  }

  else {
    s = std::thread(search_position, std::ref(position), std::ref(thread_info),
                    std::ref(TT));
  }
}

uint64_t perft(int depth, Position &position, bool first)
// Performs a perft search to the desired depth,
// displaying results for each move at the root.
{
  std::array<Move, ListSize> list;
  uint64_t total_nodes = 0;
  int nmoves =
      movegen(position, list,
              attacks_square(position, get_king_pos(position, position.color),
                             position.color ^ 1));

  if (depth <= 1) {
    for (int i = 0; i < nmoves; i++) {
      total_nodes += is_legal(position, list[i]);
    }
    return total_nodes;
  }

  for (int i = 0; i < nmoves;
       i++) // Loop through all of the moves, skipping illegal ones.
  {
    if (!is_legal(position, list[i])) {
      continue;
    }
    Position new_position = position;
    make_move(new_position, list[i]);

    uint64_t nodes = perft(depth - 1, new_position, false);

    if (first) {
      printf("%s: %" PRIu64 "\n", internal_to_uci(position, list[i]).c_str(),
             nodes);
    }
    total_nodes += nodes;
  }

  return total_nodes;
}

Move uci_to_internal(const Position &position, std::string uci) {
  // Converts a uci move into an internal move.
  std::array<Move, ListSize> list;
  int nmoves =
      movegen(position, list,
              attacks_square(position, get_king_pos(position, position.color),
                             position.color ^ 1));

  for (int i = 0; i < nmoves; i++) {
    if (internal_to_uci(position, list[i]) == uci)
      return list[i];
  }

  return 0;
}

void uci(ThreadInfo &thread_info, Position &position) {
  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("Patricia Chess Engine, written by Adam Kulju\n\n\n");

  new_game(thread_info, TT);
  set_board(position, thread_info,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  std::string input;

  std::thread s;

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
        thread_data.terminate = true;

        reset_barrier.arrive_and_wait();
        idle_barrier.arrive_and_wait();

        for (int i = 0; i < thread_data.threads.size(); i++) {
          if (thread_data.threads[i].joinable()) {
            thread_data.threads[i].join();
          }
        }
      std::exit(0);
    }

    else if (command == "uci") {
      printf("id name Patricia 4.0\n"
             "id author Adam Kulju\n"
             "option name Hash type spin default 32 min 1 max 131072\n"
             "option name Threads type spin default 1 min 1 max 1024\n"
             "option name MultiPV type spin default 1 min 1 max 255\n"
             "option name UCI_LimitStrength type check default false\n"
             "option name Skill_Level type spin default 21 min 1 max 21\n"
             "option name UCI_Elo type spin default 3001 min 500 max 3001\n"
             "option name UCI_Chess960 type check default false\n");

      /*for (auto &param : params) {
        std::cout << "option name " << param.name << " type spin default "
                  << param.value << " min " << param.min << " max " << param.max
                  << "\n";
      }*/

      printf("uciok\n");
    }

    else if (command == "printparams") {
      print_params_for_ob();
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

      if (name == "UCI_LimitStrength" || name == "UCI_Chess960") {
        std::string value;
        input_stream >> value;
        if (value == "true") {
          if (name == "UCI_LimitStrength") {
            thread_info.is_human = true;
          } else {
            thread_data.is_frc = true;
          }
        }

        else {
          if (name == "UCI_LimitStrength") {
            thread_info.is_human = false;
          } else {
            thread_data.is_frc = false;
          }
        }

        continue;
      }

      input_stream >> value;

      if (name == "Hash") {
        resize_TT(value);
      }

      else if (name == "Threads") {

        thread_data.terminate = true;

        reset_barrier.arrive_and_wait();
        idle_barrier.arrive_and_wait();

        for (int i = 0; i < thread_data.threads.size(); i++) {
          if (thread_data.threads[i].joinable()) {
            thread_data.threads[i].join();
          }
        }

        thread_data.thread_infos.clear();
        thread_data.threads.clear();

        thread_data.terminate = false;
        thread_data.num_threads = value;

        reset_barrier.reset(thread_data.num_threads);
        idle_barrier.reset(thread_data.num_threads);
        search_end_barrier.reset(thread_data.num_threads);

        for (int i = 0; i < value - 1; i++) {
          thread_data.thread_infos.emplace_back();
          thread_data.threads.emplace_back(loop, i);
        }
      }

      else if (name == "UCI_Elo" && value != 3001) {
        thread_info.cp_loss = 200 - (value / 13);
      }

      else if (name == "Skill_Level" && value != 21) {
        int to_elo = skill_levels[value - 1];
        thread_info.cp_loss = 200 - (to_elo / 13);
      }

      else if (name == "MultiPV") {
        thread_info.multipv = value;
      }

      else {
        for (auto &param : params) {
          if (name == param.name) {
            param.value = value;
            if (name == "LMRBase" || name == "LMRRatio") {
              init_LMR();
            }
          }
        }
      }
    }

    else if (command == "stop") {
      thread_data.stop = true;

      if (s.joinable()) {
        s.join();
      }
    }

    else if (command == "ucinewgame") {
      thread_data.stop = true;

      if (s.joinable()) {
        s.join();
      }

      new_game(thread_info, TT);
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
      calculate(position);
      std::string has_moves;
      if (input_stream >>
          has_moves) { // we're at the "moves" part of the command now

        std::string moves;
        while (input_stream >> moves) {
          Move move = uci_to_internal(position, moves);
          ss_push(position, thread_info,
                  move); // fill the game hist stack as we go
          make_move(position, move);
        }
      }

    }

    else if (command == "go") {
      thread_info.start_time = std::chrono::steady_clock::now();

      for (int i = 0; i < thread_data.thread_infos.size(); i++) {
        while (thread_data.thread_infos[i].searching) {
          ;
        }
      }
      if (s.joinable()) {
        s.join();
      }
      thread_info.max_nodes_searched = UINT64_MAX / 2;
      thread_info.max_iter_depth = MaxSearchDepth;

      int color = position.color, time = INT32_MAX, increment = 0;
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
        } else if (token == "nodes") {
          uint64_t nodes;
          input_stream >> nodes;
          thread_info.max_nodes_searched = nodes;
        } else if (token == "depth") {
          int depth;
          input_stream >> depth;
          thread_info.max_iter_depth = depth;
        } else if (token == "movetime") {
          int time;
          input_stream >> time;
          thread_info.max_time = time;
          thread_info.opt_time = INT32_MAX / 2;
          goto run;
        }
      }

      // Calculate time allotted to search

      time = std::max(2, time - 50);
      thread_info.max_time = time / 2;
      thread_info.opt_time = (time / 20 + increment * 8 / 10) * 6 / 10;

    run:
      run_thread(position, thread_info, s);
    } else if (command == "perft") {
      int depth;
      input_stream >> depth;
      auto start_time = std::chrono::steady_clock::now();

      uint64_t nodes = perft(depth, position, true);

      printf("%" PRIu64 " nodes %" PRIu64 " nps\n", nodes,
             (uint64_t)(nodes * 1000 /
                        (std::max((int64_t)1, time_elapsed(start_time)))));
    }
  }
}
