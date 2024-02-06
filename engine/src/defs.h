#pragma once
#include <stdint.h>

namespace Colors {
constexpr uint8_t White = 0;
constexpr uint8_t Black = 1;
}; // namespace Colors
namespace Pieces {
constexpr uint8_t Blank = 0;
constexpr uint8_t WPawn = 2;
constexpr uint8_t BPawn = 3;
constexpr uint8_t WKnight = 4;
constexpr uint8_t BKnight = 5;
constexpr uint8_t WBishop = 6;
constexpr uint8_t BBishop = 7;
constexpr uint8_t WRook = 8;
constexpr uint8_t BRook = 9;
constexpr uint8_t WQueen = 10;
constexpr uint8_t BQueen = 11;
constexpr uint8_t WKing = 12;
constexpr uint8_t BKing = 13;
}; // namespace Pieces
namespace Directions {
constexpr int8_t North = 16;
constexpr int8_t South = -16;
constexpr int8_t East = 1;
constexpr int8_t West = -1;
constexpr int8_t Northeast = 17;
constexpr int8_t Southeast = -15;
constexpr int8_t Northwest = 15;
constexpr int8_t Southwest = -17;
} // namespace Directions
namespace Sides {
constexpr int8_t Kingside = 1;
constexpr int8_t Queenside = 0;
} // namespace Sides

typedef uint32_t Move;
/*The format of a move structure is:      from     to   promo
                                         (<< 10)  (<< 2)
                                        xxxxxxxx xxxxxxxx  xx
                                        */

struct MoveInfo {
  Move move;
  int score;
};

struct Position {
  uint8_t board[0x80];          // Stores the board itself
  uint8_t material_count[10]; // Stores material
  bool castling_rights[2][2];
  uint8_t kingpos[2]; // Stores King positions
  uint8_t ep_square;  // stores ep square
  bool color;         // whose side to move
  uint8_t halfmoves;
};

struct GameHistory {
  uint64_t position_key;
  Move played_move;
  uint8_t piece_moved;
};

struct ThreadInfo {
  uint16_t game_length;
  uint64_t zobrist_key;
  uint16_t thread_id;
  GameHistory game_hist[1000];
};

constexpr int StandardToMailbox[64] = {
    0x0,  0x1,  0x2,  0x3,  0x4,  0x5,  0x6,  0x7,  0x10, 0x11, 0x12,
    0x13, 0x14, 0x15, 0x16, 0x17, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
    0x26, 0x27, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x40,
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x50, 0x51, 0x52, 0x53,
    0x54, 0x55, 0x56, 0x57, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
    0x67, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77};

constexpr int MailboxToStandard_NNUE[0x80] = {
    56, 57, 58, 59, 60, 61, 62, 63, 99, 99, 99, 99, 99, 99, 99, 99, 48, 49, 50,
    51, 52, 53, 54, 55, 99, 99, 99, 99, 99, 99, 99, 99, 40, 41, 42, 43, 44, 45,
    46, 47, 99, 99, 99, 99, 99, 99, 99, 99, 32, 33, 34, 35, 36, 37, 38, 39, 99,
    99, 99, 99, 99, 99, 99, 99, 24, 25, 26, 27, 28, 29, 30, 31, 99, 99, 99, 99,
    99, 99, 99, 99, 16, 17, 18, 19, 20, 21, 22, 23, 99, 99, 99, 99, 99, 99, 99,
    99, 8,  9,  10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99, 0,  1,
    2,  3,  4,  5,  6,  7,  99, 99, 99, 99, 99, 99, 99, 99,
};

constexpr int MailboxToStandard[0x80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 99, 99, 99, 99, 99, 99, 99, 99,
    8,  9,  10, 11, 12, 13, 14, 15, 99, 99, 99, 99, 99, 99, 99, 99,
    16, 17, 18, 19, 20, 21, 22, 23, 99, 99, 99, 99, 99, 99, 99, 99, 
    24, 25, 26, 27, 28, 29, 30, 31, 99, 99, 99, 99, 99, 99, 99, 99, 
    32, 33, 34, 35, 36, 37, 38, 39, 99, 99, 99, 99, 99, 99, 99, 99,
    40, 41, 42, 43, 44, 45, 46, 47, 99, 99, 99, 99, 99, 99, 99, 99, 
    48, 49, 50, 51, 52, 53, 54, 55, 99, 99, 99, 99, 99, 99, 99, 99, 
    56, 57, 58, 59, 60, 61, 62, 63, 99, 99, 99, 99, 99, 99, 99, 99,
};

constexpr int8_t AttackRays[8] = {Directions::East,      Directions::West,
                                  Directions::South,     Directions::North,
                                  Directions::Southeast, Directions::Southwest,
                                  Directions::Northeast, Directions::Northwest};

constexpr int8_t KnightAttacks[8] = {Directions::East * 2 + Directions::North,
                                     Directions::East * 2 + Directions::South,
                                     Directions::South * 2 + Directions::East,
                                     Directions::South * 2 + Directions::West,
                                     Directions::West * 2 + Directions::South,
                                     Directions::West * 2 + Directions::North,
                                     Directions::North * 2 + Directions::West,
                                     Directions::North * 2 + Directions::East};

constexpr int8_t SliderAttacks[4][8] = {
    {Directions::Southeast, Directions::Southwest, Directions::Northeast,
     Directions::Northwest},
    {Directions::East, Directions::West, Directions::South, Directions::North},
    {Directions::East, Directions::West, Directions::South, Directions::North,
     Directions::Southeast, Directions::Southwest, Directions::Northeast,
     Directions::Northwest},
    {Directions::East, Directions::West, Directions::South, Directions::North,
     Directions::Southeast, Directions::Southwest, Directions::Northeast,
     Directions::Northwest}};


#define out_of_board(x) (x & 0x88)
#define get_rank(x) (x / 16)
#define get_file(x) (x % 16)
#define flip(x) (x ^ 112)
#define get_color(x) (x & 1)
#define pack_move(from, to, promo) ((from << 10) + (to << 2) + promo)
#define extract_from(move) (move >> 10)
#define extract_to(move) ((move >> 2) & 127)
#define extract_promo(move) (move & 3)
#define friendly_square(color, piece) (piece && (piece & 1) == color)
#define enemy_square(color, piece) (piece && (piece & 1) != color)