#pragma once

#include "imgui/imgui.h"

#include <windows.h>

#include <filesystem>

#include "primitives.hpp"

namespace fs = std::filesystem;

void render_unicode_test_window() noexcept(true)
{
    if (!ImGui::Begin("Unicode Test")) {
        ImGui::End();
        return;
    }

    for (auto const &entry : fs::recursive_directory_iterator(".\\dummy\\unicode")) {
        if (fs::is_regular_file(entry.path())) {
            wchar_t const *utf_16_path = entry.path().c_str();
            i32 utf_16_len = lstrlenW(entry.path().c_str());

            char utf_8_buffer[1024] = { 0 };

            WideCharToMultiByte(CP_UTF8, WC_COMPOSITECHECK, utf_16_path, utf_16_len, utf_8_buffer, sizeof(utf_8_buffer), "!", nullptr);

            ImGui::TextUnformatted(utf_8_buffer);
        }
    }

    ImGui::End();
}
