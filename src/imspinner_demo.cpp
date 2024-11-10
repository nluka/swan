#define IMSPINNER_DEMO
#include "imgui/imspinner.h"
#include "imgui_extension.hpp"

#include "common_functions.hpp"

bool swan_windows::render_imspinner_demo(bool &open, bool any_popups_open) noexcept
{
    imgui::ScopedColor bg(ImGuiCol_ChildBg, ImVec4(10, 10, 10, 255));

    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::imspinner_demo))) {
        return false;
    }

    ImSpinner::demoSpinners();

    return true;
}
