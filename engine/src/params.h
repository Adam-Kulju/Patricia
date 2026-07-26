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
    items.reserve(64);
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

TUNE_PARAM(NMPMinDepth, 2, 1, 5);
TUNE_PARAM(NMPBase, 5, 2, 8);
TUNE_PARAM(NMPDepthDiv, 4, 2, 9);
TUNE_PARAM(NMPEvalDiv, 140, 80, 300);
TUNE_PARAM(RFPMargin, 70, 40, 110);
TUNE_PARAM(RFPMaxDepth, 11, 6, 14);
TUNE_PARAM(ProbcutMargin, 190, 120, 400);
TUNE_PARAM(LMRBase, 7, 2, 12);
TUNE_PARAM(LMRRatio, 17, 12, 30);
TUNE_PARAM(LMPBase, 2, 1, 5);
TUNE_PARAM(LMPDepth, 9, 3, 12);
TUNE_PARAM(SEDepth, 4, 3, 10);
TUNE_PARAM(HistPruningDepth, 6, 2, 10);
TUNE_PARAM(SEDoubleExtMargin, 13, 6, 30);
TUNE_PARAM(SETripleExtMargin, 98, 50, 175);
TUNE_PARAM(FPDepth, 10, 5, 14);
TUNE_PARAM(FPMargin1, 84, 40, 150);
TUNE_PARAM(FPMargin2, 106, 60, 175);
TUNE_PARAM(IIRMinDepth, 2, 1, 5);
TUNE_PARAM(SeePruningDepth, 9, 5, 13);
TUNE_PARAM(SeePruningNoisyMargin, -68, -110, -30);
TUNE_PARAM(SeePruningQuietMargin, -72, -150, -30);
TUNE_PARAM(HistBonus, 328, 200, 500);
TUNE_PARAM(HistMax, 2688, 1500, 4000);
TUNE_PARAM(HistDiv, 8900, 4000, 15000);
TUNE_PARAM(AgeDiffDiv, 4, 2, 6);
TUNE_PARAM(CorrWeight, 31, 10, 50);
TUNE_PARAM(LMRMinDepth, 2, 1, 6);
TUNE_PARAM(AspStartWindow, 12, 6, 30);
TUNE_PARAM(NodeTmFactor1, 140, 100, 220);
TUNE_PARAM(NodeTmFactor2, 92, 50, 200);
TUNE_PARAM(ScoreDropDiv, 90, 40, 300);
TUNE_PARAM(ScoreDropMin, 88, 60, 100);
TUNE_PARAM(ScoreDropMax, 220, 100, 320);
TUNE_PARAM(OnlyMoveFactor, 30, 10, 80);
TUNE_PARAM(NodeTmMinDepth, 10, 6, 16);
TUNE_PARAM(NodeTmMin, 50, 25, 85);
TUNE_PARAM(NodeTmMax, 150, 110, 200);
TUNE_PARAM(BmChangeBase, 15, 0, 40);
TUNE_PARAM(BmChangeSlope, 20, 5, 50);
TUNE_PARAM(OptTotalMin, 25, 10, 60);
TUNE_PARAM(OptTotalMax, 500, 250, 750);

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

  static std::vector<double> logs;
  const int maxIndex = std::max<int>(MaxSearchDepth + 1, ListSize);

  if (static_cast<int>(logs.size()) < maxIndex) {
    logs.resize(static_cast<std::size_t>(maxIndex));
    logs[0] = 0.0;
    for (int i = 1; i < maxIndex; ++i) {
      logs[static_cast<std::size_t>(i)] = std::log(static_cast<double>(i));
    }
  }

  for (int depth = 0; depth <= MaxSearchDepth; ++depth) {
    LMRTable[depth][0] = 0;
  }

  for (int moveCount = 0; moveCount < ListSize; ++moveCount) {
    LMRTable[0][moveCount] = 0;
  }

  for (int depth = 1; depth <= MaxSearchDepth; ++depth) {
    const double logDepth = logs[static_cast<std::size_t>(depth)];
    const int cap = std::max(0, depth - 1);

    for (int moveCount = 1; moveCount < ListSize; ++moveCount) {
      const double logMoveCount = logs[static_cast<std::size_t>(moveCount)];
      const double reduction = base + logDepth * logMoveCount * scale;
      LMRTable[depth][moveCount] =
          std::clamp(static_cast<int>(reduction), 0, cap);
    }
  }
}
