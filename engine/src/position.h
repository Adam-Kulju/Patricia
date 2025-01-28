#pragma once
#include "bitboard.h"
#include "utils.h"
#include <cctype>
#include <cstring>
#include <sstream>

int16_t total_mat(const Position &position) {
  int m = (position.material_count[0] + position.material_count[1]) * 100 +
          (position.material_count[2] + position.material_count[3]) * 300 +
          (position.material_count[4] + position.material_count[5]) * 300 +
          (position.material_count[6] + position.material_count[7]) * 500 +
          (position.material_count[8] + position.material_count[9]) * 900;

  return m;
}

std::string
internal_to_uci(const Position &position,
                Move move) { // Converts an internal move into a uci move.

  int from = extract_from(move), to = extract_to(move),
      promo = extract_promo(move);

  if (extract_type(move) == MoveTypes::Castling && !thread_data.is_frc) {
    if (get_file(to) == 0) {
      to += 2;
    } else {
      to--;
    }
  }

  std::string uci{};
  uci += get_file(from) + 'a';
  uci += get_rank(from) + '1';

  uci += get_file(to) + 'a';
  uci += get_rank(to) + '1';

  if (extract_type(move) == MoveTypes::Promotion) {
    uci += "nbrq"[promo];
  }

  return uci;
}

