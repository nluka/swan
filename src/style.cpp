#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_extension.hpp"
#include "imgui_dependent_functions.hpp"

// https://github.com/ocornut/imgui/issues/5102
// static
// ImVec4 HexA_to_ImVec4(char const *hex, s32 a) {
//     s32 r, g, b;
//     std::sscanf(hex, "%02x%02x%02x", &r, &g, &b);
//     return ImVec4(r, g, b, a);
// }

void apply_swan_style_overrides() noexcept
{
    ImGuiStyle &style = ImGui::GetStyle();

    style.WindowBorderSize = 1;
    style.WindowRounding = 5;
    style.WindowPadding = ImVec2(12.0f, 12.0f);

    style.ChildBorderSize = 0;

    style.TabBorderSize = 0;
    style.TabRounding = 5;

    style.FrameBorderSize = 0;
    style.FrameRounding = 2;
    style.FramePadding = ImVec2(5.0f, 5.0f);

    style.PopupBorderSize = 1;
    style.PopupRounding = 2;

    style.CellPadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing      = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);

    style.Colors[ImGuiCol_DragDropTarget] = style.Colors[ImGuiCol_NavHighlight];

    style.SeparatorTextBorderSize = 0;
    style.ScrollbarRounding = 0;

    style.Colors[ImGuiCol_WindowBg].w = 1; // fully opaque windows
    style.Colors[ImGuiCol_PopupBg].w = 1; // fully opaque popups

    style.Colors[ImGuiCol_FrameBg] = imgui::RGBA_to_ImVec4(35,40,45, 255);
    style.Colors[ImGuiCol_FrameBg] = imgui::RGBA_to_ImVec4(35,40,45, 255);

    style.Colors[ImGuiCol_TableHeaderBg] = imgui::RGBA_to_ImVec4(0,0,0,0);
    style.Colors[ImGuiCol_TableRowBg]    = ImVec4(0.07f, 0.07f, 0.07f, 0.94f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);

    style.Colors[ImGuiCol_Button]        = imgui::RGBA_to_ImVec4(45,60,80, s32(255*0.4f));
    style.Colors[ImGuiCol_ButtonHovered] = imgui::RGBA_to_ImVec4(60,90,120, 255);
    style.Colors[ImGuiCol_ButtonActive]  = imgui::RGBA_to_ImVec4(75,110,140, 255);

    style.Colors[ImGuiCol_Separator]        = imgui::RGBA_to_ImVec4(30,30,30, 255);
    style.Colors[ImGuiCol_SeparatorHovered] = imgui::RGBA_to_ImVec4(30,30,30, 255);
    style.Colors[ImGuiCol_SeparatorActive]  = imgui::RGBA_to_ImVec4(30,30,30, 255);
}
