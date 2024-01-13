#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"

static s32 s_focused_window = -1;

s32 global_state::focused_window() noexcept { return s_focused_window; };

bool global_state::save_focused_window(s32 window_code) noexcept
{
    bool success;
    bool same_window_as_before = s_focused_window == window_code;

    if (same_window_as_before) {
        success = true;
    }
    else {
        s_focused_window = window_code;

        std::filesystem::path file_path = global_state::execution_path() / "data\\focused_window.txt";

        // the currently focused window has changed, save new state to disk
        try {
            std::ofstream out(file_path);
            if (!out) {
                success = false;
            }
            else {
                out << window_code;
                success = true;
            }
        }
        catch (...) {
            success = false;
        }

        print_debug_msg("global_state::save_focused_window disk: %d (new code: %d)", success, window_code);
    }

    return success;
}

bool global_state::load_focused_window_from_disk(s32 &out) noexcept
{
    std::filesystem::path file_path = global_state::execution_path() / "data\\focused_window.txt";

    try {
        std::ifstream in(file_path);

        if (!in) {
            print_debug_msg("FAILED global_state::load_focused_window_from_disk: !in");
            return false;
        }

        in >> s_focused_window;
        out = s_focused_window;

        print_debug_msg("SUCCESS load_focused_window_from_disk");
        return true;
    }
    catch (...) {
        print_debug_msg("FAILED load_focused_window_from_disk: catch(...)");
        return false;
    }
}
