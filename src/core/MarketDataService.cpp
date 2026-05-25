#include "MarketDataService.h"
#include <ixwebsocket/IXHttpClient.h>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>

namespace quantum {

MarketDataService::MarketDataService() {
}

MarketDataService::~MarketDataService() {
    Stop();
}

void MarketDataService::Start() {
    if (m_running) return;
    
    InitWebSocket();
    m_ws.start();
    m_running = true;
    std::cout << "MarketDataService started." << std::endl;
}

void MarketDataService::Stop() {
    if (!m_running) return;
    
    m_ws.stop();
    m_running = false;
    m_connected = false;
    std::cout << "MarketDataService stopped." << std::endl;
}

void MarketDataService::Subscribe(const std::string& symbol, const std::string& timeframe) {
    if (symbol.empty()) return;
    
    std::string upper_symbol = symbol;
    std::transform(upper_symbol.begin(), upper_symbol.end(), upper_symbol.begin(), ::toupper);
    
    std::string lower_symbol = symbol;
    std::transform(lower_symbol.begin(), lower_symbol.end(), lower_symbol.begin(), ::tolower);
    
    std::string kline_stream = lower_symbol + "@kline_" + timeframe;
    
    {
        std::lock_guard<std::mutex> lock(m_subscription_mutex);
        
        // Subscribe to kline if not already
        if (m_subscribed_klines.find(kline_stream) == m_subscribed_klines.end()) {
            m_subscribed_klines.insert(kline_stream);
            SendSubscribe(kline_stream);
        }
        
        // Handle symbol depth & trade subscriptions
        int current_count = m_symbol_ref_counts[upper_symbol];
        m_symbol_ref_counts[upper_symbol] = current_count + 1;
        
        if (current_count == 0) {
            SendSubscribe(lower_symbol + "@depth5@100ms");
            SendSubscribe(lower_symbol + "@trade");
        }
    }
    
    // Fetch history in background
    std::thread(&MarketDataService::FetchHistory, this, upper_symbol, timeframe).detach();
}

void MarketDataService::Unsubscribe(const std::string& symbol, const std::string& timeframe) {
    if (symbol.empty()) return;
    
    std::string upper_symbol = symbol;
    std::transform(upper_symbol.begin(), upper_symbol.end(), upper_symbol.begin(), ::toupper);
    
    std::string lower_symbol = symbol;
    std::transform(lower_symbol.begin(), lower_symbol.end(), lower_symbol.begin(), ::tolower);
    
    std::string kline_stream = lower_symbol + "@kline_" + timeframe;
    
    {
        std::lock_guard<std::mutex> lock(m_subscription_mutex);
        
        auto it = m_subscribed_klines.find(kline_stream);
        if (it != m_subscribed_klines.end()) {
            m_subscribed_klines.erase(it);
            SendUnsubscribe(kline_stream);
        }
        
        int current_count = m_symbol_ref_counts[upper_symbol];
        if (current_count > 0) {
            m_symbol_ref_counts[upper_symbol] = current_count - 1;
            if (current_count - 1 == 0) {
                SendUnsubscribe(lower_symbol + "@depth5@100ms");
                SendUnsubscribe(lower_symbol + "@trade");
            }
        }
    }
}

std::vector<MarketDataUpdate> MarketDataService::PollUpdates() {
    std::vector<MarketDataUpdate> updates;
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    while (!m_updates_queue.empty()) {
        updates.push_back(m_updates_queue.front());
        m_updates_queue.pop();
    }
    return updates;
}

void MarketDataService::InitWebSocket() {
    // Connect to Binance combined streams URL
    m_ws.setUrl("wss://stream.binance.com:9443/stream");
    
    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto j = nlohmann::json::parse(msg->str);
                
                if (j.contains("stream") && j.contains("data")) {
                    std::string stream = j["stream"];
                    auto data = j["data"];
                    
                    if (stream.find("@kline_") != std::string::npos) {
                        std::string symbol = data["s"];
                        auto k = data["k"];
                        
                        TickerTick tick;
                        tick.symbol = symbol;
                        tick.open = std::stod(k["o"].get<std::string>());
                        tick.close = std::stod(k["c"].get<std::string>());
                        tick.high = std::stod(k["h"].get<std::string>());
                        tick.low = std::stod(k["l"].get<std::string>());
                        tick.is_candle_closed = k["x"].get<bool>();
                        tick.timestamp = k["t"].get<uint64_t>();
                        
                        MarketDataUpdate update;
                        update.type = MarketDataUpdate::Tick;
                        update.symbol = symbol;
                        
                        // Extract timeframe from stream name: e.g. "btcusdt@kline_1m" -> "1m"
                        size_t pos = stream.find("@kline_");
                        if (pos != std::string::npos) {
                            update.timeframe = stream.substr(pos + 7);
                        }
                        
                        update.tick = tick;
                        update.tick_volume = std::stod(k["v"].get<std::string>());
                        
                        PushUpdate(update);
                    }
                    else if (stream.find("@depth5") != std::string::npos) {
                        // Extract symbol name from stream: e.g. "btcusdt@depth5@100ms" -> "BTCUSDT"
                        size_t pos = stream.find("@");
                        std::string symbol = (pos != std::string::npos) ? stream.substr(0, pos) : "";
                        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::toupper);
                        
                        std::vector<OrderBookItem> bids;
                        for (const auto& item : data["bids"]) {
                            bids.push_back({ std::stod(item[0].get<std::string>()), std::stod(item[1].get<std::string>()) });
                        }
                        
                        std::vector<OrderBookItem> asks;
                        for (const auto& item : data["asks"]) {
                            asks.push_back({ std::stod(item[0].get<std::string>()), std::stod(item[1].get<std::string>()) });
                        }
                        
                        MarketDataUpdate update;
                        update.type = MarketDataUpdate::Depth;
                        update.symbol = symbol;
                        update.bids = bids;
                        update.asks = asks;
                        PushUpdate(update);
                    }
                    else if (stream.find("@trade") != std::string::npos) {
                        std::string symbol = data["s"];
                        
                        // Format time
                        uint64_t t_ms = data["T"].get<uint64_t>();
                        time_t t_sec = t_ms / 1000;
                        struct tm* timeinfo = std::localtime(&t_sec);
                        char time_str[16];
                        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", timeinfo);
                        
                        PublicTradeItem trade;
                        trade.time = time_str;
                        trade.price = std::stod(data["p"].get<std::string>());
                        trade.qty = std::stod(data["q"].get<std::string>());
                        trade.is_buy = !data["m"].get<bool>(); // m = true is buyer is maker (Sell)
                        
                        MarketDataUpdate update;
                        update.type = MarketDataUpdate::Trade;
                        update.symbol = symbol;
                        update.trade = trade;
                        PushUpdate(update);
                    }
                }
            } catch (const std::exception& e) {
                // Ignore parse errors on subscription responses
            }
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            m_connected = true;
            std::cout << "WebSocket connected to Binance combined streams!" << std::endl;
            
            // Re-subscribe to all active klines, depths, and trades
            std::lock_guard<std::mutex> lock(m_subscription_mutex);
            for (const auto& kline : m_subscribed_klines) {
                SendSubscribe(kline);
            }
            for (const auto& pair : m_symbol_ref_counts) {
                if (pair.second > 0) {
                    std::string lower_symbol = pair.first;
                    std::transform(lower_symbol.begin(), lower_symbol.end(), lower_symbol.begin(), ::tolower);
                    SendSubscribe(lower_symbol + "@depth5@100ms");
                    SendSubscribe(lower_symbol + "@trade");
                }
            }
        } else if (msg->type == ix::WebSocketMessageType::Close || msg->type == ix::WebSocketMessageType::Error) {
            m_connected = false;
            std::cout << "WebSocket disconnected/error: " << msg->errorInfo.reason << std::endl;
        }
    });
}

