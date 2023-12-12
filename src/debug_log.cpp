#include "common_fns.hpp"
#include "imgui_specific.hpp"

bool debug_log_package::s_logging_enabled = true;
ImGuiTextBuffer debug_log_package::s_buffer = {};
std::mutex debug_log_package::s_mutex = {};
static s32 s_debug_log_text_limit_megabytes = 5;

s32 &global_state::debug_log_text_limit_megabytes() noexcept { return s_debug_log_text_limit_megabytes; }

void swan_windows::render_debug_log(bool &open) noexcept
{
    if (imgui::Begin(" Debug Log ", &open)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::save_focused_window(swan_windows::debug_log);
        }

        static bool auto_scroll = true;

        // first line

        bool jump_to_top = imgui::ArrowButton("Top", ImGuiDir_Up);
        imgui::SameLine();
        bool jump_to_bottom = imgui::ArrowButton("Bottom", ImGuiDir_Down);

        imgui::SameLineSpaced(1);

        if (imgui::Button("Clear")) {
            debug_log_package::clear_buffer();
        }

        imgui::SameLine();

        if (imgui::Button("Save")) {
            // TODO
        }

        imgui::SameLine();

    #if 0
        imgui::BeginDisabled(true);
        if (imgui::Button("Save to file")) {
        }
        imgui::EndDisabled();
    #endif

        imgui::SameLineSpaced(1);

        imgui::Checkbox("Logging Enabled", &debug_log_package::s_logging_enabled);

        imgui::SameLineSpaced(1);

        imgui::Checkbox("Auto-scroll at bottom", &auto_scroll);

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

        imgui::Text("%-5s %10s %18s:%-5s %s", "tid", "ssssss.mmm", "source_file", "line", "message");
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
            else if ( jump_to_bottom || ( auto_scroll && imgui::GetScrollY() >= imgui::GetScrollMaxY() ) ) {
                imgui::SetScrollHereY(1.0f);
            }
        }
        imgui::EndChild();
    }

    imgui::End();
}
