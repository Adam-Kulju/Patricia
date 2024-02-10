#pragma once
#include "defs.h"
#include "nnue.h"
#include <vector>

struct ThreadInfo {
  uint64_t zobrist_key; // hash key of the position we're currently on
  uint16_t thread_id;   // ID of the thread
  GameHistory game_hist[GameSize]; // all positions from earlier in the game
  uint16_t game_ply;               // how far we're into the game
  uint16_t search_ply;             // depth that we are in the search tree
  uint64_t nodes;                  // Total nodes searched so far this search
  std::chrono::_V2::steady_clock::time_point
      start_time; // Start time of the search
  NNUE_State nnue_state;
};

uint64_t TT_size = (1 << 20);
uint64_t TT_mask = TT_size - 1;
std::vector<TTEntry> TT(TT_size);

void clear_TT(){
   memset(&TT[0], 0, TT_size * sizeof(TT[0]));
}

void insert_entry(uint64_t hash, int depth, Move best_move, int32_t score,
                  uint8_t bound_type) {
  int indx = hash & TT_mask;
  TT[indx].position_key = static_cast<uint32_t>(get_hash_upper_bits(hash)),
  TT[indx].depth = static_cast<uint8_t>(depth),
  TT[indx].type = bound_type,
  TT[indx].score = score;
  if (bound_type > EntryTypes::UBound){
    TT[indx].best_move = best_move;
  }
}

uint64_t calculate(Position position) {
  uint64_t hash = 0;
  for (int indx : StandardToMailbox) {
    int piece = position.board[indx];
    if (piece) {
      hash ^= zobrist_keys[get_zobrist_key(piece, standard(indx))];
    }
  }
  if (position.color) {
    hash ^= zobrist_keys[side_index];
  }
  if (position.ep_square != 255) {
    hash ^= zobrist_keys[ep_index];
  }
  for (int indx = castling_index; indx < 778; indx++) {
    if (position.castling_rights[indx > 775][(indx & 1)]) {
      hash ^= zobrist_keys[indx];
    }
  }
  return hash;
}