#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

void swan_windows::render_analytics() noexcept
{
    if (imgui::Begin(swan_windows::get_name(swan_windows::id::analytics), &global_state::settings().show.analytics)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::focused_window_set(swan_windows::id::analytics);
        }

        auto &io = imgui::GetIO();

        imgui::Text("%s build", get_build_mode().str);
        imgui::SameLineSpaced(2);
        imgui::Text("%.0f FPS", io.Framerate);
        imgui::SameLineSpaced(2);
        imgui::Text("%.3f ms/frame", 1000.0f / io.Framerate);

        imgui::Spacing();

        imgui::Text("IsMouseClicked(left): %d", imgui::IsMouseClicked(ImGuiMouseButton_Left));
        imgui::Text("IsMouseDown(left): %d", imgui::IsMouseDown(ImGuiMouseButton_Left));
        imgui::Text("IsMouseDragging(left): %d", imgui::IsMouseDragging(ImGuiMouseButton_Left));
        imgui::Text("IsMouseReleased(left): %d", imgui::IsMouseReleased(ImGuiMouseButton_Left));
        imgui::Separator();
        imgui::Text("io.KeyCtrl: %d", io.KeyCtrl);
    }
    imgui::End();
}
