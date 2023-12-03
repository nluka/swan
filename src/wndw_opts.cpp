#include "stdafx.hpp"
#include "common.hpp"

bool windows_options::save_to_disk() const noexcept
{
    try {
        std::ofstream out("data/windows_options.txt", std::ios::binary);
        if (!out) {
            return false;
        }

        out << "show_pinned " << this->show_pins_mgr << '\n';
        out << "show_file_operations " << this->show_file_operations << '\n';
        out << "show_explorer_0 " << this->show_explorer_0 << '\n';
        out << "show_explorer_1 " << this->show_explorer_1 << '\n';
        out << "show_explorer_2 " << this->show_explorer_2 << '\n';
        out << "show_explorer_3 " << this->show_explorer_3 << '\n';
        out << "show_analytics " << this->show_analytics << '\n';
    #if !defined(NDEBUG)
        out << "show_demo " << this->show_demo << '\n';
        out << "show_debug_log " << this->show_debug_log << '\n';
        out << "show_fa_icons " << this->show_fa_icons << '\n';
        out << "show_ci_icons " << this->show_ci_icons << '\n';
        out << "show_md_icons " << this->show_md_icons << '\n';
    #endif

        return true;
    }
    catch (...) {
        return false;
    }
}

bool windows_options::load_from_disk() noexcept
{
    try {
        std::ifstream in("data/windows_options.txt", std::ios::binary);
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
            assert(what == "show_pinned");
            in >> bit_ch;
            this->show_pins_mgr = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_file_operations");
            in >> bit_ch;
            this->show_file_operations = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_0");
            in >> bit_ch;
            this->show_explorer_0 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_1");
            in >> bit_ch;
            this->show_explorer_1 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_2");
            in >> bit_ch;
            this->show_explorer_2 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_explorer_3");
            in >> bit_ch;
            this->show_explorer_3 = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_analytics");
            in >> bit_ch;
            this->show_analytics = bit_ch == '1' ? 1 : 0;
        }

    #if !defined(NDEBUG)
        {
            in >> what;
            assert(what == "show_demo");
            in >> bit_ch;
            this->show_demo = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_debug_log");
            in >> bit_ch;
            this->show_debug_log = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_fa_icons");
            in >> bit_ch;
            this->show_fa_icons = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_ci_icons");
            in >> bit_ch;
            this->show_ci_icons = bit_ch == '1' ? 1 : 0;
        }
        {
            in >> what;
            assert(what == "show_md_icons");
            in >> bit_ch;
            this->show_md_icons = bit_ch == '1' ? 1 : 0;
        }
    #endif

        return true;
    }
    catch (...) {
        return false;
    }
}
