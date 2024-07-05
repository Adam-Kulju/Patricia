#pragma once
#include "defs.h"
#include <cmath>
#include <iostream>

MultiArray<int, MaxSearchDepth, ListSize> LMRTable;


struct Parameter {
  std::string name;
  int &value;
  int min, max;
};

std::vector<Parameter> params;

// SPSA parameter code is based off Clover
// (https://github.com/lucametehau/CloverEngine)
struct CreateParam {
  int _value;
  CreateParam(std::string name, int value, int min, int max) : _value(value) {
    params.push_back({name, _value, min, max});
  }

  operator int() const { return _value; }
};


#define TUNE_PARAM(name, value, min, max)                                      \
  CreateParam name(#name, value, min, max);


TUNE_PARAM(NMPMinDepth, 3, 1, 5);
TUNE_PARAM(NMPBase, 3, 1, 5);
TUNE_PARAM(NMPDepthDiv, 6, 3, 9);
TUNE_PARAM(NMPEvalDiv, 200, 100, 300);
TUNE_PARAM(RFPMargin, 80, 50, 110);
TUNE_PARAM(RFPMaxDepth, 9, 6, 12);
TUNE_PARAM(LMRBase, 5, 2, 8);
TUNE_PARAM(LMRRatio, 23, 15, 30);
TUNE_PARAM(LMPBase, 3, 1, 5);
TUNE_PARAM(LMPDepth, 5, 3, 7);
TUNE_PARAM(SEDepth, 7, 4, 10);
TUNE_PARAM(SEDoubleExtMargin, 20, 10, 30);
TUNE_PARAM(FPDepth, 8, 5, 11);
TUNE_PARAM(FPMargin1, 100, 50, 150);
TUNE_PARAM(FPMargin2, 125, 75, 175);
TUNE_PARAM(IIRMinDepth, 3, 1, 5);
TUNE_PARAM(SeePruningDepth, 8, 5, 11);
TUNE_PARAM(SeePruningQuietMargin, -80, -110, -50);
TUNE_PARAM(SeePruningNoisyMargin, -30, -50, -10);
TUNE_PARAM(HistBonus, 300, 200, 400);
TUNE_PARAM(HistMax, 2500, 1500, 3500);
TUNE_PARAM(AgeDiffDiv, 4, 2, 6);
TUNE_PARAM(OldBonusMult, 20, 10, 30);
TUNE_PARAM(NewBonusMult, 30, 20, 40);


void print_params_for_ob() {
  for (auto &param : params) {
    std::cout << param.name << ", int, " << param.value << ", " << param.min
              << ", " << param.max << ", "
              << std::max(0.5, (param.max - param.min) / 20.0) << ", 0.002\n";
  }
}

void init_LMR() {
  for (int i = 0; i < MaxSearchDepth; i++) {
    for (int n = 0; n < ListSize; n++) {
      LMRTable[i][n] = LMRBase / 10.0 + log(i) * log(n) / (LMRRatio / 10.0);
    }
  }
}