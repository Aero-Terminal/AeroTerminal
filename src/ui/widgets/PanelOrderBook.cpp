#include "PanelOrderBook.h"
#include "core/AppContext.h"
#include "core/MarketDataService.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <iostream>

#define ICON_FA_LADDER "\xef\x89\x82" // FontAwesome icon fallback

namespace quantum::ui {

void RenderOrderBookPanel(bool* p_open)
{
    if (!ImGui::Begin("Orderbook###orderbook_panel", p_open)) {
        ImGui::End();
        return;
    }

    auto& ctx = AppContext::instance();
    std::string symbol = ctx.active_symbol;
    std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);

    // Find the chart tab for this symbol to read its order book data
    ChartTab* active_chart = nullptr;
    for (auto& chart : ctx.active_charts) {
        std::string c_ticker = chart.ticker;
        std::transform(c_ticker.begin(), c_ticker.end(), c_ticker.begin(), ::toupper);
        if (c_ticker == symbol) {
            active_chart = &chart;
            break;
        }
    }

    // Default to first active chart if no symbol match
    if (!active_chart && !ctx.active_charts.empty()) {
        active_chart = &ctx.active_charts.front();
    }

    if (!active_chart) {
        ImGui::Text("No active charts found.");
        ImGui::End();
        return;
    }

    // Check if we have order book data. If not, generate mock data centered around the last price
    std::vector<OrderBookItem> bids = active_chart->bids;
    std::vector<OrderBookItem> asks = active_chart->asks;

    if (bids.empty() || asks.empty()) {
        double center_price = 100.0;
        if (!active_chart->candles.empty()) {
            center_price = active_chart->candles.back().close;
        }
        
        // Mock 10 levels
        double tick_size = center_price * 0.0002;
        if (tick_size < 0.01) tick_size = 0.01;
        
        bids.clear();
        asks.clear();
        // Seed with a stable pseudo-random generator
        unsigned int seed = 42;
        for (char c : active_chart->ticker) seed = seed * 31 + c;
        std::srand(seed);

        for (int i = 0; i < 8; ++i) {
            double bid_p = center_price - (i + 1) * tick_size;
            double ask_p = center_price + (i + 1) * tick_size;
            
            double bid_q = ((double)std::rand() / RAND_MAX) * 15.0 + 0.1;
            double ask_q = ((double)std::rand() / RAND_MAX) * 15.0 + 0.1;
            
            bids.push_back({ bid_p, bid_q });
            asks.push_back({ ask_p, ask_q });
        }
    }

    // Calculate maximum quantity for relative horizontal volume bar scaling
    double max_qty = 1e-6;
    double total_bid_qty = 0.0;
    double total_ask_qty = 0.0;
    
    for (const auto& item : bids) {
        if (item.qty > max_qty) max_qty = item.qty;
        total_bid_qty += item.qty;
    }
    for (const auto& item : asks) {
        if (item.qty > max_qty) max_qty = item.qty;
        total_ask_qty += item.qty;
    }

    // Header info
    ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.15f, 1.0f), "%s Live Book", active_chart->ticker.c_str());
    ImGui::Separator();

    // RENDER LADDER TABLE
    // Price | Qty | Size Bar
    float table_height = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() - 10.0f;
    if (ImGui::BeginChild("LadderScroll", ImVec2(0, table_height))) {
        ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoKeepColumnsVisible;
        if (ImGui::BeginTable("OrderBookTable", 3, flags)) {
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Depth", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Render asks in descending order (highest price at the top)
            for (int i = (int)asks.size() - 1; i >= 0; --i) {
                const auto& item = asks[i];
                ImGui::TableNextRow();
                
                // Price column
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%.2f", item.price);

                // Qty column
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", item.qty);

                // Graphical Depth column
                ImGui::TableSetColumnIndex(2);
                ImVec2 cell_pos = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                float height = ImGui::GetFrameHeight();
                float fill_width = width * (float)(item.qty / max_qty);
                
                ImGui::GetWindowDrawList()->AddRectFilled(
                    cell_pos, 
                    ImVec2(cell_pos.x + fill_width, cell_pos.y + height - 2.0f), 
                    IM_COL32(239, 83, 80, 50),
                    2.0f
                );
            }

            // Render Midpoint/Spread Row
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            double midpoint = (asks.front().price + bids.front().price) / 2.0;
            ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.1f, 1.0f), "%.2f", midpoint);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SPREAD");
            ImGui::TableSetColumnIndex(2);
            double spread = std::abs(asks.front().price - bids.front().price);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.2f (%.4f%%)", spread, (spread / midpoint) * 100.0);

            // Render bids in descending order (highest price at the top)
            for (size_t i = 0; i < bids.size(); ++i) {
                const auto& item = bids[i];
                ImGui::TableNextRow();
                
                // Price column
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.15f, 0.7f, 0.4f, 1.0f), "%.2f", item.price);

                // Qty column
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", item.qty);

                // Graphical Depth column
                ImGui::TableSetColumnIndex(2);
                ImVec2 cell_pos = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                float height = ImGui::GetFrameHeight();
                float fill_width = width * (float)(item.qty / max_qty);
                
                ImGui::GetWindowDrawList()->AddRectFilled(
                    cell_pos, 
                    ImVec2(cell_pos.x + fill_width, cell_pos.y + height - 2.0f), 
                    IM_COL32(38, 166, 154, 50),
                    2.0f
                );
            }

            ImGui::EndTable();
        }
        ImGui::EndChild();
    }

    // Draw buy/sell ratio bar at the bottom
    double total_qty = total_bid_qty + total_ask_qty;
    float bid_pct = (total_qty > 0.0) ? (float)(total_bid_qty / total_qty) : 0.5f;
    float ask_pct = 1.0f - bid_pct;

    ImVec2 bar_pos = ImGui::GetCursorScreenPos();
    float bar_width = ImGui::GetContentRegionAvail().x;
    float bar_height = 16.0f;

    ImGui::GetWindowDrawList()->AddRectFilled(
        bar_pos, 
        ImVec2(bar_pos.x + bar_width * bid_pct, bar_pos.y + bar_height), 
        IM_COL32(38, 166, 154, 200),
        4.0f,
        ImDrawFlags_RoundCornersLeft
    );
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(bar_pos.x + bar_width * bid_pct, bar_pos.y), 
        ImVec2(bar_pos.x + bar_width, bar_pos.y + bar_height), 
        IM_COL32(239, 83, 80, 200),
        4.0f,
        ImDrawFlags_RoundCornersRight
    );

    char ratio_str[64];
    std::sprintf(ratio_str, "Bids: %d%%  |  Asks: %d%%", (int)(bid_pct * 100.f), (int)(ask_pct * 100.f));
    ImVec2 text_sz = ImGui::CalcTextSize(ratio_str);
    ImGui::GetWindowDrawList()->AddText(
        ImVec2(bar_pos.x + (bar_width - text_sz.x) / 2.f, bar_pos.y + (bar_height - text_sz.y) / 2.f), 
        IM_COL32(255, 255, 255, 255), 
        ratio_str
    );

    ImGui::Dummy(ImVec2(0, bar_height)); // Occupy space for layout

    ImGui::End();
}

} // namespace quantum::ui
