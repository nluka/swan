#define IMSPINNER_DEMO

#pragma warning(push, 1)
#include "imgui/imspinner.h"
#pragma warning(pop)

#include "imgui_extension.hpp"
#include "common_functions.hpp"

bool swan_windows::render_imspinner_demo(bool &open, [[maybe_unused]] bool any_popups_open) noexcept
{
    imgui::ScopedColor bg(ImGuiCol_ChildBg, ImVec4(10, 10, 10, 255));

    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::imspinner_demo), &open)) {
        return false;
    }

    ImSpinner::demoSpinners();

    return true;
}
