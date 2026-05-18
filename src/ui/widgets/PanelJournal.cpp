#include "PanelJournal.h"
#include "core/AppContext.h"
#include "imgui.h"
#include <cmath>

#define ICON_FA_BOOK "\xef\x80\xad"

namespace quantum::ui {

void RenderJournalPanel(bool* p_open)
{
    ImGui::Begin(ICON_FA_BOOK " Trading Journal", p_open);

    auto& ctx = AppContext::instance();

    if (ImGui::BeginTable("TradesTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Ticker");
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Entry");
        ImGui::TableSetupColumn("Exit");
        ImGui::TableSetupColumn("Qty");
        ImGui::TableSetupColumn("P&L");
        ImGui::TableSetupColumn("Trade Notes");
        ImGui::TableHeadersRow();

        for (const auto& trade : ctx.journal_trades)
        {
            ImGui::TableNextRow();
            
            // Ticker
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(trade.ticker.c_str());

            // Type
            ImGui::TableSetColumnIndex(1);
            if (trade.direction == "BUY")
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "LONG");
            else
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "SHORT");

            // Prices & Qty
            ImGui::TableSetColumnIndex(2); ImGui::Text("$%.2f", trade.entry_price);
            ImGui::TableSetColumnIndex(3); ImGui::Text("$%.2f", trade.exit_price);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.0f", trade.quantity);

            // PNL
            ImGui::TableSetColumnIndex(5);
            if (trade.pnl >= 0)
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), "+$%.2f", trade.pnl);
            else
                ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "-$%.2f", std::abs(trade.pnl));

            // Notes
            ImGui::TableSetColumnIndex(6);
            ImGui::TextWrapped("%s", trade.notes.c_str());
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace quantum::ui
