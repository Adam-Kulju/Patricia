#include "search.h"
#include "uci.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>

namespace {

constexpr auto kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

bool parse_depth(const char* text, int& depth) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  int value = 0;
  constexpr int max_value = std::numeric_limits<int>::max();

  for (const char* current = text; *current != '\0'; ++current) {
    if (*current < '0' || *current > '9') {
      return false;
    }

    const int digit = *current - '0';

    if (value > (max_value - digit) / 10) {
      return false;
    }

    value = value * 10 + digit;
  }

  depth = value;
  return true;
}

int run_perft(int argc, char* argv[], Position& position, ThreadInfo& thread_info) {
  if (argc < 3) {
    const char* executable = argc > 0 && argv[0] != nullptr ? argv[0] : "engine";
    std::fprintf(stderr, "Usage: %s perft <depth>\n", executable);
    return EXIT_FAILURE;
  }

  int depth = 0;

  if (!parse_depth(argv[2], depth)) {
    std::fprintf(stderr, "Invalid depth: %s\n", argv[2] != nullptr ? argv[2] : "");
    return EXIT_FAILURE;
  }

  set_board(position, thread_info, kStartFen);
  perft(depth, position, true, thread_info);

  return EXIT_SUCCESS;
}

}

int main(int argc, char* argv[]) {
  Position position{};
  auto thread_info = std::make_unique<ThreadInfo>();

  init_LMR();
  init_bbs();

  if (argc > 1 && argv[1] != nullptr) {
    const char* command = argv[1];

    if (std::strcmp(command, "bench") == 0) {
      bench(position, *thread_info);
      return EXIT_SUCCESS;
    }

    if (std::strcmp(command, "perft") == 0) {
      return run_perft(argc, argv, position, *thread_info);
    }
  }

  uci(*thread_info, position);
  return EXIT_SUCCESS;
}
