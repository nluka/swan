#include "stdafx.hpp"
#include "imgui_dependent_functions.hpp"
#include "util.hpp"

void new_frame(char const *ini_file_path) noexcept
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    imgui::NewFrame(ini_file_path);
}

void render_frame(GLFWwindow *window) noexcept
{
    imgui::Render();

    s32 display_w, display_h;
    ImVec4 clear_color(0.45f, 0.55f, 0.60f, 1.00f);

    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(imgui::GetDrawData());

    glfwSwapBuffers(window);
}

ImVec4 success_color() noexcept { return global_state::settings().success_color; }
ImVec4 warning_color() noexcept { return global_state::settings().warning_color; }
ImVec4 error_color() noexcept { return global_state::settings().error_color; }
ImVec4 directory_color() noexcept { return get_color(basic_dirent::kind::directory); }
ImVec4 symlink_color() noexcept { return get_color(basic_dirent::kind::symlink_ambiguous); }
ImVec4 file_color() noexcept { return get_color(basic_dirent::kind::file); }

bool imgui::EnumButton::Render(s32 &enum_value, s32 enum_first, s32 enum_count, char const *labels[], [[maybe_unused]] u64 num_labels) noexcept
{
    [[maybe_unused]] u64 num_enums = enum_count - enum_first;
    assert(num_enums > 1);
    assert(num_enums == num_labels);

    this->current_label = labels[enum_value];

    if (this->name != nullptr && !strempty(this->name)) {
        imgui::AlignTextToFramePadding();
        imgui::TextUnformatted(this->name);
        imgui::SameLine();
    }

    auto label = make_str_static<128>("%s##%zu-%zu", this->current_label, this->rand_1, this->rand_2);
    bool activated = imgui::Button(label.data());

    if (activated) {
        inc_or_wrap(enum_value, enum_first, enum_count - 1);
        this->rand_1 = fast_rand(0, UINT32_MAX);
        this->rand_2 = fast_rand(0, UINT32_MAX);
    }

    return activated;
}

s32 filter_chars_callback(ImGuiInputTextCallbackData *data) noexcept
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCharFilter) {
        wchar_t *chars_to_filter = (filter_chars_callback_user_data_t)data->UserData;
        bool is_forbidden = StrChrW(chars_to_filter, data->EventChar);
        if (is_forbidden) {
            data->EventChar = L'\0';
        }
    }

    return 0;
}

ImVec4 get_color(basic_dirent::kind t) noexcept
{
    switch (t) {
        case basic_dirent::kind::directory:            return global_state::settings().directory_color;
        case basic_dirent::kind::file:                 return global_state::settings().file_color;
        case basic_dirent::kind::symlink_to_directory: return global_state::settings().directory_color;
        case basic_dirent::kind::symlink_to_file:      return global_state::settings().file_color;
        case basic_dirent::kind::symlink_ambiguous:    return global_state::settings().symlink_color;
        case basic_dirent::kind::invalid_symlink:      return global_state::settings().error_color;
        default:                                       return imgui::GetStyle().Colors[ImGuiCol_Text];
    }
}

void center_window_and_set_size_when_appearing(f32 width, f32 height) noexcept
{
    f32 clamped_width = std::min(width, imgui::GetWindowViewport()->Size.x - 10.f);
    f32 clamped_height = std::min(height, imgui::GetWindowViewport()->Size.y - 10.f);

    imgui::SetNextWindowPos(
        {
            (f32(global_state::settings().window_w) / 2.f) - (clamped_width / 2.f),
            (f32(global_state::settings().window_h) / 2.f) - (clamped_height / 2.f),
        },
        ImGuiCond_Appearing
    );

    imgui::SetNextWindowSize({ clamped_width, clamped_height }, ImGuiCond_Appearing);
}

