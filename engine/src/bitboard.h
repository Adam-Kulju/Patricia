#pragma once
#include "defs.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

enum Square : int { // a1 = 0. a8 = 7, etc. thus a1 is the LSB and h8 is the
                    // MSB.
  a1,
  a2,
  a3,
  a4,
  a5,
  a6,
  a7,
  a8,
  b1,
  b2,
  b3,
  b4,
  b5,
  b6,
  b7,
  b8,
  c1,
  c2,
  c3,
  c4,
  c5,
  c6,
  c7,
  c8,
  d1,
  d2,
  d3,
  d4,
  d5,
  d6,
  d7,
  d8,
  e1,
  e2,
  e3,
  e4,
  e5,
  e6,
  e7,
  e8,
  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  g1,
  g2,
  g3,
  g4,
  g5,
  g6,
  g7,
  g8,
  h1,
  h2,
  h3,
  h4,
  h5,
  h6,
  h7,
  h8,
  SqNone
};

constexpr uint64_t Ranks[8] = {0xFFull,       0xFFull << 8,  0xFFull << 16,
                               0xFFull << 24, 0xFFull << 32, 0xFFull << 40,
                               0xFFull << 48, 0xFFull << 56};

constexpr uint64_t Files[8] = {
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
constexpr uint8_t Pawn = 0;
constexpr uint8_t Knight = 1;
constexpr uint8_t Bishop = 2;
constexpr uint8_t Rook = 3;
constexpr uint8_t Queen = 4;
constexpr uint8_t King = 5;
constexpr uint8_t PieceNone = 6;
} // namespace Pieces_BB

struct Position_BB {
  uint64_t colors[2];
  uint64_t pieces[6];
};

uint64_t RookMasks[64];
uint64_t BishopMasks[64];
uint64_t BishopAttacks[64][512];
uint64_t RookAttacks[64][4096];

constexpr uint64_t BishopMagics[64] = {
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
constexpr uint64_t RookMagics[64] = {
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

int get_lsb(uint64_t bb) { return __builtin_ctzll(bb); }

int pop_lsb(uint64_t &bb) {
  int s = get_lsb(bb);
  bb &= ~1;
  return s;
}

uint64_t set_occ(int idx, int size, uint64_t mask) {
  uint64_t occ = 0;
  for (int i = 0; i < size; i++) {
    if (idx & (1 << size)) {
      occ |= pop_lsb(mask);
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
    int bits = __builtin_popcountll(BishopMasks[square]);
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
    int bits = __builtin_popcountll(RookMasks[square]);
    int occ_var = 1 << bits;
    for (int i = 0; i < occ_var; i++) {

      uint64_t occ = set_occ(i, bits, RookMasks[square]);
      uint64_t magic_idx = occ * RookMagics[square] >> 52;
      RookAttacks[square][magic_idx] = rook_sliders(square, occ);
    }
  }
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
}

void generate_bb(std::string fen, Position_BB &pos) {
  std::memset(&pos, 0, sizeof(pos));
  int sq = a8;

  for (char c : fen) {
    if (sq == SqNone) {
      break;
    } else if (c == '/') {
      sq -= 65;
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
        printf("%s\n", fen.c_str());
        exit(1);
      }

      bool color_indx = islower(c);

      pos.pieces[index] += (1ull << sq);
      pos.colors[color_indx] += (1ull << sq);

      sq += Directions_BB::East;
    }
  }
}

void update_bb(Position_BB &pos, int from_piece, int from, int to_piece, int to,
               int captured_piece, int capture_sq) {
  int color = from_piece & 1;
  int from_type = from_piece / 2 - 1;
  int to_type = to_piece / 2 - 1;
  int capt_type = captured_piece / 2 - 1;

  pos.colors[color] += (1ull << to) - (1ull << from);
  pos.pieces[from_type] -= (1ull << from);
  pos.pieces[to_type] += (1ull << to);

  if (capture_sq != SquareNone) {
    pos.colors[color ^ 1] -= (1ull << capture_sq);
    pos.pieces[capt_type] -= (1ull << capture_sq);
  }
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

void print_bbs(Position_BB &pos) {
  printf("Pawn bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::Pawn]);

  printf("Knight bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::Knight]);

  printf("Bishop bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::Bishop]);

  printf("Rook bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::Rook]);

  printf("Queen bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::Queen]);

  printf("King bitboard:\n");
  print_bb(pos.pieces[Pieces_BB::King]);

  printf("White Piece bitboard:\n");
  print_bb(pos.colors[Colors::White]);

  printf("Black Piece bitboard:\n");
  print_bb(pos.colors[Colors::Black]);
}