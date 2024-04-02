#pragma once
#include "defs.h"
#include <math.h>
constexpr int NMPMinDepth = 3;
constexpr int NMPBase = 3;
constexpr int NMPDepthDiv = 6;
constexpr int RFPMargin = 80;
constexpr int RFPMaxDepth = 9;
constexpr int LMRBase = 5;
constexpr int LMRRatio = 23;
constexpr int LMPBase = 5;
constexpr int LMPDepth = 5;
constexpr int SEDepth = 7;
constexpr int SEDoubleExtMargin = 20;
constexpr int FPDepth = 8;
constexpr int IIRMinDepth = 3;


int LMRTable[MaxSearchDepth][ListSize];

void init_LMR(){
    for (int i = 0; i < MaxSearchDepth; i++){
        for (int n = 0; n < ListSize; n++){
            LMRTable[i][n] = LMRBase / 10.0 + log(i) * log(n) / (LMRRatio / 10.0);
        }
    }
}