#include "search.h"

void multipv_search(Position &position, ThreadInfo &thread_info) {

  thread_info.multipv = 5;
  thread_info.opt_time = thread_info.opt_time * 3 / 4;
  thread_info.pv_material = {0};

  search_position(position, thread_info, TT);

  thread_info.multipv = 1;
}
// cp loss 5 - 2900
// cp loss 10 - 2700
// cp loss 15 - 2550
// cp loss 20 - 2500
// cp loss 25 - 2300
// cp loss 30 - 1900-2000
// cp loss 40 - 1800
// cp loss 50 - 1600
// cp loss 80 - 1200


void search_human(Position &position, ThreadInfo &thread_info) {

  thread_info.cp_loss = 60;

  int starting_mat = material_eval(position);

  multipv_search(position, thread_info);

  Move best_move = thread_info.best_moves[0];

  int mistake = 0;

  for (int i = 0; i < 5 && thread_info.best_moves[i] != MoveNone; i++) {

    int m_diff = starting_mat - thread_info.pv_material[i];
    int eval_diff = thread_info.best_scores[0] - thread_info.best_scores[i];

    if (thread_info.game_ply < 7 && eval_diff < 25 &&
        thread_info.best_scores[i] > -100 && i < 4) {
      if (dist(rd) % 5 == i) {
        best_move = thread_info.best_moves[i];
        mistake = eval_diff;
      }
    }

    if (thread_info.game_ply >= 7 && eval_diff > 0 &&
        eval_diff < thread_info.cp_accum_loss + 10 &&
        thread_info.best_scores[0] < 5000) {
      best_move = thread_info.best_moves[i];
      mistake = eval_diff;

      if (m_diff > 0 && thread_info.best_scores[i] > -100){
        break;
      }
    }
  }

  if (mistake) {
    thread_info.cp_accum_loss -= mistake;
  }
  if (thread_info.game_ply >= 7) {
    thread_info.cp_accum_loss += thread_info.cp_loss;
  }

  printf("bestmove %s\n", internal_to_uci(position, best_move).c_str());
}
