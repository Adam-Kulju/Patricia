#pragma once
#include "defs.h"
#include "nnue.h"
#include "params.h"
#include <condition_variable>
#include <mutex>
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

  uint64_t tb_hits;
  uint64_t nodes; // Total nodes searched so far this search
  std::vector<RootMoveInfo> root_moves;

  std::chrono::steady_clock::time_point start_time; // Start time of the search

  int seldepth;

  uint32_t max_time;
  uint32_t opt_time;
  uint32_t original_opt;

  uint16_t time_checks;

  NNUE_State nnue_state;

  MultiArray<int16_t, 14, 64> HistoryScores;
  MultiArray<int16_t, 14, 64, 14, 64> ContHistScores;
  MultiArray<int16_t, 14, 64> CapHistScores;
  MultiArray<int16_t, 2, 16384> PawnCorrHist;
  MultiArray<int16_t, 2, 2, 16384> NonPawnCorrHist;
  std::array<Move, MaxSearchDepth + 1> KillerMoves;
  std::array<int16_t, MaxSearchDepth + 2> FailHighCount;

  uint8_t current_iter;
  uint16_t multipv = 1;
  uint16_t multipv_index;

  Move excluded_move;
  std::array<Move, ListSize> best_moves;
  std::array<int, ListSize> best_scores;

  uint8_t max_iter_depth = MaxSearchDepth;
  uint64_t max_nodes_searched = UINT64_MAX / 2;
  uint64_t opt_nodes_searched = UINT64_MAX / 2;

  bool doing_datagen = false;
  bool datagen_stop = false;

  bool is_human = false;
  int cp_accum_loss = 0;
  int cp_loss = 0;

  std::array<Move, MaxSearchDepth * MaxSearchDepth> pv;
  std::array<int, 5> pv_material;

  Position position;

  uint8_t searches = 0;
  volatile bool searching = false;
  uint8_t phase;
};

RootMoveInfo *find_root_move(ThreadInfo &thread_info, Move move) {
  for (int i = 0; i < thread_info.root_moves.size(); i++) {
    if (thread_info.root_moves[i].move == move)
      return &thread_info.root_moves[i];
  }
  return nullptr;
}

struct ThreadData {
  std::vector<ThreadInfo> thread_infos;
  std::vector<std::thread> threads;
  int num_threads = 1;
  std::atomic<bool> stop = true;
  std::atomic<bool> terminate = false;
  bool is_frc = false;
};

ThreadData thread_data;

uint64_t TT_size = (1 << 20);
std::vector<TTBucket> TT(TT_size);

void new_game(ThreadInfo &thread_info, std::vector<TTBucket> &TT) {
  // Reset TT and other thread_info values for a new game

  thread_info.game_ply = 6;
  thread_info.thread_id = 0;
  std::memset(&thread_info.HistoryScores, 0, sizeof(thread_info.HistoryScores));
  std::memset(&thread_info.ContHistScores, 0,
              sizeof(thread_info.ContHistScores));
  std::memset(&thread_info.CapHistScores, 0, sizeof(thread_info.CapHistScores));
  std::memset(&thread_info.PawnCorrHist, 0, sizeof(thread_info.PawnCorrHist));
  std::memset(&thread_info.NonPawnCorrHist, 0, sizeof(thread_info.NonPawnCorrHist));
  std::memset(&thread_info.game_hist, 0, sizeof(thread_info.game_hist));
  std::memset(&TT[0], 0, TT_size * sizeof(TT[0]));
  thread_info.searches = 0;
  thread_info.cp_accum_loss = 0;
}

uint16_t get_hash_low_bits(uint64_t hash) {
  return static_cast<uint16_t>(hash);
}

int32_t score_to_tt(int32_t score, int32_t ply) {
  if (score == ScoreNone)
    return ScoreNone;
  if (score >= ScoreWin)
    return score + ply;
  if (score <= ScoreLost)
    return score - ply;
  return score;
}

int32_t score_from_tt(int32_t score, int32_t ply) {
  if (score == ScoreNone)
    return ScoreNone;
  if (score >= ScoreWin)
    return score - ply;
  if (score <= ScoreLost)
    return score + ply;
  return score;
}

void resize_TT(int size) {
  TT_size = static_cast<uint64_t>(size) * 1024 * 1024 / sizeof(TTBucket);
  TT.resize(TT_size);
  std::memset(&TT[0], 0, TT_size * sizeof(TT[0]));
}

uint64_t hash_to_idx(uint64_t hash) {
  return (uint128_t(hash) * uint128_t(TT_size)) >> 64;
}

