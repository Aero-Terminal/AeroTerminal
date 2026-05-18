#include "PanelExecution.h"
#include "core/AppContext.h"
#include "imgui.h"

#define ICON_FA_FILE_INVOICE "\xef\x95\x97"

namespace quantum::ui {

void RenderExecutionPanel(bool* p_open)
{
    ImGui::Begin(ICON_FA_FILE_INVOICE " Trade Execution", p_open);

    static char ticker[16] = "AAPL";
    static float price = 197.50f;
    static int quantity = 100;
    static float stop_loss = 194.00f;
    static float take_profit = 205.00f;
    static int order_type_idx = 0;
    const char* order_types[] = { "Limit", "Market", "Stop" };

    ImGui::InputText("Ticker", ticker, sizeof(ticker));
    
    ImGui::Combo("Order Type", &order_type_idx, order_types, IM_ARRAYSIZE(order_types));
    
    ImGui::InputFloat("Price", &price, 0.1f, 1.0f, "%.2f");
    ImGui::InputInt("Quantity", &quantity);

    ImGui::Separator();
    ImGui::Text("Risk Management");
    ImGui::InputFloat("Stop Loss", &stop_loss, 0.1f, 1.0f, "%.2f");
    ImGui::InputFloat("Take Profit", &take_profit, 0.1f, 1.0f, "%.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Estimate total capital required
    float estimated_cost = price * quantity;
    ImGui::Text("Estimated Total: $%.2f", estimated_cost);

    ImGui::Spacing();

    ImVec2 btn_sz = ImVec2(ImGui::GetContentRegionAvail().x * 0.47f, 40);
    
    auto& ctx = AppContext::instance();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.75f, 0.40f, 1.0f));
    if (ImGui::Button("BUY / LONG", btn_sz))
    {
        ctx.journal_trades.push_back({ticker, "BUY", price, 0.0, (double)quantity, 0.0, "Position opened via Execution Panel."});
        ctx.chat_history.push_back({"Saathi", std::string("Opened LONG position for ") + std::to_string(quantity) + " " + ticker + " at $" + std::to_string(price), true});
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine(0, ImGui::GetContentRegionAvail().x * 0.06f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
    if (ImGui::Button("SELL / SHORT", btn_sz))
    {
        ctx.journal_trades.push_back({ticker, "SELL", price, 0.0, (double)quantity, 0.0, "Position opened via Execution Panel."});
        ctx.chat_history.push_back({"Saathi", std::string("Opened SHORT position for ") + std::to_string(quantity) + " " + ticker + " at $" + std::to_string(price), true});
    }
    ImGui::PopStyleColor(2);

    ImGui::End();
}

} // namespace quantum::ui
