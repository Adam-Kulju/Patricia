#pragma once
#include "defs.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

enum Square : int { // a1 = 0. a8 = 7, etc. thus a1 is the LSB and h8 is the
                    // MSB.
  a1,
  b1,
  c1,
  d1,
  e1,
  f1,
  g1,
  h1,
  a2,
  b2,
  c2,
  d2,
  e2,
  f2,
  g2,
  h2,
  a3,
  b3,
  c3,
  d3,
  e3,
  f3,
  g3,
  h3,
  a4,
  b4,
  c4,
  d4,
  e4,
  f4,
  g4,
  h4,
  a5,
  b5,
  c5,
  d5,
  e5,
  f5,
  g5,
  h5,
  a6,
  b6,
  c6,
  d6,
  e6,
  f6,
  g6,
  h6,
  a7,
  b7,
  c7,
  d7,
  e7,
  f7,
  g7,
  h7,
  a8,
  b8,
  c8,
  d8,
  e8,
  f8,
  g8,
  h8,
  SqNone
};

constexpr std::array<uint64_t, 8> Ranks = {
    0xFFull,       0xFFull << 8,  0xFFull << 16, 0xFFull << 24,
    0xFFull << 32, 0xFFull << 40, 0xFFull << 48, 0xFFull << 56};

constexpr std::array<uint64_t, 8> Files = {
    0x101010101010101ull,      0x101010101010101ull << 1,
    0x101010101010101ull << 2, 0x101010101010101ull << 3,
    0x101010101010101ull << 4, 0x101010101010101ull << 5,
    0x101010101010101ull << 6, 0x101010101010101ull << 7};

namespace Directions_BB {
constexpr int8_t North = 8;
constexpr int8_t South = -8;
constexpr int8_t East = 1;
constexpr int8_t West = -1;
constexpr int8_t Northeast = 9;
constexpr int8_t Southeast = -7;
constexpr int8_t Northwest = 7;
constexpr int8_t Southwest = -9;
} // namespace Directions_BB

namespace Pieces_BB {
constexpr uint8_t PieceNone = 0;
constexpr uint8_t Pawn = 1;
constexpr uint8_t Knight = 2;
constexpr uint8_t Bishop = 3;
constexpr uint8_t Rook = 4;
constexpr uint8_t Queen = 5;
constexpr uint8_t King = 6;
} // namespace Pieces_BB

std::array<uint64_t, 64> RookMasks;
std::array<uint64_t, 64> BishopMasks;
MultiArray<uint64_t, 64, 512> BishopAttacks;
MultiArray<uint64_t, 64, 4096> RookAttacks;
MultiArray<uint64_t, 2, 64> PawnAttacks;
std::array<uint64_t, 64> KingAttacks;
std::array<uint64_t, 64> KnightAttacks;

constexpr std::array<uint64_t, 64> BishopMagics = {
    0x2020420401002200, 0x05210A020A002118, 0x1110040454C00484,
    0x1008095104080000, 0xC409104004000000, 0x0002901048080200,
    0x0044040402084301, 0x2002030188040200, 0x0000C8084808004A,
    0x1040040808010028, 0x40040C0114090051, 0x40004820802004C4,
    0x0010042420260012, 0x10024202300C010A, 0x000054013D101000,
    0x0100020482188A0A, 0x0120090421020200, 0x1022204444040C00,
    0x0008000400440288, 0x0008060082004040, 0x0044040081A00800,
    0x021200014308A010, 0x8604040080880809, 0x0000802D46009049,
    0x00500E8040080604, 0x0024030030100320, 0x2004100002002440,
    0x02090C0008440080, 0x0205010000104000, 0x0410820405004A00,
    0x8004140261012100, 0x0A00460000820100, 0x201004A40A101044,
    0x840C024220208440, 0x000C002E00240401, 0x2220A00800010106,
    0x88C0080820060020, 0x0818030B00A81041, 0xC091280200110900,
    0x08A8114088804200, 0x228929109000C001, 0x1230480209205000,
    0x0A43040202000102, 0x1011284010444600, 0x0003041008864400,
    0x0115010901000200, 0x01200402C0840201, 0x001A009400822110,
    0x2002111128410000, 0x8420410288203000, 0x0041210402090081,
    0x8220002442120842, 0x0140004010450000, 0xC0408860086488A0,
    0x0090203E00820002, 0x0820020083090024, 0x1040440210900C05,
    0x0818182101082000, 0x0200800080D80800, 0x32A9220510209801,
    0x0000901010820200, 0x0000014064080180, 0xA001204204080186,
    0xC04010040258C048};
