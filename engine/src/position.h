#pragma once
#include "bitboard.h"
#include "utils.h"
#include <cctype>
#include <cstring>
#include <sstream>

std::string
internal_to_uci(const Position &position,
                Move move) { // Converts an internal move into a uci move.

  int from = extract_from(move), to = extract_to(move),
      promo = extract_promo(move);

  std::string uci{};
  uci += get_file(from) + 'a';
  uci += get_rank(from) + '1';

  uci += get_file(to) + 'a';
  uci += get_rank(to) + '1';

  if (position.board[from] - position.color == Pieces::WPawn &&
      get_rank(to) == (position.color ? 0 : 7)) {
    uci += "nbrq"[promo];
  }

  return uci;
}

Move uci_to_internal(std::string uci) {
  // Converts a uci move into an internal move.

  int from_file = uci[0] - 'a', from_rank = uci[1] - '1',
      to_file = uci[2] - 'a', to_rank = uci[3] - '1', promo = 0;
  if (uci[4] != '\0') {
    promo = std::string("nbrq").find(uci[4]);
  }
  return pack_move((from_rank * 8 + from_file), (to_rank * 8 + to_file), promo);
}

void print_board(
    Position position) { // Prints the board. Very helpful for debugging.
  for (int i = 56; i >= 0;) {
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
    i -= 8;
  }
  printf("+---+---+---+---+---+---+---+---+\n\n");
}

void set_board(Position &position, ThreadInfo &thread_info,
               std::string f) { // Sets the board to a given fen.
  std::memset(&position, 0, sizeof(Position));

  generate_bb(f, position);

  std::istringstream fen(f);
  std::string fen_pos;
  fen >> fen_pos;
  int indx = 0;
  for (int i = 56; i >= 0 && fen_pos[indx] != '\0'; i++, indx++) {
    if (fen_pos[indx] == '/') {
      i -= 17; // the '/' denotes that we've reached the end of the file. If
               // we're on the second file, our index is going to be 16.
               // This needs to be at 0 next iteration for the first rank to
               // be set up,
               // so we subtract 17 and then the loop adds 1.
    } else if (std::isdigit(fen_pos[indx])) {
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

      int color = indx > 1 ? Colors::Black : Colors::White;
      int side = indx % 2 == 0 ? Sides::Kingside : Sides::Queenside;

      position.castling_rights[color][side] = true;
    }
    indx++;
  }

  std::string ep_square; // Set en passant square
  fen >> ep_square;
  if (ep_square[0] == '-') {
    position.ep_square = SquareNone;
  } else {
    uint8_t file = (ep_square[0] - 'a');
    uint8_t rank = (ep_square[1] - '1');
    position.ep_square = rank * 8 + file;
  }

  int halfmoves;
  fen >> halfmoves;

  if (!fen) {
    return;
  }

  position.halfmoves = halfmoves;
}

uint64_t
attacks_square(const Position &position, int sq,
               int color) { // Do we attack the square at position "sq"?

  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];
  uint64_t occ =
      position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];

  uint64_t attackers =
      (PawnAttacks[color ^ 1][sq] & position.pieces_bb[PieceTypes::Pawn]) |
      (KnightAttacks[sq] & position.pieces_bb[PieceTypes::Knight]) |
      (get_bishop_attacks(sq, occ) & bishops) |
      (get_rook_attacks(sq, occ) & rooks) |
      (KingAttacks[sq] & position.pieces_bb[PieceTypes::King]);

  return attackers & position.colors_bb[color];
}

uint64_t
attacks_square(const Position &position, int sq, int color,
               uint64_t occ) { // Do we attack the square at position "sq"?

  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];

  uint64_t attackers =
      (PawnAttacks[color ^ 1][sq] & position.pieces_bb[PieceTypes::Pawn]) |
      (KnightAttacks[sq] & position.pieces_bb[PieceTypes::Knight]) |
      (get_bishop_attacks(sq, occ) & bishops) |
      (get_rook_attacks(sq, occ) & rooks) |
      (KingAttacks[sq] & position.pieces_bb[PieceTypes::King]);

  return attackers & position.colors_bb[color] & occ;
}

