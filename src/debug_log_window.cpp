#ifndef SWAN_DEBUG_LOG_WINDOW_CPP
#define SWAN_DEBUG_LOG_WINDOW_CPP

#include "common.hpp"

void render_debug_log_window() noexcept(true)
{
    if (ImGui::Begin("Debug Log")) {
        auto same_line_with_spacing = [] {
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
        };

        static bool auto_scroll = true;

        // first line

        bool jump_to_top = ImGui::ArrowButton("Top", ImGuiDir_Up);

        same_line_with_spacing();

        if (ImGui::Button("Clear")) {
            debug_log_package::clear_buffer();
        }

        ImGui::SameLine();

        if (ImGui::Button("Save to file")) {
            debug_log_package::clear_buffer();
        }

        // second line

        bool jump_to_bottom = ImGui::ArrowButton("Bottom", ImGuiDir_Down);

        same_line_with_spacing();

        ImGui::Checkbox("Auto-scroll at bottom", &auto_scroll);

        ImGui::Spacing();
        ImGui::Separator();

        if (ImGui::BeginChild("debug_log_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            auto const &debug_buffer = debug_log_package::s_debug_buffer;
            ImGui::TextUnformatted(debug_buffer.c_str());

            if ( jump_to_top ) {
                ImGui::SetScrollHereY(0.0f);
            }
            else if ( jump_to_bottom || ( auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() ) ) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}

#endif // SWAN_DEBUG_LOG_WINDOW_CPP
