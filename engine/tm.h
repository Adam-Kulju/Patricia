#pragma once
#include "defs.h"
#include "utils.h"

void adjust_soft_limit(ThreadInfo &thread_info, uint64_t best_move_nodes, int bm_stability) {
  double fract = (double)best_move_nodes / thread_info.nodes;
  double factor = (1.5 - fract) * 1.75;
  double bm_factor = 1.5 - (bm_stability * 0.06);

  thread_info.opt_time = std::min<uint32_t>(thread_info.original_opt * factor * bm_factor,
                                            thread_info.max_time);
}