#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_ext.hpp"
#include "imgui_specific.hpp"

// https://github.com/ocornut/imgui/issues/5102
ImVec4 imgui::RGBA_to_ImVec4(s32 r, s32 g, s32 b, s32 a) noexcept {
    f32 newr = f32(r) / 255.0f;
    f32 newg = f32(g) / 255.0f;
    f32 newb = f32(b) / 255.0f;
    f32 newa = f32(a);
    return ImVec4(newr, newg, newb, newa);
}

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

    style.WindowPadding    = ImVec2(12.0f, 12.0f);
    style.FramePadding     = ImVec2(5.0f, 5.0f);
    style.CellPadding      = ImVec2(8.0f, 5.0f);
    style.ItemSpacing      = ImVec2(10.0f, 10.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);

    style.TabBorderSize           = 0;
    style.ChildBorderSize         = 0;
    style.FrameBorderSize         = 0;
    style.PopupBorderSize         = 1;
    style.WindowBorderSize        = 1;
    style.SeparatorTextBorderSize = 0;
    style.ScrollbarRounding       = 0;
    style.FrameRounding           = 2;
    style.TabRounding             = 0;

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
