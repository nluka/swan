#include "common.hpp"
#include "imgui_specific.hpp"

bool debug_log_package::s_logging_enabled = true;
ImGuiTextBuffer debug_log_package::s_debug_buffer = {};

void swan_render_window_debug_log() noexcept
{
    namespace imgui = ImGui;

    if (imgui::Begin("Debug Log")) {
        static bool auto_scroll = true;

        // first line

        bool jump_to_top = imgui::ArrowButton("Top", ImGuiDir_Up);

        imgui_sameline_spacing(1);

        if (imgui::Button("Clear")) {
            debug_log_package::clear_buffer();
        }

        imgui::SameLine();

    #if 0
        imgui::BeginDisabled(true);
        if (imgui::Button("Save to file")) {
        }
        imgui::EndDisabled();
    #endif

        imgui_sameline_spacing(1);

        imgui::Checkbox("Logging Enabled", &debug_log_package::s_logging_enabled);

        imgui_sameline_spacing(1);

        imgui::Checkbox("Auto-scroll at bottom", &auto_scroll);

        // second line

        bool jump_to_bottom = imgui::ArrowButton("Bottom", ImGuiDir_Down);

        imgui::Spacing();
        imgui::Separator();

        // third line

        if (imgui::BeginChild("debug_log_scroll")) {
            auto const &debug_buffer = debug_log_package::s_debug_buffer;
            imgui::TextUnformatted(debug_buffer.c_str());

            if ( jump_to_top ) {
                imgui::SetScrollHereY(0.0f);
            }
            else if ( jump_to_bottom || ( auto_scroll && imgui::GetScrollY() >= imgui::GetScrollMaxY() ) ) {
                imgui::SetScrollHereY(1.0f);
            }
        }
        imgui::EndChild();
    }

    imgui::End();
}
