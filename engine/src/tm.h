#pragma once
#include "defs.h"
#include "utils.h"

#include <algorithm>
#include <array>
#include <cstdint>

namespace TimeManagement {
inline constexpr int OnlyMoveFactor = 30;
inline constexpr int NodeTmMinDepth = 10;
inline constexpr int NodeTmMin = 50;
inline constexpr int NodeTmMax = 150;
inline constexpr int BmChangeBase = 15;
inline constexpr int BmChangeSlope = 20;
inline constexpr int OptTotalMin = 25;
inline constexpr int OptTotalMax = 500;

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

  if (root_move_count == 1) {
    const uint64_t only_move_time =
        static_cast<uint64_t>(thread_info.original_opt) * OnlyMoveFactor / 100;
    thread_info.opt_time = static_cast<uint32_t>(
        std::min<uint64_t>(only_move_time, thread_info.max_time));
    return;
  }

  const double node_fraction = std::clamp(
      static_cast<double>(best_move_nodes) /
          static_cast<double>(std::max<uint64_t>(thread_info.nodes, 1)),
      0.0, 1.0);

  const double node_factor =
      depth < NodeTmMinDepth
          ? 1.0
          : std::clamp(
                (static_cast<int>(NodeTmFactor1) / 100.0 - node_fraction) *
                    static_cast<int>(NodeTmFactor2) / 100.0,
                NodeTmMin / 100.0, NodeTmMax / 100.0);

  const int stability_index = std::clamp(
      bm_stability, 0, static_cast<int>(BmStabilityTable.size()) - 1);
  const double bm_factor = BmStabilityTable[stability_index];

  const double change_rate =
      static_cast<double>(std::max(bm_changes, 0)) / std::max(depth, 1);
  const double change_factor =
      1.0 + std::max(0.0, change_rate - BmChangeBase / 100.0) *
                BmChangeSlope / 100.0;

  const double score_factor = std::clamp(
      1.0 + (prev_score - score) /
                static_cast<double>(static_cast<int>(ScoreDropDiv)),
      static_cast<int>(ScoreDropMin) / 100.0,
      static_cast<int>(ScoreDropMax) / 100.0);

  const double total_factor = std::clamp(
      node_factor * bm_factor * change_factor * score_factor,
      OptTotalMin / 100.0, OptTotalMax / 100.0);

  const double adjusted_time =
      static_cast<double>(thread_info.original_opt) * total_factor;
  thread_info.opt_time = static_cast<uint32_t>(std::min(
      adjusted_time, static_cast<double>(thread_info.max_time)));
}