constexpr std::array<uint64_t, 64> RookMagics = {
    0x5080008011400020, 0x0140001000402000, 0x0280091000200480,
    0x0700081001002084, 0x0300024408010030, 0x510004004E480100,
    0x0400044128020090, 0x8080004100012080, 0x0220800480C00124,
    0x0020401001C02000, 0x000A002204428050, 0x004E002040100A00,
    0x0102000A00041020, 0x0A0880040080C200, 0x0002000600018408,
    0x0025001200518100, 0x8900328001400080, 0x0848810020400100,
    0xC001410020010153, 0x4110C90020100101, 0x00A0808004004800,
    0x401080801C000601, 0x0100040028104221, 0x840002000900A054,
    0x1000348280004000, 0x001000404000E008, 0x0424410300200035,
    0x2008C22200085200, 0x0005304D00080100, 0x000C040080120080,
    0x8404058400080210, 0x0001848200010464, 0x6000204001800280,
    0x2410004003C02010, 0x0181200A80801000, 0x000C60400A001200,
    0x0B00040180802800, 0xC00A000280804C00, 0x4040080504005210,
    0x0000208402000041, 0xA200400080628000, 0x0021020240820020,
    0x1020027000848022, 0x0020500018008080, 0x10000D0008010010,
    0x0100020004008080, 0x0008020004010100, 0x12241C0880420003,
    0x4000420024810200, 0x0103004000308100, 0x008C200010410300,
    0x2410008050A80480, 0x0820880080040080, 0x0044220080040080,
    0x2040100805120400, 0x0129000080C20100, 0x0010402010800101,
    0x0648A01040008101, 0x0006084102A00033, 0x0002000870C06006,
    0x0082008820100402, 0x0012008410050806, 0x2009408802100144,
    0x821080440020810A};

int get_file(int square) { return square % 8; }
int get_rank(int square) { return square / 8; }

uint64_t file_bb(int square) { return Files[square % 8]; }
uint64_t rank_bb(int square) { return Ranks[square / 8]; }

int pop_count(uint64_t bb) { return __builtin_popcountll(bb); }

int get_lsb(uint64_t bb) { return __builtin_ctzll(bb); }

int pop_lsb(uint64_t &bb) {
  int s = get_lsb(bb);
  bb &= (bb - 1);
  return s;
}

void print_bb(uint64_t bb) {
  printf("\n");
  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      int sq = file + (rank << 3);
      printf("%i ", bool(bb & (1ull << sq)));
    }
    printf("\n");
  }
}

uint64_t set_occ(int idx, int size, uint64_t mask) {
  uint64_t occ = 0;

  for (int i = 0; i < size; i++) {

    int square = pop_lsb(mask);

    if (idx & (1 << i)) {
      occ |= (1ull << square);
    }
  }
  return occ;
}

uint64_t bishop_sliders(int square, uint64_t occ) {
  uint64_t bb = 0;

  int dirs_file[4] = {1, -1, 1, -1};
  int dirs_rank[4] = {1, 1, -1, -1};
  for (int i = 0; i < 4; i++) {
    int temp_file = get_file(square) + dirs_file[i];
    int temp_rank = get_rank(square) + dirs_rank[i];

    while (temp_file >= 0 && temp_file <= 7 && temp_rank >= 0 &&
           temp_rank <= 7) {
      int temp_sq = temp_file + (temp_rank * 8);
      bb |= (1ull << temp_sq);
      if (occ & (1ull << temp_sq)) {
        break;
      }

      temp_file += dirs_file[i];
      temp_rank += dirs_rank[i];
    }
  }

  return bb;
}

uint64_t rook_sliders(int square, uint64_t occ) {
  uint64_t bb = 0;

  int dirs_file[4] = {0, 0, 1, -1};
  int dirs_rank[4] = {-1, 1, 0, 0};
  for (int i = 0; i < 4; i++) {
    int temp_file = get_file(square) + dirs_file[i];
    int temp_rank = get_rank(square) + dirs_rank[i];

    while (temp_file >= 0 && temp_file <= 7 && temp_rank >= 0 &&
           temp_rank <= 7) {
      int temp_sq = temp_file + (temp_rank * 8);
      bb |= (1ull << temp_sq);
      if (occ & (1ull << temp_sq)) {
        break;
      }

      temp_file += dirs_file[i];
      temp_rank += dirs_rank[i];
    }
  }

  return bb;
}

void fill_bishop_attacks() {
  for (int square = a1; square < SqNone; square++) {
    int bits = pop_count(BishopMasks[square]);
    int occ_var = 1 << bits;
    for (int i = 0; i < occ_var; i++) {
      uint64_t occ = set_occ(i, bits, BishopMasks[square]);

      uint64_t magic_idx = occ * BishopMagics[square] >> 55;
      BishopAttacks[square][magic_idx] = bishop_sliders(square, occ);
    }
  }
}

void fill_rook_attacks() {
  for (int square = a1; square < SqNone; square++) {
    int bits = pop_count(RookMasks[square]);
    int occ_var = 1 << bits;
    for (int i = 0; i < occ_var; i++) {

      uint64_t occ = set_occ(i, bits, RookMasks[square]);
      uint64_t magic_idx = occ * RookMagics[square] >> 52;
      RookAttacks[square][magic_idx] = rook_sliders(square, occ);
    }
  }
}

void fill_king_attacks() {
  for (int square = a1; square < SqNone; square++) {
    uint64_t occ = 0;
    int left   = std::max(0, get_file(square) - 1),
        right  = std::min(7, get_file(square) + 1),
        bottom = std::max(0, get_rank(square) - 1),
        top    = std::min(7, get_rank(square) + 1);

    for (int file = left; file <= right; file++) {
      for (int rank = bottom; rank <= top; rank++) {

        if (file + rank * 8 == square) {
          continue;
        }

        occ |= (1ull << (file + rank * 8));
      }
    }
    KingAttacks[square] = occ;
  }
}

