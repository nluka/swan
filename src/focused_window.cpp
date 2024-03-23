#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace swan
{
    static swan_windows::id g_focused_window_id = swan_windows::id::nil_window;
};

swan_windows::id global_state::focused_window_get() noexcept
{
    using namespace swan;

    return g_focused_window_id;
};

bool global_state::focused_window_set(swan_windows::id window_id) noexcept
{
    using namespace swan;

    bool success;
    bool same_window_as_before = g_focused_window_id == window_id;

    if (same_window_as_before) {
        success = true;
    }
    else {
        g_focused_window_id = window_id;

        std::filesystem::path file_path = global_state::execution_path() / "data\\focused_window.txt";

        // the currently focused window has changed, save new state to disk
        try {
            std::ofstream out(file_path);
            if (!out) {
                success = false;
            }
            else {
                out << (s32 &)window_id;
                success = true;
            }
        }
        catch (...) {
            success = false;
        }

        print_debug_msg("%s global_state::save_focused_window (%s)", success ? "SUCCESS" : "FAILED", swan_windows::get_name(window_id));
    }

    return success;
}

bool global_state::focused_window_load_from_disk(swan_windows::id &out) noexcept
{
    using namespace swan;

    std::filesystem::path file_path = global_state::execution_path() / "data\\focused_window.txt";

    try {
        std::ifstream in(file_path);

        if (!in) {
            print_debug_msg("FAILED global_state::focused_window_load_from_disk: !in");
            return false;
        }

        in >> (s32 &)g_focused_window_id;
        out = g_focused_window_id;

        print_debug_msg("SUCCESS focused_window_load_from_disk");
        return true;
    }
    catch (...) {
        print_debug_msg("FAILED focused_window_load_from_disk: catch(...)");
        return false;
    }
}
