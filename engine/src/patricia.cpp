#include "board.h"
#include <stdio.h>

uint64_t perft(int depth, Position position,
               bool first) // Performs a perft search to the desired depth,
                           // displaying results for each move at the root.
{
  if (!depth) {
    return 1; // a terminal node
  }
  Move list[216];
  uint64_t l = 0;
  int nmoves = movegen(position, list);
  for (int i = 0; i < nmoves;
       i++) // Loop through all of the moves, skipping illegal ones.
  {
    Position new_position = position;
    if (move(new_position, list[i])) {
      continue;
    }

    uint64_t b = perft(depth - 1, new_position, false);
    if (first) {
      char temp[6];
      printf("%s: %lu\n", internal_to_uci(position, list[i]).c_str(), b);
    }
    l += b;
  }
  return l;
}

int main(void) {
  Position position;
  ThreadInfo thread_info;
  set_board(position, thread_info,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  print_board(position);
  clock_t start = clock();
  uint64_t p = perft(6, position, true);
  printf("%lu nodes %lu nps\n", p, (uint64_t)(p / ((clock() - start) / (float)CLOCKS_PER_SEC)) );
  return 0;
}