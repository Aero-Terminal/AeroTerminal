#include "PanelTrades.h"
#include "core/AppContext.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <vector>

namespace quantum::ui {

void RenderTradesPanel(bool* p_open)
{
    if (!ImGui::Begin("Trades###trades_panel", p_open)) {
        ImGui::End();
        return;
    }

    auto& ctx = AppContext::instance();
    std::string symbol = ctx.active_symbol;
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

    ChartTab* active_chart = nullptr;
    for (auto& chart : ctx.active_charts) {
        std::string c_ticker = chart.ticker;
        std::transform(c_ticker.begin(), c_ticker.end(), c_ticker.begin(), ::toupper);
        if (c_ticker == symbol) {
            active_chart = &chart;
            break;
        }
    }

    if (!active_chart && !ctx.active_charts.empty()) {
        active_chart = &ctx.active_charts.front();
    }

    if (!active_chart) {
        ImGui::Text("No active charts found.");
        ImGui::End();
        return;
    }

    // Generate mock trade events in mock simulation mode
    if (!active_chart->is_streaming) {
        // Seed initial trades if empty
        if (active_chart->trades_log.empty()) {
            double last_price = 100.0;
            if (!active_chart->candles.empty()) {
                last_price = active_chart->candles.back().close;
            }
            
            for (int i = 0; i < 20; ++i) {
                time_t t_now = std::time(nullptr) - (20 - i) * 3;
                struct tm* timeinfo = std::localtime(&t_now);
                char time_str[16];
                std::strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
                
                double p_diff = (((double)std::rand() / RAND_MAX) - 0.5) * (last_price * 0.002);
                double qty = ((double)std::rand() / RAND_MAX) * 2.5 + 0.01;
                bool is_buy = (std::rand() % 2 == 0);
                
                active_chart->trades_log.push_back({ time_str, last_price + p_diff, qty, is_buy });
            }
        }
        
        // Randomly append a new trade (5% chance per frame) to simulate live updates
        static int tick_counter = 0;
        if (tick_counter++ % 15 == 0) {
            time_t t_now = std::time(nullptr);
            struct tm* timeinfo = std::localtime(&t_now);
            char time_str[16];
            std::strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
            
            double last_price = active_chart->candles.back().close;
            double p_diff = (((double)std::rand() / RAND_MAX) - 0.5) * (last_price * 0.001);
            double qty = ((double)std::rand() / RAND_MAX) * 1.5 + 0.005;
            bool is_buy = (std::rand() % 2 == 0);
            
            active_chart->trades_log.push_back({ time_str, last_price + p_diff, qty, is_buy });
            
            // Keep logs to latest 50
            if (active_chart->trades_log.size() > 50) {
                active_chart->trades_log.erase(active_chart->trades_log.begin());
            }
        }
    }

    ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.15f, 1.0f), "%s Live Trades log", active_chart->ticker.c_str());
    ImGui::Separator();

    // Render table
    if (ImGui::BeginTable("TradesLogTable", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        // Render trades in reverse order (newest at the top)
        for (int i = (int)active_chart->trades_log.size() - 1; i >= 0; --i) {
            const auto& trade = active_chart->trades_log[i];
            ImGui::TableNextRow();
            
            // Text color based on side
            ImVec4 text_col = trade.is_buy ? ImVec4(0.15f, 0.7f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);

            // Time
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(trade.time.c_str());

            // Price
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(text_col, "%.2f", trade.price);

            // Qty
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(text_col, "%.4f", trade.qty);

            // Side
            ImGui::TableSetColumnIndex(3);
            ImGui::TextColored(text_col, trade.is_buy ? "BUY" : "SELL");
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace quantum::ui
