#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

struct context
{
    ImGuiStyle &style;
    ImGuiStyle &ImGui_style_at_frame_start;
    ImGuiStyle const &fallback_style;
    bool *ImGuiCol_checks_at_frame_start;
    bool swan_color_changed;
    bool *swan_color_checks_at_frame_start;
};

static void render_colors_tab_content(context ctx) noexcept
{
    auto &[style, ImGui_style_at_frame_end, fallback_style,
        ImGuiCol_checks_at_frame_start, swan_color_changed, swan_color_checks_at_frame_start] = ctx;

#if 0
    static bool s_None = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_None);
    static bool s_NoAlpha = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoAlpha);
    static bool s_NoPicker = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoPicker);
    static bool s_NoOptions = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoOptions);
    static bool s_NoSmallPreview = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoSmallPreview);
    static bool s_NoInputs = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoInputs);
    static bool s_NoTooltip = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoTooltip);
    static bool s_NoLabel = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoLabel);
    static bool s_NoSidePreview = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoSidePreview);
    static bool s_NoDragDrop = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoDragDrop);
    static bool s_NoBorder = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_NoBorder);
    static bool s_AlphaBar = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaBar);
    static bool s_AlphaPreview = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaPreview);
    static bool s_AlphaPreviewHalf = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_AlphaPreviewHalf);
    static bool s_HDR = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_HDR);
    static bool s_DisplayRGB = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayRGB);
    static bool s_DisplayHSV = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayHSV);
    static bool s_DisplayHex = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_DisplayHex);
    static bool s_Uint8 = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_Uint8);
    static bool s_Float = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_Float);
    static bool s_PickerHueBar = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_PickerHueBar);
    static bool s_PickerHueWheel = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_PickerHueWheel);
    static bool s_InputRGB = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_InputRGB);
    static bool s_InputHSV = bool(ImGuiColorEditFlags_DefaultOptions_ & ImGuiColorEditFlags_InputHSV);
#endif

    struct swan_color_def
    {
        char const *label = nullptr;
        ImVec4 *data = nullptr;
        ImVec4 (*get_default_data)() = nullptr;
        bool *check = nullptr;
    };
    static swan_color_def s_swan_colors[] = {
        { "Success",      &global_state::settings().success_color,      default_success_color,      &global_state::settings().check_success_color },
        { "Warning",      &global_state::settings().warning_color,      default_warning_color,      &global_state::settings().check_warning_color },
        { "Warning Lite", &global_state::settings().warning_lite_color, default_warning_lite_color, &global_state::settings().check_warning_lite_color },
        { "Error",        &global_state::settings().error_color,        default_error_color,        &global_state::settings().check_error_color },
        { "Directory",    &global_state::settings().directory_color,    default_directory_color,    &global_state::settings().check_directory_color },
        { "File",         &global_state::settings().file_color,         default_file_color,         &global_state::settings().check_file_color },
        { "Symlink",      &global_state::settings().symlink_color,      default_symlink_color,      &global_state::settings().check_symlink_color },
    };

    u64 constexpr longest_label = 32; // 21
    static std::array<char, longest_label+1> s_search_input = {};

    {
        imgui::ScopedItemWidth w(imgui::CalcTextSize("_").x * longest_label);
        imgui::InputTextWithHint("## theme_editor Colors search", ICON_CI_SEARCH, s_search_input.data(), s_search_input.max_size());
    }

    imgui::SameLine();

    if (imgui::Button(ICON_CI_REFRESH "## Colors reset all")) {
        imgui::OpenConfirmationModalWithCallback(
            /* confirmation_id  = */ swan_id_confirm_theme_editor_color_reset,
            /* confirmation_msg = */ "Are you sure you want to reset all colors to their default values? This action cannot be undone.",
            /* on_yes_callback  = */
            [&]() noexcept {
                (void) std::memcpy(style.Colors, fallback_style.Colors, sizeof(style.Colors));
                for (auto const &col_def : s_swan_colors) {
                    *col_def.data = col_def.get_default_data();
                }
                (void) global_state::settings().save_to_disk();
            },
            /* confirmation_enabled = */ &(global_state::settings().confirm_theme_editor_color_reset)
        );
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Reset all colors");
    }

    imgui::SameLine(0, 0);

    if (imgui::Button(ICON_CI_CODE "## Colors")) {
        std::string serialized = {};
        serialized.reserve(global_state::page_size());
        serialize_ImGuiStyle_only_colors(style.Colors, serialized, serialize_ImGuiStyle_mode::cpp_code);
        imgui::SetClipboardText(serialized.c_str());
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Copy colors as C++ source code");
    }

    imgui::SameLine(0, 0);

    if (imgui::Button(ICON_CI_SYMBOL_TEXT "## Colors")) {
        std::string serialized = {};
        serialized.reserve(global_state::page_size());
        serialize_ImGuiStyle_only_colors(style.Colors, serialized, serialize_ImGuiStyle_mode::plain_text);
        imgui::SetClipboardText(serialized.c_str());
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Copy colors as [swan_settings.txt] text");
    }

