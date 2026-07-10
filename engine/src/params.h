#pragma once

#include "defs.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string_view>
#include <vector>

struct Parameter {
  std::string_view name;
  int* value;
  int min;
  int max;

  [[nodiscard]] int current() const noexcept { return *value; }

  [[nodiscard]] double step() const noexcept {
    return std::max(0.5, static_cast<double>(max - min) / 20.0);
  }
};

namespace tune_detail {
inline std::vector<Parameter>& parameter_registry() {
  static auto registry = [] {
    std::vector<Parameter> items;
    items.reserve(32);
    return items;
  }();
  return registry;
}
}

inline std::vector<Parameter>& get_params() {
  return tune_detail::parameter_registry();
}

struct CreateParam {
  int _value;

  CreateParam(std::string_view name, int value, int min, int max) : _value(value) {
    tune_detail::parameter_registry().push_back({name, &_value, min, max});
  }

  CreateParam(const CreateParam&) = delete;
  CreateParam& operator=(const CreateParam&) = delete;
  CreateParam(CreateParam&&) = delete;
  CreateParam& operator=(CreateParam&&) = delete;

  [[nodiscard]] operator int() const noexcept { return _value; }
};

#define TUNE_PARAM(name, value, min, max) inline CreateParam name{#name, value, min, max}

TUNE_PARAM(NMPMinDepth, 3, 1, 5);
TUNE_PARAM(NMPBase, 4, 1, 5);
TUNE_PARAM(NMPDepthDiv, 5, 3, 9);
TUNE_PARAM(NMPEvalDiv, 175, 100, 300);
TUNE_PARAM(RFPMargin, 85, 50, 110);
TUNE_PARAM(RFPMaxDepth, 10, 6, 12);
TUNE_PARAM(ProbcutMargin, 250, 150, 400);
TUNE_PARAM(LMRBase, 4, 2, 8);
TUNE_PARAM(LMRRatio, 20, 15, 30);
TUNE_PARAM(LMPBase, 2, 1, 5);
TUNE_PARAM(LMPDepth, 6, 3, 7);
TUNE_PARAM(SEDepth, 5, 4, 10);
TUNE_PARAM(HistPruningDepth, 4, 2, 7);
TUNE_PARAM(SEDoubleExtMargin, 18, 10, 30);
TUNE_PARAM(SETripleExtMargin, 126, 75, 175);
TUNE_PARAM(FPDepth, 9, 5, 11);
TUNE_PARAM(FPMargin1, 98, 50, 150);
TUNE_PARAM(FPMargin2, 121, 75, 175);
TUNE_PARAM(IIRMinDepth, 2, 1, 5);
TUNE_PARAM(SeePruningDepth, 7, 5, 11);
TUNE_PARAM(SeePruningNoisyMargin, -86, -110, -50);
TUNE_PARAM(SeePruningQuietMargin, -94, -150, -50);
TUNE_PARAM(HistBonus, 291, 200, 400);
TUNE_PARAM(HistMax, 2476, 1500, 3500);
TUNE_PARAM(HistDiv, 9818, 5000, 15000);
TUNE_PARAM(AgeDiffDiv, 5, 2, 6);
TUNE_PARAM(CorrWeight, 25, 10, 40);
TUNE_PARAM(LMRMinDepth, 3, 2, 6);
TUNE_PARAM(AspStartWindow, 20, 10, 30);
TUNE_PARAM(NodeTmFactor1, 149, 100, 200);
TUNE_PARAM(NodeTmFactor2, 177, 125, 225);
TUNE_PARAM(BmFactor1, 152, 100, 200);
TUNE_PARAM(ScoreDropDiv, 540, 250, 850);
TUNE_PARAM(ScoreDropMin, 90, 70, 100);
TUNE_PARAM(ScoreDropMax, 118, 100, 140);

#undef TUNE_PARAM

inline std::vector<Parameter>& params = get_params();

inline MultiArray<int, MaxSearchDepth + 1, ListSize> LMRTable{};

inline void print_params_for_ob(std::ostream& out = std::cout) {
  for (const auto& param : get_params()) {
    out << param.name << ", int, " << param.current() << ", " << param.min
        << ", " << param.max << ", " << param.step() << ", 0.002\n";
  }
}

inline void init_LMR() {
  const double base = static_cast<double>(LMRBase) / 10.0;
  const double scale = 10.0 / static_cast<double>(LMRRatio);

  for (int depth = 0; depth <= MaxSearchDepth; ++depth) {
    LMRTable[depth][0] = 0;
  }

  for (int moveCount = 0; moveCount < ListSize; ++moveCount) {
    LMRTable[0][moveCount] = 0;
  }

  for (int depth = 1; depth <= MaxSearchDepth; ++depth) {
    const double logDepth = std::log(static_cast<double>(depth));

    for (int moveCount = 1; moveCount < ListSize; ++moveCount) {
      const double logMoveCount = std::log(static_cast<double>(moveCount));
      const double reduction = base + logDepth * logMoveCount * scale;
      LMRTable[depth][moveCount] = static_cast<int>(reduction);
    }
  }
}
