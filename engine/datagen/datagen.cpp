#include "../src/search.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <random>
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
std::atomic<bool> stop_requested{false};
std::atomic<bool> generation_failed{false};
std::atomic<uint64_t> reserved_fens{0};
std::atomic<uint64_t> written_fens{0};
std::atomic<uint64_t> next_progress{1000};

std::mt19937_64 create_random_engine() {
  static std::atomic<uint64_t> seed_counter{0};

  std::random_device device;
  const uint64_t counter = seed_counter.fetch_add(1);
  const uint64_t timestamp = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const uint64_t thread_hash = static_cast<uint64_t>(
      std::hash<std::thread::id>{}(std::this_thread::get_id()));
  std::seed_seq seed{
      device(),
      device(),
      device(),
      device(),
      static_cast<uint32_t>(counter),
      static_cast<uint32_t>(counter >> 32),
      static_cast<uint32_t>(timestamp),
      static_cast<uint32_t>(timestamp >> 32),
      static_cast<uint32_t>(thread_hash),
      static_cast<uint32_t>(thread_hash >> 32)
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

void normalize_opening_line(std::string &line, bool first_line) {
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  if (first_line && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {
    line.erase(0, 3);
  }

  const std::size_t first = line.find_first_not_of(" \t\r");

  if (first == std::string::npos) {
    line.clear();
    return;
  }

  const std::size_t last = line.find_last_not_of(" \t\r");
  line = line.substr(first, last - first + 1);
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
  bool first_line = true;

  while (std::getline(input, line)) {
    normalize_opening_line(line, first_line);
    first_line = false;

    if (!line.empty()) {
      openings.push_back(line);
    }
  }

  if (input.bad()) {
    std::fprintf(stderr, "The openings file could not be read: %s\n",
                 filename.c_str());
    openings.clear();
    return false;
  }

  if (openings.empty()) {
    std::fprintf(stderr, "The openings file contains no usable positions: %s\n",
                 filename.c_str());
    return false;
  }

  std::printf("%zu openings found.\n", openings.size());

  use_openings = true;
  return true;
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

int get_multipv_threshold(uint64_t game_ply) {
  if (game_ply >= 30) {
    return 0;
  }

  return static_cast<int>(
      80.0 * std::pow(0.9, static_cast<double>(game_ply) - 5.0));
}

void print_progress(uint64_t total_fens) {
  uint64_t expected = next_progress.load();

  while (total_fens >= expected) {
    const uint64_t desired = (total_fens / 1000 + 1) * 1000;

    if (next_progress.compare_exchange_weak(expected, desired)) {
      break;
    }
  }

  if (total_fens < expected) {
    return;
  }

  int64_t elapsed_ms = static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - start_time)
          .count());

  if (elapsed_ms <= 0) {
    elapsed_ms = 1;
  }

  const uint64_t positions_per_second =
      total_fens * 1000 / static_cast<uint64_t>(elapsed_ms);

  std::printf("~%" PRIu64 " positions written\n", total_fens);
  std::printf("Approximate speed: %" PRIu64 " pos/s\n\n",
              positions_per_second);
}

std::size_t reserve_fen_slots(std::size_t requested) {
  if (requested == 0) {
    return 0;
  }

  uint64_t current = reserved_fens.load();

  while (current < kFenLimit) {
    const uint64_t available = kFenLimit - current;
    const uint64_t accepted = std::min<uint64_t>(requested, available);

    if (reserved_fens.compare_exchange_weak(current, current + accepted)) {
      return static_cast<std::size_t>(accepted);
    }
  }

  return 0;
}

bool write_fens(const std::vector<std::string> &fens, double result, int id) {
  if (fens.empty()) {
    return true;
  }

  const std::string filename = "data" + std::to_string(id) + ".txt";
  std::ofstream output(filename, std::ios::out | std::ios::app);

  if (!output) {
    std::fprintf(stderr, "The output file could not be opened: %s\n",
                 filename.c_str());
    return false;
  }

  output << std::fixed << std::setprecision(1);

  for (const std::string &fen : fens) {
    output << fen << result << '\n';
  }

  output.close();

  if (!output) {
    std::fprintf(stderr, "The output file could not be written: %s\n",
                 filename.c_str());
    return false;
  }

  return true;
}

void play_game(ThreadInfo &thread_info,
               int id,
               std::vector<TTBucket> &tt) {
  new_game(thread_info, tt);

  Position position{};

  if (!setup_position(position, thread_info)) {
    return;
  }

  if (!has_legal_move(position) || is_draw(position, thread_info)) {
    return;
  }

  thread_info.multipv = 1;
  thread_info.start_time = std::chrono::steady_clock::now();
  search_position(position, thread_info, tt);

  if (thread_info.best_moves[0] == MoveNone) {
    std::fprintf(stderr,
                 "Search returned no move in a non-terminal position on worker %d.\n",
                 id);
    std::fprintf(stderr, "Nodes searched: %" PRIu64 "\n",
                 static_cast<uint64_t>(thread_info.nodes));
    print_board(position);
    generation_failed.store(true);
    stop_requested.store(true);
    return;
  }

  if (thread_info.best_scores[0] < -400 ||
      thread_info.best_scores[0] > 400) {
    return;
  }

  std::vector<std::string> fens;
  fens.reserve(kMaxFenBuffer);

  double result = 0.5;

  while (result == 0.5) {
    if (stop_requested.load()) {
      return;
    }

    const bool color = position.color;
    const bool in_check =
        attacks_square(position, get_king_pos(position, color), color ^ 1);

    if (!has_legal_move(position)) {
      if (in_check) {
        result = color ? 1.0 : 0.0;
      }

      break;
    }

    if (is_draw(position, thread_info)) {
      break;
    }

    const std::string fen = export_fen(position, thread_info);
    const uint64_t game_ply = static_cast<uint64_t>(thread_info.game_ply);
    const int threshold = get_multipv_threshold(game_ply);
    const int roll = random_int(100);

    thread_info.multipv =
        (roll > threshold && threshold > kQuietThresholdLimit) ? 2 : 1;

    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, tt);

    int64_t score = thread_info.best_scores[0];
    Move best_move = thread_info.best_moves[0];

    if (thread_info.multipv > 1 && thread_info.best_moves[1] != MoveNone) {
      score = thread_info.best_scores[1];
      best_move = thread_info.best_moves[1];
    }

    thread_info.multipv = 1;

    if (best_move == MoveNone) {
      std::fprintf(stderr,
                   "Search returned no move in a non-terminal position on worker %d.\n",
                   id);
      std::fprintf(stderr, "Nodes searched: %" PRIu64 "\n",
                   static_cast<uint64_t>(thread_info.nodes));
      print_board(position);
      generation_failed.store(true);
      stop_requested.store(true);
      return;
    }

    if (color) {
      score = -score;
    }

    if (score < -1000 || score > 1000) {
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

    const bool is_noisy = is_cap(position, best_move);

    ss_push(position, thread_info, best_move);
    make_move(position, best_move);

    if (!is_noisy && !in_check && threshold <= kQuietThresholdLimit) {
      if (fens.size() >= kMaxFenBuffer) {
        std::fprintf(stderr,
                     "The position buffer limit was exceeded on worker %d at ply %" PRIu64 ".\n",
                     id,
                     static_cast<uint64_t>(thread_info.game_ply));
        generation_failed.store(true);
        stop_requested.store(true);
        return;
      }

      fens.push_back(fen + " | " + std::to_string(score) + " | ");
    }
  }

  const std::size_t slots = reserve_fen_slots(fens.size());

  if (slots == 0) {
    if (reserved_fens.load() >= kFenLimit) {
      stop_requested.store(true);
    }
    return;
  }

  fens.resize(slots);

  if (!write_fens(fens, result, id)) {
    generation_failed.store(true);
    stop_requested.store(true);
    return;
  }

  const uint64_t total_fens =
      written_fens.fetch_add(static_cast<uint64_t>(slots)) +
      static_cast<uint64_t>(slots);
  print_progress(total_fens);

  if (reserved_fens.load() >= kFenLimit) {
    stop_requested.store(true);
  }
}

void run(int id) {
  try {
    std::vector<TTBucket> tt(TT_size);
    std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

    thread_info->doing_datagen = true;
    thread_info->multipv = 1;
    thread_info->opt_time = std::numeric_limits<uint32_t>::max() / 2;
    thread_info->max_time = std::numeric_limits<uint32_t>::max() / 2;
    thread_info->opt_nodes_searched = 5000;
    thread_info->max_nodes_searched = 50000;

    while (!stop_requested.load()) {
      play_game(*thread_info, id, tt);
    }
  } catch (const std::exception &error) {
    std::fprintf(stderr, "Worker %d failed: %s\n", id, error.what());
    generation_failed.store(true);
    stop_requested.store(true);
  } catch (...) {
    std::fprintf(stderr, "Worker %d failed with an unknown error.\n", id);
    generation_failed.store(true);
    stop_requested.store(true);
  }
}

bool parse_num_threads(const char *value, int &parsed_threads) {
  if (value == nullptr) {
    return false;
  }

  errno = 0;
  char *end = nullptr;
  const long parsed = std::strtol(value, &end, 10);

  if (errno == ERANGE || end == value || *end != '\0' || parsed <= 0 ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }

  parsed_threads = static_cast<int>(parsed);
  return true;
}

}

int main(int argc, char *argv[]) {
  if (argc > 3) {
    std::fprintf(stderr, "Usage: %s [threads] [openings_file]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (argc > 1 && !parse_num_threads(argv[1], num_threads)) {
    std::fprintf(stderr, "Invalid thread count: %s\n", argv[1]);
    return EXIT_FAILURE;
  }

  try {
    if (argc > 2 && !load_openings(argv[2])) {
      return EXIT_FAILURE;
    }
  } catch (const std::exception &error) {
    std::fprintf(stderr, "The openings file could not be loaded: %s\n",
                 error.what());
    return EXIT_FAILURE;
  } catch (...) {
    std::fprintf(stderr,
                 "The openings file could not be loaded due to an unknown error.\n");
    return EXIT_FAILURE;
  }

  stop_requested.store(false);
  generation_failed.store(false);
  reserved_fens.store(0);
  written_fens.store(0);
  next_progress.store(1000);

  init_LMR();
  init_bbs();

  thread_data.is_frc = true;

  start_time = std::chrono::steady_clock::now();

  std::vector<std::thread> threads;

  try {
    threads.reserve(static_cast<std::size_t>(num_threads));

    for (int i = 0; i < num_threads; ++i) {
      threads.emplace_back(run, i);
    }
  } catch (const std::exception &error) {
    std::fprintf(stderr, "The worker threads could not be started: %s\n",
                 error.what());
    generation_failed.store(true);
    stop_requested.store(true);
  } catch (...) {
    std::fprintf(stderr,
                 "The worker threads could not be started due to an unknown error.\n");
    generation_failed.store(true);
    stop_requested.store(true);
  }

  for (std::thread &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  if (generation_failed.load()) {
    return EXIT_FAILURE;
  }

  std::printf("Generation complete: %" PRIu64 " positions written.\n",
              written_fens.load());
  return EXIT_SUCCESS;
}
