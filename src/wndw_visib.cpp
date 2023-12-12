#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"

// TODO: maybe switch to boost.property_tree?
bool window_visibilities::save_to_disk() const noexcept
{
    bool success;
    try {
        std::ofstream out("data/window_visibilities.txt", std::ios::binary);
        if (!out) {
            success = false;
        } else {
            out << "pin_manager " << this->pin_manager << '\n';
            out << "file_operations " << this->file_operations << '\n';
            out << "explorer_0 " << this->explorer_0 << '\n';
            out << "explorer_1 " << this->explorer_1 << '\n';
            out << "explorer_2 " << this->explorer_2 << '\n';
            out << "explorer_3 " << this->explorer_3 << '\n';
            out << "analytics " << this->analytics << '\n';
        #if !defined(NDEBUG)
            out << "imgui_demo " << this->imgui_demo << '\n';
            out << "debug_log " << this->debug_log << '\n';
            out << "fa_icons " << this->fa_icons << '\n';
            out << "ci_icons " << this->ci_icons << '\n';
            out << "md_icons " << this->md_icons << '\n';
        #endif
            success = true;
        }
    }
    catch (...) {
        success = false;
    }

    print_debug_msg("window_visibilities::save_to_disk: %d", success);
    return success;
}

// TODO: maybe switch to boost.property_tree?
bool window_visibilities::load_from_disk() noexcept
try {
    std::ifstream in("data/window_visibilities.txt", std::ios::binary);
    if (!in) {
        return false;
    }

    static_assert(s8(1) == s8(true));
    static_assert(s8(0) == s8(false));

    std::string what = {};
    what.reserve(100);
    char bit_ch = 0;

    {
        in >> what;
        assert(what == "pin_manager");
        in >> bit_ch;
        this->pin_manager = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "file_operations");
        in >> bit_ch;
        this->file_operations = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "explorer_0");
        in >> bit_ch;
        this->explorer_0 = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "explorer_1");
        in >> bit_ch;
        this->explorer_1 = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "explorer_2");
        in >> bit_ch;
        this->explorer_2 = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "explorer_3");
        in >> bit_ch;
        this->explorer_3 = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "analytics");
        in >> bit_ch;
        this->analytics = bit_ch == '1' ? 1 : 0;
    }

#if !defined(NDEBUG)
    {
        in >> what;
        assert(what == "imgui_demo");
        in >> bit_ch;
        this->imgui_demo = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "debug_log");
        in >> bit_ch;
        this->debug_log = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "fa_icons");
        in >> bit_ch;
        this->fa_icons = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "ci_icons");
        in >> bit_ch;
        this->ci_icons = bit_ch == '1' ? 1 : 0;
    }
    {
        in >> what;
        assert(what == "md_icons");
        in >> bit_ch;
        this->md_icons = bit_ch == '1' ? 1 : 0;
    }
#endif

    return true;
}
catch (...) {
    return false;
}