#if 0
    imgui::SameLine(0, 0);

    if (imgui::Button(ICON_LC_SETTINGS)) {
        imgui::OpenPopup("Color Settings (Theme Editor)");
    }
    bool modal_open;

    if (imgui::BeginPopupModal("Color Settings (Theme Editor)", &modal_open)) {
        imgui::SeparatorText("ImGuiColorEditFlags_");

        if (imgui::BeginChild("## theme_editor ImGuiColorEditFlags_")) {
            // imgui::Checkbox("None", &s_None);
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
            imgui::Checkbox("AlphaPreviewHalf", &s_AlphaPreviewHalf); // imgui::SameLineSpaced(2); imgui::NewLine(); // add some padding to column right edge
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

            // if (imgui::Checkbox("InputRGB", &s_InputRGB)) {
            //     if (s_InputRGB) s_InputHSV = false;
            // }
            // if (imgui::Checkbox("InputHSV", &s_InputHSV)) {
            //     if (s_InputHSV) s_InputRGB = false;
            // }
        }
        imgui::EndChild();
        imgui::EndPopup();
    }
#endif

    ImGuiColorEditFlags color_edit_flags = ImGuiColorEditFlags_DefaultOptions_;
    // TODO fix
#if 0
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
#endif

    if (imgui::BeginChild("## theme_editor Colors child")) {
        enum color_col : ImGuiID {
            color_col_property,
            color_col_value,
            color_col_checked,
            color_col_reset,
            color_col_count
        };
        s32 table_flags =
            ImGuiTableFlags_SizingStretchProp|
            ImGuiTableFlags_Hideable|
            ImGuiTableFlags_Resizable|
            ImGuiTableFlags_Reorderable|
            ImGuiTableFlags_BordersV|
            ImGuiTableFlags_ScrollY|
            (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
            (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
        ;
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        if (imgui::BeginTable("## theme_editor Colors table", color_col_count, table_flags)) {
            imgui::TableSetupColumn("Color", 0, 0.f, color_col_property);
            imgui::TableSetupColumn("Value", 0, 0.f, color_col_value);
            imgui::TableSetupColumn("Mark", 0, 0.f, color_col_checked);
            imgui::TableSetupColumn("Reset", 0, 0.f, color_col_reset);
            ImGui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            auto render_swan_color_picker = [&](swan_color_def &col_def) noexcept {
                if (imgui::TableSetColumnIndex(color_col_property)) {
                    imgui::AlignTextToFramePadding();
                    imgui::Text("%s", col_def.label);
                }
                if (imgui::TableSetColumnIndex(color_col_value)) {
                    imgui::ScopedAvailWidth w = {};
                    auto color_edit_label = make_str_static<64>("## %s", col_def.label);
                    swan_color_changed |= imgui::ColorEdit4(color_edit_label.data(), (f32 *)&col_def.data->x, color_edit_flags);

                    col_def.data->x = std::clamp(col_def.data->x, 0.f, 1.f);
                    col_def.data->y = std::clamp(col_def.data->y, 0.f, 1.f);
                    col_def.data->z = std::clamp(col_def.data->z, 0.f, 1.f);
                    col_def.data->w = std::clamp(col_def.data->w, 0.f, 1.f);
                }
                if (imgui::TableSetColumnIndex(color_col_checked)) {
                    auto label = make_str_static<64>("## %s.checked", col_def.label);
                    imgui::Checkbox(label.data(), col_def.check);
                }
                if (imgui::TableSetColumnIndex(color_col_reset)) {
                    auto btn_label = make_str_static<64>(ICON_CI_REFRESH "## Swan %s", col_def.label);
                    if (imgui::Button(btn_label.data())) {
                        *col_def.data = col_def.get_default_data();
                        swan_color_changed = true;
                    }
                    if (imgui::IsItemHovered()) {
                        imgui::SetTooltip("Reset color [%s]", col_def.label);
                    }
                }
            };

            for (auto &c : s_swan_colors) {
                if (cstr_empty(s_search_input.data()) || StrStrIA(c.label, s_search_input.data())) {
                    imgui::TableNextRow();
                    render_swan_color_picker(c);
                }
            }

            // imgui::Separator();

            struct imgui_color_def
            {
                char const *label = nullptr;
                u64 index = u64(-1);
            };

            imgui_color_def imgui_colors[] = {
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

            auto render_imgui_color_picker = [&](imgui_color_def &col_def) noexcept {
                ImVec4 *data = &style.Colors[col_def.index];
                ImVec4 const &fallback_val = fallback_style.Colors[col_def.index];

                if (imgui::TableSetColumnIndex(color_col_property)) {
                    imgui::AlignTextToFramePadding();
                    imgui::Text("%s", col_def.label);
                }
                if (imgui::TableSetColumnIndex(color_col_value)) {
                    imgui::ScopedAvailWidth w = {};
                    auto color_edit_label = make_str_static<64>("## %s", col_def.label);
                    imgui::ColorEdit4(color_edit_label.data(), (f32 *)&data->x, color_edit_flags);

                    data->x = std::clamp(data->x, 0.f, 1.f);
                    data->y = std::clamp(data->y, 0.f, 1.f);
                    data->z = std::clamp(data->z, 0.f, 1.f);
                    data->w = std::clamp(data->w, 0.f, 1.f);
                }
                if (imgui::TableSetColumnIndex(color_col_checked)) {
                    auto label = make_str_static<64>("## %s.checked", col_def.label);
                    imgui::Checkbox(label.data(), &( global_state::settings().checks_ImGuiCol[col_def.index] ));
                }
                if (imgui::TableSetColumnIndex(color_col_reset)) {
                    auto btn_label = make_str_static<64>(ICON_CI_REFRESH "## ImGuiCol_%s", col_def.label);
                    if (imgui::Button(btn_label.data())) {
                        *data = fallback_val;
                    }
                    if (imgui::IsItemHovered()) {
                        imgui::SetTooltip("Reset color [%s]", col_def.label);
                    }
                }
            };

            u64 i = 0;
            for (; i < lengthof(imgui_colors); ++i) {
                auto &c = imgui_colors[i];
                if (cstr_empty(s_search_input.data()) || StrStrIA(c.label, s_search_input.data())) {
                    imgui::TableNextRow();
                    render_imgui_color_picker(c);
                }
            }
            assert(i == ImGuiCol_COUNT);

            imgui::EndTable();
        }
    }
    imgui::EndChild();
}

static void render_style_tab_content(context ctx) noexcept
{
    auto [style, ImGui_style_at_frame_end, fallback_style,
        ImGuiCol_checks_at_frame_start, swan_color_changed, swan_color_checks_at_frame_start] = ctx;

    u64 constexpr longest_label = 32; // 26
    static std::array<char, longest_label+1> s_search_input = {};

    {
        imgui::ScopedItemWidth w(imgui::CalcTextSize("_").x * longest_label);
        imgui::InputTextWithHint("## theme_editor Style search", ICON_CI_SEARCH, s_search_input.data(), s_search_input.max_size());
    }

    imgui::SameLine();

    if (imgui::Button(ICON_CI_REFRESH "## Style reset all")) {
        imgui::OpenConfirmationModalWithCallback(
            /* confirmation_id  = */ swan_id_confirm_theme_editor_style_reset,
            /* confirmation_msg = */ "Are you sure you want to reset all non-color styles to their default values? This action cannot be undone.",
            /* on_yes_callback  = */
            [&]() noexcept {
                auto &style = imgui::GetStyle();

                ImVec4 colors[ImGuiCol_COUNT];
                (void) memcpy(colors, style.Colors, sizeof(style.Colors));

                (void) std::memcpy(&style, &fallback_style, sizeof(style));
                (void) std::memcpy(&style.Colors, &colors, sizeof(style.Colors));

                (void) global_state::settings().save_to_disk();
            },
            /* confirmation_enabled = */ &(global_state::settings().confirm_theme_editor_style_reset)
        );
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Reset all styles");
    }

    imgui::SameLine(0,0);

    if (imgui::Button(ICON_CI_CODE "## Style")) {
        std::string serialized = {};
        serialized.reserve(global_state::page_size());
        serialize_ImGuiStyle_all_except_colors(style, serialized, serialize_ImGuiStyle_mode::cpp_code);
        imgui::SetClipboardText(serialized.c_str());
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Copy styles as C++ code");
    }

    imgui::SameLine(0,0);

    if (imgui::Button(ICON_CI_SYMBOL_TEXT "## Style")) {
        std::string serialized = {};
        serialized.reserve(global_state::page_size());
        serialize_ImGuiStyle_all_except_colors(style, serialized, serialize_ImGuiStyle_mode::plain_text);
        imgui::SetClipboardText(serialized.c_str());
    }
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("Copy styles as [swan_settings.txt] text");
    }

    if (imgui::BeginChild("## theme_editor Style child")) {
        enum style_col : ImGuiID {
            style_col_property,
            style_col_value,
            style_col_reset,
            style_col_count
        };
        s32 table_flags =
            ImGuiTableFlags_SizingStretchProp|
            ImGuiTableFlags_Hideable|
            ImGuiTableFlags_Resizable|
            ImGuiTableFlags_Reorderable|
            ImGuiTableFlags_BordersV|
            ImGuiTableFlags_ScrollY|
            (global_state::settings().tables_alt_row_bg ? ImGuiTableFlags_RowBg : 0)|
            (global_state::settings().table_borders_in_body ? 0 : ImGuiTableFlags_NoBordersInBody)
        ;
        imgui::ScopedItemFlag no_nav(ImGuiItemFlags_NoNav, true);

        if (imgui::BeginTable("## theme_editor Style table", style_col_count, table_flags)) {
            imgui::TableSetupColumn("Property", 0, 0.f, style_col_property);
            imgui::TableSetupColumn("Value", 0, 0.f, style_col_value);
            imgui::TableSetupColumn("Reset", 0, 0.f, style_col_reset);
            ImGui::TableSetupScrollFreeze(0, 1);
            imgui::TableHeadersRow();

            auto render_input_f32 = [](char const *label, f32 &val, f32 const &fallback_val, char const *format, f32 min = NAN, f32 max = NAN) noexcept {
                if (cstr_empty(s_search_input.data()) || StrStrIA(label, s_search_input.data())) {
                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(style_col_property)) {
                        imgui::AlignTextToFramePadding();
                        imgui::Text("%s", label);
                    }
                    if (imgui::TableSetColumnIndex(style_col_value)) {
                        imgui::ScopedAvailWidth w = {};
                        imgui::ScopedStyle<f32> fbs(imgui::GetStyle().FrameBorderSize, 0);
                        auto actual_label = make_str_static<64>("## %s", label);
                        imgui::InputFloat(actual_label.data(), &val, 0, 0, format);
                    }
                    if (imgui::TableSetColumnIndex(style_col_reset)) {
                        auto btn_label = make_str_static<64>(ICON_CI_REFRESH "## %s", label);
                        if (imgui::Button(btn_label.data())) {
                            val = fallback_val;
                        }
                        if (imgui::IsItemHovered()) {
                            imgui::SetTooltip("Reset style [%s]", label);
                        }
                    }
                }
                if (min != NAN || max != NAN) {
                    assert(min != NAN && max != NAN && "Both min and max should have a value");
                    val = std::clamp(val, min, max);
                }
            };

            auto render_input_bool = [](char const *label, bool &val, bool const &fallback_val) noexcept {
                if (cstr_empty(s_search_input.data()) || StrStrIA(label, s_search_input.data())) {
                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(style_col_property)) {
                        imgui::AlignTextToFramePadding();
                        imgui::Text("%s", label);
                    }
                    if (imgui::TableSetColumnIndex(style_col_value)) {
                        auto chk_label = make_str_static<64>("## Checkbox %s", label);
                        imgui::Checkbox(chk_label.data(), &val);
                    }
                    if (imgui::TableSetColumnIndex(style_col_reset)) {
                        auto btn_label = make_str_static<64>(ICON_CI_REFRESH "## %s", label);
                        if (imgui::Button(btn_label.data())) {
                            val = fallback_val;
                        }
                        if (imgui::IsItemHovered()) {
                            imgui::SetTooltip("Reset style [%s]", label);
                        }
                    }
                }
            };

            auto render_input_ImGuiDir = [](char const *label, ImGuiDir &val, ImGuiDir const &fallback_val) noexcept {
                if (cstr_empty(s_search_input.data()) || StrStrIA(label, s_search_input.data())) {
                    imgui::TableNextRow();

                    if (imgui::TableSetColumnIndex(style_col_property)) {
                        imgui::AlignTextToFramePadding();
                        imgui::Text("%s", label);
                    }
                    if (imgui::TableSetColumnIndex(style_col_value)) {
                        static char const *s_options[] = {
                            /*ImGuiDir_*/"Left",
                            /*ImGuiDir_*/"Right",
                            // /*ImGuiDir_*/"Up",
                            // /*ImGuiDir_*/"Down",
                        };
                        imgui::ScopedItemWidth w(imgui::CalcTextSize("123456789_").x);
                        auto combo_label = make_str_static<64>("## Combo %s", label);
                        imgui::Combo(combo_label.data(), &val, s_options, lengthof(s_options));
                    }
                    if (imgui::TableSetColumnIndex(style_col_reset)) {
                        auto btn_label = make_str_static<64>(ICON_CI_REFRESH "## %s", label);
                        if (imgui::Button(btn_label.data())) {
                            val = fallback_val;
                        }
                        if (imgui::IsItemHovered()) {
                            imgui::SetTooltip("Reset style [%s]", label);
                        }
                    }
                }
            };

            render_input_f32("Alpha", style.Alpha, fallback_style.Alpha, "%.2f", 0.1f, 1.f);
            render_input_f32("DisabledAlpha", style.DisabledAlpha, fallback_style.DisabledAlpha, "%.2f", 0.1f, 1.f);
            render_input_f32("WindowPadding.x", style.WindowPadding.x, fallback_style.WindowPadding.x, "%.0f");
            render_input_f32("WindowPadding.y", style.WindowPadding.y, fallback_style.WindowPadding.y, "%.0f");
            render_input_f32("WindowRounding", style.WindowRounding, fallback_style.WindowRounding, "%.0f");
            render_input_f32("WindowBorderSize", style.WindowBorderSize, fallback_style.WindowBorderSize, "%.0f");
            render_input_f32("WindowMinSize.x", style.WindowMinSize.x, fallback_style.WindowMinSize.x, "%.0f");
            render_input_f32("WindowMinSize.y", style.WindowMinSize.y, fallback_style.WindowMinSize.y, "%.0f");
            render_input_f32("WindowTitleAlign.x", style.WindowTitleAlign.x, fallback_style.WindowTitleAlign.x, "%.0f");
            render_input_f32("WindowTitleAlign.y", style.WindowTitleAlign.y, fallback_style.WindowTitleAlign.y, "%.0f");
            render_input_f32("ChildRounding", style.ChildRounding, fallback_style.ChildRounding, "%.0f");
            render_input_f32("ChildBorderSize", style.ChildBorderSize, fallback_style.ChildBorderSize, "%.0f");
            render_input_f32("PopupRounding", style.PopupRounding, fallback_style.PopupRounding, "%.0f");
            render_input_f32("PopupBorderSize", style.PopupBorderSize, fallback_style.PopupBorderSize, "%.0f");
            render_input_f32("FramePadding.x", style.FramePadding.x, fallback_style.FramePadding.x, "%.0f");
            render_input_f32("FramePadding.y", style.FramePadding.y, fallback_style.FramePadding.y, "%.0f");
            render_input_f32("FrameRounding", style.FrameRounding, fallback_style.FrameRounding, "%.0f");
            render_input_f32("FrameBorderSize", style.FrameBorderSize, fallback_style.FrameBorderSize, "%.0f");
            render_input_f32("ItemSpacing.x", style.ItemSpacing.x, fallback_style.ItemSpacing.x, "%.0f");
            render_input_f32("ItemSpacing.y", style.ItemSpacing.y, fallback_style.ItemSpacing.y, "%.0f");
            render_input_f32("ItemInnerSpacing.x", style.ItemInnerSpacing.x, fallback_style.ItemInnerSpacing.x, "%.0f");
            render_input_f32("ItemInnerSpacing.y", style.ItemInnerSpacing.y, fallback_style.ItemInnerSpacing.y, "%.0f");
            render_input_f32("CellPadding.x", style.CellPadding.x, fallback_style.CellPadding.x, "%.0f");
            render_input_f32("CellPadding.y", style.CellPadding.y, fallback_style.CellPadding.y, "%.0f");
            render_input_f32("TouchExtraPadding.x", style.TouchExtraPadding.x, fallback_style.TouchExtraPadding.x, "%.0f");
            render_input_f32("TouchExtraPadding.y", style.TouchExtraPadding.y, fallback_style.TouchExtraPadding.y, "%.0f");
            render_input_f32("IndentSpacing", style.IndentSpacing, fallback_style.IndentSpacing, "%.0f");
            render_input_f32("ColumnsMinSpacing", style.ColumnsMinSpacing, fallback_style.ColumnsMinSpacing, "%.0f");
            render_input_f32("ScrollbarSize", style.ScrollbarSize, fallback_style.ScrollbarSize, "%.0f");
            render_input_f32("ScrollbarRounding", style.ScrollbarRounding, fallback_style.ScrollbarRounding, "%.0f");
            render_input_f32("GrabMinSize", style.GrabMinSize, fallback_style.GrabMinSize, "%.0f");
            render_input_f32("GrabRounding", style.GrabRounding, fallback_style.GrabRounding, "%.0f");
            render_input_f32("LogSliderDeadzone", style.LogSliderDeadzone, fallback_style.LogSliderDeadzone, "%.0f");
            render_input_f32("TabRounding", style.TabRounding, fallback_style.TabRounding, "%.0f");
            render_input_f32("TabBorderSize", style.TabBorderSize, fallback_style.TabBorderSize, "%.0f");
            render_input_f32("TabMinWidthForCloseButton", style.TabMinWidthForCloseButton, fallback_style.TabMinWidthForCloseButton, "%.0f");
            render_input_f32("ButtonTextAlign.x", style.ButtonTextAlign.x, fallback_style.ButtonTextAlign.x, "%.0f");
            render_input_f32("ButtonTextAlign.y", style.ButtonTextAlign.y, fallback_style.ButtonTextAlign.y, "%.0f");
            render_input_f32("SelectableTextAlign.x", style.SelectableTextAlign.x, fallback_style.SelectableTextAlign.x, "%.0f");
            render_input_f32("SelectableTextAlign.y", style.SelectableTextAlign.y, fallback_style.SelectableTextAlign.y, "%.0f");
            render_input_f32("SeparatorTextBorderSize", style.SeparatorTextBorderSize, fallback_style.SeparatorTextBorderSize, "%.0f");
            render_input_f32("SeparatorTextAlign.x", style.SeparatorTextAlign.x, fallback_style.SeparatorTextAlign.x, "%.0f");
            render_input_f32("SeparatorTextAlign.y", style.SeparatorTextAlign.y, fallback_style.SeparatorTextAlign.y, "%.0f");
            render_input_f32("SeparatorTextPadding.x", style.SeparatorTextPadding.x, fallback_style.SeparatorTextPadding.x, "%.0f");
            render_input_f32("SeparatorTextPadding.y", style.SeparatorTextPadding.y, fallback_style.SeparatorTextPadding.y, "%.0f");
            render_input_f32("DisplayWindowPadding.x", style.DisplayWindowPadding.x, fallback_style.DisplayWindowPadding.x, "%.0f");
            render_input_f32("DisplayWindowPadding.y", style.DisplayWindowPadding.y, fallback_style.DisplayWindowPadding.y, "%.0f");
            render_input_f32("DisplaySafeAreaPadding.x", style.DisplaySafeAreaPadding.x, fallback_style.DisplaySafeAreaPadding.x, "%.0f");
            render_input_f32("DisplaySafeAreaPadding.y", style.DisplaySafeAreaPadding.y, fallback_style.DisplaySafeAreaPadding.y, "%.0f");
            render_input_f32("MouseCursorScale", style.MouseCursorScale, fallback_style.MouseCursorScale, "%.0f");
            render_input_f32("CurveTessellationTol", style.CurveTessellationTol, fallback_style.CurveTessellationTol, "%.3f");
            render_input_f32("CircleTessellationMaxError", style.CircleTessellationMaxError, fallback_style.CircleTessellationMaxError, "%.3f");

            render_input_ImGuiDir("WindowMenuButtonPosition", style.WindowMenuButtonPosition, fallback_style.WindowMenuButtonPosition);
            render_input_ImGuiDir("ColorButtonPosition", style.ColorButtonPosition, fallback_style.ColorButtonPosition);

            render_input_bool("AntiAliasedLines", style.AntiAliasedLines, fallback_style.AntiAliasedLines);
            render_input_bool("AntiAliasedLinesUseTex", style.AntiAliasedLinesUseTex, fallback_style.AntiAliasedLinesUseTex);
            render_input_bool("AntiAliasedFill", style.AntiAliasedFill, fallback_style.AntiAliasedFill);

            imgui::EndTable();
        }
    }
    imgui::EndChild();
}

