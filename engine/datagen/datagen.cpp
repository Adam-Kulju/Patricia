#include "../src/search.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kMaxFenBuffer = 5000;
constexpr int kQuietThresholdLimit = 20;
constexpr uint64_t kFenLimit = 251000000ULL;

constexpr char kStartFen[] =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

std::vector<std::string> openings;
bool use_openings = false;
int num_threads = 1;
std::chrono::steady_clock::time_point start_time;

std::mt19937_64 create_random_engine() {
  std::random_device device;
  std::seed_seq seed{
      device(), device(), device(), device(),
      device(), device(), device(), device()
  };
  return std::mt19937_64(seed);
}

std::mt19937_64 &rng() {
  static thread_local std::mt19937_64 engine = create_random_engine();
  return engine;
}

std::size_t random_index(std::size_t limit) {
  if (limit == 0) {
    return 0;
  }

  std::uniform_int_distribution<std::size_t> distribution(0, limit - 1);
  return distribution(rng());
}

int random_int(int limit) {
  if (limit <= 0) {
    return 0;
  }

  std::uniform_int_distribution<int> distribution(0, limit - 1);
  return distribution(rng());
}

void remove_carriage_return(std::string &line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }
}

bool load_openings(const std::string &filename) {
  openings.clear();
  use_openings = false;

  std::ifstream input(filename);

  if (!input) {
    std::fprintf(stderr, "The openings file could not be opened: %s\n",
                 filename.c_str());
    return false;
  }

  std::string line;

  while (std::getline(input, line)) {
    remove_carriage_return(line);

    if (!line.empty()) {
      openings.push_back(line);
    }
  }

  std::printf("%zu openings found.\n", openings.size());

  use_openings = !openings.empty();
  return use_openings;
}

Move random_legal_move(Position &position) {
  MoveInfo move_info{};
  const int num_moves = legal_movegen(position, move_info.moves);

  if (num_moves <= 0) {
    return MoveNone;
  }

  return move_info.moves[random_index(static_cast<std::size_t>(num_moves))];
}

bool has_legal_move(Position &position) {
  MoveInfo move_info{};
  return legal_movegen(position, move_info.moves) > 0;
}

bool play_random_moves(Position &position, ThreadInfo &thread_info, int count) {
  for (int i = 0; i < count; ++i) {
    Move move = random_legal_move(position);

    if (move == MoveNone) {
      return false;
    }

    ss_push(position, thread_info, move);
    make_move(position, move);
  }

  return true;
}

bool setup_position(Position &position, ThreadInfo &thread_info) {
  if (use_openings && !openings.empty()) {
    const std::string &opening = openings[random_index(openings.size())];
    set_board(position, thread_info, opening);

    return play_random_moves(position, thread_info, 4 + random_int(2));
  }

  set_board(position, thread_info, kStartFen);
  return play_random_moves(position, thread_info, 8 + random_int(2));
}

int get_multipv_threshold(int game_ply) {
  if (game_ply >= 30) {
    return 0;
  }

  return static_cast<int>(80.0 * std::pow(0.9, game_ply - 5));
}

void print_progress(uint64_t num_fens, int id) {
  if (id != 0 || num_fens % 1000 != 0) {
    return;
  }

  const uint64_t total_fens = num_fens * static_cast<uint64_t>(num_threads);
  int64_t elapsed_ms = static_cast<int64_t>(time_elapsed(start_time));

  if (elapsed_ms <= 0) {
    elapsed_ms = 1;
  }

  std::printf("~%" PRIu64 " positions written\n", total_fens);
  std::printf("Approximate speed: %" PRIi64 " pos/s\n\n",
              static_cast<int64_t>(total_fens * 1000 / elapsed_ms));

  if (total_fens > kFenLimit) {
    std::exit(EXIT_SUCCESS);
  }
}

void write_fens(const std::vector<std::string> &fens, double result, int id) {
  if (fens.empty()) {
    return;
  }

  const std::string filename = "data" + std::to_string(id) + ".txt";
  std::ofstream output(filename, std::ios::out | std::ios::app);

  if (!output) {
    std::fprintf(stderr, "The output file could not be opened: %s\n",
                 filename.c_str());
    return;
  }

  output << std::fixed << std::setprecision(1);

  for (const std::string &fen : fens) {
    output << fen << result << '\n';
  }
}