void serialize_ImGuiStyle_all_except_colors(ImGuiStyle const &style, std::string &out, serialize_ImGuiStyle_mode mode) noexcept
{
    auto append_f32_plain_text = [&](char const *label, f32 const &val) noexcept {
        auto txt = make_str_static<128>("style.%s %.3f\n", label, val);
        out += txt.data();
    };
    auto append_f32_cpp_code = [&](char const *label, f32 const &val) noexcept {
        auto txt = make_str_static<128>("style.%s = %.3ff;\n", label, val);
        out += txt.data();
    };

    auto append_ImGuiDir_plain_text = [&](char const *label, ImGuiDir const &val) noexcept {
        s32 const &real_val = (s32)val;
        auto txt = make_str_static<128>("style.%s %d\n", label, real_val);
        out += txt.data();
    };
    auto append_ImGuiDir_cpp_code = [&](char const *label, ImGuiDir const &val) noexcept {
        s32 const &real_val = (s32)val;
        auto txt = make_str_static<128>("style.%s = static_cast<ImGuiDir>(%d);\n", label, real_val);
        out += txt.data();
    };

    auto append_bool_plain_text = [&](char const *label, bool const &val) noexcept {
        s32 const &real_val = (s32)val;
        auto txt = make_str_static<128>("style.%s %d\n", label, real_val);
        out += txt.data();
    };
    auto append_bool_cpp_code = [&](char const *label, bool const &val) noexcept {
        s32 const &real_val = (s32)val;
        auto txt = make_str_static<128>("style.%s = static_cast<bool>(%d);\n", label, real_val);
        out += txt.data();
    };

    std::function< void (char const *, f32 const &) > append_f32;
    std::function< void (char const *, ImGuiDir const &) > append_ImGuiDir;
    std::function< void (char const *, bool const &) > append_bool;

    if (mode == serialize_ImGuiStyle_mode::plain_text) {
        append_f32 = append_f32_plain_text;
        append_ImGuiDir = append_ImGuiDir_plain_text;
        append_bool = append_bool_plain_text;
    }
    else if (mode == serialize_ImGuiStyle_mode::cpp_code) {
        append_f32 = append_f32_cpp_code;
        append_ImGuiDir = append_ImGuiDir_cpp_code;
        append_bool = append_bool_cpp_code;
    }
    else {
        assert(false && "Bad serialize_ImGuiStyle_mode");
    }

    append_f32("Alpha", style.Alpha);
    append_f32("DisabledAlpha", style.DisabledAlpha);
    append_f32("WindowPadding.x", style.WindowPadding.x);
    append_f32("WindowPadding.y", style.WindowPadding.y);
    append_f32("WindowRounding", style.WindowRounding);
    append_f32("WindowBorderSize", style.WindowBorderSize);
    append_f32("WindowMinSize.x", style.WindowMinSize.x);
    append_f32("WindowMinSize.y", style.WindowMinSize.y);
    append_f32("WindowTitleAlign.x", style.WindowTitleAlign.x);
    append_f32("WindowTitleAlign.y", style.WindowTitleAlign.y);
    append_ImGuiDir("WindowMenuButtonPosition", style.WindowMenuButtonPosition);
    append_f32("ChildRounding", style.ChildRounding);
    append_f32("ChildBorderSize", style.ChildBorderSize);
    append_f32("PopupRounding", style.PopupRounding);
    append_f32("PopupBorderSize", style.PopupBorderSize);
    append_f32("FramePadding.x", style.FramePadding.x);
    append_f32("FramePadding.y", style.FramePadding.y);
    append_f32("FrameRounding", style.FrameRounding);
    append_f32("FrameBorderSize", style.FrameBorderSize);
    append_f32("ItemSpacing.x", style.ItemSpacing.x);
    append_f32("ItemSpacing.y", style.ItemSpacing.y);
    append_f32("ItemInnerSpacing.x", style.ItemInnerSpacing.x);
    append_f32("ItemInnerSpacing.y", style.ItemInnerSpacing.y);
    append_f32("CellPadding.x", style.CellPadding.x);
    append_f32("CellPadding.y", style.CellPadding.y);
    append_f32("TouchExtraPadding.x", style.TouchExtraPadding.x);
    append_f32("TouchExtraPadding.y", style.TouchExtraPadding.y);
    append_f32("IndentSpacing", style.IndentSpacing);
    append_f32("ColumnsMinSpacing", style.ColumnsMinSpacing);
    append_f32("ScrollbarSize", style.ScrollbarSize);
    append_f32("ScrollbarRounding", style.ScrollbarRounding);
    append_f32("GrabMinSize", style.GrabMinSize);
    append_f32("GrabRounding", style.GrabRounding);
    append_f32("LogSliderDeadzone", style.LogSliderDeadzone);
    append_f32("TabRounding", style.TabRounding);
    append_f32("TabBorderSize", style.TabBorderSize);
    append_f32("TabMinWidthForCloseButton", style.TabMinWidthForCloseButton);
    append_ImGuiDir("ColorButtonPosition", style.ColorButtonPosition);
    append_f32("ButtonTextAlign.x", style.ButtonTextAlign.x);
    append_f32("ButtonTextAlign.y", style.ButtonTextAlign.y);
    append_f32("SelectableTextAlign.x", style.SelectableTextAlign.x);
    append_f32("SelectableTextAlign.y", style.SelectableTextAlign.y);
    append_f32("SeparatorTextBorderSize", style.SeparatorTextBorderSize);
    append_f32("SeparatorTextAlign.x", style.SeparatorTextAlign.x);
    append_f32("SeparatorTextAlign.y", style.SeparatorTextAlign.y);
    append_f32("SeparatorTextPadding.x", style.SeparatorTextPadding.x);
    append_f32("SeparatorTextPadding.y", style.SeparatorTextPadding.y);
    append_f32("DisplayWindowPadding.x", style.DisplayWindowPadding.x);
    append_f32("DisplayWindowPadding.y", style.DisplayWindowPadding.y);
    append_f32("DisplaySafeAreaPadding.x", style.DisplaySafeAreaPadding.x);
    append_f32("DisplaySafeAreaPadding.y", style.DisplaySafeAreaPadding.y);
    append_f32("MouseCursorScale", style.MouseCursorScale);
    append_bool("AntiAliasedLines", style.AntiAliasedLines);
    append_bool("AntiAliasedLinesUseTex", style.AntiAliasedLinesUseTex);
    append_bool("AntiAliasedFill", style.AntiAliasedFill);
    append_f32("CurveTessellationTol", style.CurveTessellationTol);
    append_f32("CircleTessellationMaxError", style.CircleTessellationMaxError);
}

