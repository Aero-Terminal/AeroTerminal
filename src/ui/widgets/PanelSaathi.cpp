#include "PanelSaathi.h"
#include "core/AppContext.h"
#include "imgui.h"
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>
#include <cstdlib>
#include <ixwebsocket/IXHttpClient.h>

#define ICON_FA_ROBOT "\xef\x94\x89"
#define ICON_FA_PAPER_PLANE "\xef\x86\x9b"

namespace quantum::ui {

static std::mutex g_chat_mutex;

// Clean code blocks and write as a .ipynb notebook
static void ParseAndCreateNotebook(const std::string& text) {
    size_t pos = 0;
    std::vector<std::string> code_blocks;
    while (true) {
        size_t start = text.find("```python", pos);
        if (start == std::string::npos) {
            start = text.find("```py", pos);
            if (start == std::string::npos) break;
        }
        
        size_t code_start = (text.substr(start, 9) == "```python") ? start + 9 : start + 5;
        size_t end = text.find("```", code_start);
        if (end == std::string::npos) break;
        
        std::string code = text.substr(code_start, end - code_start);
        // Clean leading/trailing newlines
        while (!code.empty() && (code.front() == '\r' || code.front() == '\n')) {
            code.erase(code.begin());
        }
        while (!code.empty() && (code.back() == '\r' || code.back() == '\n')) {
            code.pop_back();
        }
        if (!code.empty()) {
            code_blocks.push_back(code);
        }
        pos = end + 3;
    }
    
    if (code_blocks.empty()) return;
    
    // Create the notebook
    std::string filename = "saathi_generated.ipynb";
    std::string path = AppContext::instance().notebook_dir + "/" + filename;
    
    create_directory(AppContext::instance().storage_dir);
    create_directory(AppContext::instance().notebook_dir);
    std::ofstream file(path);
    if (file.is_open()) {
        nlohmann::json j;
        j["nbformat"] = 4;
        j["nbformat_minor"] = 2;
        j["metadata"] = {
            {"language_info", {{"name", "python"}}}
        };
        
        nlohmann::json cells = nlohmann::json::array();
        
        // Title block
        cells.push_back({
            {"cell_type", "markdown"},
            {"metadata", nlohmann::json::object()},
            {"source", {
                "# AI-Generated Quantitative Analysis\n",
                "Generated dynamically by **Saathi AI Assistant** based on your prompt."
            }}
        });
        
        // Add code cells for each block
        for (size_t i = 0; i < code_blocks.size(); ++i) {
            cells.push_back({
                {"cell_type", "code"},
                {"execution_count", nullptr},
                {"metadata", nlohmann::json::object()},
                {"outputs", nlohmann::json::array()},
                {"source", { code_blocks[i] }}
            });
        }
        
        j["cells"] = cells;
        file << j.dump(4);
        file.close();
    }
}

static void CallLLMAPI(std::string query, std::string gemini_key) {
    std::string system_prompt = 
        "You are Saathi, an advanced quantitative trading AI assistant inside the AeroTerminal trading workspace. "
        "Help the user with strategy development, risk management, and market analysis. "
        "If they ask you to write a Python script or create a notebook, provide high-quality, executable Python code "
        "enclosed in standard markdown code blocks (e.g. ```python\\n...\\n```). The system will automatically "
        "extract these blocks and create a Jupyter notebook file named 'saathi_generated.ipynb' in their workspace "
        "so they can execute it.";

    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=" + gemini_key;
    nlohmann::json j;
    j["contents"] = nlohmann::json::array({
        {
            {"role", "user"},
            {"parts", nlohmann::json::array({
                {{"text", system_prompt + "\n\nUser request: " + query}}
            })}
        }
    });
    std::string body = j.dump();
    
    ix::HttpClient httpClient;
    ix::SocketTLSOptions tlsOptions;
    tlsOptions.caFile = "NONE";
    httpClient.setTLSOptions(tlsOptions);

    auto args = httpClient.createRequest();
    args->connectTimeout = 10;
    args->transferTimeout = 30;
    args->extraHeaders["Content-Type"] = "application/json";

    auto response = httpClient.post(url, body, args);
    std::string reply_text;
    
    if (response->statusCode == 200) {
        try {
            auto res_json = nlohmann::json::parse(response->body);
            reply_text = res_json["candidates"][0]["content"]["parts"][0]["text"].get<std::string>();
        } catch (const std::exception& e) {
            reply_text = "Error parsing LLM response: " + std::string(e.what());
        }
    } else {
        reply_text = "LLM API request failed with status code: " + std::to_string(response->statusCode) + "\nResponse: " + response->body;
    }

    // Post-process the response: check if it contains python code blocks to write to notebook
    ParseAndCreateNotebook(reply_text);
    
    // Add file output notification to user if notebook was created
    if (reply_text.find("```python") != std::string::npos || reply_text.find("```py") != std::string::npos) {
        reply_text += "\n\n*[System: Saathi has successfully extracted your code blocks and saved them into a new Jupyter notebook: `saathi_generated.ipynb`!]*";
    }

    // Add to chat history safely
    {
        std::lock_guard<std::mutex> lock(g_chat_mutex);
        AppContext::instance().chat_history.push_back({"Saathi", reply_text, true});
    }
}

void RenderSaathiPanel(bool* p_open)
{
    ImGui::Begin(ICON_FA_ROBOT " Saathi Assistant", p_open);

    auto& ctx = AppContext::instance();
    static char chat_input_buf[256] = "";

    // Chat log area
    float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("ChatHistory", ImVec2(0, -footer_height), true);

    {
        std::lock_guard<std::mutex> lock(g_chat_mutex);
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
            {
                std::lock_guard<std::mutex> lock(g_chat_mutex);
                ctx.chat_history.push_back({"You", query, false});
            }
            memset(chat_input_buf, 0, sizeof(chat_input_buf));

            // Check if Gemini API key is set
            std::string gemini_key = ctx.gemini_api_key;

            if (!gemini_key.empty()) {
                // Call real LLM asynchronously to keep UI responsive
                std::thread(CallLLMAPI, query, gemini_key).detach();
            } else {
                std::string reply = "*(No API Key configured)*\n\n"
                                    "Gemini API key is not set. Please go to **Workspace** -> **Settings** in the bottom menu bar "
                                    "to enter your Gemini API key and activate the Saathi Assistant.";
                {
                    std::lock_guard<std::mutex> lock(g_chat_mutex);
                    ctx.chat_history.push_back({"Saathi", reply, true});
                }
            }
        }
    }

    ImGui::End();
}

} // namespace quantum::ui
