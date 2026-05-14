#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include "core/json.hpp"

#ifdef _WIN32
#include <direct.h>
inline void create_directory(const std::string& path) {
    _mkdir(path.c_str());
}
#else
#include <sys/stat.h>
inline void create_directory(const std::string& path) {
    mkdir(path.c_str(), 0777);
}
#endif

namespace quantum {


struct ChatMessage {
    std::string sender;
    std::string text;
    bool is_ai;
};

struct JournalTrade {
    std::string ticker;
    std::string direction; // "BUY" or "SELL"
    double entry_price;
    double exit_price;
    double quantity;
    double pnl;
    std::string notes;
};

struct ChartCandle {
    double open, close, high, low;
    uint64_t timestamp = 0;
};

struct ActiveIndicator {
    std::string type; // "SMA", "EMA", "RSI"
    std::string name; // For display
    bool enabled = true;
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::unordered_map<std::string, int> int_params;
    
    // Calculated values
    std::vector<double> values;
};

struct OrderBookItem {
    double price;
    double qty;
};

struct PublicTradeItem {
    std::string time;
    double price;
    double qty;
    bool is_buy;
};

struct ChartTab {
    int id;
    std::string ticker;
    bool open = true;
    std::vector<ChartCandle> candles;
    std::vector<double> volumes;
    bool initialized = false;
    bool is_streaming = false;
    
    // Zoom and panning
    std::string timeframe = "1m";
    int visible_candles = 60;
    int scroll_offset = 0;
    
    // Axis adjustments
    float y_scale_factor = 1.0f;
    float y_offset_factor = 0.0f;
    
    // Active Indicators
    std::vector<ActiveIndicator> indicators;
    bool legend_collapsed = false;
    
    // Live Order book and Trades
    std::vector<OrderBookItem> bids;
    std::vector<OrderBookItem> asks;
    std::vector<PublicTradeItem> trades_log;

    void AddDefaultIndicators() {
        ActiveIndicator sma;
        sma.type = "SMA";
        sma.name = "SMA (20)";
        sma.enabled = true;
        sma.color[0] = 1.0f; sma.color[1] = 0.92f; sma.color[2] = 0.23f; sma.color[3] = 1.0f; // Yellow
        sma.int_params["period"] = 20;
        
        ActiveIndicator ema;
        ema.type = "EMA";
        ema.name = "EMA (9)";
        ema.enabled = true;
        ema.color[0] = 0.91f; ema.color[1] = 0.12f; ema.color[2] = 0.39f; ema.color[3] = 1.0f; // Magenta
        ema.int_params["period"] = 9;
        
        indicators.push_back(sma);
        indicators.push_back(ema);
    }
};

class AppContext {
public:
    static AppContext& instance() {
        static AppContext inst;
        return inst;
    }

    void* main_window_handle = nullptr; // Win32 HWND handle for webview pairing

    std::vector<ChatMessage> chat_history;
    std::vector<JournalTrade> journal_trades;
    bool is_replay_mode = false;
    float replay_speed = 1.0f;

    // Singleton panel visibility toggles
    bool show_execution = true;
    bool show_saathi = true;
    bool show_journal = true;
    bool show_calendar = true;
    bool show_whiteboard = true;
    bool show_orderbook = true;
    bool show_trades = true;
    bool show_screener = true;

    std::string active_symbol = "BTCUSDT";

    // Dynamic Chart tabs
    std::vector<ChartTab> active_charts;
    int next_chart_id = 2;

    // Calendar state
    std::unordered_map<std::string, std::string> daily_notes;
    int selected_year = 2026;
    int selected_month = 5;  // 1-12
    int selected_day = 27;   // 1-31



    void SaveNotes() {
        create_directory("storage");
        nlohmann::json j = daily_notes;
        std::ofstream file("storage/daily_notes.json");
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

    void LoadNotes() {
        std::ifstream file("storage/daily_notes.json");
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                daily_notes = j.get<std::unordered_map<std::string, std::string>>();
            } catch (const std::exception& e) {
                std::cerr << "Error loading daily notes: " << e.what() << std::endl;
            }
        }
    }



private:
    AppContext() {
        // Initialize mock data
        chat_history.push_back({"Saathi", "Welcome to AeroTerminal! I am Saathi, your trading workflow assistant. How can I assist you with your trading thesis today?", true});
        
        journal_trades.push_back({"AAPL", "BUY", 175.20, 182.50, 50, 365.00, "Broke out of daily bull flag. Clean follow through."});
        journal_trades.push_back({"TSLA", "SELL", 220.40, 215.10, 30, 159.00, "Short trade on resistance failure. Target hit."});
        journal_trades.push_back({"NVDA", "BUY", 890.00, 882.00, 10, -80.00, "Failed breakout attempt. Stopped out."});

        // Default chart tab
        ChartTab default_tab{1, "BTCUSDT", true};
        default_tab.AddDefaultIndicators();
        active_charts.push_back(default_tab);

        // Set default calendar date to current local time (May 27, 2026 for now)
        selected_year = 2026;
        selected_month = 5;
        selected_day = 27;

        // Try loading saved data
        LoadNotes();
    }
};

} // namespace quantum
