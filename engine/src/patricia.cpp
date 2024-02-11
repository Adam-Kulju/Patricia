#include "position.h"
#include "search.h"
#include "movegen.h"
#include "defs.h"
#include "nnue.h"
#include "utils.h"
#include <stdio.h>
#include "uci.h"
#include <memory>

uint64_t perft(int depth, Position position,
               bool first, ThreadInfo &thread_info) // Performs a perft search to the desired depth,
                           // displaying results for each move at the root.
{
  if (!depth) {
    return 1; // a terminal node
  } else if (first) {
  }
  Move list[216];
  uint64_t l = 0;
  int nmoves = movegen(position, list);
  for (int i = 0; i < nmoves;
       i++) // Loop through all of the moves, skipping illegal ones.
  {
    Position new_position = position;
    if (make_move(new_position, list[i], thread_info, false)) {
      continue;
    }

    uint64_t b = perft(depth - 1, new_position, false, thread_info);
    if (first) {
      char temp[6];
      printf("%s: %" PRIu64 "\n", internal_to_uci(position, list[i]).c_str(), b);
    }
    l += b;
  }
  return l;
}

int main(void) {
  if (0) {
    Position position;
    std::unique_ptr<ThreadInfo> thread_info(new ThreadInfo);
    set_board(position, *thread_info,
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    print_board(position);
    clock_t start = clock();
    uint64_t p = perft(6, position, true, *thread_info);
    printf("%" PRIu64 " nodes %" PRIu64 " nps\n", p,
           (uint64_t)(p / ((clock() - start) / (float)CLOCKS_PER_SEC)));
    exit(0);
  }
  uci();
  return 0;
}