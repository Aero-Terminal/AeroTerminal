#include "PanelSaathi.h"
#include "core/AppContext.h"
#include "imgui.h"
#include <cstring>

#define ICON_FA_ROBOT "\xef\x94\x89"
#define ICON_FA_PAPER_PLANE "\xef\x86\x9b"

namespace quantum::ui {

void RenderSaathiPanel(bool* p_open)
{
    ImGui::Begin(ICON_FA_ROBOT " Saathi Assistant", p_open);

    auto& ctx = AppContext::instance();
    static char chat_input_buf[256] = "";

    // Chat log area
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ChatHistory", ImVec2(0, -footer_height), true);

    for (const auto& msg : ctx.chat_history)
    {
        if (msg.is_ai)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.85f, 0.95f, 1.0f));
            ImGui::TextUnformatted("[Saathi]");
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
            ImGui::TextUnformatted("[You]");
            ImGui::PopStyleColor();
        }
        ImGui::SameLine();
        ImGui::TextWrapped("%s", msg.text.c_str());
        ImGui::Spacing();
    }

    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    // Input box
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60);
    bool entered = ImGui::InputText("##ChatInput", chat_input_buf, IM_ARRAYSIZE(chat_input_buf), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    
    if (ImGui::Button(ICON_FA_PAPER_PLANE) || entered)
    {
        if (strlen(chat_input_buf) > 0)
        {
            std::string query = chat_input_buf;
            ctx.chat_history.push_back({"You", query, false});
            memset(chat_input_buf, 0, sizeof(chat_input_buf));

            // Generate simple mock AI responses
            std::string reply = "I've analyzed your query. ";
            if (query.find("AAPL") != std::string::npos || query.find("aapl") != std::string::npos) {
                reply += "Apple (AAPL) is testing $197.50 resistance. Daily chart shows a strong bull flag breakout with support holding at $195.00.";
            } else if (query.find("risk") != std::string::npos || query.find("Risk") != std::string::npos) {
                reply += "Your current average risk profile is $80 per trade. Keep stop losses tight, ideally below the recent swing low.";
            } else {
                reply += "I'm checking the market feed context. The current trend is bullish across indices, watch out for volume breakouts.";
            }
            ctx.chat_history.push_back({"Saathi", reply, true});
        }
    }

    ImGui::End();
}

} // namespace quantum::ui
