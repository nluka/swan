#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

bool swan_windows::render_analytics(std::array<swan_windows::id, (u64)swan_windows::id::count - 1> const &window_render_order) noexcept
{
    if (imgui::Begin(swan_windows::get_name(swan_windows::id::analytics), &global_state::settings().show.analytics)) {
        auto &io = imgui::GetIO();

        imgui::Text("%s build", get_build_mode().str);
        imgui::SameLineSpaced(2);
        imgui::Text("%.0f FPS", io.Framerate);
        imgui::SameLineSpaced(2);
        imgui::Text("%.3f ms/frame", 1000.0f / io.Framerate);

        imgui::Separator();

        imgui::Text("IsMouseClicked(left): %d", imgui::IsMouseClicked(ImGuiMouseButton_Left));
        imgui::Text("IsMouseDown(left): %d", imgui::IsMouseDown(ImGuiMouseButton_Left));
        imgui::Text("IsMouseDragging(left): %d", imgui::IsMouseDragging(ImGuiMouseButton_Left));
        imgui::Text("IsMouseReleased(left): %d", imgui::IsMouseReleased(ImGuiMouseButton_Left));

        // imgui::Separator();
        // imgui::Text("io.KeyCtrl: %d", io.KeyCtrl);

        imgui::Separator();

        imgui::TextUnformatted("window_render_order:");
        for (u64 i = 0; i < window_render_order.size(); ++i) {
            auto const &id = window_render_order[i];
            imgui::Text("[%02zu] %s", i, get_name(id));
        }

        return true;
    }

    return false;
}
