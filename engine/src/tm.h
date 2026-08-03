#pragma once
#include "defs.h"
#include "params.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace TimeManagement {
inline constexpr std::array<double, 10> BmStabilityTable = {
    2.50, 1.35, 1.05, 0.90, 0.85, 0.82, 0.80, 0.78, 0.76, 0.75};
} // namespace TimeManagement

inline void adjust_soft_limit(ThreadInfo &thread_info,
                              uint64_t best_move_nodes,
                              int depth,
                              int bm_stability,
                              int bm_changes,
                              int score,
                              int prev_score,
                              int root_move_count) {
  using namespace TimeManagement;

  if (thread_info.original_opt >= INT32_MAX / 2){
    return;
  }

  if (root_move_count == 1) {
    const uint64_t only_move_time =
        static_cast<uint64_t>(thread_info.original_opt) *
        static_cast<int>(OnlyMoveFactor) / 100;
    thread_info.opt_time = static_cast<uint32_t>(
        std::min<uint64_t>(only_move_time, thread_info.max_time));
    return;
  }

  const double node_fraction = std::clamp(
      static_cast<double>(best_move_nodes) /
          static_cast<double>(std::max<uint64_t>(thread_info.nodes, 1)),
      0.0, 1.0);

  const double node_factor =
      depth < static_cast<int>(NodeTmMinDepth)
          ? 1.0
          : std::clamp(
                (static_cast<int>(NodeTmFactor1) / 100.0 - node_fraction) *
                    static_cast<int>(NodeTmFactor2) / 100.0,
                static_cast<int>(NodeTmMin) / 100.0,
                static_cast<int>(NodeTmMax) / 100.0);

  const int stability_index = std::clamp(
      bm_stability, 0, static_cast<int>(BmStabilityTable.size()) - 1);
  const double bm_factor = BmStabilityTable[stability_index];

  const double change_rate =
      static_cast<double>(std::max(bm_changes, 0)) / std::max(depth, 1);
  const double change_factor =
      1.0 + std::max(0.0, change_rate - static_cast<int>(BmChangeBase) / 100.0) *
                static_cast<int>(BmChangeSlope) / 100.0;

  const double score_factor = std::clamp(
      1.0 + (prev_score - score) /
                static_cast<double>(static_cast<int>(ScoreDropDiv)),
      static_cast<int>(ScoreDropMin) / 100.0,
      static_cast<int>(ScoreDropMax) / 100.0);

  const double total_factor = std::clamp(
      node_factor * bm_factor * change_factor * score_factor,
      static_cast<int>(OptTotalMin) / 100.0,
      static_cast<int>(OptTotalMax) / 100.0);
      
    thread_info.opt_time = std::min<uint32_t>(
      thread_info.original_opt * total_factor,
      thread_info.max_time);
  }
