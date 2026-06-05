#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include "core/json.hpp"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <wincrypt.h>
inline void create_directory(const std::string& path) {
    _mkdir(path.c_str());
}
inline bool directory_exists(const std::string& path) {
    DWORD attrib = GetFileAttributesA(path.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}
#else
#include <sys/stat.h>
inline void create_directory(const std::string& path) {
    mkdir(path.c_str(), 0777);
}
inline bool directory_exists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) return false;
    return (info.st_mode & S_IFDIR);
}
#endif

inline void create_directory_recursive(const std::string& path) {
    size_t pos = 0;
    do {
        pos = path.find_first_of("\\/", pos + 1);
        std::string sub = path.substr(0, pos);
        if (!sub.empty() && sub != "." && sub != "..") {
            create_directory(sub);
        }
    } while (pos != std::string::npos);
}

#ifdef _WIN32
inline std::string EncryptAPIKey(const std::string& plain_key) {
    DATA_BLOB data_in;
    data_in.pbData = (BYTE*)plain_key.data();
    data_in.cbData = (DWORD)plain_key.size();

    DATA_BLOB data_out;
    if (CryptProtectData(&data_in, L"AeroTerminal API Key", NULL, NULL, NULL, 0, &data_out)) {
        std::string hex_encoded = "";
        char buf[3];
        for (DWORD i = 0; i < data_out.cbData; i++) {
            sprintf(buf, "%02x", data_out.pbData[i]);
            hex_encoded += buf;
        }
        LocalFree(data_out.pbData);
        return hex_encoded;
    }
    return plain_key;
}

