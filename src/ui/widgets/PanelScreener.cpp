#include "PanelScreener.h"
#include "core/AppContext.h"
#include "core/MarketDataService.h"
#include "imgui.h"
#include <ixwebsocket/IXHttpClient.h>
#include <algorithm>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <iostream>

#define ICON_FA_SEARCH "\xef\x80\x82"

namespace quantum::ui {

struct ScreenerItem {
    std::string symbol;
    double price_change_percent = 0.0;
    double last_price = 0.0;
    double high_price = 0.0;
    double low_price = 0.0;
    double volume = 0.0;
};

static std::vector<ScreenerItem> s_screener_items;
static std::mutex s_screener_mutex;
static bool s_screener_loaded = false;
static bool s_fetching = false;

static void FetchScreenerData() {
    if (s_fetching) return;
    s_fetching = true;
    
    std::thread([]() {
        ix::HttpClient httpClient;
        // Request popular tickers
        std::string url = "https://api.binance.com/api/v3/ticker/24hr?symbols=%5B%22BTCUSDT%22,%22ETHUSDT%22,%22SOLUSDT%22,%22BNBUSDT%22,%22ADAUSDT%22,%22DOGEUSDT%22,%22XRPUSDT%22,%22LTCUSDT%22,%22LINKUSDT%22,%22DOTUSDT%22%5D";
        
        auto args = httpClient.createRequest();
        args->connectTimeout = 8;
        
        auto response = httpClient.get(url, args);
        if (response->statusCode == 200) {
            try {
                auto j = nlohmann::json::parse(response->body);
                std::vector<ScreenerItem> temp_items;
                for (const auto& item : j) {
                    ScreenerItem s;
                    s.symbol = item["symbol"];
                    s.price_change_percent = std::stod(item["priceChangePercent"].get<std::string>());
                    s.last_price = std::stod(item["lastPrice"].get<std::string>());
                    s.high_price = std::stod(item["highPrice"].get<std::string>());
                    s.low_price = std::stod(item["lowPrice"].get<std::string>());
                    s.volume = std::stod(item["volume"].get<std::string>());
                    temp_items.push_back(s);
                }
                
                std::lock_guard<std::mutex> lock(s_screener_mutex);
                s_screener_items = temp_items;
                s_screener_loaded = true;
            } catch (const std::exception& e) {
                std::cerr << "Screener JSON parse error: " << e.what() << std::endl;
            }
        }
        s_fetching = false;
    }).detach();
}

void RenderScreenerPanel(bool* p_open)
{
    if (!ImGui::Begin("Screener###screener_panel", p_open)) {
        ImGui::End();
        return;
    }

    // Trigger initial fetch or periodic fetch
    static float timer = 0.0f;
    timer += ImGui::GetIO().DeltaTime;
    if (!s_screener_loaded && !s_fetching) {
        // Load mock values first so the panel is never empty
        std::lock_guard<std::mutex> lock(s_screener_mutex);
        if (s_screener_items.empty()) {
            s_screener_items = {
                { "BTCUSDT", 2.45, 68520.00, 69100.00, 67200.00, 24500.0 },
                { "ETHUSDT", -1.20, 3820.50, 3910.00, 3780.00, 158000.0 },
                { "SOLUSDT", 5.67, 168.45, 172.00, 158.50, 890000.0 },
                { "BNBUSDT", 0.35, 595.20, 602.00, 590.00, 75000.0 },
                { "ADAUSDT", -2.15, 0.4820, 0.4980, 0.4750, 12000000.0 },
                { "DOGEUSDT", 8.92, 0.1645, 0.1720, 0.1510, 45000000.0 },
                { "XRPUSDT", 0.10, 0.5230, 0.5310, 0.5180, 8900000.0 },
                { "LTCUSDT", 1.88, 84.60, 85.90, 82.50, 145000.0 },
                { "LINKUSDT", 3.12, 18.25, 18.90, 17.50, 230000.0 },
                { "DOTUSDT", -0.45, 6.85, 7.10, 6.70, 420000.0 }
            };
        }
        FetchScreenerData();
    }
    // Refresh every 20 seconds
    if (timer > 20.0f) {
        timer = 0.0f;
        FetchScreenerData();
    }

    // Top search bar
    static char search_buf[64] = "";
    ImGui::AlignTextToFramePadding();
    ImGui::Text(ICON_FA_SEARCH);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
    ImGui::InputTextWithHint("##screener_search", "Search...", search_buf, sizeof(search_buf));
    
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        FetchScreenerData();
    }
    
    ImGui::Separator();

    std::vector<ScreenerItem> items_to_show;
    {
        std::lock_guard<std::mutex> lock(s_screener_mutex);
        items_to_show = s_screener_items;
    }

    std::string search_str = search_buf;
    std::transform(search_str.begin(), search_str.end(), search_str.begin(), ::toupper);

    if (ImGui::BeginTable("ScreenerTable", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Change 24h", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Last Price", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("High 24h", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Low 24h", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Volume", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (const auto& item : items_to_show) {
            if (!search_str.empty()) {
                std::string item_sym = item.symbol;
                std::transform(item_sym.begin(), item_sym.end(), item_sym.begin(), ::toupper);
                if (item_sym.find(search_str) == std::string::npos) {
                    continue;
                }
            }

            ImGui::TableNextRow();
            
            // Highlight active symbol row
            bool is_active_sym = (AppContext::instance().active_symbol == item.symbol);
            if (is_active_sym) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderActive));
            }

            // Symbol (acts as select button)
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(item.symbol.c_str(), is_active_sym, ImGuiSelectableFlags_SpanAllColumns)) {
                // Click handler - sync workspace active symbol
                auto& ctx = AppContext::instance();
                std::string prev_symbol = ctx.active_symbol;
                ctx.active_symbol = item.symbol;
                
                // Update active chart symbol
                if (!ctx.active_charts.empty()) {
                    for (auto& chart : ctx.active_charts) {
                        if (chart.open) {
                            if (chart.ticker != item.symbol) {
                                MarketDataService::instance().Unsubscribe(chart.ticker, chart.timeframe);
                                chart.ticker = item.symbol;
                                chart.initialized = false;
                                chart.is_streaming = false;
                                chart.candles.clear();
                                chart.volumes.clear();
                                chart.bids.clear();
                                chart.asks.clear();
                                chart.trades_log.clear();
                                MarketDataService::instance().Subscribe(item.symbol, chart.timeframe);
                            }
                            break;
                        }
                    }
                }
            }

            // Change 24h
            ImGui::TableSetColumnIndex(1);
            ImVec4 col = (item.price_change_percent >= 0.0) ? ImVec4(0.15f, 0.7f, 0.4f, 1.0f) : ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
            ImGui::TextColored(col, "%+.2f%%", item.price_change_percent);

            // Last Price
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2f", item.last_price);

            // High 24h
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f", item.high_price);

            // Low 24h
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.2f", item.low_price);

            // Volume
            ImGui::TableSetColumnIndex(5);
            if (item.volume >= 1000000.0) {
                ImGui::Text("%.2fM", item.volume / 1000000.0);
            } else if (item.volume >= 1000.0) {
                ImGui::Text("%.2fk", item.volume / 1000.0);
            } else {
                ImGui::Text("%.2f", item.volume);
            }
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace quantum::ui
