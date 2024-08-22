#ifndef PATRICIA__HPP
#define PATRICIA__HPP
#include "defs.hpp"
#include "utils.hpp"
#include <algorithm>

inline void adjust_soft_limit(ThreadInfo &thread_info, const uint64_t best_move_nodes) {
    const double FRACTION = (static_cast<double>(thread_info.nodes) != 0) ? (static_cast<double>(best_move_nodes) / static_cast<double>(thread_info.nodes)) : 0.0;
    const double FACTOR = (1.5 - FRACTION) * 1.75;

    const double calculated_opt_time = thread_info.original_opt * FACTOR;
    thread_info.opt_time = std::min(static_cast<uint32_t>(calculated_opt_time), thread_info.max_time);
}

#endif
