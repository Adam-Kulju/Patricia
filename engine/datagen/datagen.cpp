#include "../src/search.h"
#include <fstream>

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
  uint64_t hash = thread_info.zobrist_key;

  bool in_check = attacks_square(position, position.kingpos[color], color ^ 1);
  MoveInfo moves;
  int num_moves = movegen(position, moves.moves, in_check);

  MoveInfo legal_moves;

  int num_legal = 0;
  for (int i = 0; i < num_moves; i++) {
    Position moved_position = position;
    Move move = moves.moves[i];

    if (!make_move(moved_position, move, thread_info, false)) {
      thread_info.zobrist_key = hash;
      legal_moves.moves[num_legal++] = move;
    }
  }
  if (!num_legal) {
    return MoveNone;
  }
  int rand_index = rand() % (num_legal);
  return legal_moves.moves[rand_index];
}

void play_game(ThreadInfo &thread_info, uint64_t &num_fens, int id,
               std::vector<TTEntry> &TT) { // Plays a game, copying all "good"
                                           // FENs to a file.

  new_game(thread_info, TT);
  Position position;
  set_board(position, thread_info,
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  int moves = 6 + (rand() % 2);

  for (int i = 0; i < moves;
       i++) { // Perform 6-7 random moves to increase the scope of the data
    Move move = random_move(position, thread_info);
    if (move == MoveNone) {
      return;
    }
    make_move(position, move, thread_info, false);
  }

  int decisive_flag = 0;
  std::string fens[1000] = {""};
  int fkey = 0;
  float result = 0.5;

  std::string filename = "data" + std::to_string(id) + ".txt";
  std::ofstream fr;
  fr.open(filename, std::ios::out | std::ios::app);

  while (result == 0.5 && thread_info.game_ply < 900 &&
         !is_draw(position, thread_info)) {

    bool color = position.color;
    std::string fen = export_fen(position, thread_info);
    bool in_check =
        attacks_square(position, position.kingpos[color], color ^ 1);

    if (random_move(position, thread_info) == MoveNone) {
      break;
    }

    thread_info.start_time = std::chrono::steady_clock::now();
    search_position(position, thread_info, TT);

    int score = thread_info.score;
    Move best_move = thread_info.best_move;

    if (color) {
      score *= -1;
    }
    if (abs(score) > 1000) { // If one side is completely winning, break
      if (score > 0) {
        result = 1;
      } else {
        score = 0;
      }
      break;
    }

    if (best_move == MoveNone) {
      print_board(position);
      thread_info.is_datagen = false;
      search_position(position, thread_info, TT);
      std::exit(1);
    }

    bool is_noisy = is_cap(position, best_move);

    make_move(position, best_move, thread_info, false);

    if (!(is_noisy ||
          in_check)) { // If the best move isn't a noisy move and we're not in
                       // check, add the position to the ones to write to a file
      fens[fkey++] = fen + " | " + std::to_string(score) + " | ";

      num_fens++;
      if (num_fens % 1000 == 0 && id == 0) {
        uint64_t total_fens = num_fens * num_threads; // Approximation
        printf("~%" PRIu64 " positions written\n", total_fens);
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

  std::vector<TTEntry> TT(TT_size);
  std::unique_ptr<ThreadInfo> thread_info = std::make_unique<ThreadInfo>();

  uint64_t num_fens = 0;
  thread_info->is_datagen = true;
  thread_info->opt_nodes_searched = 5000;
  thread_info->max_nodes_searched = 50000;
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

   std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back(run, i);
  }

  for (auto &t : threads){
    t.join();
  }

  return 0;
}
