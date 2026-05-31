#pragma once
#include "core/AppContext.h"

namespace quantum {

class IndicatorManager {
public:
    static void RecalculateIndicators(ChartTab& chart);
    static void CalculateBuiltin(ActiveIndicator& ind, const ChartTab& chart);
};

} // namespace quantum