uint64_t
br_attacks_square(const Position &position, int sq, int color,
               uint64_t occ) { // Do we attack the square at position "sq"?

  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];

  uint64_t attackers =
      (get_bishop_attacks(sq, occ) & bishops) |
      (get_rook_attacks(sq, occ) & rooks);

  return attackers & position.colors_bb[color] & occ;
}

bool is_queen_promo(Move move) { return extract_promo(move) == 3; }

bool is_cap(const Position &position, Move &move) {
  int to = extract_to(move);
  return (position.board[to] ||
          (to == position.ep_square && position.board[extract_from(move)] ==
                                           Pieces::WPawn + position.color) ||
          is_queen_promo((move)));
}

int get_king_pos(const Position &position, int color) {
  return get_lsb(position.colors_bb[color] &
                 position.pieces_bb[PieceTypes::King]);
}

void update_nnue_state(NNUE_State &nnue_state, Move move,
                       const Position &position) { // Updates the nnue state

  int from = extract_from(move), to = extract_to(move);
  int from_piece = position.board[from];
  int to_piece = from_piece, color = position.color;

  if (get_piece_type(from_piece) == PieceTypes::Pawn &&
      get_rank(to) == (color ? 0 : 7)) { // Grab promos

    to_piece = extract_promo(move) * 2 + 4 + color;
  }

  int captured_piece = Pieces::Blank, captured_square = SquareNone;

  if (position.board[to]) {
    captured_piece = position.board[to], captured_square = to;
  }
  // en passant
  else if (get_piece_type(from_piece) == PieceTypes::Pawn &&
           to == position.ep_square) {
    captured_square = to + (color ? Directions::North : Directions::South);
    captured_piece = position.board[captured_square];
  }

  int to_square = to;

  if (captured_piece) {
    nnue_state.add_sub_sub(from_piece, from, to_piece, to, captured_piece,
                           captured_square);
  }

  else if (get_piece_type(from_piece) == PieceTypes::King &&
           abs(to - from) ==
               Directions::East *
                   2) { // update the rook that moved if we castled

    int indx = color ? 56 : 0;

    if (get_file(to_square) > 4) {

      nnue_state.add_add_sub_sub(from_piece, from, to, Pieces::WRook + color,
                                 indx + 7, indx + 5);

    } else {

      nnue_state.add_add_sub_sub(from_piece, from, to, Pieces::WRook + color,
                                 indx, indx + 3);
    }
  }

  else {
    nnue_state.add_sub(from_piece, from, to_piece, to);
  }
}

