#include "PanelAlgoTrading.h"
#include "core/AppContext.h"
#include "imgui.h"
#include "core/webview.h"
#include <windows.h>
#include <iostream>

#define ICON_FA_CODE "\xef\x84\x89"

namespace quantum::ui {

static webview::webview* g_algo_webview = nullptr;
static bool g_algo_webview_visible = false;
static bool g_algo_webview_failed = false;

void RenderAlgoTradingPanel(bool* p_open) {
    auto hide_webview = []() {
        if (g_algo_webview && g_algo_webview_visible) {
            g_algo_webview->set_visible(false);
            g_algo_webview_visible = false;
        }
    };

    if (!p_open || !*p_open) {
        hide_webview();
        return;
    }

    if (!ImGui::Begin(ICON_FA_CODE " Algorithmic Trading", p_open)) {
        hide_webview();
        ImGui::End();
        return;
    }

    if (g_algo_webview_failed) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Algorithmic Trading Panel Error:");
        ImGui::TextWrapped("Microsoft Edge WebView2 Runtime is not installed, or Jupyter Server failed to start.");
        ImGui::Spacing();
        if (ImGui::Button("Retry Connection")) {
            g_algo_webview_failed = false;
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
        if (g_algo_webview == nullptr && !g_algo_webview_failed && ctx.main_window_handle != nullptr) {
            HWND parent = (HWND)ctx.main_window_handle;
            g_algo_webview = new webview::webview(true, &parent, L"algotrading");
            
            if (!g_algo_webview->is_valid()) {
                delete g_algo_webview;
                g_algo_webview = nullptr;
                g_algo_webview_failed = true;
            } else {
                // Navigate to the local Jupyter notebook server
                g_algo_webview->navigate("http://localhost:8888/lab");
            }
        }

        // Keep webview child window size & position locked to the ImGui window panel client rect
        if (g_algo_webview) {
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

            g_algo_webview->set_bounds(pt.x, pt.y, (int)w, final_h);
            
            // Hide the heavyweight WebView2 child window if any ImGui popup/menu is open
            // to prevent it from clipping popups and dropdown menus.
            if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup)) {
                g_algo_webview->set_visible(false);
                g_algo_webview_visible = false;
            } else {
                g_algo_webview->set_visible(true);
                g_algo_webview_visible = true;
            }
        }
    }

    ImGui::End();
}

void ShutdownAlgoTradingPanel() {
    if (g_algo_webview) {
        delete g_algo_webview;
        g_algo_webview = nullptr;
        g_algo_webview_visible = false;
    }
}

} // namespace quantum::ui
