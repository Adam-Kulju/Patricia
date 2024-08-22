#ifndef PATRICIA_HUMAN_HPP
#define PATRICIA_HUMAN_HPP
#include "search.hpp"

inline void multipv_search(Position &position, ThreadInfo &thread_info) {

  thread_info.multipv = 5;
  thread_info.opt_time = thread_info.opt_time * 3 / 4;
  thread_info.pv_material = {0};

  iterative_deepen(position, thread_info, TT);

  thread_info.multipv = 1;
}

// unnerfed - 3000
// cp loss 5 - 2900
// cp loss 10 - 2800
// cp loss 15 - 2700                                                      
// cp loss 20 - 2500
// cp loss 30 - 2150
// cp loss 40 - 1950
// cp loss 60 - 1800
// cp loss 80 - 1700
// cp loss 120 - 1300
// cp loss 150 - 1000

inline void search_human(Position &position, ThreadInfo &thread_info) {

  int starting_mat = material_eval(position);

  multipv_search(position, thread_info);

  Move best_move = thread_info.best_moves[0];

  int mistake = 0;

  for (int i = 0; i < 5; i++) {

    int m_diff = starting_mat - thread_info.pv_material[i];
    int eval_diff = thread_info.best_scores[0] - thread_info.best_scores[i];

    if (thread_info.game_ply < 10 && m_diff > 0 && eval_diff < 80 &&
        thread_info.best_scores[i] > -100) {
      best_move = thread_info.best_moves[i];
      mistake = eval_diff;
      break;
    }

    if (thread_info.game_ply < 5 && eval_diff < 50 && thread_info.best_scores[i] > -100 && i < 3){
      if (dist(rd) % 2 == 1){
        best_move = thread_info.best_moves[i];
        mistake = eval_diff;
      }
    }

    if (thread_info.game_ply > 5 && eval_diff > 0 &&
        eval_diff < thread_info.cp_accum_loss + 10 && thread_info.best_scores[0] < 1000) {
      best_move = thread_info.best_moves[i];
      mistake = eval_diff;
    }
  }

  if (mistake) {
    thread_info.cp_accum_loss -= mistake;
  }
  if (thread_info.game_ply > 5) {
    thread_info.cp_accum_loss += thread_info.cp_loss;
  }

  printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
}

#endif
