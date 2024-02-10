#pragma once
#include "utils.h"
#include <cstring>
#include <ctype.h>
#include <sstream>

std::string
internal_to_uci(Position position,
                Move move) { // Converts an internal move into a uci move.
  int from = extract_from(move), to = extract_to(move),
      promo = extract_promo(move);
  std::string uci = "     ";
  uci[0] = get_file(from) + 'a', uci[1] = get_rank(from) + '1',
  uci[2] = get_file(to) + 'a', uci[3] = get_rank(to) + '1', uci[4] = '\0';
  if (position.board[from] - position.color == Pieces::WPawn &&
      get_rank(to) == (position.color ? 0 : 7)) {
    std::string promos = "NBRQ";
    uci[4] = promos[promo], uci[5] = '\0';
  }
  return uci;
}

void print_board(
    Position position) { // Prints the board. Very helpful for debugging.
  for (int i = 0x70; i >= 0;) {
    printf("+---+---+---+---+---+---+---+---+\n");
    for (int n = i; n != i + 8; n++) {
      printf("| ");
      if (position.board[n] == Pieces::Blank) {
        printf("  ");
      } else {

        switch (position.board[n]) {
        case Pieces::WPawn:
          printf("P ");
          break;
        case Pieces::WKnight:
          printf("N ");
          break;
        case Pieces::WBishop:
          printf("B ");
          break;
        case Pieces::WRook:
          printf("R ");
          break;
        case Pieces::WQueen:
          printf("Q ");
          break;
        case Pieces::WKing:
          printf("K ");
          break;
        case Pieces::BPawn:
          printf("p ");
          break;
        case Pieces::BKnight:
          printf("n ");
          break;
        case Pieces::BBishop:
          printf("b ");
          break;
        case Pieces::BRook:
          printf("r ");
          break;
        case Pieces::BQueen:
          printf("q ");
          break;
        case Pieces::BKing:
          printf("k ");
          break;
        default:
          printf("# ");
        }
      }
    }
    printf("|\n");
    i -= 0x10;
  }
  printf("+---+---+---+---+---+---+---+---+\n\n");
}

void set_board(Position &position, ThreadInfo &thread_info,
               std::string f) { // Sets the board to a given fen.
  memset(&position, 0, sizeof(Position));
  std::istringstream fen(f);
  std::string fen_pos;
  fen >> fen_pos;
  int indx = 0;
  for (int i = 0x70; i >= 0 && fen_pos[indx] != '\0'; i++, indx++) {
    if (fen_pos[indx] == '/') {
      i -= 0x19; // the '/' denotes that we've reached the end of the file. If
                 // we're on the second file, our index is going to be 0x18.
                 // This needs to be at 0 next iteration for the first rank to
                 // be set up,
                 // so we subtract 0x19 and then the loop adds 1.
    } else if (isdigit(fen_pos[indx])) {
      i += fen_pos[indx] - '1';
    } else {
      switch (fen_pos[indx]) {
      case 'P':
        position.board[i] = Pieces::WPawn;
        position.material_count[0]++;
        break;
      case 'N':
        position.board[i] = Pieces::WKnight;
        position.material_count[2]++;
        break;
      case 'B':
        position.board[i] = Pieces::WBishop;
        position.material_count[4]++;
        break;
      case 'R':
        position.board[i] = Pieces::WRook;
        position.material_count[6]++;
        break;
      case 'Q':
        position.board[i] = Pieces::WQueen;
        position.material_count[8]++;
        break;
      case 'K':
        position.board[i] = Pieces::WKing;
        position.kingpos[Colors::White] = i;
        break;
      case 'p':
        position.board[i] = Pieces::BPawn;
        position.material_count[1]++;
        break;
      case 'n':
        position.board[i] = Pieces::BKnight;
        position.material_count[3]++;
        break;
      case 'b':
        position.board[i] = Pieces::BBishop;
        position.material_count[5]++;
        break;
      case 'r':
        position.board[i] = Pieces::BRook;
        position.material_count[7]++;
        break;
      case 'q':
        position.board[i] = Pieces::BQueen;
        position.material_count[9]++;
        break;
      case 'k':
        position.board[i] = Pieces::BKing;
        position.kingpos[Colors::Black] = i;
        break;
      default:
        printf("Error parsing FEN: %s\n", f.c_str());
        std::exit(1);
      }
    }
  }

  std::string color;
  fen >> color;
  if (color[0] == 'w') { // Set color
    position.color = Colors::White;
  } else {
    position.color = Colors::Black;
  }

  std::string castling_rights;
  fen >> castling_rights;

  indx = 0;
  for (char rights : "KQkq") { // Set castling rights
    if (castling_rights.find(rights) != std::string::npos) {
      position.castling_rights[indx > 1][!(indx % 2)] = true;
    }
    indx++;
  }

  std::string ep_square; // Set en passant square
  fen >> ep_square;
  if (ep_square[0] == '-') {
    position.ep_square = 255;
  } else {
    uint8_t file = (ep_square[0] - 'a');
    uint8_t rank = (ep_square[1] - '1');
    position.ep_square = rank * 16 + file;
  }
  int halfmoves;
  fen >> halfmoves;
  position.halfmoves = halfmoves;
  thread_info.game_ply = 0;
}

