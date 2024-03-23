#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

bool debug_log_package::s_logging_enabled = true;
ImGuiTextBuffer debug_log_package::s_buffer = {};
std::mutex debug_log_package::s_mutex = {};
static s32 s_debug_log_text_limit_megabytes = 5;

s32 &global_state::debug_log_text_limit_megabytes() noexcept { return s_debug_log_text_limit_megabytes; }

void swan_windows::render_debug_log(bool &open) noexcept
{
    if (imgui::Begin(swan_windows::get_name(swan_windows::id::debug_log), &open)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::focused_window_set(swan_windows::id::debug_log);
        }

        static bool s_auto_scroll = true;

        // first line

        bool jump_to_top = imgui::ArrowButton("Top", ImGuiDir_Up);
        imgui::SameLine();
        bool jump_to_bottom = imgui::ArrowButton("Bottom", ImGuiDir_Down);

        imgui::SameLineSpaced(1);

        if (imgui::Button("Clear")) {
            debug_log_package::clear_buffer();
        }

        imgui::SameLine();

    #if 0
        if (imgui::Button("Save")) {
            // TODO
        }
    #endif

        imgui::SameLineSpaced(1);

        imgui::Checkbox("Logging enabled", &debug_log_package::s_logging_enabled);

        imgui::SameLineSpaced(1);

        imgui::Checkbox("Auto-scroll at bottom", &s_auto_scroll);

        imgui::SameLineSpaced(1);

        {
            imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_12345").x);
            s32 &size_limit = global_state::debug_log_text_limit_megabytes();
            if (imgui::InputInt("MB limit", &size_limit, 1, 10)) {
                size_limit = std::clamp(size_limit, 1, 50);
            }
        }

        imgui::Separator();

        // second line

        imgui::Text("%-5s %10s %40s:%-6s %s", "tid", "ssssss.mmm", "source_file", "line", "message");
        imgui::Separator();

        // third line

        if (imgui::BeginChild("debug_log_scroll")) {
            {
                std::scoped_lock lock(debug_log_package::s_mutex);
                auto const &debug_buffer = debug_log_package::s_buffer;
                imgui::TextUnformatted(debug_buffer.c_str());
            }

            if ( jump_to_top ) {
                imgui::SetScrollHereY(0.0f);
            }
            else if ( jump_to_bottom || ( s_auto_scroll && imgui::GetScrollY() >= imgui::GetScrollMaxY() ) ) {
                imgui::SetScrollHereY(1.0f);
            }
        }
        imgui::EndChild();
    }

    imgui::End();
}