int get_king_pos(const Position &position, int color) {
  return get_lsb(position.colors_bb[color] &
                 position.pieces_bb[PieceTypes::King]);
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

  for (int i = 0; i < 2; i++) {
    for (int n = 0; n < 2; n++) {
      position.castling_squares[i][n] = SquareNone;
    }
  }

  for (char right : castling_rights) { // Set castling rights
    if (right == '-') {
      break;
    }

    int color = std::islower(right) ? Colors::Black : Colors::White;

    right = std::tolower(right);

    int square;
    int base = 56 * color;

    if (right == 'k') {
      square = base + 7;
    } else if (right == 'q') {
      square = base;
    } else {
      square = right - 'a' + 56 * color;
    }

    int side = square > get_king_pos(position, color) ? Sides::Kingside
                                                      : Sides::Queenside;
    position.castling_squares[color][side] = square;
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

// Does anyone attack the square
uint64_t attacks_square(const Position &position, int sq, uint64_t occ) {

  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];

  return (PawnAttacks[Colors::White][sq] & position.colors_bb[Colors::Black] &
          position.pieces_bb[PieceTypes::Pawn]) |
         (PawnAttacks[Colors::Black][sq] & position.colors_bb[Colors::White] &
          position.pieces_bb[PieceTypes::Pawn]) |
         (KnightAttacks[sq] & position.pieces_bb[PieceTypes::Knight]) |
         (get_bishop_attacks(sq, occ) & bishops) |
         (get_rook_attacks(sq, occ) & rooks) |
         (KingAttacks[sq] & position.pieces_bb[PieceTypes::King]);
}

uint64_t
br_attacks_square(const Position &position, int sq, int color,
                  uint64_t occ) { // Do we attack the square at position "sq"?

  uint64_t bishops = position.pieces_bb[PieceTypes::Bishop] |
                     position.pieces_bb[PieceTypes::Queen];
  uint64_t rooks = position.pieces_bb[PieceTypes::Rook] |
                   position.pieces_bb[PieceTypes::Queen];

  uint64_t attackers = (get_bishop_attacks(sq, occ) & bishops) |
                       (get_rook_attacks(sq, occ) & rooks);

  return attackers & position.colors_bb[color] & occ;
}

bool is_queen_promo(Move move) { return extract_promo(move) == 3; }

bool is_cap(const Position &position, Move &move) {
  if (extract_type(move) == MoveTypes::Castling) {
    return false;
  }
  int to = extract_to(move);
  return (position.board[to] ||
          (to == position.ep_square && position.board[extract_from(move)] ==
                                           Pieces::WPawn + position.color) ||
          is_queen_promo((move)));
}

void update_nnue_state(ThreadInfo &thread_info, Move move,
                       const Position &position) { // Updates the nnue state

  int from = extract_from(move), to = extract_to(move);
  int from_piece = position.board[from];
  int to_piece = from_piece, color = position.color;

  int phase = thread_info.phase;

  if (extract_type(move) == MoveTypes::Promotion) { // Grab promos
    to_piece = (extract_promo(move) + 2) * 2 + color;
  }

  int captured_piece = Pieces::Blank, captured_square = SquareNone;

  if (position.board[to]) {
    captured_piece = position.board[to], captured_square = to;
  }
  // en passant
  else if (extract_type(move) == MoveTypes::EnPassant) {
    captured_square = to + (color ? Directions::North : Directions::South);
    captured_piece = position.board[captured_square];
  }

  if (extract_type(move) ==
      MoveTypes::Castling) { // update the rook that moved if we castled

    int indx = color ? 56 : 0;
    int side = to > from;

    if (side) {
      to = indx + 6;
      thread_info.nnue_state.add_add_sub_sub(
          from_piece, from, to, Pieces::WRook + color,
          position.castling_squares[color][side], indx + 5, phase);

    } else {
      to = indx + 2;
      thread_info.nnue_state.add_add_sub_sub(
          from_piece, from, to, Pieces::WRook + color,
          position.castling_squares[color][side], indx + 3, phase);
    }
  }

  else if (captured_piece) {

    if (thread_info.phase == PhaseTypes::Middlegame &&
        total_mat(position) - MaterialValues[get_piece_type(captured_piece)] <
            PhaseBound) {
      thread_info.nnue_state.reset_and_add_sub_sub(
          position, from_piece, from, to_piece, to, captured_piece,
          captured_square, PhaseTypes::Endgame);

      thread_info.phase = PhaseTypes::Endgame;

    }

    else {
      thread_info.nnue_state.add_sub_sub(from_piece, from, to_piece, to,
                                         captured_piece, captured_square,
                                         phase);
    }
  }

  else {
    thread_info.nnue_state.add_sub(from_piece, from, to_piece, to, phase);
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
  uint64_t temp_pawns = position.pawn_key;
  uint64_t non_pawn_white = position.non_pawn_key[Colors::White],
           non_pawn_black = position.non_pawn_key[Colors::Black];

  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1, captured_piece = Pieces::Blank,
      captured_square = SquareNone;
  int base_rank = (color ? a8 : 0);
  int ep_square = SquareNone;

  int from_piece = position.board[from];
  int from_type = get_piece_type(from_piece);

  int king_pos = get_king_pos(position, color);
  int side = to > king_pos;

  if (extract_type(move) == MoveTypes::Castling) {
    to = base_rank + 2 + (side)*4;
  }

  // update material counts and 50 move rules for a capture
  else if (position.board[to]) {
    // Update hash key for the piece that was taken
    position.halfmoves = 0;
    position.material_count[position.board[to] - 2]--;
    captured_piece = position.board[to], captured_square = to;

    temp_hash ^= zobrist_keys[get_zobrist_key(captured_piece, captured_square)];

    if (get_piece_type(captured_piece) == PieceTypes::Pawn) {
      temp_pawns ^=
          zobrist_keys[get_zobrist_key(captured_piece, captured_square)];
    }

    else {
      position.non_pawn_key[color ^ 1] ^=
          zobrist_keys[get_zobrist_key(captured_piece, captured_square)];
    }

  }
  // en passant
  else if (extract_type(move) == MoveTypes::EnPassant) {
    position.material_count[opp_color]--;
    captured_square = to + (color ? Directions::North : Directions::South);
    captured_piece = position.board[captured_square];

    temp_hash ^= zobrist_keys[get_zobrist_key(position.board[captured_square],
                                              captured_square)];
    temp_pawns ^= zobrist_keys[get_zobrist_key(position.board[captured_square],
                                               captured_square)];
    // Update hash key for the piece that was taken
    // (not covered above)
    position.board[captured_square] = Pieces::Blank;
  }

  // Move the piece

  position.board[from] = Pieces::Blank;
  position.board[to] = from_piece;

  int to_piece = position.board[to];

  // handle promotions and double pawn pushes

  if (from_type == PieceTypes::Pawn) {
    position.halfmoves = 0;

    // promotions
    if (extract_type(move) == MoveTypes::Promotion) {
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

    if (extract_type(move) == MoveTypes::Castling) {
      int rook_to, rook_from;
      if (side) {
        rook_to = base_rank + 5;
        rook_from = position.castling_squares[color][Sides::Kingside];
      } else {
        rook_to = base_rank + 3;
        rook_from = position.castling_squares[color][Sides::Queenside];
      }

      if (position.board[rook_from] == Pieces::WRook + color) {
        position.board[rook_from] = Pieces::Blank;
      }
      position.board[rook_to] = Pieces::WRook + color;

      temp_hash ^=
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, rook_to)] ^
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, rook_from)];

      position.non_pawn_key[color] ^=
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, rook_to)] ^
          zobrist_keys[get_zobrist_key(Pieces::WRook + color, rook_from)];

      update_bb(position, Pieces::WRook + color, rook_from,
                Pieces::WRook + color, rook_to, Pieces::Blank, SquareNone);
    }

    // If the king moves, castling rights are gone.
    if (position.castling_squares[color][Sides::Queenside] != SquareNone) {
      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Queenside];
      position.castling_squares[color][Sides::Queenside] = SquareNone;
    }

    if (position.castling_squares[color][Sides::Kingside] != SquareNone) {
      temp_hash ^= zobrist_keys[castling_index + color * 2 + Sides::Kingside];
      position.castling_squares[color][Sides::Kingside] = SquareNone;
    }
  }

  // If we've moved a piece from our starting rook squares, set castling on that
  // side to false, because if it's not a rook it means either the rook left
  // that square or the king left its original square.
  if (from == position.castling_squares[color][Sides::Queenside] ||
      from == position.castling_squares[color][Sides::Kingside]) {

    int side = from < king_pos ? Sides::Queenside : Sides::Kingside;

    if (position.castling_squares[color][side] != SquareNone) {
      position.castling_squares[color][side] = SquareNone;
      temp_hash ^= zobrist_keys[castling_index + color * 2 + side];
    }
  }
  // If we've moved a piece onto one of the opponent's starting rook square, set
  // their castling to false, because either we just captured it, the rook
  // already moved, or the opposing king moved.

  if (to == position.castling_squares[opp_color][Sides::Queenside] ||
      to == position.castling_squares[opp_color][Sides::Kingside]) {

    int side = to < get_king_pos(position, opp_color) ? Sides::Queenside
                                                      : Sides::Kingside;
    if (position.castling_squares[opp_color][side] != SquareNone) {
      position.castling_squares[opp_color][side] = SquareNone;
      temp_hash ^= zobrist_keys[castling_index + opp_color * 2 + side];
    }
  }

  temp_hash ^= zobrist_keys[get_zobrist_key(from_piece, from)];
  temp_hash ^= zobrist_keys[get_zobrist_key(to_piece, to)];

  if (get_piece_type(from_piece) == PieceTypes::Pawn) {
    temp_pawns ^= zobrist_keys[get_zobrist_key(from_piece, from)];
    if (get_piece_type(to_piece) == PieceTypes::Pawn) {
      temp_pawns ^= zobrist_keys[get_zobrist_key(to_piece, to)];
    } else {
      position.non_pawn_key[color] ^=
          zobrist_keys[get_zobrist_key(to_piece, to)];
    }
  }

  else {
    position.non_pawn_key[color] ^=
        zobrist_keys[get_zobrist_key(from_piece, from)];
    position.non_pawn_key[color] ^= zobrist_keys[get_zobrist_key(to_piece, to)];
  }

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
  position.pawn_key = temp_pawns;

  __builtin_prefetch(&TT[hash_to_idx(temp_hash)]);
}

bool is_legal(const Position &position,
              Move move) { // Perform a move on the board.
  uint64_t occupied =
      position.colors_bb[Colors::White] | position.colors_bb[Colors::Black];
  int from = extract_from(move), to = extract_to(move), color = position.color,
      opp_color = color ^ 1;

  int from_piece = position.board[from];

  if (get_piece_type(from_piece) == PieceTypes::King) {
    if (extract_type(move) == MoveTypes::Castling) {
      to = 56 * color + 2 + (to > from) * 4;
    }
    return !attacks_square(position, to, opp_color, occupied ^ (1ull << from));
  }

  int king_pos = get_king_pos(position, color);

  // en passant
  if (extract_type(move) == MoveTypes::EnPassant) {
    int cap_square = to + (color ? Directions::North : Directions::South);
    return !br_attacks_square(position, king_pos, opp_color,
                              occupied ^ (1ull << from) ^ (1ull << to) ^
                                  (1ull << cap_square));
  }

  if (position.board[to]) {
    return !(br_attacks_square(position, king_pos, opp_color,
                               occupied ^ (1ull << from)) &
             ~(1ull << to));
  }

  return !br_attacks_square(position, king_pos, opp_color,
                            occupied ^ (1ull << from) ^ (1ull << to));
}