inline std::string DecryptAPIKey(const std::string& hex_key) {
    if (hex_key.empty()) return "";
    if (hex_key.length() % 2 != 0) return hex_key;
    
    std::vector<BYTE> encrypted_bytes;
    for (size_t i = 0; i < hex_key.length(); i += 2) {
        std::string byteString = hex_key.substr(i, 2);
        BYTE byte = (BYTE)strtol(byteString.c_str(), NULL, 16);
        encrypted_bytes.push_back(byte);
    }

    DATA_BLOB data_in;
    data_in.pbData = encrypted_bytes.data();
    data_in.cbData = (DWORD)encrypted_bytes.size();

    DATA_BLOB data_out;
    if (CryptUnprotectData(&data_in, NULL, NULL, NULL, NULL, 0, &data_out)) {
        std::string decrypted((char*)data_out.pbData, data_out.cbData);
        LocalFree(data_out.pbData);
        return decrypted;
    }
    return hex_key;
}
#else
inline std::string EncryptAPIKey(const std::string& key) { return key; }
inline std::string DecryptAPIKey(const std::string& key) { return key; }
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
    bool show_algo = false;

    std::string active_symbol = "BTCUSDT";

    // Dynamic Chart tabs
    std::vector<ChartTab> active_charts;
    int next_chart_id = 2;

    // Calendar state
    std::unordered_map<std::string, std::string> daily_notes;
    int selected_year = 2026;
    int selected_month = 5;  // 1-12
    int selected_day = 27;   // 1-31

    std::string gemini_api_key;
    std::string notebook_dir = "storage/notebooks";
    std::string storage_dir = "storage";

    void ResolveStoragePaths() {
        // 1. Check if "storage" folder exists in current working directory
        if (directory_exists("storage")) {
            storage_dir = "storage";
            notebook_dir = "storage/notebooks";
            std::cout << "[AeroTerminal] Using local storage directory: " << storage_dir << std::endl;
            return;
        }
        
        // 2. Check if "../storage" exists (development mode in build/ folder)
        if (directory_exists("../storage")) {
            storage_dir = "../storage";
            notebook_dir = "../storage/notebooks";
            std::cout << "[AeroTerminal] Using parent storage directory: " << storage_dir << std::endl;
            return;
        }
        
        // 3. Fallback to %APPDATA%/AeroTerminal/storage
#ifdef _WIN32
        char* appdata = std::getenv("APPDATA");
        if (appdata) {
            std::string target = std::string(appdata) + "\\AeroTerminal\\storage";
            create_directory_recursive(target);
            create_directory_recursive(target + "\\notebooks");
            storage_dir = target;
            notebook_dir = target + "\\notebooks";
            std::cout << "[AeroTerminal] Using APPDATA storage directory: " << storage_dir << std::endl;
            return;
        }
#endif
        // Final fallback
        storage_dir = "storage";
        notebook_dir = "storage/notebooks";
        std::cout << "[AeroTerminal] Using fallback storage directory: " << storage_dir << std::endl;
    }

    void SaveSettings() {
        create_directory(storage_dir);
        nlohmann::json j;
        
        std::string enc_key = "";
        if (!gemini_api_key.empty()) {
            enc_key = EncryptAPIKey(gemini_api_key);
        }
        
        j["gemini_api_key"] = enc_key;
        j["notebook_dir"] = notebook_dir;
        std::ofstream file(storage_dir + "/settings.json");
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

    void LoadSettings() {
        char* env_key = std::getenv("GEMINI_API_KEY");
        if (env_key) {
            gemini_api_key = env_key;
        }
        std::ifstream file(storage_dir + "/settings.json");
        if (file.is_open()) {
            try {
                nlohmann::json j;
                file >> j;
                if (j.contains("gemini_api_key")) {
                    std::string key_val = j["gemini_api_key"].get<std::string>();
                    // Encrypted key starts with "01000000" in hex for DPAPI
                    if (key_val.rfind("01000000", 0) == 0) {
                        gemini_api_key = DecryptAPIKey(key_val);
                    } else {
                        gemini_api_key = key_val;
                    }
                }
                if (j.contains("notebook_dir")) {
                    notebook_dir = j["notebook_dir"].get<std::string>();
                }
            } catch (const std::exception& e) {
                std::cerr << "Error loading settings: " << e.what() << std::endl;
            }
        }
    }

    void SaveNotes() {
        create_directory(storage_dir);
        nlohmann::json j = daily_notes;
        std::ofstream file(storage_dir + "/daily_notes.json");
        if (file.is_open()) {
            file << j.dump(4);
        }
    }

    void LoadNotes() {
        std::ifstream file(storage_dir + "/daily_notes.json");
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

#ifdef _WIN32
    HANDLE jupyter_process_handle = nullptr;
    HANDLE jupyter_thread_handle = nullptr;
#endif

    void StartJupyterServer() {
#ifdef _WIN32
        create_directory(storage_dir);
        create_directory(notebook_dir);

        std::string cmd = "cmd.exe /c jupyter notebook --no-browser --NotebookApp.token='' --NotebookApp.password='' --port=8888 --notebook-dir=\"" + notebook_dir + "\"";
        
        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE; // Hidden window
        ZeroMemory(&pi, sizeof(pi));

        if (CreateProcessA(
            NULL,
            (char*)cmd.c_str(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            NULL,
            &si,
            &pi
        )) {
            jupyter_process_handle = pi.hProcess;
            jupyter_thread_handle = pi.hThread;
            std::cout << "[AeroTerminal] Spawned background Jupyter server process on port 8888" << std::endl;
        } else {
            std::cerr << "[AeroTerminal] Failed to spawn background Jupyter server process. Error: " << GetLastError() << std::endl;
        }
#endif
    }

    void StopJupyterServer() {
#ifdef _WIN32
        if (jupyter_process_handle != nullptr) {
            DWORD pid = GetProcessId(jupyter_process_handle);
            if (pid != 0) {
                std::string kill_cmd = "taskkill /F /T /PID " + std::to_string(pid);
                std::system(kill_cmd.c_str());
            }
            CloseHandle(jupyter_process_handle);
            if (jupyter_thread_handle != nullptr) {
                CloseHandle(jupyter_thread_handle);
            }
            jupyter_process_handle = nullptr;
            jupyter_thread_handle = nullptr;
            std::cout << "[AeroTerminal] Cleaned up background Jupyter server process" << std::endl;
        }
#endif
    }

    void RestartJupyterServer() {
        StopJupyterServer();
        StartJupyterServer();
    }

private:
    AppContext() {
        ResolveStoragePaths();
        
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
        LoadSettings();
        LoadNotes();

        // Start background Jupyter Server
        StartJupyterServer();
    }

    ~AppContext() {
        StopJupyterServer();
    }
};

} // namespace quantum