bool attacks_square(Position position, int sq,
                    int color) { // Do we attack the square at position "sq"?
  int opp_color = color ^ 1, indx = -1;

  for (int dirs : AttackRays) {
    indx++;
    int temp_pos = sq + dirs;

    while (!out_of_board(temp_pos)) {
      int piece = position.board[temp_pos];
      if (!piece) {
        temp_pos += dirs;
        continue;
      } else if (get_color(piece) == opp_color) {
        break;
      }

      piece -= color; // This statement lets us get the piece type without
                      // needing to account for color.

      if (piece == Pieces::WQueen || (piece == Pieces::WRook && indx < 4) ||
          (piece == Pieces::WBishop &&
           indx > 3)) { // A queen attack is a check from every direction, rooks
                        // and bishops only from orthogonal/diagonal directions
                        // respectively.
        return true;
      } else if (piece == Pieces::WPawn) {
        // Pawns and kings are only attackers if they're right next to the
        // square. Pawns additionally have to be on the right vector.
        if (temp_pos == sq + dirs &&
            (color ? (indx > 5) : (indx == 4 || indx == 5))) {
          return true;
        }
      } else if (piece == Pieces::WKing && temp_pos == sq + dirs) {
        return true;
      }
      break;
    }

    temp_pos = sq + KnightAttacks[indx]; // Check for knight attacks
    if (!out_of_board(temp_pos) &&
        position.board[temp_pos] - color == Pieces::WKnight) {
      return true;
    }
  }
  return false;
}

bool is_cap(Position &position, Move move) {
  int to = extract_to(move);
  return (position.board[to] ||
          (to == position.ep_square && position.board[extract_from(move)] ==
                                           Pieces::WPawn + position.color));
}

bool is_queen_promo(Move move) { return extract_promo(move) == 3; }

void update_nnue_state(NNUE_State &nnue_state, Move move, int from_piece,
                       int captured_piece, int captured_square,
                       bool color) {

  nnue_state.push();

  int from = extract_from(move), to = extract_to(move);
  int to_piece = from_piece;


  if (from_piece - color == Pieces::WPawn && get_rank(to) == (color ? 0 : 7)){
    
    to_piece = extract_promo(move) * 2 + 4 + color;
  }

  int to_square = to;
  from = MailboxToStandard_NNUE[from], to = MailboxToStandard_NNUE[to];

  nnue_state.update_feature<true>(to_piece, to);
  nnue_state.update_feature<false>(from_piece, from);


  if (captured_piece) {
    captured_square = MailboxToStandard_NNUE[captured_square];
    nnue_state.update_feature<false>(captured_piece, captured_square);
  }
  if (from_piece - color == Pieces::WKing &&
      abs(to - from) == Directions::East * 2) {

    int indx = color ? 0x70 : 0;
    if (get_file(to_square) > 4) {
      nnue_state.update_feature<true>(Pieces::WRook + color,
                                      MailboxToStandard_NNUE[indx + 5]);
      nnue_state.update_feature<false>(Pieces::WRook + color,
                                       MailboxToStandard_NNUE[indx + 7]);
    } else {
      nnue_state.update_feature<true>(Pieces::WRook + color,
                                      MailboxToStandard_NNUE[indx + 3]);
      nnue_state.update_feature<false>(Pieces::WRook + color,
                                       MailboxToStandard_NNUE[indx]);
    }
  }
}

