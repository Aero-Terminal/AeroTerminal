#include "IndicatorManager.h"
#include <algorithm>
#include <cmath>

namespace quantum {

void IndicatorManager::RecalculateIndicators(ChartTab& chart) {
    for (auto& ind : chart.indicators) {
        if (!ind.enabled) continue;
        
        if (ind.type == "SMA" || ind.type == "EMA" || ind.type == "RSI") {
            CalculateBuiltin(ind, chart);
        }
    }
}

void IndicatorManager::CalculateBuiltin(ActiveIndicator& ind, const ChartTab& chart) {
    if (ind.type == "SMA") {
        int period = 20;
        if (ind.int_params.find("period") != ind.int_params.end()) {
            period = ind.int_params["period"];
        }
        if (period < 1) period = 1;
        
        size_t n_total = chart.candles.size();
        ind.values.assign(n_total, 0.0);
        for (size_t i = 0; i < n_total; ++i) {
            double sum = 0.0;
            int count = 0;
            for (int k = std::max(0, (int)i - period + 1); k <= (int)i; ++k) {
                sum += chart.candles[k].close;
                count++;
            }
            ind.values[i] = sum / count;
        }
    } 
    else if (ind.type == "EMA") {
        int period = 9;
        if (ind.int_params.find("period") != ind.int_params.end()) {
            period = ind.int_params["period"];
        }
        if (period < 1) period = 1;
        
        size_t n_total = chart.candles.size();
        ind.values.assign(n_total, 0.0);
        if (n_total > 0) {
            ind.values[0] = chart.candles[0].close;
            double k = 2.0 / (period + 1.0);
            for (size_t i = 1; i < n_total; ++i) {
                ind.values[i] = (chart.candles[i].close * k) + (ind.values[i - 1] * (1.0 - k));
            }
        }
    }
    else if (ind.type == "RSI") {
        int period = 14;
        if (ind.int_params.find("period") != ind.int_params.end()) {
            period = ind.int_params["period"];
        }
        if (period < 1) period = 1;
        
        size_t n_total = chart.candles.size();
        ind.values.assign(n_total, 50.0);
        
        if (n_total > (size_t)period) {
            std::vector<double> gains(n_total, 0.0);
            std::vector<double> losses(n_total, 0.0);
            
            for (size_t i = 1; i < n_total; ++i) {
                double diff = chart.candles[i].close - chart.candles[i - 1].close;
                if (diff > 0.0) {
                    gains[i] = diff;
                } else {
                    losses[i] = -diff;
                }
            }
            
            double avg_gain = 0.0;
            double avg_loss = 0.0;
            for (int i = 1; i <= period; ++i) {
                avg_gain += gains[i];
                avg_loss += losses[i];
            }
            avg_gain /= period;
            avg_loss /= period;
            
            if (avg_loss == 0.0) {
                ind.values[period] = 100.0;
            } else {
                ind.values[period] = 100.0 - (100.0 / (1.0 + (avg_gain / avg_loss)));
            }
            
            for (size_t i = period + 1; i < n_total; ++i) {
                avg_gain = (avg_gain * (period - 1) + gains[i]) / period;
                avg_loss = (avg_loss * (period - 1) + losses[i]) / period;
                
                if (avg_loss == 0.0) {
                    ind.values[i] = 100.0;
                } else {
                    ind.values[i] = 100.0 - (100.0 / (1.0 + (avg_gain / avg_loss)));
                }
            }
        }
    }
}

} // namespace quantum
