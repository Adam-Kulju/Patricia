#include <stdio.h>
#include "board.h"


int main(void){
    Position position;
    ThreadInfo thread_info;
    set_board(position, thread_info, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 18 1");
    clock_t a = clock();
    for (int i = 0; i < 10000000; i++){
        bool b = attacks_square(position, 0x41, Colors::White);
    }
    printf("%f seconds\n", (float)(clock() - a) / CLOCKS_PER_SEC);
    return 0;
}