void serialize_ImGuiStyle_only_colors(ImVec4 const *colors, std::string &out, serialize_ImGuiStyle_mode mode) noexcept
{
    auto append_ImVec4_plain_text = [&](char const *label, ImVec4 const &val) noexcept {
        auto txt = make_str_static<128>("ImGuiCol.%s %f %f %f %f\n", label, val.x, val.y, val.z, val.w);
        out += txt.data();
    };
    auto append_ImVec4_cpp_code = [&](char const *label, ImVec4 const &val) noexcept {
        auto txt = make_str_static<128>("style.Colors[ImGuiCol_%s] = ImVec4(%ff, %ff, %ff, %ff);\n", label, val.x, val.y, val.z, val.w);
        out += txt.data();
    };

    std::function< void (char const *, ImVec4 const &) > append_ImVec4;

    if (mode == serialize_ImGuiStyle_mode::plain_text) {
        append_ImVec4 = append_ImVec4_plain_text;
    }
    else if (mode == serialize_ImGuiStyle_mode::cpp_code) {
        append_ImVec4 = append_ImVec4_cpp_code;
    }
    else {
        assert(false && "Bad serialize_ImGuiStyle_mode");
    }

    append_ImVec4("Text", colors[ImGuiCol_Text]);
    append_ImVec4("TextDisabled", colors[ImGuiCol_TextDisabled]);
    append_ImVec4("WindowBg", colors[ImGuiCol_WindowBg]);
    append_ImVec4("ChildBg", colors[ImGuiCol_ChildBg]);
    append_ImVec4("PopupBg", colors[ImGuiCol_PopupBg]);
    append_ImVec4("Border", colors[ImGuiCol_Border]);
    append_ImVec4("BorderShadow", colors[ImGuiCol_BorderShadow]);
    append_ImVec4("FrameBg", colors[ImGuiCol_FrameBg]);
    append_ImVec4("FrameBgHovered", colors[ImGuiCol_FrameBgHovered]);
    append_ImVec4("FrameBgActive", colors[ImGuiCol_FrameBgActive]);
    append_ImVec4("TitleBg", colors[ImGuiCol_TitleBg]);
    append_ImVec4("TitleBgActive", colors[ImGuiCol_TitleBgActive]);
    append_ImVec4("TitleBgCollapsed", colors[ImGuiCol_TitleBgCollapsed]);
    append_ImVec4("MenuBarBg", colors[ImGuiCol_MenuBarBg]);
    append_ImVec4("ScrollbarBg", colors[ImGuiCol_ScrollbarBg]);
    append_ImVec4("ScrollbarGrab", colors[ImGuiCol_ScrollbarGrab]);
    append_ImVec4("ScrollbarGrabHovered", colors[ImGuiCol_ScrollbarGrabHovered]);
    append_ImVec4("ScrollbarGrabActive", colors[ImGuiCol_ScrollbarGrabActive]);
    append_ImVec4("CheckMark", colors[ImGuiCol_CheckMark]);
    append_ImVec4("SliderGrab", colors[ImGuiCol_SliderGrab]);
    append_ImVec4("SliderGrabActive", colors[ImGuiCol_SliderGrabActive]);
    append_ImVec4("Button", colors[ImGuiCol_Button]);
    append_ImVec4("ButtonHovered", colors[ImGuiCol_ButtonHovered]);
    append_ImVec4("ButtonActive", colors[ImGuiCol_ButtonActive]);
    append_ImVec4("Header", colors[ImGuiCol_Header]);
    append_ImVec4("HeaderHovered", colors[ImGuiCol_HeaderHovered]);
    append_ImVec4("HeaderActive", colors[ImGuiCol_HeaderActive]);
    append_ImVec4("Separator", colors[ImGuiCol_Separator]);
    append_ImVec4("SeparatorHovered", colors[ImGuiCol_SeparatorHovered]);
    append_ImVec4("SeparatorActive", colors[ImGuiCol_SeparatorActive]);
    append_ImVec4("ResizeGrip", colors[ImGuiCol_ResizeGrip]);
    append_ImVec4("ResizeGripHovered", colors[ImGuiCol_ResizeGripHovered]);
    append_ImVec4("ResizeGripActive", colors[ImGuiCol_ResizeGripActive]);
    append_ImVec4("Tab", colors[ImGuiCol_Tab]);
    append_ImVec4("TabHovered", colors[ImGuiCol_TabHovered]);
    append_ImVec4("TabActive", colors[ImGuiCol_TabActive]);
    append_ImVec4("TabUnfocused", colors[ImGuiCol_TabUnfocused]);
    append_ImVec4("TabUnfocusedActive", colors[ImGuiCol_TabUnfocusedActive]);
    append_ImVec4("DockingPreview", colors[ImGuiCol_DockingPreview]);
    append_ImVec4("DockingEmptyBg", colors[ImGuiCol_DockingEmptyBg]);
    append_ImVec4("PlotLines", colors[ImGuiCol_PlotLines]);
    append_ImVec4("PlotLinesHovered", colors[ImGuiCol_PlotLinesHovered]);
    append_ImVec4("PlotHistogram", colors[ImGuiCol_PlotHistogram]);
    append_ImVec4("PlotHistogramHovered", colors[ImGuiCol_PlotHistogramHovered]);
    append_ImVec4("TableHeaderBg", colors[ImGuiCol_TableHeaderBg]);
    append_ImVec4("TableBorderStrong", colors[ImGuiCol_TableBorderStrong]);
    append_ImVec4("TableBorderLight", colors[ImGuiCol_TableBorderLight]);
    append_ImVec4("TableRowBg", colors[ImGuiCol_TableRowBg]);
    append_ImVec4("TableRowBgAlt", colors[ImGuiCol_TableRowBgAlt]);
    append_ImVec4("TextSelectedBg", colors[ImGuiCol_TextSelectedBg]);
    append_ImVec4("DragDropTarget", colors[ImGuiCol_DragDropTarget]);
    append_ImVec4("NavHighlight", colors[ImGuiCol_NavHighlight]);
    append_ImVec4("NavWindowingHighlight", colors[ImGuiCol_NavWindowingHighlight]);
    append_ImVec4("NavWindowingDimBg", colors[ImGuiCol_NavWindowingDimBg]);
    append_ImVec4("ModalWindowDimBg", colors[ImGuiCol_ModalWindowDimBg]);
}