void MarketDataService::SendSubscribe(const std::string& param) {
    if (!m_connected) return;
    
    nlohmann::json j;
    j["method"] = "SUBSCRIBE";
    j["params"] = { param };
    j["id"] = 1;
    
    m_ws.send(j.dump());
}

void MarketDataService::SendUnsubscribe(const std::string& param) {
    if (!m_connected) return;
    
    nlohmann::json j;
    j["method"] = "UNSUBSCRIBE";
    j["params"] = { param };
    j["id"] = 2;
    
    m_ws.send(j.dump());
}

void MarketDataService::FetchHistory(const std::string& symbol, const std::string& timeframe) {
    std::string upper_symbol = symbol;
    std::transform(upper_symbol.begin(), upper_symbol.end(), upper_symbol.begin(), ::toupper);
    
    ix::HttpClient httpClient;
    auto args = httpClient.createRequest();
    args->connectTimeout = 10;

    std::vector<ChartCandle> all_candles;
    std::vector<double> all_volumes;
    uint64_t end_time = 0;

    // Fetch up to 5 pages of 1000 candles each (5000 candles total) to allow deep scrolling
    for (int page = 0; page < 5; ++page) {
        std::string url = "https://api.binance.com/api/v3/klines?symbol=" + upper_symbol + "&interval=" + timeframe + "&limit=1000";
        if (end_time > 0) {
            url += "&endTime=" + std::to_string(end_time);
        }

        auto response = httpClient.get(url, args);
        if (response->statusCode == 200) {
            try {
                auto j = nlohmann::json::parse(response->body);
                if (j.empty() || !j.is_array()) break;

                std::vector<ChartCandle> page_candles;
                std::vector<double> page_volumes;

                for (const auto& item : j) {
                    ChartCandle candle;
                    candle.open = std::stod(item[1].get<std::string>());
                    candle.high = std::stod(item[2].get<std::string>());
                    candle.low = std::stod(item[3].get<std::string>());
                    candle.close = std::stod(item[4].get<std::string>());
                    candle.timestamp = item[0].get<uint64_t>();

                    double volume = std::stod(item[5].get<std::string>());

                    page_candles.push_back(candle);
                    page_volumes.push_back(volume);
                }

                if (page_candles.empty()) break;

                // Capture timestamp of oldest candle on this page for the next iteration's end_time
                end_time = page_candles.front().timestamp - 1;

                // Prepend the page candles (older) before the ones we fetched so far (newer)
                all_candles.insert(all_candles.begin(), page_candles.begin(), page_candles.end());
                all_volumes.insert(all_volumes.begin(), page_volumes.begin(), page_volumes.end());

            } catch (const std::exception& e) {
                std::cerr << "Error parsing kline page: " << e.what() << std::endl;
                break;
            }
        } else {
            std::cerr << "HTTP page request failed for " << upper_symbol << " (Code: " << response->statusCode << ")" << std::endl;
            break;
        }
    }

    if (!all_candles.empty()) {
        MarketDataUpdate update;
        update.type = MarketDataUpdate::History;
        update.symbol = upper_symbol;
        update.timeframe = timeframe;
        update.candles = all_candles;
        update.volumes = all_volumes;
        
        PushUpdate(update);
        std::cout << "Fetched " << all_candles.size() << " total historical candles for " << upper_symbol << " (" << timeframe << ")" << std::endl;
    }
}

void MarketDataService::PushUpdate(const MarketDataUpdate& update) {
    std::lock_guard<std::mutex> lock(m_queue_mutex);
    m_updates_queue.push(update);
}

} // namespace quantum
