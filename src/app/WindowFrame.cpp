#include "WindowFrame.h"
#include "ui/widgets/PanelChart.h"
#include "ui/widgets/PanelExecution.h"
#include "ui/widgets/PanelSaathi.h"
#include "ui/widgets/PanelJournal.h"
#include "ui/widgets/PanelCalendar.h"
#include "ui/widgets/PanelWhiteboard.h"
#include "ui/widgets/PanelOrderBook.h"
#include "ui/widgets/PanelTrades.h"
#include "ui/widgets/PanelScreener.h"
#include "core/AppContext.h"
#include "core/MarketDataService.h"
#include "imgui.h"
#include "core/indicators/IndicatorManager.h"
#include <algorithm>
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <iostream>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <vector>
#include <string>
#include <windows.h>

#define ICON_FA_PLAY "\xef\x81\x8b"
#define ICON_FA_PAUSE "\xef\x81\x8c"
#define ICON_FA_STOP "\xef\x81\x8d"

namespace quantum {

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

WindowFrame::WindowFrame() {}

WindowFrame::~WindowFrame() {
    Shutdown();
}

bool WindowFrame::Initialize(int width, int height, const char* title)
{
    // 1. Initialize GLFW
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return false;

    // OpenGL 3.0 + GLSL 130
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Create window with graphics context
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (m_window == nullptr)
        return false;
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    HWND hwnd = glfwGetWin32Window(m_window);
    AppContext::instance().main_window_handle = (void*)hwnd;

    // 2. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport

    // 3. Style (Sleek Dark Mode matching Tracy's custom styling)
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Customize colors for Tracy-like appearance
    style.Colors[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_Header]            = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]     = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]     = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]      = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]           = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_FrameBgHovered]    = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
    style.Colors[ImGuiCol_FrameBgActive]     = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    style.Colors[ImGuiCol_TitleBg]           = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_TitleBgActive]     = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.05f, 0.05f, 0.05f, 1.00f);
    style.Colors[ImGuiCol_Tab]               = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_TabHovered]        = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    style.Colors[ImGuiCol_TabActive]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocused]      = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);

    // 4. Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(m_glsl_version);

    // 5. Load Fonts & Icons
    io.Fonts->AddFontFromFileTTF("fonts/Roboto-Regular.ttf", 15.0f);
    
    // Merge FontAwesome icons
    static const ImWchar icons_ranges[] = { 0xe000, 0xf8ff, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fonts/Font Awesome 6 Free-Solid-900.otf", 14.0f, &icons_config, icons_ranges);

    // Start market data service and subscribe to initial charts
    auto& mds = MarketDataService::instance();
    mds.Start();
    for (const auto& chart : AppContext::instance().active_charts) {
        mds.Subscribe(chart.ticker, chart.timeframe);
    }

    return true;
}

void WindowFrame::Run()
{
    ImGuiIO& io = ImGui::GetIO();
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();

        // Process live market data updates
        ProcessMarketDataUpdates();

        // Start Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        SetupDockspace();
        RenderUI();
        RenderBottomMenuBar();

        // Render loop finalize
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(m_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows (if multi-viewport enabled)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(m_window);
    }
}

void WindowFrame::SetupDockspace()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float menu_bar_height = ImGui::GetFrameHeight();
    
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - menu_bar_height));
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("AeroTerminalWorkspace", nullptr, window_flags);
    ImGui::PopStyleVar(3);

    // Submit the DockSpace
    ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    ImGui::End();
}

