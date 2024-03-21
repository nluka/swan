#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

void swan_windows::render_theme_editor(bool &open, ImVec4 *starting_colors) noexcept
{
    assert(starting_colors != nullptr);

    if (!imgui::Begin(swan_windows::get_name(swan_windows::theme_editor), &open, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize)) {
        imgui::End();
        return;
    }

    if (imgui::BeginTable("## theme_editor", 3, ImGuiTableFlags_BordersInnerV)) {
        imgui::TableSetupColumn("ImGuiColorEditFlags_");
        imgui::TableSetupColumn("ImGuiCol_");
        imgui::TableSetupColumn("Swan");
        imgui::TableHeadersRow();

        s32 color_edit_flags = 0;

        imgui::TableNextColumn();
        {
            static bool s_None = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_None;
            static bool s_NoAlpha = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoAlpha;
            static bool s_NoPicker = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoPicker;
            static bool s_NoOptions = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoOptions;
            static bool s_NoSmallPreview = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoSmallPreview;
            static bool s_NoInputs = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoInputs;
            static bool s_NoTooltip = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoTooltip;
            static bool s_NoLabel = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoLabel;
            static bool s_NoSidePreview = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoSidePreview;
            static bool s_NoDragDrop = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoDragDrop;
            static bool s_NoBorder = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoBorder;
            static bool s_AlphaBar = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaBar;
            static bool s_AlphaPreview = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaPreview;
            static bool s_AlphaPreviewHalf = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaPreviewHalf;
            static bool s_HDR = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_HDR;
            static bool s_DisplayRGB = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayRGB;
            static bool s_DisplayHSV = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayHSV;
            static bool s_DisplayHex = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayHex;
            static bool s_Uint8 = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_Uint8;
            static bool s_Float = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_Float;
            static bool s_PickerHueBar = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_PickerHueBar;
            static bool s_PickerHueWheel = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_PickerHueWheel;
            static bool s_InputRGB = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_InputRGB;
            static bool s_InputHSV = ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_InputHSV;

            imgui::Checkbox("None", &s_None);
            imgui::Checkbox("NoAlpha", &s_NoAlpha);
            imgui::Checkbox("NoPicker", &s_NoPicker);
            imgui::Checkbox("NoOptions", &s_NoOptions);
            imgui::Checkbox("NoSmallPreview", &s_NoSmallPreview);
            imgui::Checkbox("NoInputs", &s_NoInputs);
            imgui::Checkbox("NoTooltip", &s_NoTooltip);
            // imgui::Checkbox("NoLabel", &s_NoLabel);
            imgui::Checkbox("NoSidePreview", &s_NoSidePreview);
            imgui::Checkbox("NoDragDrop", &s_NoDragDrop);
            imgui::Checkbox("NoBorder", &s_NoBorder);
            imgui::Checkbox("AlphaBar", &s_AlphaBar);
            imgui::Checkbox("AlphaPreview", &s_AlphaPreview);
            imgui::Checkbox("AlphaPreviewHalf", &s_AlphaPreviewHalf); imgui::SameLineSpaced(2); imgui::NewLine(); // add some padding to column right edge
            imgui::Checkbox("HDR", &s_HDR);

            if (imgui::Checkbox("DisplayRGB", &s_DisplayRGB)) {
                if (s_DisplayRGB) s_DisplayHSV = s_DisplayHex = false;
            }
            if (imgui::Checkbox("DisplayHSV", &s_DisplayHSV)) {
                if (s_DisplayHSV) s_DisplayRGB = s_DisplayHex = false;
            }
            if (imgui::Checkbox("DisplayHex", &s_DisplayHex)) {
                if (s_DisplayHex) s_DisplayRGB = s_DisplayHSV = false;
            }

            // imgui::Checkbox("Uint8", &s_Uint8);
            imgui::Checkbox("Float", &s_Float);

            if (imgui::Checkbox("PickerHueBar", &s_PickerHueBar)) {
                if (s_PickerHueBar) s_PickerHueWheel = false;
            }
            if (imgui::Checkbox("PickerHueWheel", &s_PickerHueWheel)) {
                if (s_PickerHueWheel) s_PickerHueBar = false;
            }

            if (imgui::Checkbox("InputRGB", &s_InputRGB)) {
                if (s_InputRGB) s_InputHSV = false;
            }
            // if (imgui::Checkbox("InputHSV", &s_InputHSV)) {
            //     if (s_InputHSV) s_InputRGB = false;
            // }

            color_edit_flags |= s_None ? ImGuiColorEditFlags_None : 0;
            color_edit_flags |= s_NoAlpha ? ImGuiColorEditFlags_NoAlpha : 0;
            color_edit_flags |= s_NoPicker ? ImGuiColorEditFlags_NoPicker : 0;
            color_edit_flags |= s_NoOptions ? ImGuiColorEditFlags_NoOptions : 0;
            color_edit_flags |= s_NoSmallPreview ? ImGuiColorEditFlags_NoSmallPreview : 0;
            color_edit_flags |= s_NoInputs ? ImGuiColorEditFlags_NoInputs : 0;
            color_edit_flags |= s_NoTooltip ? ImGuiColorEditFlags_NoTooltip : 0;
            color_edit_flags |= s_NoLabel ? ImGuiColorEditFlags_NoLabel : 0;
            color_edit_flags |= s_NoSidePreview ? ImGuiColorEditFlags_NoSidePreview : 0;
            color_edit_flags |= s_NoDragDrop ? ImGuiColorEditFlags_NoDragDrop : 0;
            color_edit_flags |= s_NoBorder ? ImGuiColorEditFlags_NoBorder : 0;
            color_edit_flags |= s_AlphaBar ? ImGuiColorEditFlags_AlphaBar : 0;
            color_edit_flags |= s_AlphaPreview ? ImGuiColorEditFlags_AlphaPreview : 0;
            color_edit_flags |= s_AlphaPreviewHalf ? ImGuiColorEditFlags_AlphaPreviewHalf : 0;
            color_edit_flags |= s_HDR ? ImGuiColorEditFlags_HDR : 0;
            color_edit_flags |= s_DisplayRGB ? ImGuiColorEditFlags_DisplayRGB : 0;
            color_edit_flags |= s_DisplayHSV ? ImGuiColorEditFlags_DisplayHSV : 0;
            color_edit_flags |= s_DisplayHex ? ImGuiColorEditFlags_DisplayHex : 0;
            color_edit_flags |= s_Uint8 ? ImGuiColorEditFlags_Uint8 : 0;
            color_edit_flags |= s_Float ? ImGuiColorEditFlags_Float : 0;
            color_edit_flags |= s_PickerHueBar ? ImGuiColorEditFlags_PickerHueBar : 0;
            color_edit_flags |= s_PickerHueWheel ? ImGuiColorEditFlags_PickerHueWheel : 0;
            color_edit_flags |= s_InputRGB ? ImGuiColorEditFlags_InputRGB : 0;
            color_edit_flags |= s_InputHSV ? ImGuiColorEditFlags_InputHSV : 0;
        }

        imgui::TableNextColumn();
        {
            struct color_def
            {
                char const *label = nullptr;
                u64 index = u64(-1);
            };

            color_def imgui_colors[] = {
                { .label = "Text",                  .index = ImGuiCol_Text },
                { .label = "TextDisabled",          .index = ImGuiCol_TextDisabled },
                { .label = "WindowBg",              .index = ImGuiCol_WindowBg },
                { .label = "ChildBg",               .index = ImGuiCol_ChildBg },
                { .label = "PopupBg",               .index = ImGuiCol_PopupBg },
                { .label = "Border",                .index = ImGuiCol_Border },
                { .label = "BorderShadow",          .index = ImGuiCol_BorderShadow },
                { .label = "FrameBg",               .index = ImGuiCol_FrameBg },
                { .label = "FrameBgHovered",        .index = ImGuiCol_FrameBgHovered },
                { .label = "FrameBgActive",         .index = ImGuiCol_FrameBgActive },
                { .label = "TitleBg",               .index = ImGuiCol_TitleBg },
                { .label = "TitleBgActive",         .index = ImGuiCol_TitleBgActive },
                { .label = "TitleBgCollapsed",      .index = ImGuiCol_TitleBgCollapsed },
                { .label = "MenuBarBg",             .index = ImGuiCol_MenuBarBg },
                { .label = "ScrollbarBg",           .index = ImGuiCol_ScrollbarBg },
                { .label = "ScrollbarGrab",         .index = ImGuiCol_ScrollbarGrab },
                { .label = "ScrollbarGrabHovered",  .index = ImGuiCol_ScrollbarGrabHovered },
                { .label = "ScrollbarGrabActive",   .index = ImGuiCol_ScrollbarGrabActive },
                { .label = "CheckMark",             .index = ImGuiCol_CheckMark },
                { .label = "SliderGrab",            .index = ImGuiCol_SliderGrab },
                { .label = "SliderGrabActive",      .index = ImGuiCol_SliderGrabActive },
                { .label = "Button",                .index = ImGuiCol_Button },
                { .label = "ButtonHovered",         .index = ImGuiCol_ButtonHovered },
                { .label = "ButtonActive",          .index = ImGuiCol_ButtonActive },
                { .label = "Header",                .index = ImGuiCol_Header },
                { .label = "HeaderHovered",         .index = ImGuiCol_HeaderHovered },
                { .label = "HeaderActive",          .index = ImGuiCol_HeaderActive },
                { .label = "Separator",             .index = ImGuiCol_Separator },
                { .label = "SeparatorHovered",      .index = ImGuiCol_SeparatorHovered },
                { .label = "SeparatorActive",       .index = ImGuiCol_SeparatorActive },
                { .label = "ResizeGrip",            .index = ImGuiCol_ResizeGrip },
                { .label = "ResizeGripHovered",     .index = ImGuiCol_ResizeGripHovered },
                { .label = "ResizeGripActive",      .index = ImGuiCol_ResizeGripActive },
                { .label = "Tab",                   .index = ImGuiCol_Tab },
                { .label = "TabHovered",            .index = ImGuiCol_TabHovered },
                { .label = "TabActive",             .index = ImGuiCol_TabActive },
                { .label = "TabUnfocused",          .index = ImGuiCol_TabUnfocused },
                { .label = "TabUnfocusedActive",    .index = ImGuiCol_TabUnfocusedActive },
                { .label = "DockingPreview",        .index = ImGuiCol_DockingPreview },
                { .label = "DockingEmptyBg",        .index = ImGuiCol_DockingEmptyBg },
                { .label = "PlotLines",             .index = ImGuiCol_PlotLines },
                { .label = "PlotLinesHovered",      .index = ImGuiCol_PlotLinesHovered },
                { .label = "PlotHistogram",         .index = ImGuiCol_PlotHistogram },
                { .label = "PlotHistogramHovered",  .index = ImGuiCol_PlotHistogramHovered },
                { .label = "TableHeaderBg",         .index = ImGuiCol_TableHeaderBg },
                { .label = "TableBorderStrong",     .index = ImGuiCol_TableBorderStrong },
                { .label = "TableBorderLight",      .index = ImGuiCol_TableBorderLight },
                { .label = "TableRowBg",            .index = ImGuiCol_TableRowBg },
                { .label = "TableRowBgAlt",         .index = ImGuiCol_TableRowBgAlt },
                { .label = "TextSelectedBg",        .index = ImGuiCol_TextSelectedBg },
                { .label = "DragDropTarget",        .index = ImGuiCol_DragDropTarget },
                { .label = "NavHighlight",          .index = ImGuiCol_NavHighlight },
                { .label = "NavWindowingHighlight", .index = ImGuiCol_NavWindowingHighlight },
                { .label = "NavWindowingDimBg",     .index = ImGuiCol_NavWindowingDimBg },
                { .label = "ModalWindowDimBg",      .index = ImGuiCol_ModalWindowDimBg },
            };

            auto render_color_picker = [&](color_def &col_def) noexcept {
                auto &style = imgui::GetStyle();
                ImVec4 *data = &style.Colors[col_def.index];
                ImVec4 *reset = &starting_colors[col_def.index];

                imgui::ColorEdit4(col_def.label, (f32 *)&data->x, color_edit_flags | ImGuiColorEditFlags_NoLabel);

                imgui::SameLine();

                auto btn_label = make_str_static<64>(ICON_CI_REFRESH "##%s", col_def.label);
                if (imgui::Button(btn_label.data())) {
                    *data = *reset;
                }

                imgui::SameLine();

                imgui::TextUnformatted(col_def.label);
            };

            u64 i = 0;
            for (; i < lengthof(imgui_colors); ++i) {
                render_color_picker(imgui_colors[i]);
            }
            assert(i == ImGuiCol_COUNT);
        }

        imgui::EndTable();
    }

    imgui::End();
}
