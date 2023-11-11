#include "stdafx.hpp"
#include "common.hpp"
#include "imgui_specific.hpp"

static std::string s_focused_window_name = "";

bool save_focused_window(char const *window_name) noexcept
{
    bool success;
    char const *file_path = "data/focused_window.txt";
    bool same_window_as_before = s_focused_window_name == window_name;

    if (same_window_as_before) {
        success = true;
    }
    else {
        s_focused_window_name = window_name;

        // the currently focused window has changed, save new state to disk

        try {
            std::ofstream out(file_path);
            if (!out) {
                success = false;
            }
            else {
                out << window_name;
                success = true;
            }
        }
        catch (...) {
            success = false;
        }

        debug_log("[%s] save_focused_window disk: %d", file_path, success);
    }

    return success;
}

bool load_focused_window_from_disk(char const *out) noexcept
{
    bool success;
    char const *file_path = "data/focused_window.txt";

    try {
        std::ifstream in(file_path);
        if (!in) {
            success = false;
        }
        else {
            in >> s_focused_window_name;
            out = s_focused_window_name.c_str();
            success = true;
        }
    }
    catch (...) {
        success = false;
    }

    debug_log("[%s] load_focused_window_from_disk: %d", file_path, success);
    return success;
}
