#pragma once
#include "defs.h"
#include "utils.h"
#include <algorithm>

void adjust_soft_limit(ThreadInfo &thread_info, uint64_t best_move_nodes,
                        int bm_stability, int score, int prev_score) {
  double fract = (double)best_move_nodes / thread_info.nodes;
  double factor = (NodeTmFactor1 / 100.0f - fract) * NodeTmFactor2 / 100.0f;
  double bm_factor = BmFactor1 / 100.0f - (bm_stability * 0.06);

  // If the score dropped since the last iteration, the position likely got
  // more critical, so allocate extra time; if it rose, allocate less.
  double score_factor =
      std::clamp(1.0 + (prev_score - score) / (double)ScoreDropDiv,
                 ScoreDropMin / 100.0, ScoreDropMax / 100.0);

  thread_info.opt_time = std::min<uint32_t>(
      thread_info.original_opt * factor * bm_factor * score_factor,
      thread_info.max_time);
}