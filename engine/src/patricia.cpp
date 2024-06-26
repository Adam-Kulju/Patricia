#include "params.h"
#include "position.h"
#include "search.h"
#include "uci.h"
#include <memory>
#include <stdio.h>

uint64_t
perft(int depth, Position &position, bool first,
      ThreadInfo &thread_info) // Performs a perft search to the desired depth,
                               // displaying results for each move at the root.
{
  if (!depth) {
    return 1; // a terminal node
  } else if (first) {
    new_game(thread_info);
    thread_info.start_time = std::chrono::steady_clock::now();
    set_board(position, thread_info,
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  }
  std::array<Move, ListSize> list;
  uint64_t total_nodes = 0;
  int nmoves =
      movegen(position, list,
              attacks_square(position, position.kingpos[position.color],
                             position.color ^ 1));
  for (int i = 0; i < nmoves;
       i++) // Loop through all of the moves, skipping illegal ones.
  {
    Position new_position = position;
    if (make_move(new_position, list[i], thread_info, false)) {
      continue;
    }

    uint64_t nodes = perft(depth - 1, new_position, false, thread_info);
    if (first) {
      char temp[6];
      printf("%s: %" PRIu64 "\n", internal_to_uci(position, list[i]).c_str(),
             nodes);
    }
    total_nodes += nodes;
  }
  if (first) {
    printf("%" PRIu64 " nodes %" PRIu64 " nps\n", total_nodes,
           (uint64_t)(total_nodes * 1000 /
                      (time_elapsed(thread_info.start_time))));

    std::exit(0);
  }
  return total_nodes;
}

void bench(Position &position, ThreadInfo &thread_info) {
  std::vector<std::string> fens = {
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"};

  thread_info.max_time = INT32_MAX / 2, thread_info.opt_time = INT32_MAX / 2;
  thread_info.max_iter_depth = 12;
  uint64_t total_nodes = 0;

  thread_info.start_time = std::chrono::steady_clock::now();

  for (std::string fen : fens) {
    new_game(thread_info);
    set_board(position, thread_info, fen);
    iterative_deepen(position, thread_info);
    total_nodes += thread_info.nodes;
  }

  printf("Bench: %" PRIu64 " nodes %" PRIi64 " nps\n", total_nodes,
         (int64_t)(total_nodes * 1000 / time_elapsed(thread_info.start_time)));

  std::exit(0);
}

int main(int argc, char *argv[]) {
  Position position;
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  init_LMR();
  if (argc > 1) {
    if (std::string(argv[1]) == "perft") {
      perft(atoi(argv[2]), position, true, *thread_info);
    }

    else if (std::string(argv[1]) == "bench") {
      bench(position, *thread_info);
    }
  }

  uci(*thread_info, position);
  return 0;
}