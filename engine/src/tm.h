#pragma once
#include "utils.h"
#include "defs.h"

void adjust_soft_limit(ThreadInfo &thread_info){
    double fract = (double)thread_info.best_move_nodes / thread_info.iter_nodes;
    double factor = (1.5 - fract) * 1.75;
    thread_info.opt_time = std::min<uint32_t>(thread_info.original_opt * factor, thread_info.max_time);
}