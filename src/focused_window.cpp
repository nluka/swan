#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace swan
{
    static s32 g_focused_window = -1;
};

s32 global_state::focused_window() noexcept
{
    using namespace swan;

    return g_focused_window;
};

bool global_state::save_focused_window(s32 window_code) noexcept
{
    using namespace swan;

    bool success;
    bool same_window_as_before = g_focused_window == window_code;

    if (same_window_as_before) {
        success = true;
    }
    else {
        g_focused_window = window_code;

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

        print_debug_msg("global_state::save_focused_window disk: %d (%s)", success, swan_windows::get_name(window_code));
    }

    return success;
}

bool global_state::load_focused_window_from_disk(s32 &out) noexcept
{
    using namespace swan;

    std::filesystem::path file_path = global_state::execution_path() / "data\\focused_window.txt";

    try {
        std::ifstream in(file_path);

        if (!in) {
            print_debug_msg("FAILED global_state::load_focused_window_from_disk: !in");
            return false;
        }

        in >> g_focused_window;
        out = g_focused_window;

        print_debug_msg("SUCCESS load_focused_window_from_disk");
        return true;
    }
    catch (...) {
        print_debug_msg("FAILED load_focused_window_from_disk: catch(...)");
        return false;
    }
}
