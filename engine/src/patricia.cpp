#include "search.h"
#include "uci.h"
#include <memory>
#include <stdio.h>

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