void play_game(ThreadInfo &thread_info,
               uint64_t &num_fens,
               int id,
               std::vector<TTBucket> &tt) {
  new_game(thread_info, tt);

  Position position{};

  if (!setup_position(position, thread_info)) {
    return;
  }

  thread_info.multipv = 1;
  thread_info.start_time = std::chrono::steady_clock::now();
  search_position(position, thread_info, tt);

  if (std::abs(thread_info.best_scores[0]) > 400) {
    return;
  }

  std::vector<std::string> fens;
  fens.reserve(kMaxFenBuffer);

  double result = 0.5;

  while (result == 0.5 && !is_draw(position, thread_info)) {
    const bool color = position.color;
    const std::string fen = export_fen(position, thread_info);
    const bool in_check =
        attacks_square(position, get_king_pos(position, color), color ^ 1);

    if (!has_legal_move(position)) {
      if (in_check) {
        result = color ? 1.0 : 0.0;
      }

      break;
    }

    const int game_ply = static_cast<int>(thread_info.game_ply);
    const int threshold = get_multipv_threshold(game_ply);
    const int roll = random_int(100);

    thread_info.multipv =
        (roll > threshold && threshold > kQuietThresholdLimit) ? 2 : 1;

    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, tt);

    int score = thread_info.best_scores[0];
    Move best_move = thread_info.best_moves[0];

    if (thread_info.multipv > 1 && thread_info.best_moves[1] != MoveNone) {
      score = thread_info.best_scores[1];
      best_move = thread_info.best_moves[1];
    }

    thread_info.multipv = 1; // ????

    if (color) {
      score *= -1;
    }

    if (std::abs(score) > 1000) {
      result = score > 0 ? 1.0 : 0.0;
      break;
    }

    if (game_ply > 200) {
      if (score < -200) {
        result = 0.0;
      } else if (score > 200) {
        result = 1.0;
      }

      break;
    }

    if (best_move == MoveNone) {
      std::printf("%" PRIu64 "\n", static_cast<uint64_t>(thread_info.nodes));
      print_board(position);

      thread_info.doing_datagen = false;
      search_position(position, thread_info, tt);

      std::exit(EXIT_FAILURE);
    }

    const bool is_noisy = is_cap(position, best_move);

    ss_push(position, thread_info, best_move);
    make_move(position, best_move);

    if (!is_noisy && !in_check && threshold < kQuietThresholdLimit) {
      if (fens.size() >= kMaxFenBuffer) {
        std::fprintf(stderr, "%zu %i\n", fens.size(),
                     static_cast<int>(thread_info.game_ply));
        std::exit(EXIT_FAILURE);
      }

      fens.push_back(fen + " | " + std::to_string(score) + " | ");
      ++num_fens;

      print_progress(num_fens, id);
    }
  }

  write_fens(fens, result, id);
}

void run(int id) {
  std::vector<TTBucket> tt(TT_size);
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  uint64_t num_fens = 0;

  thread_info->doing_datagen = true;
  thread_info->multipv = 1;
  thread_info->opt_time = UINT32_MAX / 2;
  thread_info->max_time = UINT32_MAX / 2;
  thread_info->opt_nodes_searched = 5000;
  thread_info->max_nodes_searched = 50000;

  while (true) {
    play_game(*thread_info, num_fens, id, tt);
  }
}

int parse_num_threads(const char *value) {
  char *end = nullptr;
  const long parsed = std::strtol(value, &end, 10);

  if (end == value || parsed <= 0) {
    return 1;
  }

  return static_cast<int>(parsed);
}

} 

int main(int argc, char *argv[]) {
  init_LMR();
  init_bbs();

  thread_data.is_frc = true;

  if (argc > 1) {
    num_threads = parse_num_threads(argv[1]);
  }

  if (argc > 2) {
    load_openings(argv[2]);
  }

  start_time = std::chrono::steady_clock::now();

  std::vector<std::thread> threads;
  threads.reserve(static_cast<std::size_t>(num_threads));

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(run, i);
  }

  for (std::thread &thread : threads) {
    thread.join();
  }

  return 0;
}