void WindowFrame::RenderBottomMenuBar()
{
    auto& ctx = AppContext::instance();
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float menu_bar_height = ImGui::GetFrameHeight();
    
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + viewport->WorkSize.y - menu_bar_height));
    ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, menu_bar_height));
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    flags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus;
    flags |= ImGuiWindowFlags_MenuBar;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    // Helper to scan layouts directory
    auto GetSavedLayouts = []() {
        std::vector<std::string> layouts;
        CreateDirectoryA("layouts", NULL);
        WIN32_FIND_DATAA find_data;
        HANDLE h_find = FindFirstFileA("layouts/*.ini", &find_data);
        if (h_find != INVALID_HANDLE_VALUE) {
            do {
                std::string filename = find_data.cFileName;
                if (filename.size() > 4) {
                    layouts.push_back(filename.substr(0, filename.size() - 4));
                }
            } while (FindNextFileA(h_find, &find_data));
            FindClose(h_find);
        }
        return layouts;
    };

    static char layout_name_buf[128] = "";
    static bool open_save_popup = false;

    if (ImGui::Begin("BottomMenuBarWindow", nullptr, flags))
    {
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("Workspace"))
            {
                if (ImGui::MenuItem("Save Layout")) {
                    open_save_popup = true;
                }
                
                if (ImGui::BeginMenu("Load Layout")) {
                    auto layouts = GetSavedLayouts();
                    for (const auto& layout : layouts) {
                        if (ImGui::MenuItem(layout.c_str())) {
                            std::string path = "layouts/" + layout + ".ini";
                            ImGui::LoadIniSettingsFromDisk(path.c_str());
                        }
                    }
                    if (layouts.empty()) {
                        ImGui::MenuItem("(No saved layouts)", nullptr, false, false);
                    }
                    ImGui::EndMenu();
                }
                
                ImGui::Separator();
                if (ImGui::MenuItem("Reset Default Layout")) {
                    ImGui::LoadIniSettingsFromDisk("imgui.ini");
                }
                ImGui::EndMenu();
            }

            if (open_save_popup) {
                ImGui::OpenPopup("Save Layout As");
                open_save_popup = false;
            }

            if (ImGui::BeginPopupModal("Save Layout As", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Enter layout profile name:");
                ImGui::InputText("##layout_name_input", layout_name_buf, sizeof(layout_name_buf));
                ImGui::Spacing();
                if (ImGui::Button("Save", ImVec2(120, 0))) {
                    if (strlen(layout_name_buf) > 0) {
                        CreateDirectoryA("layouts", NULL);
                        std::string path = "layouts/" + std::string(layout_name_buf) + ".ini";
                        ImGui::SaveIniSettingsToDisk(path.c_str());
                        ImGui::CloseCurrentPopup();
                        layout_name_buf[0] = '\0';
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                    ImGui::CloseCurrentPopup();
                    layout_name_buf[0] = '\0';
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginMenu("View"))
            {
                ImGui::MenuItem("Trade Execution", nullptr, &ctx.show_execution);
                ImGui::MenuItem("Saathi Assistant", nullptr, &ctx.show_saathi);
                ImGui::MenuItem("Trading Journal", nullptr, &ctx.show_journal);
                ImGui::MenuItem("Calendar", nullptr, &ctx.show_calendar);
                ImGui::MenuItem("Research Canvas", nullptr, &ctx.show_whiteboard);
                ImGui::MenuItem("Orderbook", nullptr, &ctx.show_orderbook);
                ImGui::MenuItem("Trades Feed", nullptr, &ctx.show_trades);
                ImGui::MenuItem("Market Screener", nullptr, &ctx.show_screener);
                ImGui::Separator();
                
                if (ImGui::BeginMenu("Add Chart"))
                {
                    const char* tickers[] = { "BTCUSDT", "ETHUSDT", "SOLUSDT", "AAPL", "MSFT", "GOOGL", "TSLA", "NVDA", "AMZN" };
                    for (const char* ticker : tickers)
                    {
                        if (ImGui::MenuItem(ticker))
                        {
                            ChartTab new_tab{ctx.next_chart_id++, ticker, true};
                            new_tab.AddDefaultIndicators();
                            ctx.active_charts.push_back(new_tab);
                            MarketDataService::instance().Subscribe(ticker, "1m");
                        }
                    }
                    
                    ImGui::Separator();
                    static char custom_ticker[16] = "";
                    ImGui::Text("Custom Ticker:");
                    ImGui::SetNextItemWidth(100);
                    if (ImGui::InputText("##custom_ticker", custom_ticker, sizeof(custom_ticker), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase))
                    {
                        if (strlen(custom_ticker) > 0)
                        {
                            ChartTab new_tab{ctx.next_chart_id++, custom_ticker, true};
                            new_tab.AddDefaultIndicators();
                            ctx.active_charts.push_back(new_tab);
                            MarketDataService::instance().Subscribe(custom_ticker, "1m");
                            custom_ticker[0] = '\0';
                        }
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Add"))
                    {
                        if (strlen(custom_ticker) > 0)
                        {
                            ChartTab new_tab{ctx.next_chart_id++, custom_ticker, true};
                            new_tab.AddDefaultIndicators();
                            ctx.active_charts.push_back(new_tab);
                            MarketDataService::instance().Subscribe(custom_ticker, "1m");
                            custom_ticker[0] = '\0';
                        }
                    }
                    
                    ImGui::EndMenu();
                }
                
                if (ImGui::BeginMenu("Closed Charts"))
                {
                    int closed_count = 0;
                    for (auto& chart : ctx.active_charts)
                    {
                        if (!chart.open)
                        {
                            closed_count++;
                            std::string label = "Reopen " + chart.ticker + "###reopen_" + std::to_string(chart.id);
                            if (ImGui::MenuItem(label.c_str()))
                            {
                                chart.open = true;
                                MarketDataService::instance().Subscribe(chart.ticker, chart.timeframe);
                            }
                        }
                    }
                    if (closed_count == 0)
                    {
                        ImGui::MenuItem("(No closed charts)", nullptr, false, false);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
}

void WindowFrame::RenderUI()
{
    auto& ctx = AppContext::instance();

    // Render active dynamic chart panels
    for (auto& chart : ctx.active_charts)
    {
        if (chart.open)
        {
            quantum::ui::RenderChartPanel(chart.id, chart.ticker.c_str(), &chart.open);
        }
    }

    // Render singleton panels if active
    if (ctx.show_execution)
        quantum::ui::RenderExecutionPanel(&ctx.show_execution);

    if (ctx.show_saathi)
        quantum::ui::RenderSaathiPanel(&ctx.show_saathi);

    if (ctx.show_journal)
        quantum::ui::RenderJournalPanel(&ctx.show_journal);

    if (ctx.show_calendar)
        quantum::ui::RenderCalendarPanel(&ctx.show_calendar);

    if (ctx.show_whiteboard)
        quantum::ui::RenderWhiteboardPanel(&ctx.show_whiteboard);

    if (ctx.show_orderbook)
        quantum::ui::RenderOrderBookPanel(&ctx.show_orderbook);

    if (ctx.show_trades)
        quantum::ui::RenderTradesPanel(&ctx.show_trades);

    if (ctx.show_screener)
        quantum::ui::RenderScreenerPanel(&ctx.show_screener);
}

void WindowFrame::Shutdown()
{
    MarketDataService::instance().Stop();
    quantum::ui::ShutdownWhiteboardPanel();
    if (m_window)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

void WindowFrame::ProcessMarketDataUpdates()
{
    auto updates = MarketDataService::instance().PollUpdates();
    auto& ctx = AppContext::instance();
    for (const auto& update : updates) {
        for (auto& chart : ctx.active_charts) {
            std::string c_symbol = chart.ticker;
            std::string u_symbol = update.symbol;
            std::transform(c_symbol.begin(), c_symbol.end(), c_symbol.begin(), ::toupper);
            std::transform(u_symbol.begin(), u_symbol.end(), u_symbol.begin(), ::toupper);
            
            if (c_symbol == u_symbol) {
                if (update.type == MarketDataUpdate::History) {
                    if (update.timeframe == chart.timeframe) {
                        chart.candles = update.candles;
                        chart.volumes = update.volumes;
                        chart.initialized = true;
                        chart.is_streaming = true;
                    }
                } else if (update.type == MarketDataUpdate::Tick) {
                    if (update.timeframe == chart.timeframe) {
                        if (chart.candles.empty()) {
                            chart.candles.push_back({
                                update.tick.open,
                                update.tick.close,
                                update.tick.high,
                                update.tick.low,
                                update.tick.timestamp
                            });
                            chart.volumes.push_back(update.tick_volume);
                        } else {
                            auto& last_candle = chart.candles.back();
                            last_candle.close = update.tick.close;
                            last_candle.high = update.tick.high;
                            last_candle.low = update.tick.low;
                            last_candle.open = update.tick.open;
                            last_candle.timestamp = update.tick.timestamp;
                            
                            if (chart.volumes.empty()) {
                                chart.volumes.push_back(update.tick_volume);
                            } else {
                                chart.volumes.back() = update.tick_volume;
                            }
                            
                            if (update.tick.is_candle_closed) {
                                chart.candles.push_back({
                                    update.tick.close,
                                    update.tick.close,
                                    update.tick.close,
                                    update.tick.close,
                                    update.tick.timestamp
                                });
                                chart.volumes.push_back(0.0);
                            }
                        }
                        chart.initialized = true;
                        chart.is_streaming = true;
                    }
                } else if (update.type == MarketDataUpdate::Depth) {
                    chart.bids = update.bids;
                    chart.asks = update.asks;
                } else if (update.type == MarketDataUpdate::Trade) {
                    chart.trades_log.push_back(update.trade);
                    if (chart.trades_log.size() > 50) {
                        chart.trades_log.erase(chart.trades_log.begin());
                    }
                }
            }
        }
    }
    
    for (auto& chart : ctx.active_charts) {
        if (chart.initialized && !chart.candles.empty()) {
            IndicatorManager::RecalculateIndicators(chart);
        }
    }
}

} // namespace quantum