bool swan_windows::render_theme_editor(bool &open, ImGuiStyle const &fallback_style, [[maybe_unused]] bool any_popups_open) noexcept
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;

    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::theme_editor), &open, window_flags)) {
        return false;
    }

    // auto &io = imgui::GetIO();
    auto &style = imgui::GetStyle();
    ImGuiStyle ImGui_style_at_frame_start = style;
    bool ImGuiCol_checks_at_frame_start[ImGuiCol_COUNT] = {};
    bool swan_color_changed = false;

    bool swan_color_checks_at_frame_start[7] = {
        global_state::settings().check_success_color,
        global_state::settings().check_warning_color,
        global_state::settings().check_warning_lite_color,
        global_state::settings().check_error_color,
        global_state::settings().check_directory_color,
        global_state::settings().check_file_color,
        global_state::settings().check_symlink_color,
    };

    memcpy(&ImGuiCol_checks_at_frame_start, &( global_state::settings().checks_ImGuiCol ), ImGuiCol_COUNT * sizeof(bool));

    context ctx = {
        .style = style,
        .ImGui_style_at_frame_start = ImGui_style_at_frame_start,
        .fallback_style = fallback_style,
        .ImGuiCol_checks_at_frame_start = ImGuiCol_checks_at_frame_start,
        .swan_color_changed = swan_color_changed,
        .swan_color_checks_at_frame_start = swan_color_checks_at_frame_start,
    };

    if (ImGui::BeginTabBar("MyTabBar")) {
        if (ImGui::BeginTabItem("Colors")) {
            render_colors_tab_content(ctx);
            imgui::EndTabItem();
        }
        if (imgui::BeginTabItem("ImGuiStyle")) {
            render_style_tab_content(ctx);
            imgui::EndTabItem();
        }
        imgui::EndTabBar();
    }

    static bool s_save_requested = false;
    static std::optional<time_point_precise_t> s_last_save_time = std::nullopt;

    auto const &ImGui_style_at_frame_end = style;
    //! this is dodgy because padding bytes have undefined values. But I don't know of a compact way to do this comparison
    //! because ImGuiStyle does not have any comparison operators
    bool ImGui_style_changed = memcmp(&ImGui_style_at_frame_start, &ImGui_style_at_frame_end, sizeof(ImGui_style_at_frame_start)) != 0;

    bool ImGuiCol_checks_at_frame_end[ImGuiCol_COUNT] = {};
    memcpy(&ImGuiCol_checks_at_frame_end, &( global_state::settings().checks_ImGuiCol ), ImGuiCol_COUNT * sizeof(bool));
    bool ImGuiCol_checks_changed = memcmp(&ImGuiCol_checks_at_frame_start, &ImGuiCol_checks_at_frame_end, sizeof(ImGuiCol_checks_at_frame_start)) != 0;

    bool swan_color_checks_at_frame_end[sizeof(swan_color_checks_at_frame_start)] = {
        global_state::settings().check_success_color,
        global_state::settings().check_warning_color,
        global_state::settings().check_warning_lite_color,
        global_state::settings().check_error_color,
        global_state::settings().check_directory_color,
        global_state::settings().check_file_color,
        global_state::settings().check_symlink_color,
    };
    bool swan_color_checks_changed = memcmp(swan_color_checks_at_frame_start, swan_color_checks_at_frame_end, sizeof(swan_color_checks_at_frame_start)) != 0;

    if (ImGui_style_changed || swan_color_changed || ImGuiCol_checks_changed || swan_color_checks_changed) {
        s_save_requested = true;
    }

    if (s_save_requested) {
        if (!s_last_save_time.has_value()) {
            // first save, don't wait
            (void) global_state::settings().save_to_disk();
            s_last_save_time = get_time_precise();
            s_save_requested = false;
        }
        else {
            s64 ms_since_last_save = time_diff_ms(s_last_save_time.value(), get_time_precise());
            if (ms_since_last_save >= 250) {
                (void) global_state::settings().save_to_disk();
                s_last_save_time = get_time_precise();
                s_save_requested = false;
            } else {
                // wait to avoid spamming disk
            }
        }
    }

    return true;
}
