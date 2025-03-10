#include "../src/search.h"
#include <fstream>
#include <random>

int OpeningsSize = -1;
int OpeningsIndex = 0;

std::vector<std::string> openings;
bool use_openings = false;

void fill(std::string filename) {
  std::ifstream in(filename);
  std::string line;
  int i = 0;
  while (std::getline(in, line)) {
    openings.push_back(line);
    i++;
  }

  OpeningsSize = i;
  printf("%i openings found.\n", OpeningsSize);
  use_openings = true;
}

int num_threads = 1;
std::chrono::steady_clock::time_point start_time;

Move random_move(Position &position,
                 ThreadInfo &thread_info) { // Get a random legal move

  bool color = position.color;
  MoveInfo moves;
  int num_moves = legal_movegen(position, moves.moves);
  if (!num_moves) {
    return MoveNone;
  }
  return moves.moves[dist(rd) % (num_moves)];
}

void play_game(ThreadInfo &thread_info, uint64_t &num_fens, int id,
               std::vector<TTBucket> &TT) { // Plays a game, copying all "good"
                                            // FENs to a file.

  new_game(thread_info, TT);
  std::string opening = "";

  Position position;

  if (use_openings) {
    opening = openings[dist(rd) % OpeningsSize];
    set_board(position, thread_info, opening);
    int moves = 4 + (dist(rd) % 2);

    for (int i = 0; i < moves; i++) {
      Move move = random_move(position, thread_info);
      if (move == MoveNone) {
        return;
      }

      ss_push(position, thread_info, move);
      make_move(position, move);
    }
  }

  else {
    set_board(position, thread_info,
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int moves = 10 + (dist(rd) % 2);

    for (int i = 0; i < moves;
         i++) { // Perform 10-11 random moves to increase the scope of the data
      Move move = random_move(position, thread_info);
      if (move == MoveNone) {
        return;
      }

      ss_push(position, thread_info, move); // fill the game hist stack as we go
      make_move(position, move);
    }
  }

  /*set_board(position, thread_info,
  "rnbqkbnr/4pppp/8/p2P4/p1pP1PP1/7P/1PP5/RNBQKBNR b KQkq - 0 7");
  print_board(position);
  print_bbs(position);
  Move moves[256];
  int g = movegen(position, moves, 0);
  for (int i = 0; i < g; i++){
    printf("%i %i\n", extract_from(moves[i]), extract_to(moves[i]));
  }
  exit(0);*/

  std::string fens[5000] = {""};
  int fkey = 0;
  float result = 0.5;

  std::string filename = "data" + std::to_string(id) + ".txt";
  std::ofstream fr;
  fr.open(filename, std::ios::out | std::ios::app);

  while (result == 0.5 && !is_draw(position, thread_info)) {

    int repeats = 1;

    bool color = position.color;
    std::string fen = export_fen(position, thread_info);
    bool in_check =
        attacks_square(position, get_king_pos(position, color), color ^ 1);

    if (random_move(position, thread_info) == MoveNone) {
      break;
    }

    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, TT);

    int score = thread_info.best_scores[0];

    int s = score;

    Move best_move = thread_info.best_moves[0];

    if (color) {
      score *= -1;
    }
    if (abs(score) > 1000) { // If one side is completely winning, break
      if (score > 0) {
        result = 1;
      } else {
        result = 0;
      }
      break;
    }

    else if (thread_info.game_ply > 200) {
      if (score < -200) {
        result = 0;
      } else if (score > 200) {
        result = 1;
      }
      break;
    }

    if (best_move == MoveNone) {
      printf("%lu\n", thread_info.nodes);
      print_board(position);
      thread_info.doing_datagen = false;
      search_position(position, thread_info, TT);
      std::exit(1);
    }

    bool is_noisy = is_cap(position, best_move);

    ss_push(position, thread_info,
            best_move); // fill the game hist stack as we go

    make_move(position, best_move);

    if (!(is_noisy ||
          in_check)) { // If the best move isn't a noisy move and we're not in
      // check, add the position to the ones to write to a file
      if (fkey > 4999) {
        printf("%i %i\n", fkey, thread_info.game_ply);
        exit(0);
      }

      for (int i = 0; i < repeats; i++) {
        fens[fkey++] = fen + " | " + std::to_string(score) + " | ";
      }

      num_fens++;
      if (num_fens % 1000 == 0 && id == 0) {
        uint64_t total_fens = num_fens * num_threads; // Approximation
        printf("~%li positions written\n", total_fens);
        printf("Approximate speed: %" PRIi64 " pos/s\n\n",
               (int64_t)(total_fens * 1000 / time_elapsed(start_time)));
      }
    }
  }

  char res[4];

  sprintf(res, "%.1f", result);
  for (int i = 0; i < fkey; i++) { // Write all data to the output file
    fr << fens[i] << result << std::endl;
  }
  fr.close();
  return;
}

void run(int id) {

  std::vector<TTBucket> TT(TT_size);
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  uint64_t num_fens = 0;
  thread_info->doing_datagen = true;
  thread_info->opt_time = UINT32_MAX / 2;
  thread_info->max_time = UINT32_MAX / 2;
  thread_info->opt_nodes_searched = 5000;
  thread_info->max_nodes_searched = 50000;

  start_time = std::chrono::steady_clock::now();

  while (true) {
    play_game(*thread_info, num_fens, id, TT);
  }
}

int main(int argc, char *argv[]) {

  init_LMR();
  init_bbs();

  thread_data.is_frc = true;

  if (argc > 1) {
    num_threads = std::atoi(argv[1]);
  }

  if (argc > 2) {
    fill(std::string(argv[2]));
  }

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(run, i);
  }

  for (auto &t : threads) {
    t.join();
  }

  return 0;
}
