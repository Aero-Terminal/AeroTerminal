#include "PanelChart.h"
#include "core/AppContext.h"
#include "core/MarketDataService.h"
#include "core/indicators/IndicatorManager.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <random>
#include <cstring>
#include <vector>
#include <fstream>

#define ICON_FA_CHART_LINE "\xef\x88\x81"

namespace quantum::ui {

inline std::string FormatTimestamp(uint64_t ms, const std::string& timeframe, bool detailed = false) {
    if (ms == 0) return "";
    time_t sec = ms / 1000;
    struct tm* timeinfo = std::localtime(&sec);
    if (!timeinfo) return "";
    char buf[32];
    if (timeframe == "1d") {
        std::strftime(buf, sizeof(buf), detailed ? "%Y-%m-%d" : "%m-%d", timeinfo);
    } else {
        std::strftime(buf, sizeof(buf), detailed ? "%m-%d %H:%M" : "%H:%M", timeinfo);
    }
    return std::string(buf);
}

void RenderChartPanel(int id, const char* ticker, bool* p_open)
{
    std::string title = std::string(ICON_FA_CHART_LINE " Historical Chart (") + ticker + ")###chart_" + std::to_string(id);
    if (!ImGui::Begin(title.c_str(), p_open)) {
        ImGui::End();
        return;
    }

    auto& ctx = AppContext::instance();
    ChartTab* active_chart = nullptr;
    for (auto& chart : ctx.active_charts) {
        if (chart.id == id) {
            active_chart = &chart;
            break;
        }
    }

    if (!active_chart) {
        ImGui::End();
        return;
    }

    // Top control bar
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Ticker:");
    ImGui::SameLine();
    
    char ticker_buf[16];
    std::strncpy(ticker_buf, active_chart->ticker.c_str(), sizeof(ticker_buf));
    ticker_buf[sizeof(ticker_buf) - 1] = '\0';
    
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::InputText("##ticker_input", ticker_buf, sizeof(ticker_buf), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase)) {
        if (strlen(ticker_buf) > 0 && active_chart->ticker != ticker_buf) {
            MarketDataService::instance().Unsubscribe(active_chart->ticker, active_chart->timeframe);
            active_chart->ticker = ticker_buf;
            active_chart->initialized = false;
            active_chart->is_streaming = false;
            active_chart->candles.clear();
            active_chart->volumes.clear();
            active_chart->bids.clear();
            active_chart->asks.clear();
            active_chart->trades_log.clear();
            MarketDataService::instance().Subscribe(ticker_buf, active_chart->timeframe);
        }
    }
    
    ImGui::SameLine();
    
