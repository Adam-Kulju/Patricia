#include "search.h"
#include "uci.h"
#include <memory>
#include <stdio.h>

#ifdef WASM_BUILD
#include <emscripten/emscripten.h>
#include <sstream>

// Global state for WASM
static Position g_position;
static std::unique_ptr<ThreadInfo> g_thread_info;
static bool g_initialized = false;

extern "C" {

// Initialize the engine (called once at startup)
EMSCRIPTEN_KEEPALIVE
void wasm_init() {
    if (!g_initialized) {
        g_thread_info = std::make_unique<ThreadInfo>();
        init_LMR();
        init_bbs();
        init_nnue_wasm(); // Load NNUE networks from embedded filesystem
        new_game(*g_thread_info, TT);
        set_board(g_position, *g_thread_info,
                  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        g_initialized = true;
    }
}

// Process a single UCI command
EMSCRIPTEN_KEEPALIVE
void wasm_uci_command(const char* cmd) {
    if (!g_initialized) {
        wasm_init();
    }

    std::string input(cmd);
    std::istringstream input_stream(input);
    std::string command;
    input_stream >> std::skipws >> command;

    if (command == "uci") {
        printf("id name Patricia 5.0 WASM\n"
               "id author Adam Kulju\n"
               "option name Hash type spin default 32 min 1 max 131072\n"
               "option name MultiPV type spin default 1 min 1 max 255\n"
               "uciok\n");
    }
    else if (command == "isready") {
        printf("readyok\n");
    }
    else if (command == "ucinewgame") {
        new_game(*g_thread_info, TT);
        set_board(g_position, *g_thread_info,
                  "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }
    else if (command == "position") {
        g_thread_info->game_ply = 6;
        std::string setup;
        input_stream >> setup;
        if (setup == "fen") {
            std::string fen;
            for (int i = 0; i < 6; i++) {
                std::string substr;
                input_stream >> substr;
                fen += substr + " ";
            }
            set_board(g_position, *g_thread_info, fen);
        } else {
            set_board(g_position, *g_thread_info,
                      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        }
        calculate(g_position);

        std::string has_moves;
        if (input_stream >> has_moves) {
            std::string moves;
            while (input_stream >> moves) {
                Move move = uci_to_internal(g_position, moves);
                ss_push(g_position, *g_thread_info, move);
                make_move(g_position, move);
            }
        }
    }
    else if (command == "go") {
        g_thread_info->start_time = std::chrono::steady_clock::now();
        g_thread_info->max_nodes_searched = UINT64_MAX / 2;
        g_thread_info->max_iter_depth = MaxSearchDepth;

        int color = g_position.color, time = INT32_MAX, increment = 0;
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
                g_thread_info->max_nodes_searched = nodes;
            } else if (token == "depth") {
                int depth;
                input_stream >> depth;
                g_thread_info->max_iter_depth = depth;
            } else if (token == "movetime") {
                int movetime;
                input_stream >> movetime;
                g_thread_info->max_time = movetime;
                g_thread_info->opt_time = INT32_MAX / 2;
                goto run;
            }
        }

        time = std::max(2, time - 50);
        g_thread_info->max_time = time * 8 / 10;
        g_thread_info->opt_time = (time / 20 + increment) * 6 / 10;

    run:
        thread_data.stop = false;
        search_position(g_position, *g_thread_info, TT);
    }
    else if (command == "stop") {
        thread_data.stop = true;
    }
    else if (command == "setoption") {
        std::string name_token;
        int value;
        input_stream >> name_token; // "name"
        std::string name;
        input_stream >> name;
        input_stream >> name_token; // "value"
        input_stream >> value;

        if (name == "Hash") {
            resize_TT(value);
        }
        else if (name == "MultiPV") {
            g_thread_info->multipv = value;
        }
    }
    else if (command == "perft") {
        int depth;
        input_stream >> depth;
        auto start_time = std::chrono::steady_clock::now();
        uint64_t nodes = perft(depth, g_position, true, *g_thread_info);
        printf("%" PRIu64 " nodes %" PRIu64 " nps\n", nodes,
               (uint64_t)(nodes * 1000 /
                          (std::max((int64_t)1, time_elapsed(start_time)))));
    }
    else if (command == "bench") {
        bench(g_position, *g_thread_info);
    }

    fflush(stdout);
}

} // extern "C"

int main(int argc, char *argv[]) {
    // For WASM, main just initializes and returns
    // Commands are processed via wasm_uci_command
    wasm_init();
    return 0;
}

#else
// Native build

int main(int argc, char *argv[]) {
  Position position;
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  init_LMR();
  init_bbs();

  if (argc > 1) {
    if (std::string(argv[1]) == "bench") {
      bench(position, *thread_info);
      std::exit(0);
    }
    else if (std::string(argv[1]) == "perft"){
      set_board(position, *thread_info, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
      perft(std::atoi(argv[2]), position, true, *thread_info);
      std::exit(0);
    }
  }

  uci(*thread_info, position);
  return 0;
}

#endif // WASM_BUILD