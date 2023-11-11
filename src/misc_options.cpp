#include "stdafx.hpp"
#include "common.hpp"

bool misc_options::save_to_disk() const noexcept
{
    try {
        std::ofstream out("data/misc_options.txt", std::ios::binary);

        if (!out) {
            return false;
        }

        static_assert(s8(1) == s8(true));
        static_assert(s8(0) == s8(false));

        out << "stylesheet " << (s32)this->stylesheet << '\n';

        return true;
    }
    catch (...) {
        return false;
    }
}

bool misc_options::load_from_disk() noexcept
{
    try {
        std::ifstream in("data/misc_options.txt", std::ios::binary);
        if (!in) {
            return false;
        }

        static_assert(s8(1) == s8(true));
        static_assert(s8(0) == s8(false));

        std::string what = {};
        what.reserve(100);
        // char bit_ch = 0;

        {
            in >> what;
            assert(what == "stylesheet");
            in >> (s32 &)this->stylesheet;
        }

        return true;
    }
    catch (...) {
        return false;
    }
}