    // Timeframe selector buttons
    const char* timeframes[] = { "1m", "5m", "15m", "1h", "1d" };
    for (const char* tf : timeframes) {
        bool selected = (active_chart->timeframe == tf);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.38f, 1.0f));
        }
        if (ImGui::Button(tf)) {
            if (active_chart->timeframe != tf) {
                MarketDataService::instance().Unsubscribe(active_chart->ticker, active_chart->timeframe);
                active_chart->timeframe = tf;
                active_chart->initialized = false;
                active_chart->is_streaming = false;
                active_chart->candles.clear();
                active_chart->volumes.clear();
                MarketDataService::instance().Subscribe(active_chart->ticker, tf);
            }
        }
        if (selected) {
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
    }
    
    if (active_chart->is_streaming) {
        ImGui::TextColored(ImVec4(0.15f, 0.70f, 0.40f, 1.0f), "[LIVE]");
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.15f, 1.0f), "[MOCK SIM]");
    }
    
    ImGui::Separator();

    // 1. Initialize chart candles if not initialized
    if (!active_chart->initialized) {
        MarketDataService::instance().Subscribe(active_chart->ticker, active_chart->timeframe);

        double base_price = 100.0;
        if (active_chart->ticker == "AAPL") base_price = 180.0;
        else if (active_chart->ticker == "TSLA") base_price = 220.0;
        else if (active_chart->ticker == "NVDA") base_price = 820.0;
        else if (active_chart->ticker == "MSFT") base_price = 410.0;
        else if (active_chart->ticker == "GOOGL") base_price = 170.0;
        else if (active_chart->ticker == "AMZN") base_price = 185.0;
        else if (active_chart->ticker.find("USD") != std::string::npos) base_price = 70000.0;

        unsigned int seed = id;
        for (char c : active_chart->ticker) seed = seed * 31 + c;
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        double current_price = base_price;
        active_chart->candles.clear();
        active_chart->volumes.clear();
        
        uint64_t now_ms = (uint64_t)std::time(nullptr) * 1000;
        uint64_t tf_ms = 60 * 1000;
        if (active_chart->timeframe == "5m") tf_ms = 5 * 60 * 1000;
        else if (active_chart->timeframe == "15m") tf_ms = 15 * 60 * 1000;
        else if (active_chart->timeframe == "1h") tf_ms = 60 * 60 * 1000;
        else if (active_chart->timeframe == "1d") tf_ms = 24 * 60 * 60 * 1000;

        for (int i = 0; i < 150; ++i) {
            double open = current_price;
            double change = (dist(rng) - 0.5) * (base_price * 0.02);
            double close = open + change;
            double high = std::max(open, close) + (dist(rng) * (base_price * 0.008));
            double low = std::min(open, close) - (dist(rng) * (base_price * 0.008));
            double volume = dist(rng) * 500.0 + 50.0;
            uint64_t ts = now_ms - (150 - i) * tf_ms;

            active_chart->candles.push_back({open, close, high, low, ts});
            active_chart->volumes.push_back(volume);
            current_price = close;
        }
        active_chart->initialized = true;
    }

    // 2. Simulate tick updates on every frame (fluctuate the close of the last candle) if mock mode
    if (!active_chart->candles.empty() && !active_chart->is_streaming) {
        auto& last_candle = active_chart->candles.back();
        double base_price = 100.0;
        if (active_chart->ticker == "AAPL") base_price = 180.0;
        else if (active_chart->ticker == "TSLA") base_price = 220.0;
        else if (active_chart->ticker == "NVDA") base_price = 820.0;
        else if (active_chart->ticker == "MSFT") base_price = 410.0;
        else if (active_chart->ticker == "GOOGL") base_price = 170.0;
        else if (active_chart->ticker == "AMZN") base_price = 185.0;

        double tick_volatility = base_price * 0.0004;
        static std::mt19937 tick_rng(0xACE1);
        static std::uniform_real_distribution<double> tick_dist(0.0, 1.0);
        double r = tick_dist(tick_rng);
        double tick = (r - 0.5) * tick_volatility;

        last_candle.close += tick;
        if (last_candle.close > last_candle.high) last_candle.high = last_candle.close;
        if (last_candle.close < last_candle.low) last_candle.low = last_candle.close;
        
        if (active_chart->volumes.empty()) active_chart->volumes.push_back(100.0);
        active_chart->volumes.back() += std::abs(tick) * 10.0;
    }    if (active_chart->is_streaming) {
        ImGui::TextColored(ImVec4(0.15f, 0.70f, 0.40f, 1.0f), "[LIVE]");
    } else {
        ImGui::TextColored(ImVec4(0.85f, 0.65f, 0.15f, 1.0f), "[MOCK SIM]");
    }
    
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    
    static bool open_indicator_popup = false;
    if (ImGui::Button("fx Study") && ImGui::IsItemActivated()) {
        open_indicator_popup = true;
    }
    if (open_indicator_popup) {
        ImGui::OpenPopup("Indicators & Strategies");
        open_indicator_popup = false;
    }

    static int selected_category = 0;
    static char search_buf[64] = "";
    
    ImGui::SetNextWindowSize(ImVec2(650, 420), ImGuiCond_FirstUseEver);
    if (ImGui::BeginPopupModal("Indicators & Strategies", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Search:");
        ImGui::SameLine();
        ImGui::InputText("##indicator_search", search_buf, sizeof(search_buf));
        ImGui::Separator();
        
        ImGui::Columns(2, "indicator_columns", true);
        ImGui::SetColumnWidth(0, 180.0f);
        
        if (ImGui::Selectable("Overlay Indicators", selected_category == 0)) selected_category = 0;
        if (ImGui::Selectable("Oscillators", selected_category == 1)) selected_category = 1;
        
        ImGui::NextColumn();
        
        std::string search_str = search_buf;
        std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::tolower);
        
        auto matches_search = [&search_str](const std::string& name) {
            if (search_str.empty()) return true;
            std::string name_lower = name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            return name_lower.find(search_str) != std::string::npos;
        };
        
        if (selected_category == 0) {
            if (matches_search("Simple Moving Average (SMA)") && ImGui::Selectable("Simple Moving Average (SMA)")) {
                ActiveIndicator sma;
                sma.type = "SMA";
                sma.name = "SMA (20)";
                sma.color[0] = 1.0f; sma.color[1] = 0.92f; sma.color[2] = 0.23f; sma.color[3] = 1.0f;
                sma.int_params["period"] = 20;
                active_chart->indicators.push_back(sma);
                IndicatorManager::RecalculateIndicators(*active_chart);
            }
            if (matches_search("Exponential Moving Average (EMA)") && ImGui::Selectable("Exponential Moving Average (EMA)")) {
                ActiveIndicator ema;
                ema.type = "EMA";
                ema.name = "EMA (9)";
                ema.color[0] = 0.91f; ema.color[1] = 0.12f; ema.color[2] = 0.39f; ema.color[3] = 1.0f;
                ema.int_params["period"] = 9;
                active_chart->indicators.push_back(ema);
                IndicatorManager::RecalculateIndicators(*active_chart);
            }
        } 
        else if (selected_category == 1) {
            if (matches_search("Relative Strength Index (RSI)") && ImGui::Selectable("Relative Strength Index (RSI)")) {
                ActiveIndicator rsi;
                rsi.type = "RSI";
                rsi.name = "RSI (14)";
                rsi.color[0] = 0.5f; rsi.color[1] = 0.4f; rsi.color[2] = 0.9f; rsi.color[3] = 1.0f;
                rsi.int_params["period"] = 14;
                active_chart->indicators.push_back(rsi);
                IndicatorManager::RecalculateIndicators(*active_chart);
            }
        }
        
        ImGui::Columns(1);
        ImGui::Separator();
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    ImGui::Separator();

    // Determine sizes
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    const float y_gutter_w = 65.0f;
    const float x_gutter_h = 22.0f;

    float plot_w = canvas_size.x - y_gutter_w;
    float plot_h = canvas_size.y - x_gutter_h;
    if (plot_w < 50.0f) plot_w = 50.0f;
    if (plot_h < 50.0f) plot_h = 50.0f;

    ImVec2 plot_pos = canvas_pos;

    // Register interaction zones
    ImGui::SetCursorScreenPos(plot_pos);
    ImGui::InvisibleButton("##plot_area", ImVec2(plot_w, plot_h));
    bool plot_hovered = ImGui::IsItemHovered();
    bool plot_active = ImGui::IsItemActive();

    ImGui::SetCursorScreenPos(ImVec2(plot_pos.x + plot_w, plot_pos.y));
    ImGui::InvisibleButton("##y_axis_area", ImVec2(y_gutter_w, plot_h));
    bool y_axis_hovered = ImGui::IsItemHovered();
    bool y_axis_active = ImGui::IsItemActive();

    ImGui::SetCursorScreenPos(ImVec2(plot_pos.x, plot_pos.y + plot_h));
    ImGui::InvisibleButton("##x_axis_area", ImVec2(plot_w, x_gutter_h));
    bool x_axis_hovered = ImGui::IsItemHovered();
    bool x_axis_active = ImGui::IsItemActive();

    if (y_axis_hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (x_axis_hovered) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    int n_total = (int)active_chart->candles.size();

    if (plot_hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            active_chart->visible_candles -= (int)(wheel * 4);
        }
    }

    if (plot_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        float drag_x = ImGui::GetIO().MouseDelta.x;
        if (drag_x != 0.0f) {
            float candle_width = plot_w / active_chart->visible_candles;
            if (candle_width > 0.1f) {
                static float drag_acc = 0.0f;
                drag_acc += drag_x / candle_width;
                int shift = (int)drag_acc;
                if (shift != 0) {
                    active_chart->scroll_offset += shift;
                    drag_acc -= shift;
                }
            }
        }
    }

    if (y_axis_hovered || y_axis_active) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            active_chart->y_scale_factor = 1.0f;
            active_chart->y_offset_factor = 0.0f;
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float dy = ImGui::GetIO().MouseDelta.y;
            if (ImGui::GetIO().KeyShift) {
                active_chart->y_offset_factor += dy * 0.002f * active_chart->y_scale_factor;
            } else {
                active_chart->y_scale_factor += dy * 0.005f;
            }
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            float dy = ImGui::GetIO().MouseDelta.y;
            active_chart->y_offset_factor += dy * 0.002f * active_chart->y_scale_factor;
        }

        // Handle mouse wheel scroll on Y-axis to scale/zoom price scale
        float y_wheel = ImGui::GetIO().MouseWheel;
        if (y_wheel != 0.0f) {
            active_chart->y_scale_factor -= y_wheel * 0.05f;
        }

        if (active_chart->y_scale_factor < 0.05f) active_chart->y_scale_factor = 0.05f;
        if (active_chart->y_scale_factor > 20.0f) active_chart->y_scale_factor = 20.0f;
        if (active_chart->y_offset_factor < -10.0f) active_chart->y_offset_factor = -10.0f;
        if (active_chart->y_offset_factor > 10.0f) active_chart->y_offset_factor = 10.0f;
    }

    if (x_axis_hovered || x_axis_active) {
        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            active_chart->visible_candles = 60;
            active_chart->scroll_offset = 0;
        } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            float dx = ImGui::GetIO().MouseDelta.x;
            if (dx != 0.0f) {
                active_chart->visible_candles -= (int)(dx * 0.25f);
            }
        }

        // Handle mouse wheel scroll on X-axis to scale/zoom timeline
        float x_wheel = ImGui::GetIO().MouseWheel;
        if (x_wheel != 0.0f) {
            active_chart->visible_candles -= (int)(x_wheel * 4);
        }
    }

    if (active_chart->visible_candles < 15) active_chart->visible_candles = 15;
    if (active_chart->visible_candles > n_total) active_chart->visible_candles = n_total;

    int max_scroll = std::max(0, n_total - 10);
    int min_scroll = -active_chart->visible_candles + 10;
    if (min_scroll > 0) min_scroll = 0;

    if (active_chart->scroll_offset > max_scroll) active_chart->scroll_offset = max_scroll;
    if (active_chart->scroll_offset < min_scroll) active_chart->scroll_offset = min_scroll;

    int idx_start = n_total - active_chart->visible_candles - active_chart->scroll_offset;
    int idx_end = n_total - active_chart->scroll_offset;
    int n_visible = idx_end - idx_start;

    if (n_visible <= 0) {
        ImGui::End();
        return;
    }

    // Check if RSI is enabled
    bool rsi_enabled = false;
    ActiveIndicator* rsi_ind = nullptr;
    for (auto& ind : active_chart->indicators) {
        if (ind.type == "RSI" && ind.enabled) {
            rsi_enabled = true;
            rsi_ind = &ind;
            break;
        }
    }

    float draw_plot_h = rsi_enabled ? plot_h - 100.0f : plot_h;
    if (draw_plot_h < 50.0f) draw_plot_h = 50.0f;

    double min_val = 1e9;
    double max_val = -1e9;
    int check_start = std::max(0, idx_start);
    int check_end = std::min(n_total, idx_end);
    for (int i = check_start; i < check_end; ++i) {
        const auto& candle = active_chart->candles[i];
        if (candle.low < min_val) min_val = candle.low;
        if (candle.high > max_val) max_val = candle.high;
    }
    
    // Adjust bounds to fit overlay indicators (skip RSI since it has separate scale)
    for (int i = check_start; i < check_end; ++i) {
        for (const auto& ind : active_chart->indicators) {
            if (!ind.enabled || ind.type == "RSI" || ind.values.size() < (size_t)n_total) continue;
            double val = ind.values[i];
            if (val > 0.0) {
                if (val < min_val) min_val = val;
                if (val > max_val) max_val = val;
            }
        }
    }

    if (min_val > 1e8 || max_val < -1e8) {
        if (!active_chart->candles.empty()) {
            min_val = active_chart->candles.back().low;
            max_val = active_chart->candles.back().high;
        } else {
            min_val = 100.0;
            max_val = 110.0;
        }
    }

    double range = max_val - min_val;
    if (range < 0.1) range = 0.1;
    min_val -= range * 0.05;
    max_val += range * 0.05;
    range = max_val - min_val;

    double center = (max_val + min_val) / 2.0;
    double adj_range = range * active_chart->y_scale_factor;
    if (adj_range < 0.000001) adj_range = 0.000001;
    double adjusted_min = center - adj_range / 2.0 + active_chart->y_offset_factor * range;
    double adjusted_max = center + adj_range / 2.0 + active_chart->y_offset_factor * range;

    float candle_width = plot_w / n_visible;

    // 1. Draw borders
    draw_list->AddRect(plot_pos, ImVec2(plot_pos.x + plot_w, plot_pos.y + plot_h), IM_COL32(45, 45, 45, 255));
    draw_list->AddLine(ImVec2(plot_pos.x + plot_w, plot_pos.y), ImVec2(plot_pos.x + plot_w, plot_pos.y + plot_h), IM_COL32(45, 45, 45, 255));
    draw_list->AddLine(ImVec2(plot_pos.x, plot_pos.y + plot_h), ImVec2(plot_pos.x + plot_w, plot_pos.y + plot_h), IM_COL32(45, 45, 45, 255));

    // Clip rendering inside main price plot area
    draw_list->PushClipRect(plot_pos, ImVec2(plot_pos.x + plot_w, plot_pos.y + draw_plot_h), true);

    // Grid lines
    int grid_lines_y = 6;
    for (int j = 1; j < grid_lines_y; ++j) {
        double p = adjusted_min + (adj_range * j) / grid_lines_y;
        float gy = plot_pos.y + draw_plot_h * (1.0f - (float)((p - adjusted_min) / adj_range));
        draw_list->AddLine(ImVec2(plot_pos.x, gy), ImVec2(plot_pos.x + plot_w, gy), IM_COL32(28, 28, 28, 255));
    }

    int step = 2;
    if (n_visible > 150) step = 30;
    else if (n_visible > 80) step = 20;
    else if (n_visible > 40) step = 10;
    else if (n_visible > 20) step = 5;

    int first_aligned_idx = ((idx_start + step - 1) / step) * step;
    for (int raw_idx = first_aligned_idx; raw_idx < idx_end; raw_idx += step) {
        int i = raw_idx - idx_start;
        float gx = plot_pos.x + (i + 0.5f) * candle_width;
        draw_list->AddLine(ImVec2(gx, plot_pos.y), ImVec2(gx, plot_pos.y + draw_plot_h), IM_COL32(28, 28, 28, 255));
    }

    double max_visible_volume = 1.0;
    for (int i = check_start; i < check_end; ++i) {
        if (i < (int)active_chart->volumes.size() && active_chart->volumes[i] > max_visible_volume) {
            max_visible_volume = active_chart->volumes[i];
        }
    }

    // Candles & Volume
    for (int i = 0; i < n_visible; ++i)
    {
        int raw_idx = idx_start + i;
        if (raw_idx < 0) continue;
        if (raw_idx >= n_total) break;

        const auto& candle = active_chart->candles[raw_idx];

        float cx = plot_pos.x + (i + 0.5f) * candle_width;
        float cw = candle_width * 0.65f;

        float y_open = plot_pos.y + draw_plot_h * (1.0f - (float)((candle.open - adjusted_min) / adj_range));
        float y_close = plot_pos.y + draw_plot_h * (1.0f - (float)((candle.close - adjusted_min) / adj_range));
        float y_high = plot_pos.y + draw_plot_h * (1.0f - (float)((candle.high - adjusted_min) / adj_range));
        float y_low = plot_pos.y + draw_plot_h * (1.0f - (float)((candle.low - adjusted_min) / adj_range));

        ImU32 col = (candle.close >= candle.open) ? IM_COL32(38, 166, 154, 255) : IM_COL32(239, 83, 80, 255);

        draw_list->AddLine(ImVec2(cx, y_high), ImVec2(cx, y_low), col, 1.5f);

        float top_y = std::min(y_open, y_close);
        float bot_y = std::max(y_open, y_close);
        if (std::abs(top_y - bot_y) < 1.0f) bot_y = top_y + 1.0f;
        draw_list->AddRectFilled(ImVec2(cx - cw/2, top_y), ImVec2(cx + cw/2, bot_y), col);

        if (raw_idx < (int)active_chart->volumes.size()) {
            double vol = active_chart->volumes[raw_idx];
            float vol_ratio = (float)(vol / max_visible_volume);
            float vol_h = draw_plot_h * 0.18f * vol_ratio;
            ImU32 vol_col = (candle.close >= candle.open) ? IM_COL32(38, 166, 154, 75) : IM_COL32(239, 83, 80, 75);
            draw_list->AddRectFilled(
                ImVec2(cx - cw/2, plot_pos.y + draw_plot_h - vol_h),
                ImVec2(cx + cw/2, plot_pos.y + draw_plot_h),
                vol_col
            );
        }
    }

    // Draw active overlay indicators (SMA, EMA, Python)
    for (const auto& ind : active_chart->indicators) {
        if (!ind.enabled || ind.type == "RSI" || ind.values.size() < (size_t)n_total) continue;
        
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(ind.color[0], ind.color[1], ind.color[2], ind.color[3]));
        for (int i = 0; i < n_visible - 1; ++i) {
            int raw_idx1 = idx_start + i;
            int raw_idx2 = raw_idx1 + 1;
            if (raw_idx1 < 0) continue;
            if (raw_idx2 >= n_total) break;
            
            double val1 = ind.values[raw_idx1];
            double val2 = ind.values[raw_idx2];
            
            if (val1 > 0.0 && val2 > 0.0) {
                float cx1 = plot_pos.x + (i + 0.5f) * candle_width;
                float cx2 = plot_pos.x + (i + 1.5f) * candle_width;
                float cy1 = plot_pos.y + draw_plot_h * (1.0f - (float)((val1 - adjusted_min) / adj_range));
                float cy2 = plot_pos.y + draw_plot_h * (1.0f - (float)((val2 - adjusted_min) / adj_range));
                draw_list->AddLine(ImVec2(cx1, cy1), ImVec2(cx2, cy2), col, 1.5f);
            }
        }
    }

    // Current Price Line
    if (!active_chart->candles.empty()) {
        double current_price = active_chart->candles.back().close;
        float cy = plot_pos.y + draw_plot_h * (1.0f - (float)((current_price - adjusted_min) / adj_range));
        if (cy >= plot_pos.y && cy <= plot_pos.y + draw_plot_h) {
            const auto& last_c = active_chart->candles.back();
            ImU32 col = (last_c.close >= last_c.open) ? IM_COL32(38, 166, 154, 150) : IM_COL32(239, 83, 80, 150);
            draw_list->AddLine(ImVec2(plot_pos.x, cy), ImVec2(plot_pos.x + plot_w, cy), col, 1.0f);
        }
    }

    draw_list->PopClipRect();

    // RSI sub-plot panel drawing
    if (rsi_enabled) {
        float rsi_start_y = plot_pos.y + draw_plot_h + 10.0f;
        float rsi_h = 80.0f;
        
        draw_list->PushClipRect(ImVec2(plot_pos.x, rsi_start_y), ImVec2(plot_pos.x + plot_w, rsi_start_y + rsi_h), true);
        
        draw_list->AddRect(ImVec2(plot_pos.x, rsi_start_y), ImVec2(plot_pos.x + plot_w, rsi_start_y + rsi_h), IM_COL32(45, 45, 45, 255));
        
        float rsi_y30 = rsi_start_y + rsi_h * (1.0f - 30.0f / 100.0f);
        float rsi_y50 = rsi_start_y + rsi_h * (1.0f - 50.0f / 100.0f);
        float rsi_y70 = rsi_start_y + rsi_h * (1.0f - 70.0f / 100.0f);
        draw_list->AddLine(ImVec2(plot_pos.x, rsi_y30), ImVec2(plot_pos.x + plot_w, rsi_y30), IM_COL32(80, 80, 80, 80));
        draw_list->AddLine(ImVec2(plot_pos.x, rsi_y50), ImVec2(plot_pos.x + plot_w, rsi_y50), IM_COL32(80, 80, 80, 80));
        draw_list->AddLine(ImVec2(plot_pos.x, rsi_y70), ImVec2(plot_pos.x + plot_w, rsi_y70), IM_COL32(80, 80, 80, 80));
        
        if (rsi_ind && rsi_ind->values.size() >= (size_t)n_total) {
            ImU32 rsi_col = ImGui::ColorConvertFloat4ToU32(ImVec4(rsi_ind->color[0], rsi_ind->color[1], rsi_ind->color[2], rsi_ind->color[3]));
            for (int i = 0; i < n_visible - 1; ++i) {
                int raw_idx1 = idx_start + i;
                int raw_idx2 = raw_idx1 + 1;
                if (raw_idx1 < 0) continue;
                if (raw_idx2 >= n_total) break;
                
                double val1 = rsi_ind->values[raw_idx1];
                double val2 = rsi_ind->values[raw_idx2];
                
                float cx1 = plot_pos.x + (i + 0.5f) * candle_width;
                float cx2 = plot_pos.x + (i + 1.5f) * candle_width;
                float cy1 = rsi_start_y + rsi_h * (1.0f - (float)(val1 / 100.0f));
                float cy2 = rsi_start_y + rsi_h * (1.0f - (float)(val2 / 100.0f));
                draw_list->AddLine(ImVec2(cx1, cy1), ImVec2(cx2, cy2), rsi_col, 1.5f);
            }
        }
        
        draw_list->PopClipRect();
        
        draw_list->AddText(ImVec2(plot_pos.x + plot_w + 6.0f, rsi_y70 - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(180, 180, 180, 255), "70");
        draw_list->AddText(ImVec2(plot_pos.x + plot_w + 6.0f, rsi_y50 - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(180, 180, 180, 255), "50");
        draw_list->AddText(ImVec2(plot_pos.x + plot_w + 6.0f, rsi_y30 - ImGui::GetTextLineHeight() * 0.5f), IM_COL32(180, 180, 180, 255), "30");

        // Draw live RSI tracker line and badge
        if (rsi_ind && !rsi_ind->values.empty()) {
            double current_rsi = rsi_ind->values.back();
            float cy_current = rsi_start_y + rsi_h * (1.0f - (float)(current_rsi / 100.0f));
            
            draw_list->AddLine(ImVec2(plot_pos.x, cy_current), ImVec2(plot_pos.x + plot_w, cy_current), IM_COL32(rsi_ind->color[0] * 255, rsi_ind->color[1] * 255, rsi_ind->color[2] * 255, 120), 1.0f);
            
            char val_str[16];
            std::sprintf(val_str, "%.1f", current_rsi);
            ImVec2 t_size = ImGui::CalcTextSize(val_str);
            ImU32 rsi_col = ImGui::ColorConvertFloat4ToU32(ImVec4(rsi_ind->color[0], rsi_ind->color[1], rsi_ind->color[2], rsi_ind->color[3]));
            
            draw_list->AddRectFilled(
                ImVec2(plot_pos.x + plot_w, cy_current - t_size.y * 0.5f - 2.0f),
                ImVec2(plot_pos.x + plot_w + y_gutter_w - 2.0f, cy_current + t_size.y * 0.5f + 2.0f),
                rsi_col, 2.0f
            );
            draw_list->AddText(ImVec2(plot_pos.x + plot_w + 4.0f, cy_current - t_size.y * 0.5f), IM_COL32(255, 255, 255, 255), val_str);
        }
    }

    // 2. Draw Price Axis (Y-axis gutter elements)
    for (int j = 1; j < grid_lines_y; ++j) {
        double p = adjusted_min + (adj_range * j) / grid_lines_y;
        float gy = plot_pos.y + draw_plot_h * (1.0f - (float)((p - adjusted_min) / adj_range));

        draw_list->AddLine(ImVec2(plot_pos.x + plot_w, gy), ImVec2(plot_pos.x + plot_w + 4.0f, gy), IM_COL32(80, 80, 80, 255));

        char val_str[32];
        if (adj_range > 1000.0) std::sprintf(val_str, "%.0f", p);
        else if (adj_range > 10.0) std::sprintf(val_str, "%.1f", p);
        else if (adj_range > 0.1) std::sprintf(val_str, "%.2f", p);
        else if (adj_range > 0.001) std::sprintf(val_str, "%.4f", p);
        else std::sprintf(val_str, "%.6f", p);

        float text_y = gy - ImGui::GetTextLineHeight() * 0.5f;
        draw_list->AddText(ImVec2(plot_pos.x + plot_w + 6.0f, text_y), IM_COL32(180, 180, 180, 255), val_str);
    }

    // Current Price Badge
    if (!active_chart->candles.empty()) {
        double current_price = active_chart->candles.back().close;
        float cy = plot_pos.y + draw_plot_h * (1.0f - (float)((current_price - adjusted_min) / adj_range));
        if (cy >= plot_pos.y && cy <= plot_pos.y + draw_plot_h) {
            const auto& last_c = active_chart->candles.back();
            ImU32 badge_col = (last_c.close >= last_c.open) ? IM_COL32(38, 166, 154, 255) : IM_COL32(239, 83, 80, 255);

            char val_str[32];
            if (adj_range > 1000.0) std::sprintf(val_str, "%.1f", current_price);
            else if (adj_range > 10.0) std::sprintf(val_str, "%.2f", current_price);
            else std::sprintf(val_str, "%.4f", current_price);

            ImVec2 t_size = ImGui::CalcTextSize(val_str);
            float by_min = cy - t_size.y * 0.5f - 2.0f;
            float by_max = cy + t_size.y * 0.5f + 2.0f;

            draw_list->AddRectFilled(
                ImVec2(plot_pos.x + plot_w, by_min),
                ImVec2(plot_pos.x + plot_w + y_gutter_w - 2.0f, by_max),
                badge_col, 2.0f
            );
            draw_list->AddText(ImVec2(plot_pos.x + plot_w + 4.0f, cy - t_size.y * 0.5f), IM_COL32(255, 255, 255, 255), val_str);
        }
    }

    // 3. Draw Time Axis
    for (int raw_idx = first_aligned_idx; raw_idx < idx_end; raw_idx += step) {
        int i = raw_idx - idx_start;
        float gx = plot_pos.x + (i + 0.5f) * candle_width;

        draw_list->AddLine(ImVec2(gx, plot_pos.y + plot_h), ImVec2(gx, plot_pos.y + plot_h + 4.0f), IM_COL32(80, 80, 80, 255));

        uint64_t ts = 0;
        if (raw_idx >= 0 && raw_idx < n_total) {
            ts = active_chart->candles[raw_idx].timestamp;
        } else if (raw_idx >= n_total && !active_chart->candles.empty()) {
            uint64_t latest_ts = active_chart->candles.back().timestamp;
            uint64_t tf_ms = 60 * 1000;
            if (active_chart->timeframe == "5m") tf_ms = 5 * 60 * 1000;
            else if (active_chart->timeframe == "15m") tf_ms = 15 * 60 * 1000;
            else if (active_chart->timeframe == "1h") tf_ms = 60 * 60 * 1000;
            else if (active_chart->timeframe == "1d") tf_ms = 24 * 60 * 60 * 1000;
            ts = latest_ts + (raw_idx - (n_total - 1)) * tf_ms;
        } else if (raw_idx < 0 && !active_chart->candles.empty()) {
            uint64_t oldest_ts = active_chart->candles.front().timestamp;
            uint64_t tf_ms = 60 * 1000;
            if (active_chart->timeframe == "5m") tf_ms = 5 * 60 * 1000;
            else if (active_chart->timeframe == "15m") tf_ms = 15 * 60 * 1000;
            else if (active_chart->timeframe == "1h") tf_ms = 60 * 60 * 1000;
            else if (active_chart->timeframe == "1d") tf_ms = 24 * 60 * 60 * 1000;
            ts = oldest_ts + raw_idx * tf_ms;
        }

        if (ts != 0) {
            std::string time_str = FormatTimestamp(ts, active_chart->timeframe, false);
            ImVec2 t_size = ImGui::CalcTextSize(time_str.c_str());
            draw_list->AddText(ImVec2(gx - t_size.x * 0.5f, plot_pos.y + plot_h + 4.0f), IM_COL32(180, 180, 180, 255), time_str.c_str());
        }
    }

    // 4. Crosshair and Tooltip Badges
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;
    bool inside_plot = (mouse_pos.x >= plot_pos.x && mouse_pos.x <= plot_pos.x + plot_w &&
                        mouse_pos.y >= plot_pos.y && mouse_pos.y <= plot_pos.y + plot_h);
    if (inside_plot && plot_hovered) {
        draw_list->AddLine(ImVec2(plot_pos.x, mouse_pos.y), ImVec2(plot_pos.x + plot_w, mouse_pos.y), IM_COL32(200, 200, 200, 100), 1.0f);
        draw_list->AddLine(ImVec2(mouse_pos.x, plot_pos.y), ImVec2(mouse_pos.x, plot_pos.y + plot_h), IM_COL32(200, 200, 200, 100), 1.0f);

        // Y-axis crosshair label (Price chart or RSI pane)
        if (mouse_pos.y <= plot_pos.y + draw_plot_h) {
            double p = adjusted_max - (double)((mouse_pos.y - plot_pos.y) / draw_plot_h) * adj_range;
            char val_str[32];
            if (adj_range > 1000.0) std::sprintf(val_str, "%.1f", p);
            else if (adj_range > 10.0) std::sprintf(val_str, "%.2f", p);
            else std::sprintf(val_str, "%.4f", p);

            ImVec2 t_size = ImGui::CalcTextSize(val_str);
            draw_list->AddRectFilled(
                ImVec2(plot_pos.x + plot_w, mouse_pos.y - t_size.y * 0.5f - 2.0f),
                ImVec2(plot_pos.x + plot_w + y_gutter_w - 2.0f, mouse_pos.y + t_size.y * 0.5f + 2.0f),
                IM_COL32(33, 150, 243, 255), 2.0f
            );
            draw_list->AddText(ImVec2(plot_pos.x + plot_w + 4.0f, mouse_pos.y - t_size.y * 0.5f), IM_COL32(255, 255, 255, 255), val_str);
        } else if (rsi_enabled) {
            float rsi_start_y = plot_pos.y + draw_plot_h + 10.0f;
            float rsi_h = 80.0f;
            if (mouse_pos.y >= rsi_start_y && mouse_pos.y <= rsi_start_y + rsi_h) {
                double rsi_val = 100.0 * (1.0 - (double)((mouse_pos.y - rsi_start_y) / rsi_h));
                char val_str[32];
                std::sprintf(val_str, "%.1f", rsi_val);
                ImVec2 t_size = ImGui::CalcTextSize(val_str);
                draw_list->AddRectFilled(
                    ImVec2(plot_pos.x + plot_w, mouse_pos.y - t_size.y * 0.5f - 2.0f),
                    ImVec2(plot_pos.x + plot_w + y_gutter_w - 2.0f, mouse_pos.y + t_size.y * 0.5f + 2.0f),
                    IM_COL32(33, 150, 243, 255), 2.0f
                );
                draw_list->AddText(ImVec2(plot_pos.x + plot_w + 4.0f, mouse_pos.y - t_size.y * 0.5f), IM_COL32(255, 255, 255, 255), val_str);
            }
        }

        // X-axis crosshair label
        int candle_idx = (int)((mouse_pos.x - plot_pos.x) / candle_width);
        int raw_idx = idx_start + candle_idx;
        uint64_t ts = 0;
        if (raw_idx >= 0 && raw_idx < n_total) {
            ts = active_chart->candles[raw_idx].timestamp;
        } else if (raw_idx >= n_total && !active_chart->candles.empty()) {
            uint64_t latest_ts = active_chart->candles.back().timestamp;
            uint64_t tf_ms = 60 * 1000;
            if (active_chart->timeframe == "5m") tf_ms = 5 * 60 * 1000;
            else if (active_chart->timeframe == "15m") tf_ms = 15 * 60 * 1000;
            else if (active_chart->timeframe == "1h") tf_ms = 60 * 60 * 1000;
            else if (active_chart->timeframe == "1d") tf_ms = 24 * 60 * 60 * 1000;
            ts = latest_ts + (raw_idx - (n_total - 1)) * tf_ms;
        } else if (raw_idx < 0 && !active_chart->candles.empty()) {
            uint64_t oldest_ts = active_chart->candles.front().timestamp;
            uint64_t tf_ms = 60 * 1000;
            if (active_chart->timeframe == "5m") tf_ms = 5 * 60 * 1000;
            else if (active_chart->timeframe == "15m") tf_ms = 15 * 60 * 1000;
            else if (active_chart->timeframe == "1h") tf_ms = 60 * 60 * 1000;
            else if (active_chart->timeframe == "1d") tf_ms = 24 * 60 * 60 * 1000;
            ts = oldest_ts + raw_idx * tf_ms;
        }

        if (ts != 0) {
            std::string time_str = FormatTimestamp(ts, active_chart->timeframe, true);
            ImVec2 tx_size = ImGui::CalcTextSize(time_str.c_str());
            float bx_min = mouse_pos.x - tx_size.x * 0.5f - 4.0f;
            float bx_max = mouse_pos.x + tx_size.x * 0.5f + 4.0f;

            draw_list->AddRectFilled(
                ImVec2(bx_min, plot_pos.y + plot_h),
                ImVec2(bx_max, plot_pos.y + plot_h + x_gutter_h),
                IM_COL32(33, 150, 243, 255), 2.0f
            );
            draw_list->AddText(ImVec2(mouse_pos.x - tx_size.x * 0.5f, plot_pos.y + plot_h + 3.0f), IM_COL32(255, 255, 255, 255), time_str.c_str());
        }
    }

    // Draw Dashboard overlay text
    if (!active_chart->candles.empty()) {
        auto& last_candle = active_chart->candles.back();
        double current_price = last_candle.close;
        double net_change = current_price - active_chart->candles.front().open;
        double pct_change = (net_change / active_chart->candles.front().open) * 100.0;

        ImU32 text_col = (net_change >= 0) ? IM_COL32(38, 166, 154, 255) : IM_COL32(239, 83, 80, 255);

        std::string overlay_str = active_chart->ticker + " " + active_chart->timeframe + " | ";
        char price_buf[128];
        std::sprintf(price_buf, "O: %.2f H: %.2f L: %.2f C: %.2f", last_candle.open, last_candle.high, last_candle.low, last_candle.close);
        overlay_str += price_buf;
        
        for (const auto& ind : active_chart->indicators) {
            if (ind.enabled && !ind.values.empty() && ind.values.size() >= (size_t)n_total) {
                std::string label = ind.name;
                if (ind.type == "SMA" || ind.type == "EMA" || ind.type == "RSI") {
                    label = ind.type + "(" + std::to_string(ind.int_params.at("period")) + ")";
                }
                char ind_buf[64];
                std::sprintf(ind_buf, " | %s: %.2f", label.c_str(), ind.values.back());
                overlay_str += ind_buf;
            }
        }
        
        char pct_buf[32];
        std::sprintf(pct_buf, " | %.2f%%", pct_change);
        overlay_str += pct_buf;

        ImVec2 text_size = ImGui::CalcTextSize(overlay_str.c_str());

        draw_list->AddRectFilled(
            ImVec2(canvas_pos.x + 10.0f, canvas_pos.y + 10.0f),
            ImVec2(canvas_pos.x + 20.0f + text_size.x, canvas_pos.y + 20.0f + text_size.y),
            IM_COL32(15, 15, 15, 200), 4.0f
        );

        draw_list->AddText(ImVec2(canvas_pos.x + 15.0f, canvas_pos.y + 15.0f), text_col, overlay_str.c_str());
        
        // Draw Indicator Control Panel Overlay (collapsible legend in top-left)
        float legend_y = canvas_pos.y + 15.0f + text_size.y + 10.0f;
        if (!active_chart->indicators.empty()) {
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + 10.0f, legend_y));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.06f, 0.06f, 0.8f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
            
            if (active_chart->legend_collapsed) {
                // Collapsed state: just draw a small button to expand
                ImGui::BeginChild("##active_indicators_legend_collapsed", ImVec2(30.0f, 26.0f), true, ImGuiWindowFlags_NoScrollbar);
                if (ImGui::Button(">")) {
                    active_chart->legend_collapsed = false;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Expand Indicators Legend");
                }
                ImGui::EndChild();
            } else {
                // Expanded state: draw collapse button and full indicators list
                float child_h = (active_chart->indicators.size() + 1) * 26.0f + 10.0f;
                if (child_h > 180.0f) child_h = 180.0f;
                
                ImGui::BeginChild("##active_indicators_legend", ImVec2(240.0f, child_h), true, ImGuiWindowFlags_NoScrollbar);
                
                // Draw Collapse Button Row
                if (ImGui::Button("<")) {
                    active_chart->legend_collapsed = true;
                }
                ImGui::Separator();
                
                for (size_t idx = 0; idx < active_chart->indicators.size(); ++idx) {
                    auto& ind = active_chart->indicators[idx];
                    ImGui::PushID(idx);
                    
                    std::string label = ind.name;
                    if (ind.type == "SMA" || ind.type == "EMA" || ind.type == "RSI") {
                        label = ind.type + " (" + std::to_string(ind.int_params["period"]) + ")";
                    }
                    
                    ImGui::ColorButton("##ind_col", ImVec4(ind.color[0], ind.color[1], ind.color[2], ind.color[3]), ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));
                    ImGui::SameLine();
                    
                    ImGui::Text("%s", label.c_str());
                    ImGui::SameLine(100.0f);
                    
                    if (ImGui::Button(ind.enabled ? "Hide" : "Show")) {
                        ind.enabled = !ind.enabled;
                        IndicatorManager::RecalculateIndicators(*active_chart);
                    }
                    ImGui::SameLine();
                    
                    if (ImGui::Button("Set")) {
                        ImGui::OpenPopup("indicator_settings_popup");
                    }
                    if (ImGui::BeginPopup("indicator_settings_popup")) {
                        ImGui::Text("Edit Settings");
                        ImGui::Separator();
                        if (ind.type == "SMA" || ind.type == "EMA" || ind.type == "RSI") {
                            int p = ind.int_params["period"];
                            if (ImGui::SliderInt("Period", &p, 1, 200)) {
                                ind.int_params["period"] = p;
                                IndicatorManager::RecalculateIndicators(*active_chart);
                            }
                        }
                        ImGui::ColorEdit4("Line Color", ind.color);
                        ImGui::EndPopup();
                    }
                    ImGui::SameLine();
                    
                    if (ImGui::Button("X")) {
                        active_chart->indicators.erase(active_chart->indicators.begin() + idx);
                        idx--;
                        ImGui::PopID();
                        ImGui::EndChild();
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor();
                        break;
                    }
                    
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }
    }

    ImGui::End();
}

} // namespace quantum::ui
