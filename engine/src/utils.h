#pragma once
#include "defs.h"
#include "nnue.h"
#include "params.h"
#include <stdio.h>
#include <thread>
#include <vector>

using std::array;

typedef unsigned __int128 uint128_t;

struct ThreadInfo {
  uint16_t thread_id = 0; // ID of the thread
  std::array<GameHistory, GameSize>
      game_hist;       // all positions from earlier in the game
  uint16_t game_ply;   // how far we're into the game
  uint16_t search_ply; // depth that we are in the search tree
  uint64_t nodes;      // Total nodes searched so far this search
  std::chrono::steady_clock::time_point start_time; // Start time of the search

  int seldepth;

  uint32_t max_time;
  uint32_t opt_time;

  uint16_t time_checks;
  bool stop;

  NNUE_State nnue_state;

  MultiArray<int16_t, 14, 0x80> HistoryScores;
  MultiArray<int16_t, 14, 0x80, 14, 0x80> ContHistScores;
  MultiArray<int16_t, 14, 0x80> CapHistScores;
  std::array<Move, MaxSearchDepth + 1> KillerMoves;

  uint8_t current_iter;
  uint16_t multipv = 1;
  uint16_t multipv_index;

  Move excluded_move;
  std::array<Move, ListSize> best_moves;
  std::array<int, ListSize> best_scores;

  uint8_t max_iter_depth = MaxSearchDepth;
  uint64_t max_nodes_searched = UINT64_MAX / 2;
  uint64_t opt_nodes_searched = UINT64_MAX / 2;

  int32_t score = ScoreNone;
  Move best_move = MoveNone;
  bool disable_print = false;

  bool is_human = false;
  int cp_accum_loss = 0;
  int cp_loss = 0;

  std::array<Move, MaxSearchDepth * MaxSearchDepth> pv;
  std::array<int, 5> pv_material;

  Position position;

  uint8_t searches = 0;
};

struct ThreadData {
  std::vector<ThreadInfo> thread_infos;
  std::vector<std::thread> threads;
  int num_threads = 1;
};

ThreadData thread_data;

uint64_t TT_size = (1 << 20);
std::vector<TTEntry> TT(TT_size);

void new_game(ThreadInfo &thread_info, std::vector<TTEntry> &TT) {
  // Reset TT and other thread_info values for a new game

  thread_info.game_ply = 0;
  thread_info.thread_id = 0;
  std::memset(&thread_info.HistoryScores, 0, sizeof(thread_info.HistoryScores));
  std::memset(&thread_info.ContHistScores, 0,
              sizeof(thread_info.ContHistScores));
  std::memset(&thread_info.CapHistScores, 0, sizeof(thread_info.CapHistScores));
  std::memset(&thread_info.game_hist, 0, sizeof(thread_info.game_hist));
  std::memset(&TT[0], 0, TT_size * sizeof(TT[0]));
  thread_info.searches = 0;
  thread_info.cp_accum_loss = 0;
}

void resize_TT(int size) {
  TT_size =
      static_cast<uint64_t>(size) * 1024 * 1024 / sizeof(TTEntry);
  TT.resize(TT_size);
  std::memset(&TT[0], 0, TT_size * sizeof(TT[0]));
}

uint64_t hash_to_idx(uint64_t hash) {
  return (uint128_t(hash) * uint128_t(TT_size)) >> 64;
}

void insert_entry(uint64_t hash, int depth, Move best_move, int32_t score,
                  uint8_t bound_type, uint8_t searches,
                  std::vector<TTEntry>
                      &TT) { // Inserts an entry into the transposition table.
                      
  int indx = hash_to_idx(hash);
  uint16_t hash_key = get_hash_low_bits(hash);

  if (TT[indx].position_key == hash_key &&
      !(bound_type == EntryTypes::Exact &&
        TT[indx].type != EntryTypes::Exact)) {

    uint8_t age_diff = searches - TT[indx].age;

    int new_bonus =
        depth + bound_type + (age_diff * age_diff * 10 / AgeDiffDiv);
    int old_bonus = TT[indx].depth + TT[indx].type;

    if (old_bonus * OldBonusMult > new_bonus * NewBonusMult) {
      return;
    }
  }

  if (best_move != MoveNone || hash_key != TT[indx].position_key) {
    TT[indx].best_move = best_move;
  }

  TT[indx].position_key = hash_key,
  TT[indx].depth = static_cast<uint8_t>(depth), TT[indx].type = bound_type,
  TT[indx].score = score;
  TT[indx].age = searches;
}

uint64_t calculate(const Position &position) { // Calculates the zobrist key of
                                               // a given position.
  // Useful when initializing positions, in search though
  // incremental updates are faster.
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

int64_t time_elapsed(std::chrono::steady_clock::time_point start_time) {
  // get the time that has elapsed since the start of search

  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
      .count();
}
