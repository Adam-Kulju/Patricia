#include "../src/search.h"
#include <fstream>
#include <random>

int OpeningsSize;
std::vector<std::string> openings;
bool use_openings = false;

std::random_device rd;
std::uniform_int_distribution<int> dist(0, INT32_MAX);

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

std::string export_fen(const Position &position,
                       const ThreadInfo &thread_info) {

  int pos = 0x70;
  std::string fen = "";

  for (int pos = 0x70; pos >= 0; pos++) {
    if (out_of_board(pos)) {
      pos -= 0x19;
      if (pos >= -1) {
        fen += "/";
      }
    }

    else if (position.board[pos] != Pieces::Blank) {

      switch (position.board[pos]) {

      case Pieces::WPawn:
        fen += "P";
        break;
      case Pieces::WKnight:
        fen += "N";
        break;
      case Pieces::WBishop:
        fen += "B";
        break;
      case Pieces::WRook:
        fen += "R";
        break;
      case Pieces::WQueen:
        fen += "Q";
        break;
      case Pieces::WKing:
        fen += "K";
        break;
      case Pieces::BPawn:
        fen += "p";
        break;
      case Pieces::BKnight:
        fen += "n";
        break;
      case Pieces::BBishop:
        fen += "b";
        break;
      case Pieces::BRook:
        fen += "r";
        break;
      case Pieces::BQueen:
        fen += "q";
        break;
      case Pieces::BKing:
        fen += "k";
        break;
      default:
        printf("Error parsing board!");
        print_board(position);
        std::exit(1);
      }
    }

    else {
      int empty_squares = 0;

      do {
        empty_squares++;
        pos++;
      } while (position.board[pos] == Pieces::Blank && !out_of_board(pos));

      fen += std::to_string(empty_squares);
      pos--;
    }
  }

  fen += " ";

  if (position.color == Colors::Black) {
    fen += "b ";
  } else {
    fen += "w ";
  }

  bool has_castling_rights = false;
  int indx = 0;

  for (char rights : std::string("KQkq")) {

    int color = indx > 1 ? Colors::Black : Colors::White;
    int side = indx % 2 == 0 ? Sides::Kingside : Sides::Queenside;
    if (position.castling_rights[color][side]) {
      fen += rights;
      has_castling_rights = true;
    }

    indx++;
  }

  if (has_castling_rights) {
    fen += " ";
  } else {
    fen += "- ";
  }

  if (position.ep_square != SquareNone) {

    char file = get_file(position.ep_square) + 'a';
    fen += file;

    char rank = get_rank(position.ep_square) + '1';

    fen += rank;
    fen += " ";

  } else {
    fen += "- ";
  }

  fen += std::to_string(position.halfmoves) + " ";
  fen += std::to_string((thread_info.game_ply + 1) / 2);

  return fen;
}

Move random_move(Position &position,
                 ThreadInfo &thread_info) { // Get a random legal move

  bool color = position.color;

  bool in_check = attacks_square(position, position.kingpos[color], color ^ 1);
  MoveInfo moves;
  int num_moves = movegen(position, moves.moves, in_check);

  MoveInfo legal_moves;

  int num_legal = 0;
  for (int i = 0; i < num_moves; i++) {
    Position moved_position = position;
    Move move = moves.moves[i];

    if (!make_move(moved_position, move, thread_info, Updates::UpdateNone)) {
      legal_moves.moves[num_legal++] = move;
    }
  }
  if (!num_legal) {
    return MoveNone;
  }
  int rand_index = dist(rd) % (num_legal);
  return legal_moves.moves[rand_index];
}

void play_game(ThreadInfo &thread_info, uint64_t &num_fens, int id,
               std::vector<TTEntry> &TT) { // Plays a game, copying all "good"
                                           // FENs to a file.

  new_game(thread_info, TT);
  std::vector<TTEntry> TT2(TT_size);
  new_game(thread_info, TT2);

  Position position;

  int opening_num = dist(rd) % 10;
  if (opening_num == 9 && use_openings) {
    set_board(position, thread_info, openings[dist(rd) % (OpeningsSize - 1)]);
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

      ss_push(position, thread_info, move,
              thread_info.zobrist_key); // fill the game hist stack as we go
      make_move(position, move, thread_info, Updates::UpdateHash);
    }
  }

  std::string fens[5000] = {""};
  int fkey = 0;
  float result = 0.5;

  std::string filename = "data" + std::to_string(id) + ".txt";
  std::ofstream fr;
  fr.open(filename, std::ios::out | std::ios::app);

  int handicap = dist(rd) % 2;

  while (result == 0.5 && thread_info.game_ply < 900 &&
         !is_draw(position, thread_info)) {

    int repeats = 1;

    bool color = position.color;
    std::string fen = export_fen(position, thread_info);
    bool in_check =
        attacks_square(position, position.kingpos[color], color ^ 1);

    if (random_move(position, thread_info) == MoveNone) {
      break;
    }

    if (color == handicap) {
      thread_info.opt_nodes_searched = 1000;
      thread_info.max_nodes_searched = 10000;
    }

    else {
      thread_info.opt_nodes_searched = 9000;
      thread_info.max_nodes_searched = 90000;
    }

    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, color ? TT : TT2);

    int score = thread_info.score;

    int s = score;

    int s_eval = qsearch(ScoreNone, -ScoreNone, position, thread_info, color ? TT : TT2);
    int diff = abs(score - s_eval);

    if (diff > std::max(200, abs(s_eval / 2))) {
      repeats = 3;
      if (diff > std::max(300, abs(s_eval * 2 / 3))) {
        repeats = 6;
      }
    }

    Move best_move = thread_info.best_move;

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

    if (best_move == MoveNone) {
      print_board(position);
      thread_info.disable_print = false;
      search_position(position, thread_info, TT);
      std::exit(1);
    }

    bool is_noisy = is_cap(position, best_move);

    ss_push(position, thread_info, best_move,
            thread_info.zobrist_key); // fill the game hist stack as we go

    make_move(position, best_move, thread_info, Updates::UpdateHash);

    if (!(is_noisy ||
          in_check)) { // If the best move isn't a noisy move and we're not in
      // check, add the position to the ones to write to a file
      if (fkey > 999) {
        printf("%i %i\n", fkey, thread_info.game_ply);
        exit(0);
      }
      if (thread_info.score < -100000 || thread_info.score > 100000) {
        print_board(position);
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

        if (total_fens >= 51000000) {
          fr.close();
          exit(0);
        }
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

  std::vector<TTEntry> TT(TT_size);
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  uint64_t num_fens = 0;
  thread_info->disable_print = true;
  thread_info->opt_time = UINT32_MAX / 2;
  thread_info->max_time = UINT32_MAX / 2;

  start_time = std::chrono::steady_clock::now();

  while (true) {
    play_game(*thread_info, num_fens, id, TT);
  }
}

int main(int argc, char *argv[]) {

  init_LMR();

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
