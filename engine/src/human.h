#include "search.h"

void multipv_search(Position &position, ThreadInfo &thread_info){

    thread_info.multipv = 5;
    thread_info.max_nodes_searched = 100000;
    thread_info.disable_print = true;

    iterative_deepen(position, thread_info, TT);

    thread_info.multipv = 1;
    thread_info.max_nodes_searched = UINT64_MAX / 2;
    thread_info.disable_print = false;
}


void search_human(Position &position, ThreadInfo &thread_info){
    multipv_search(position, thread_info);
    printf("bestmove %s\n", internal_to_uci(position, thread_info.best_moves[0]).c_str());
}