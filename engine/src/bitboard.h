#include <cstdint>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include "defs.h"

enum Square : int {                 //a1 = 0. a8 = 7, etc. thus a1 is the LSB and h8 is the MSB.
  a1, a2, a3, a4, a5, a6, a7, a8,
  b1, b2, b3, b4, b5, b6, b7, b8,
  c1, c2, c3, c4, c5, c6, c7, c8,
  d1, d2, d3, d4, d5, d6, d7, d8,
  e1, e2, e3, e4, e5, e6, e7, e8,
  f1, f2, f3, f4, f5, f6, f7, f8,
  g1, g2, g3, g4, g5, g6, g7, g8,
  h1, h2, h3, h4, h5, h6, h7, h8,
  SqNone
};

namespace Directions_BB {
    constexpr int8_t North = 1;
    constexpr int8_t South = -1;
    constexpr int8_t East = 8;
    constexpr int8_t West = -8;
    constexpr int8_t Northeast = 9;
    constexpr int8_t Southeast = -7;
    constexpr int8_t Northwest = 7;
    constexpr int8_t Southwest = -9;
}

namespace Pieces_BB {
    constexpr uint8_t Pawn = 0;
    constexpr uint8_t Knight = 1;
    constexpr uint8_t Bishop = 2;
    constexpr uint8_t Rook = 3;
    constexpr uint8_t Queen = 4;
    constexpr uint8_t King = 5;
    constexpr uint8_t PieceNone = 6;
}

struct Position_BB{
    uint64_t colors[2];
    uint64_t pieces[6];
};

int get_lsb(uint64_t bb){
    return __builtin_ctzll(bb);
}

int pop_lsb(uint64_t &bb){
  int s = get_lsb(bb);
  bb &= ~1;
  return s;
}

void generate_bb(std::string fen, Position_BB &pos){
    std::memset(&pos, 0, sizeof(pos));
    int sq = a8;

    for (char c : fen){
        if (sq == SqNone){
            break;
        }
        else if (c == '/'){
            sq -= 65;
        }
        else if (isdigit(c)){
            sq += Directions_BB::East * (c - '0');
        }
        else{
            int index;
            switch (tolower(c)){
                case 'p':
                    index = Pieces_BB::Pawn; break;
                case 'n':
                    index = Pieces_BB::Knight; break;
                case 'b':
                    index = Pieces_BB::Bishop; break;
                case 'r':
                    index = Pieces_BB::Rook; break;
                case 'q':
                    index = Pieces_BB::Queen; break;
                case 'k':
                    index = Pieces_BB::King; break;
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

void print_bb(uint64_t bb){
    for (int sq = a8; sq != SqNone;){
        if (sq > SqNone){
            printf("\n");
            sq -= 65;
        }
        else{
            printf("%i ", (int)(bb >> sq) & 1);
            sq += Directions_BB::East;
        }
    }
    printf("\n\n");
}

void print_bbs(Position_BB &pos){
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