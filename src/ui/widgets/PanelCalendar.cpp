#include "PanelCalendar.h"
#include "core/AppContext.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <string>

namespace quantum {
namespace ui {

static inline bool IsLeapYear(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static inline int DaysInMonth(int year, int month) {
    static const int days[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2 && IsLeapYear(year)) return 29;
    if (month >= 1 && month <= 12) return days[month];
    return 30;
}

static inline int DayOfWeek(int year, int month, int day = 1) {
    static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (month < 3) year -= 1;
    return (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;
}

static inline std::string FormatDate(int y, int m, int d) {
    char buf[32];
    std::sprintf(buf, "%04d-%02d-%02d", y, m, d);
    return std::string(buf);
}

void RenderCalendarPanel(bool* p_open) {
    if (!ImGui::Begin("Calendar", p_open)) {
        ImGui::End();
        return;
    }

    auto& ctx = AppContext::instance();
    float width_left = 265.0f;

    // Left child window: Calendar grid selector
    ImGui::BeginChild("CalendarLeft", ImVec2(width_left, 0), true);

    static int year = ctx.selected_year;
    static int month = ctx.selected_month;

    static const char* month_names[] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    ImGui::Spacing();
    
    // Month navigation header
    ImGui::AlignTextToFramePadding();
    if (ImGui::Button("<##prev_month")) {
        month--;
        if (month < 1) {
            month = 12;
            year--;
        }
    }
    ImGui::SameLine();
    ImGui::Text("  %s %d  ", month_names[month], year);
    ImGui::SameLine();
    if (ImGui::Button(">##next_month")) {
        month++;
        if (month > 12) {
            month = 1;
            year++;
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Calendar table grid
    if (ImGui::BeginTable("CalendarGrid", 7, ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV)) {
        // Table headers (Days of week)
        ImGui::TableSetupColumn("Su", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Mo", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Tu", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("We", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Th", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Fr", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn("Sa", ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableHeadersRow();

        int start_day = DayOfWeek(year, month, 1);
        int total_days = DaysInMonth(year, month);

        ImGui::TableNextRow();

        // Print leading empty spaces
        int current_col = 0;
        for (int i = 0; i < start_day; ++i) {
            ImGui::TableSetColumnIndex(current_col++);
            ImGui::TextDisabled("-");
        }

        // Print days
        for (int day = 1; day <= total_days; ++day) {
            if (current_col >= 7) {
                ImGui::TableNextRow();
                current_col = 0;
            }
            ImGui::TableSetColumnIndex(current_col);

            std::string date_str = FormatDate(year, month, day);
            bool has_notes = (ctx.daily_notes.find(date_str) != ctx.daily_notes.end() && !ctx.daily_notes[date_str].empty());
            bool is_selected = (ctx.selected_year == year && ctx.selected_month == month && ctx.selected_day == day);

            // Coloring: gold for selected, bright green for has notes
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.78f, 0.00f, 1.00f)); // Gold
            } else if (has_notes) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.24f, 0.82f, 0.44f, 1.00f)); // Emerald Green
            }

            std::string label = std::to_string(day) + "##day_" + std::to_string(day);
            if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_None, ImVec2(24, 20))) {
                ctx.selected_year = year;
                ctx.selected_month = month;
                ctx.selected_day = day;
            }

            if (is_selected || has_notes) {
                ImGui::PopStyleColor();
            }

            current_col++;
        }

        // Print trailing empty spaces
        while (current_col < 7) {
            ImGui::TableSetColumnIndex(current_col++);
            ImGui::TextDisabled("-");
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Right child window: Selected date notes editor
    ImGui::BeginChild("NotesRight", ImVec2(0, 0), true);

    std::string date_str = FormatDate(ctx.selected_year, ctx.selected_month, ctx.selected_day);
    ImGui::Text("Notes for %s %d, %d", month_names[ctx.selected_month], ctx.selected_day, ctx.selected_year);
    ImGui::Separator();
    ImGui::Spacing();

    // Buffer for text multiline editor
    static char note_buf[8192];
    static std::string active_date_key = "";

    // Load active notes into editor buffer if the date changed
    if (active_date_key != date_str) {
        std::memset(note_buf, 0, sizeof(note_buf));
        auto it = ctx.daily_notes.find(date_str);
        if (it != ctx.daily_notes.end()) {
            std::strncpy(note_buf, it->second.c_str(), sizeof(note_buf) - 1);
        }
        active_date_key = date_str;
    }

    // Interactive Notes editor
    ImVec2 editor_size = ImVec2(-1, -30.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    if (ImGui::InputTextMultiline("##calendar_notes_editor", note_buf, sizeof(note_buf), editor_size, ImGuiInputTextFlags_AllowTabInput)) {
        ctx.daily_notes[date_str] = note_buf;
        ctx.SaveNotes();
    }
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::TextDisabled("Notes are saved automatically to storage/daily_notes.json");

    ImGui::EndChild();

    ImGui::End();
}

} // namespace ui
} // namespace quantum
