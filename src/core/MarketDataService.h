#pragma once
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <ixwebsocket/IXWebSocket.h>
#include "core/AppContext.h"

namespace quantum {

struct TickerTick {
    std::string symbol;
    double open = 0.0;
    double close = 0.0;
    double high = 0.0;
    double low = 0.0;
    bool is_candle_closed = false;
    uint64_t timestamp = 0;
};

struct MarketDataUpdate {
    enum Type {
        History,
        Tick,
        Depth,
        Trade
    } type;
    
    std::string symbol;
    std::string timeframe;
    
    // For history
    std::vector<ChartCandle> candles;
    std::vector<double> volumes;
    
    // For live tick
    TickerTick tick;
    double tick_volume = 0.0;
    
    // For depth
    std::vector<OrderBookItem> bids;
    std::vector<OrderBookItem> asks;
    
    // For trade
    PublicTradeItem trade;
};

class MarketDataService {
public:
    static MarketDataService& instance() {
        static MarketDataService inst;
        return inst;
    }

    void Start();
    void Stop();

    void Subscribe(const std::string& symbol, const std::string& timeframe);
    void Unsubscribe(const std::string& symbol, const std::string& timeframe);

    // Poll updates from the UI thread
    std::vector<MarketDataUpdate> PollUpdates();

    bool IsConnected() const { return m_connected; }

private:
    MarketDataService();
    ~MarketDataService();

    void InitWebSocket();
    void SendSubscribe(const std::string& param);
    void SendUnsubscribe(const std::string& param);
    void FetchHistory(const std::string& symbol, const std::string& timeframe);
    void PushUpdate(const MarketDataUpdate& update);

    ix::WebSocket m_ws;
    bool m_connected = false;
    bool m_running = false;

    // Track active kline subscriptions (e.g. "btcusdt@kline_1m")
    std::unordered_set<std::string> m_subscribed_klines;
    
    // Track active symbol subscription counts to manage depth & trade streams
    std::unordered_map<std::string, int> m_symbol_ref_counts;
    
    mutable std::mutex m_subscription_mutex;

    std::queue<MarketDataUpdate> m_updates_queue;
    mutable std::mutex m_queue_mutex;
};

} // namespace quantum