void make_move(Position &position, Move move) { // Perform a move on the board.

  position.halfmoves++;

  if (move == MoveNone) {
    position.color ^= 1;
    if (position.ep_square != SquareNone) {
      position.zobrist_key ^= zobrist_keys[ep_index];
      position.ep_square = SquareNone;
    }

    position.zobrist_key ^= zobrist_keys[side_index];
    return;
  }

  uint64_t temp_hash = position.zobrist_key;

  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1, captured_piece = Pieces::Blank,
      captured_square = SquareNone;
  int base_rank = (color ? 56 : 0);
  int ep_square = SquareNone;

  int from_piece = position.board[from];
  int from_type = get_piece_type(from_piece);

  // update material counts and 50 move rules for a capture
  if (position.board[to]) {
    temp_hash ^= zobrist_keys[get_zobrist_key(position.board[to], to)];
    // Update hash key for the piece that was taken
    position.halfmoves = 0;
    position.material_count[position.board[to] - 2]--;
    captured_piece = position.board[to], captured_square = to;
  }
  // en passant
  else if (from_type == PieceTypes::Pawn && to == position.ep_square) {
    position.material_count[opp_color]--;
    captured_square = to + (color ? Directions::North : Directions::South);
    captured_piece = position.board[captured_square];

    temp_hash ^= zobrist_keys[get_zobrist_key(position.board[captured_square],
                                              captured_square)];
    // Update hash key for the piece that was taken
    // (not covered above)
    position.board[captured_square] = Pieces::Blank;
  }

  // Move the piece
  position.board[to] = position.board[from];
  position.board[from] = Pieces::Blank;

  int to_piece = position.board[to];

  // handle promotions and double pawn pushes

  bool castle = false;

  if (from_type == PieceTypes::Pawn) {
    position.halfmoves = 0;

    // promotions
    if (get_rank(to) == (color ? 0 : 7)) {
      to_piece = extract_promo(move) * 2 + 4 + color;
      position.board[to] = to_piece;
      position.material_count[color]--, position.material_count[to_piece - 2]++;
    }

    // double pawn push
    else if (to == from + Directions::North * 2 ||
             to == from + Directions::South * 2) {
      ep_square = (to + from) / 2;
    }
  }
  // handle king moves and castling

  else if (from_type == PieceTypes::King) {

    // If the king moves, castling rights are gone.
    if (position.castling_rights[color][Sides::Queenside]) {

      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Queenside];
      position.castling_rights[color][Sides::Queenside] = false;
    }

    if (position.castling_rights[color][Sides::Kingside]) {
      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Kingside];
      position.castling_rights[color][Sides::Kingside] = false;
    }

    // kingside castle
    if (to == from + Directions::East + Directions::East) {
      castle = true;
      position.board[base_rank + 5] = position.board[base_rank + 7];
      position.board[base_rank + 7] = Pieces::Blank;
      temp_hash ^=
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, base_rank + 5)] ^
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, base_rank + 7)];

      update_bb(position, Pieces::WRook + color, base_rank + 7,
                Pieces::WRook + color, base_rank + 5, Pieces::Blank,
                SquareNone);
    }

    // queenside castle
    else if (to == from + Directions::West + Directions::West) {
      castle = true;
      position.board[base_rank + 3] = position.board[base_rank];
      position.board[base_rank] = Pieces::Blank;
      temp_hash ^=
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, base_rank + 3)] ^
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, base_rank)];

      update_bb(position, Pieces::WRook + color, base_rank,
                Pieces::WRook + color, base_rank + 3, Pieces::Blank,
                SquareNone);
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

  if (to == flip_sq(base_rank) || to == flip_sq(base_rank + 7)) {

    int side = get_file(to) < 4 ? Sides::Queenside : Sides::Kingside;
    if (position.castling_rights[opp_color][side]) {
      position.castling_rights[opp_color][side] = false;
      temp_hash ^= zobrist_keys[castling_index + opp_color * 2 + side];
    }
  }

  temp_hash ^= zobrist_keys[get_zobrist_key(from_piece, from)];
  temp_hash ^= zobrist_keys[get_zobrist_key(to_piece, to)];
  temp_hash ^= zobrist_keys[side_index];

  update_bb(position, from_piece, from, to_piece, to, captured_piece,
            captured_square);

  position.color ^= 1;

  if ((position.ep_square == SquareNone) ^
      (ep_square == SquareNone)) { // has the position's ability to perform en
                                   // passant been changed?
    temp_hash ^= zobrist_keys[ep_index];
  }
  position.ep_square = ep_square;
  position.zobrist_key = temp_hash;

  __builtin_prefetch(&TT[hash_to_idx(temp_hash)]);
}

bool is_legal(Position &position, Move move) { // Perform a move on the board.
  uint64_t occupied = position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];
  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1;

  int from_piece = position.board[from], to_piece = from_piece;

  if (get_piece_type(from_piece) == PieceTypes::King) {
    return ! attacks_square(position, to, opp_color, occupied ^ (1ull << from));
  }

  int king_pos = get_king_pos(position, color);

  // en passant
  if (get_piece_type(from_piece) == PieceTypes::Pawn &&
      to == position.ep_square) {
    int cap_square = to + (color ? Directions::North : Directions::South);
    return ! br_attacks_square(position, king_pos, opp_color,
              occupied ^ (1ull << from) ^ (1ull << to) ^ (1ull << cap_square));
  }

  int cap_piece = position.board[to];

  if (! cap_piece) {
    return ! br_attacks_square(position, king_pos, opp_color,
              occupied ^ (1ull << from) ^ (1ull << to));
  }

  position.colors_bb[color] += (1ull << to) - (1ull << from);
  position.colors_bb[color ^ 1] -= (1ull << to);

  bool is_king_attacked = br_attacks_square(position, king_pos, opp_color, 
                           position.colors_bb[Colors::White] | position.colors_bb[Colors::Black]);

  position.colors_bb[color] -= (1ull << to) - (1ull << from);
  position.colors_bb[color ^ 1] += (1ull << to);

  return !is_king_attacked;
}