void fill_knight_attacks() {
  int knight_moves_file[8] = {-2, -2, -1, 1, 2, 2, 1, -1};
  int knight_moves_rank[8] = {-1, 1, 2, 2, 1, -1, -2, -2};

  for (int square = a1; square < SqNone; square++) {

    uint64_t occ = 0;
    int s_file = get_file(square), s_rank = get_rank(square);

    for (int i = 0; i < 8; i++) {
      int file = s_file + knight_moves_file[i];
      int rank = s_rank + knight_moves_rank[i];

      if (file >= 0 && file <= 7 && rank >= 0 && rank <= 7) {
        occ |= (1ull << (file + rank * 8));
      }
    }

    KnightAttacks[square] = occ;
  }
}

void fill_pawn_attacks() {
  std::memset(&PawnAttacks, 0, sizeof(PawnAttacks));

  for (int square = a1; square < SqNone; square++) {
    if (get_file(square) > 0) {
      PawnAttacks[Colors::White][square] |=
          (1ull << (square + Directions_BB::Northwest));
      PawnAttacks[Colors::Black][square] |=
          (1ull << (square + Directions_BB::Southwest));
    }
    if (get_file(square) < 7) {
      PawnAttacks[Colors::White][square] |=
          (1ull << (square + Directions_BB::Northeast));
      PawnAttacks[Colors::Black][square] |=
          (1ull << (square + Directions_BB::Southeast));
    }
  }
}

uint64_t get_bishop_attacks(int sq, uint64_t occ) {
  return BishopAttacks[sq][(occ & BishopMasks[sq]) * BishopMagics[sq] >> 55];
}

uint64_t get_rook_attacks(int sq, uint64_t occ) {
  return RookAttacks[sq][(occ & RookMasks[sq]) * RookMagics[sq] >> 52];
}

void init_bbs() {
  for (int square = a1; square < SqNone; square++) {

    uint64_t edges = ((Ranks[0] | Ranks[7]) & ~rank_bb(square)) |
                     ((Files[0] | Files[7]) & ~file_bb(square));

    BishopMasks[square] = bishop_sliders(square, 0) & ~edges;
    RookMasks[square] = rook_sliders(square, 0) & ~edges;
  }

  fill_bishop_attacks();
  fill_rook_attacks();
  fill_king_attacks();
  fill_knight_attacks();
  fill_pawn_attacks();
}

void generate_bb(std::string fen, Position &pos) {
  std::memset(&pos, 0, sizeof(pos));
  int sq = a8;

  for (char c : fen) {
    if (c == ' ') {
      break;
    } else if (c == '/') {
      sq -= 8; // drop rank
      sq -= 8; // reset at file a
    } else if (isdigit(c)) {
      sq += Directions_BB::East * (c - '0');
    } else {
      int index;
      switch (tolower(c)) {
      case 'p':
        index = Pieces_BB::Pawn;
        break;
      case 'n':
        index = Pieces_BB::Knight;
        break;
      case 'b':
        index = Pieces_BB::Bishop;
        break;
      case 'r':
        index = Pieces_BB::Rook;
        break;
      case 'q':
        index = Pieces_BB::Queen;
        break;
      case 'k':
        index = Pieces_BB::King;
        break;
      default:
        printf("Unexpected error occured parsing FEN!\n");
        printf("%s %c\n", fen.c_str(), c);
        exit(1);
      }

      bool color_indx = islower(c);

      pos.pieces_bb[index] |= (1ull << sq);
      pos.colors_bb[color_indx] |= (1ull << sq);

      sq += Directions_BB::East;
    }
  }
}

void update_bb(Position &pos, int from_piece, int from, int to_piece, int to,
               int captured_piece, int capture_sq) {
  
  int color = from_piece & 1;
  int from_type = from_piece / 2;
  int to_type = to_piece / 2;
  int capt_type = captured_piece / 2;

  pos.colors_bb[color] += (1ull << to) - (1ull << from);
  pos.pieces_bb[from_type] -= (1ull << from);
  pos.pieces_bb[to_type] += (1ull << to);

  if (capture_sq != SquareNone) {
    pos.colors_bb[color ^ 1] -= (1ull << capture_sq);
    pos.pieces_bb[capt_type] -= (1ull << capture_sq);
  }
}

void print_bbs(Position &pos) {
  printf("Pawn bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::Pawn]);

  printf("Knight bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::Knight]);

  printf("Bishop bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::Bishop]);

  printf("Rook bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::Rook]);

  printf("Queen bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::Queen]);

  printf("King bitboard:\n");
  print_bb(pos.pieces_bb[Pieces_BB::King]);

  printf("White Piece bitboard:\n");
  print_bb(pos.colors_bb[Colors::White]);

  printf("Black Piece bitboard:\n");
  print_bb(pos.colors_bb[Colors::Black]);
}