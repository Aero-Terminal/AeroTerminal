#include "PanelWhiteboard.h"
#include "tldraw_index_html.h"
#include "core/AppContext.h"
#include "core/json.hpp"
#include "imgui.h"
#include "core/webview.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace quantum {
namespace ui {

static webview::webview* g_webview = nullptr;
static bool g_webview_visible = false;
static bool g_webview_failed = false;

// HTML template containing tldraw React App from CDN
static const char* g_tldraw_html = R"html(
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Quantum Canvas</title>
    <!-- Load tldraw stylesheet -->
    <link rel="stylesheet" href="https://unpkg.com/@tldraw/tldraw@2.4.4/tldraw.css" />
    <style>
        html, body, #root {
            margin: 0;
            padding: 0;
            width: 100%;
            height: 100%;
            overflow: hidden;
            background-color: #121614;
        }
    </style>
</head>
<body>
    <div id="root"></div>

    <!-- Load React -->
    <script src="https://unpkg.com/react@18.2.0/umd/react.production.min.js"></script>
    <script src="https://unpkg.com/react-dom@18.2.0/umd/react-dom.production.min.js"></script>

    <!-- Load tldraw -->
    <script src="https://unpkg.com/@tldraw/tldraw@2.4.4/tldraw.js"></script>

    <script>
        const { useState, useEffect } = React;
        const { Tldraw } = tldraw;

        function App() {
            const handleMount = (editor) => {
                window.editor = editor;
                
                // If there is saved data from C++, load it!
                if (window.initial_canvas_data) {
                    try {
                        const data = JSON.parse(window.initial_canvas_data);
                        editor.loadSnapshot(data);
                    } catch (e) {
                        console.error("Error loading snapshot:", e);
                    }
                }

                // Listen for changes and auto-save back to C++ (debounced)
                let timeoutId = null;
                editor.store.listen((entry) => {
                    if (entry.source === 'user') {
                        if (timeoutId) clearTimeout(timeoutId);
                        timeoutId = setTimeout(() => {
                            if (window.save_canvas_data) {
                                const snapshot = editor.getSnapshot();
                                window.save_canvas_data(JSON.stringify(snapshot));
                            }
                        }, 200);
                    }
                }, { scope: 'document' });
            };

            return React.createElement(Tldraw, {
                onMount: handleMount,
                inferDarkMode: true
            });
        }

        const root = ReactDOM.createRoot(document.getElementById('root'));
        root.render(React.createElement(App));
    </script>
</body>
</html>
)html";

void RenderWhiteboardPanel(bool* p_open) {
    auto hide_webview = []() {
        if (g_webview && g_webview_visible) {
            g_webview->set_visible(false);
            g_webview_visible = false;
        }
    };

    if (!p_open || !*p_open) {
        hide_webview();
        return;
    }

    if (!ImGui::Begin("Research Canvas", p_open)) {
        hide_webview();
        ImGui::End();
        return;
    }

    if (g_webview_failed) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Research Canvas Error:");
        ImGui::TextWrapped("Microsoft Edge WebView2 Runtime is not installed or failed to initialize.");
        ImGui::TextWrapped("The drawing canvas requires the WebView2 Runtime to embed the rich drawing board.");
        ImGui::Spacing();
        if (ImGui::Button("Retry Initialization")) {
            g_webview_failed = false;
        }
        ImGui::End();
        return;
    }

    auto& ctx = AppContext::instance();

    // Get screen positioning of the ImGui panel content area
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();

    float x = pos.x;
    float y = pos.y;
    float w = size.x;
    float h = size.y;

    bool is_collapsed = ImGui::IsWindowCollapsed();

    if (is_collapsed || w <= 10.0f || h <= 10.0f) {
        hide_webview();
    } else {
        // Initialize webview inside GLFW window as child HWND
        if (g_webview == nullptr && !g_webview_failed && ctx.main_window_handle != nullptr) {
            HWND parent = (HWND)ctx.main_window_handle;
            g_webview = new webview::webview(true, &parent, L"whiteboard");
            
            if (!g_webview->is_valid()) {
                delete g_webview;
                g_webview = nullptr;
                g_webview_failed = true;
            } else {
                // C++ to JS save callback binding
                g_webview->bind("save_canvas_data", [](std::string req) -> std::string {
                    try {
                        nlohmann::json args = nlohmann::json::parse(req);
                        if (args.is_array() && args.size() > 0) {
                            std::string snapshot_str = args[0].get<std::string>();
                            
                            create_directory(AppContext::instance().storage_dir);
                            std::ofstream file(AppContext::instance().storage_dir + "/canvas.json");
                            if (file.is_open()) {
                                file << snapshot_str;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error saving canvas data: " << e.what() << std::endl;
                    }
                    return "{}";
                });

                // Load initial snapshot from canvas.json
                std::string initial_json = "{}";
                std::ifstream file(AppContext::instance().storage_dir + "/canvas.json");
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    initial_json = buffer.str();
                }

                // Escape json for JS code injection
                std::string escaped_json = "";
                for (char c : initial_json) {
                    if (c == '"') escaped_json += "\\\"";
                    else if (c == '\\') escaped_json += "\\\\";
                    else if (c == '\n') escaped_json += "\\n";
                    else if (c == '\r') escaped_json += "\\r";
                    else escaped_json += c;
                }

                g_webview->init("window.initial_canvas_data = \"" + escaped_json + "\";");

                // Load HTML from storage/tldraw_index.html, fallback to embedded HTML
                std::string html_content = "";
                std::ifstream html_file(AppContext::instance().storage_dir + "/tldraw_index.html");
                if (html_file.is_open()) {
                    std::stringstream buffer;
                    buffer << html_file.rdbuf();
                    html_content = buffer.str();
                }
                
                if (!html_content.empty()) {
                    g_webview->set_html(html_content);
                } else {
                    // Convert embedded byte array to string and load it
                    std::string embedded_html(reinterpret_cast<const char*>(g_tldraw_index_html), g_tldraw_index_html_len);
                    g_webview->set_html(embedded_html);
                }
            }
        }

        // Keep webview child window size & position locked to the ImGui window panel client rect
        if (g_webview) {
            HWND parent = (HWND)ctx.main_window_handle;
            POINT pt = { (long)x, (long)y };
            ScreenToClient(parent, &pt);

            // Clamp height to avoid covering the bottom menu bar (25px)
            RECT parent_rect;
            GetClientRect(parent, &parent_rect);
            int parent_h = parent_rect.bottom - parent_rect.top;
            int max_h = (parent_h - 25) - pt.y;
            int final_h = (int)h;
            if (final_h > max_h) {
                final_h = max_h;
            }
            if (final_h < 0) final_h = 0;



            g_webview->set_bounds(pt.x, pt.y, (int)w, final_h);
            
            // Hide the heavyweight WebView2 child window if any ImGui popup/menu is open
            // to prevent it from clipping popups and dropdown menus.
            if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
                g_webview->set_visible(false);
                g_webview_visible = false;
            } else {
                g_webview->set_visible(true);
                g_webview_visible = true;
            }
        }
    }

    ImGui::End();
}

void ShutdownWhiteboardPanel() {
    if (g_webview) {
        delete g_webview;
        g_webview = nullptr;
        g_webview_visible = false;
    }
}

} // namespace ui
} // namespace quantum