std::string serialize_ImGuiStyle(ImGuiStyle const &style, u64 reserve_size, serialize_ImGuiStyle_mode mode) noexcept
{
    std::string str = {};
    str.reserve(reserve_size);

    serialize_ImGuiStyle_all_except_colors(style, str, mode);

    // str += '\n';

    serialize_ImGuiStyle_only_colors(style.Colors, str, mode);

    return str;
}

ImGuiStyle swan_default_imgui_style() noexcept
{
    ImGuiStyle style;

    style.Alpha = 1.000f;
    style.DisabledAlpha = 0.600f;
    style.WindowPadding.x = 12.000f;
    style.WindowPadding.y = 12.000f;
    style.WindowRounding = 5.000f;
    style.WindowBorderSize = 1.000f;
    style.WindowMinSize.x = 32.000f;
    style.WindowMinSize.y = 32.000f;
    style.WindowTitleAlign.x = 0.000f;
    style.WindowTitleAlign.y = 0.500f;
    style.WindowMenuButtonPosition = static_cast<ImGuiDir>(0);
    style.ChildRounding = 0.000f;
    style.ChildBorderSize = 0.000f;
    style.PopupRounding = 2.000f;
    style.PopupBorderSize = 1.000f;
    style.FramePadding.x = 5.000f;
    style.FramePadding.y = 5.000f;
    style.FrameRounding = 2.000f;
    style.FrameBorderSize = 0.000f;
    style.ItemSpacing.x = 10.000f;
    style.ItemSpacing.y = 10.000f;
    style.ItemInnerSpacing.x = 8.000f;
    style.ItemInnerSpacing.y = 8.000f;
    style.CellPadding.x = 10.000f;
    style.CellPadding.y = 5.000f;
    style.TouchExtraPadding.x = 0.000f;
    style.TouchExtraPadding.y = 0.000f;
    style.IndentSpacing = 21.000f;
    style.ColumnsMinSpacing = 6.000f;
    style.ScrollbarSize = 14.000f;
    style.ScrollbarRounding = 0.000f;
    style.GrabMinSize = 12.000f;
    style.GrabRounding = 0.000f;
    style.LogSliderDeadzone = 4.000f;
    style.TabRounding = 5.000f;
    style.TabBorderSize = 0.000f;
    style.TabMinWidthForCloseButton = 0.000f;
    style.ColorButtonPosition = static_cast<ImGuiDir>(1);
    style.ButtonTextAlign.x = 0.500f;
    style.ButtonTextAlign.y = 0.500f;
    style.SelectableTextAlign.x = 0.000f;
    style.SelectableTextAlign.y = 0.000f;
    style.SeparatorTextBorderSize = 0.000f;
    style.SeparatorTextAlign.x = 0.000f;
    style.SeparatorTextAlign.y = 0.500f;
    style.SeparatorTextPadding.x = 20.000f;
    style.SeparatorTextPadding.y = 3.000f;
    style.DisplayWindowPadding.x = 19.000f;
    style.DisplayWindowPadding.y = 19.000f;
    style.DisplaySafeAreaPadding.x = 3.000f;
    style.DisplaySafeAreaPadding.y = 3.000f;
    style.MouseCursorScale = 1.000f;
    style.AntiAliasedLines = static_cast<bool>(1);
    style.AntiAliasedLinesUseTex = static_cast<bool>(1);
    style.AntiAliasedFill = static_cast<bool>(1);
    style.CurveTessellationTol = 1.250f;
    style.CircleTessellationMaxError = 0.300f;

    style.Colors[ImGuiCol_Text] = ImVec4(1.000000f, 1.000000f, 1.000000f, 1.000000f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.500000f, 0.500000f, 0.500000f, 1.000000f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.060000f, 0.060000f, 0.060000f, 1.000000f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.000000f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.080000f, 0.080000f, 0.080000f, 1.000000f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.430000f, 0.430000f, 0.500000f, 0.500000f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.000000f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.137255f, 0.156863f, 0.176471f, 1.000000f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.400000f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.670000f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.040000f, 0.040000f, 0.040000f, 1.000000f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.160000f, 0.290000f, 0.480000f, 1.000000f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.510000f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.140000f, 0.140000f, 0.140000f, 1.000000f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.020000f, 0.020000f, 0.020000f, 0.530000f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.310000f, 0.310000f, 0.310000f, 1.000000f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.410000f, 0.410000f, 0.410000f, 1.000000f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.510000f, 0.510000f, 0.510000f, 1.000000f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.260000f, 0.590000f, 0.980000f, 1.000000f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.240000f, 0.520000f, 0.880000f, 1.000000f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.260000f, 0.590000f, 0.980000f, 1.000000f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.176471f, 0.235294f, 0.313726f, 1.000000f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.235294f, 0.352941f, 0.470588f, 1.000000f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.294118f, 0.431373f, 0.549020f, 1.000000f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.310000f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.800000f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.260000f, 0.590000f, 0.980000f, 1.000000f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.117647f, 0.117647f, 0.117647f, 1.000000f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.117647f, 0.117647f, 0.117647f, 1.000000f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.117647f, 0.117647f, 0.117647f, 1.000000f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.200000f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.670000f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.950000f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.180000f, 0.350000f, 0.580000f, 0.862000f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.800000f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.200000f, 0.410000f, 0.680000f, 1.000000f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.068000f, 0.102000f, 0.148000f, 0.972400f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.136000f, 0.262000f, 0.424000f, 1.000000f);
    style.Colors[ImGuiCol_DockingPreview] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.700000f);
    style.Colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.200000f, 0.200000f, 0.200000f, 1.000000f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(0.610000f, 0.610000f, 0.610000f, 1.000000f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.000000f, 0.430000f, 0.350000f, 1.000000f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.900000f, 0.700000f, 0.000000f, 1.000000f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.000000f, 0.600000f, 0.000000f, 1.000000f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.000000f, 0.000000f, 0.000000f, 0.000000f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.310000f, 0.310000f, 0.350000f, 1.000000f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.230000f, 0.230000f, 0.250000f, 1.000000f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.070000f, 0.070000f, 0.070000f, 0.940000f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.060000f, 0.060000f, 0.060000f, 0.940000f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.260000f, 0.590000f, 0.980000f, 0.350000f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.260000f, 0.590000f, 0.980000f, 1.000000f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.260000f, 0.590000f, 0.980000f, 1.000000f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000000f, 1.000000f, 1.000000f, 0.700000f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.800000f, 0.800000f, 0.800000f, 0.200000f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.800000f, 0.800000f, 0.800000f, 0.350000f);

    return style;
}