int entry_quality(TTEntry &entry, int searches) {
  int age_diff = (MaxAge + searches - entry.get_age()) % MaxAge;
  return entry.depth - age_diff * 8;
}

TTEntry &probe_entry(uint64_t hash, bool &hit, uint8_t searches,
                     std::vector<TTBucket> &TT) {

  uint16_t hash_key = get_hash_low_bits(hash);
  auto &entries = TT[hash_to_idx(hash)].entries;

  for (int i = 0; i < BucketEntries; i++) {
    bool empty =
        entries[i].score == 0 && entries[i].get_type() == EntryTypes::None;

    if (empty || entries[i].position_key == hash_key) {
      hit = !empty;
      entries[i].age_bound = (searches << 2) | entries[i].get_type();
      return entries[i];
    }
  }

  TTEntry *worst = &(entries[0]);
  int worst_quality = entry_quality(*worst, searches);

  for (int i = 1; i < BucketEntries; i++) {
    int this_quality = entry_quality(entries[i], searches);
    if (this_quality < worst_quality) {
      worst = &(entries[i]);
      worst_quality = this_quality;
    }
  }

  hit = false;
  return *worst;
}

void insert_entry(
    TTEntry &entry, uint64_t hash, int depth, Move best_move,
    int32_t static_eval, int32_t score, uint8_t bound_type,
    uint8_t searches) { // Inserts an entry into the transposition table.

  uint16_t hash_key = get_hash_low_bits(hash);

  if (best_move != MoveNone || hash_key != entry.position_key) {
    entry.best_move = best_move;
  }

  if (entry.position_key == hash_key && (bound_type != EntryTypes::Exact) &&
      entry.depth > depth + 4) {
    return;
  }

  entry.position_key = hash_key, entry.depth = static_cast<uint8_t>(depth),
  entry.static_eval = static_eval, entry.score = score,
  entry.age_bound = (searches << 2) | bound_type;
}

void calculate(Position &position) { // Calculates the zobrist key of
                                               // a given position.
  // Useful when initializing positions, in search though
  // incremental updates are faster.
  uint64_t hash = 0;
  uint64_t pawn_hash = 0;
  position.non_pawn_key[Colors::White] = 0, position.non_pawn_key[Colors::Black] = 0;

  for (int indx = 0; indx < 64; indx++) {
    int piece = position.board[indx];
    if (piece) {
      hash ^= zobrist_keys[get_zobrist_key(piece, indx)];
      if (get_piece_type(piece) == PieceTypes::Pawn){
        pawn_hash ^= zobrist_keys[get_zobrist_key(piece, indx)];
      }
      else{
        position.non_pawn_key[get_color(piece)] ^= zobrist_keys[get_zobrist_key(piece, indx)];
      }
    }
  }
  if (position.color) {
    hash ^= zobrist_keys[side_index];
  }
  if (position.ep_square != 255) {
    hash ^= zobrist_keys[ep_index];
  }
  for (int indx = castling_index; indx < 778; indx++) {
    if (position.castling_squares[indx > 775][(indx & 1)] != SquareNone) {
      hash ^= zobrist_keys[indx];
    }
  }
  position.zobrist_key = hash;
  position.pawn_key = pawn_hash;
}

int get_corrhist_index(uint64_t key){
  return key % 16384;
}

int64_t time_elapsed(std::chrono::steady_clock::time_point start_time) {
  // get the time that has elapsed since the start of search

  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
      .count();
}

// ty to Ciekce (https://github.com/Ciekce/Stormphrax) for providing this code
// in a pinch to fix a critical bug.

class Barrier {
public:
  Barrier(int64_t expected) { reset(expected); }

  auto reset(int64_t expected) -> void {

    m_total.store(expected, std::memory_order::seq_cst);
    m_current.store(expected, std::memory_order::seq_cst);
  }

  auto arrive_and_wait() {
    std::unique_lock lock{wait_mutex};

    const auto current = --m_current;

    if (current > 0) {
      const auto phase = m_phase.load(std::memory_order::relaxed);
      wait_signal.wait(lock, [this, phase] {
        return (phase - m_phase.load(std::memory_order::acquire)) < 0;
      });
    } else {
      const auto total = m_total.load(std::memory_order::acquire);
      m_current.store(total, std::memory_order::release);

      m_phase++;

      wait_signal.notify_all();
    }
  }

private:
  std::atomic<int64_t> m_total{};
  std::atomic<int64_t> m_current{};
  std::atomic<int64_t> m_phase{};

  std::mutex wait_mutex{};
  std::condition_variable wait_signal{};
};

Barrier reset_barrier{1};
Barrier idle_barrier{1};
Barrier search_end_barrier{1};
