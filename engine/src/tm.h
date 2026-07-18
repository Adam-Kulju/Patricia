#pragma once
#include "defs.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <cstdint>

inline constexpr std::array<double, 10> BmStabilityTable = {
    2.50, 1.35, 1.05, 0.90, 0.85, 0.82, 0.80, 0.78, 0.76, 0.75};

void adjust_soft_limit(ThreadInfo &thread_info, uint64_t best_move_nodes,
                       int depth, int bm_stability, int bm_changes,
                       int score, int prev_score, int root_move_count) {
  if (root_move_count == 1) {
    thread_info.opt_time = std::min<uint32_t>(
        thread_info.original_opt * OnlyMoveFactor / 100, thread_info.max_time);
    return;
  }

  double fract = std::clamp(
      (double)best_move_nodes / (double)std::max<uint64_t>(thread_info.nodes, 1),
      0.0, 1.0);

  double node_factor =
      depth < NodeTmMinDepth
          ? 1.0
          : std::clamp((NodeTmFactor1 / 100.0 - fract) * NodeTmFactor2 / 100.0,
                       NodeTmMin / 100.0, NodeTmMax / 100.0);

  double bm_factor = BmStabilityTable[std::min<int>(
      bm_stability, (int)BmStabilityTable.size() - 1)];

  double change_rate = (double)bm_changes / std::max(1, depth);
  double change_factor =
      1.0 + std::max(0.0, change_rate - BmChangeBase / 100.0) *
                BmChangeSlope / 100.0;

  double score_factor =
      std::clamp(1.0 + (prev_score - score) / (double)ScoreDropDiv,
                 ScoreDropMin / 100.0, ScoreDropMax / 100.0);

  double total = std::clamp(node_factor * bm_factor * change_factor * score_factor,
                            OptTotalMin / 100.0, OptTotalMax / 100.0);

  thread_info.opt_time = (uint32_t)std::min<double>(
      (double)thread_info.original_opt * total, (double)thread_info.max_time);
}
