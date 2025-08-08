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

uint64_t perft(int depth, Position &position, bool first,
               ThreadInfo &thread_info)
// Performs a perft search to the desired depth,
// displaying results for each move at the root.
{
  uint64_t total_nodes = 0;
  uint64_t checkers = attacks_square(
      position, get_king_pos(position, position.color), position.color ^ 1);

  if (depth <= 1) {
    std::array<Move, ListSize> list;
    int nmoves = movegen(position, list, checkers, Generate::GenAll);

    for (int i = 0; i < nmoves; i++) {
      total_nodes += is_legal(position, list[i]);
    }

    return total_nodes;
  }

  MovePicker picker;
  init_picker(picker, position, -107, checkers,
              &(thread_info.game_hist[thread_info.game_ply]));

  while (Move move = next_move(picker, position, thread_info, MoveNone,
                               false)) // Loop through all of the moves,
                                       // skipping illegal ones.
  {
    if (!is_legal(position, move)) {
      continue;
    }
    Position new_position = position;
    make_move(new_position, move);

    uint64_t nodes = perft(depth - 1, new_position, false, thread_info);

    if (first) {
      printf("%s: %" PRIu64 "\n", internal_to_uci(position, move).c_str(),
             nodes);
    }
    total_nodes += nodes;
  }

  return total_nodes;
}

void bench(Position &position, ThreadInfo &thread_info) {
  std::vector<std::string> fens = {
      "2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5 b - - 0 55\0",
      "2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1 w - - 0 42\0",
      "6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8 b - - 1 44\0",
      "6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4 w - - 1 36\0",
      "4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1 b - - 2 24\0",
      "3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1 w - - 0 23\0",
      "r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R w KQkq a6 0 14\0",
      "r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1 w - - 16 42\0",
      "6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2 b - - 4 44\0",
      "8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8 w - - 3 54\0",
      "7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1 w - - 0 36\0",
      "r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1 b - - 2 "
      "10\0",
      "3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1 w - - 3 87\0",
      "4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3 b - - 0 37\0",
      "2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1 w - - 2 34\0",
      "1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K b - - 1 37\0",
      "r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R b KQkq b3 0 "
      "17\0",
      "8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5 b - - 16 42\0",
      "1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3 w - - 8 29\0",
      "8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8 w - - 29 68\0",
      "3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2 b - - 2 20\0",
      "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49\0",
      "1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8 w - - 1 51\0",
      "q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8 b - - 2 34\0",
      "r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1 w - - 0 22\0",
      "r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R w KQkq - 0 5\0",
      "r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n b Q - 1 12\0",
      "r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1 b - - 2 19\0",
      "r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1 w - - 2 25\0",
      "r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8 w - - 0 32\0",
      "r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR w KQkq - 1 5\0",
      "3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1 w - - 0 12\0",
      "5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1 w - - 2 20\0",
      "8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1 b - - 3 27\0",
      "8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2 w - - 1 33\0",
      "8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3 b - - 1 38\0",
      "8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6 b - - 15 45\0",
      "8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3 b - - 5 49\0",
      "8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4 w - - 6 54\0",
      "8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1 b - - 6 59\0",
      "8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3 b - - 14 63\0",
      "8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8 w - - 0 67\0",
      "1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1 b - - 4 23\0",
      "4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1 w - - 3 22\0",
      "r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1 w - - 4 "
      "12\0",
      "2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K w - - 3 25\0",
      "r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R b - - 2 14\0",
      "rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR b KQkq c3 0 2\0",
      "2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1 w - f6 0 20\0",
      "2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R w - - 23 93\0"};

  thread_info.max_time = INT32_MAX / 2, thread_info.opt_time = INT32_MAX / 2;
  thread_info.max_iter_depth = 12;
  uint64_t total_nodes = 0;

  auto start = std::chrono::steady_clock::now();

  for (std::string fen : fens) {
    new_game(thread_info, TT);
    set_board(position, thread_info, fen);
    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, TT);
    total_nodes += thread_info.nodes;
  }

  printf("Bench: %" PRIu64 " nodes %" PRIi64 " nps\n", total_nodes,
         (int64_t)(total_nodes * 1000 / time_elapsed(start)));
}

Move uci_to_internal(const Position &position, std::string uci) {
  // Converts a uci move into an internal move.
  std::array<Move, ListSize> list;
  int nmoves =
      movegen(position, list,
              attacks_square(position, get_king_pos(position, position.color),
                             position.color ^ 1),
              Generate::GenAll);

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
      printf("id name Patricia 5.0\n"
             "id author Adam Kulju\n"
             "option name Hash type spin default 32 min 1 max 131072\n"
             "option name Threads type spin default 1 min 1 max 1024\n"
             "option name SyzygyPath type string default null\n"
             "option name MultiPV type spin default 1 min 1 max 255\n"
             "option name Skill_Level type spin default 21 min 1 max 21\n"
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
          thread_data.is_frc = true;

        }

        else {
          thread_data.is_frc = false;
        }

        continue;
      }

      else if (name == "SyzygyPath") {
        std::string value;
        input_stream >> value;
        tb_init(value.c_str());
        if (TB_LARGEST) {
          printf("Tablebase initialized at path %s\n", value.c_str());
        } else {
          printf("Error initializing tablebase. Please try again!\n");
        }
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

      else if (name == "Skill_Level") {
        if (value == 21) {
          thread_info.is_human = false;
        } 
        
        else {
          thread_info.is_human = true;
          int to_elo = skill_levels[value - 1];
          thread_info.cp_loss = 120 - (to_elo / 25);
        }
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
      thread_info.game_ply = 6;
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
    }

    else if (command == "perft") {
      int depth;
      input_stream >> depth;
      auto start_time = std::chrono::steady_clock::now();

      uint64_t nodes = perft(depth, position, true, thread_info);

      printf("%" PRIu64 " nodes %" PRIu64 " nps\n", nodes,
             (uint64_t)(nodes * 1000 /
                        (std::max((int64_t)1, time_elapsed(start_time)))));
    }

    else if (command == "bench") {
      bench(position, thread_info);
    }
  }
}