int make_move(Position &position, Move move,
              ThreadInfo &thread_info, bool update_nnue) { // Perform a move on the board.

  uint64_t temp_hash = thread_info.zobrist_key;

  position.halfmoves++;
  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1, captured_piece = Pieces::Blank, captured_square = 255;
  int base_rank = (color ? 0x70 : 0);
  int ep_square = 255;

  int piece_from = position.board[from] - color;

  // update material counts and 50 move rules for a capture
  if (position.board[to]) {
    temp_hash ^=
        zobrist_keys[get_zobrist_key(position.board[to], standard(to))];
    // Update hash key for the piece that was taken
    position.halfmoves = 0;
    position.material_count[position.board[to] - 2]--;
    captured_piece = position.board[to], captured_square = to;
  }
  // en passant
  else if (piece_from == Pieces::WPawn && to == position.ep_square) {
    position.material_count[opp_color]--;
    captured_square = to + (color ? Directions::North : Directions::South);
    captured_piece = position.board[captured_square];


    temp_hash ^= zobrist_keys[get_zobrist_key(position.board[captured_square],
                                              standard(captured_square))];
    // Update hash key for the piece that was taken
    // (not covered above)
    position.board[captured_square] = Pieces::Blank;
  }

  if (piece_from == Pieces::WKing){
    position.kingpos[color] = to;
  }

  // Move the piece
  position.board[to] = position.board[from];
  position.board[from] = Pieces::Blank;
  

  if (attacks_square(position, position.kingpos[color], opp_color)) {
    return 1;
  }

  int piece_to = position.board[to];

  // handle promotions and double pawn pushes

  if (piece_from == Pieces::WPawn) {
    position.halfmoves = 0;

    // promotions
    if (get_rank(to) == (color ? 0 : 7)) {
      piece_to = extract_promo(move) * 2 + 4 + color;
      position.board[to] = piece_to;
      position.material_count[color]--, position.material_count[piece_to - 2]++;
    }

    // double pawn push
    else if (to == from + Directions::North * 2 ||
             to == from + Directions::South * 2) {
      ep_square = (to + from) / 2;
    }
  }
  // handle king moves and castling

  else if (piece_from == Pieces::WKing) {

    // If the king moves, castling rights are gone.
    if (position.castling_rights[color][Sides::Queenside]) {

      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Queenside];
      position.castling_rights[color][Sides::Queenside] = false;
    }

    if (position.castling_rights[color][Sides::Kingside]) {
      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Kingside];
      position.castling_rights[color][Sides::Kingside] = false;
    }

    int converted_rank = standard(base_rank);

    // kingside castle
    if (to == from + Directions::East + Directions::East) {
      position.board[base_rank + 5] = position.board[base_rank + 7];
      position.board[base_rank + 7] = Pieces::Blank;
      temp_hash ^= zobrist_keys[get_zobrist_key(Pieces::WRook + color,
                                                converted_rank + 5)] ^
                   zobrist_keys[get_zobrist_key(Pieces::WRook + color,
                                                converted_rank + 7)];
    }

    // queenside castle
    else if (to == from + Directions::West + Directions::West) {
      position.board[base_rank + 3] = position.board[base_rank];
      position.board[base_rank] = Pieces::Blank;
      temp_hash ^=
          zobrist_keys[get_zobrist_key(Pieces::WRook + color,
                                       converted_rank + 3)] ^
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, converted_rank)];
    }
  }

  // If we've moved a piece from our starting rook squares, set castling on that
  // side to false, because if it's not a rook it means either the rook left
  // that square or the king left its original square.
  if (from == base_rank || from == base_rank + 7) {
    int side = get_file(from) < 4 ? Sides::Queenside : Sides::Kingside;
    if (position.castling_rights[color][side]) {
      position.castling_rights[color][side] = false;
      temp_hash ^= zobrist_keys[castling_index + color * 2 + side];
    }
  }
  // If we've moved a piece onto one of the opponent's starting rook square, set
  // their castling to false, because either we just captured it, the rook
  // already moved, or the opposing king moved.
  else if (to == flip_sq(base_rank) || to == flip_sq(base_rank + 7)) {
    int side = get_file(to) < 4 ? Sides::Queenside : Sides::Kingside;
    if (position.castling_rights[opp_color][side]) {
      position.castling_rights[opp_color][side] = false;
      temp_hash ^= zobrist_keys[castling_index + opp_color * 2 + side];
    }
  }

  piece_from += color;

  // Update hash key for piece that was moved and color
  temp_hash ^=
      zobrist_keys[get_zobrist_key(piece_from, standard(from))];
  temp_hash ^= zobrist_keys[get_zobrist_key(piece_to, standard(to))];
  temp_hash ^= zobrist_keys[side_index];

  position.color ^= 1;

  if ((position.ep_square == 255) ^
      (ep_square ==
       255)) { // has the position's ability to perform en passant been changed?
    temp_hash ^= zobrist_keys[ep_index];
  }
  position.ep_square = ep_square;
  thread_info.zobrist_key = temp_hash;

  if (update_nnue){
    update_nnue_state(thread_info.nnue_state, move, piece_from, captured_piece, captured_square, color);
  }

  return 0;
}
