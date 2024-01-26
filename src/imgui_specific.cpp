#include "imgui_specific.hpp"
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

ImVec4 orange()                noexcept { return ImVec4(1, 0.5f, 0, 1); }
ImVec4 red()                   noexcept { return ImVec4(1, 0.2f, 0, 1); }
ImVec4 white()                 noexcept { return ImVec4(1, 1, 1, 1); }
ImVec4 dir_color()             noexcept { return get_color(basic_dirent::kind::directory); }
ImVec4 symlink_color()         noexcept { return orange(); }
ImVec4 invalid_symlink_color() noexcept { return get_color(basic_dirent::kind::invalid_symlink); }
ImVec4 file_color()            noexcept { return get_color(basic_dirent::kind::file); }

bool imgui::EnumButton::Render(s32 &enum_value, s32 enum_first, s32 enum_count, char const *labels[], [[maybe_unused]] u64 num_labels) noexcept
{
    [[maybe_unused]] u64 num_enums = enum_count - enum_first;
    assert(num_enums > 1);
    assert(num_enums == num_labels);

    this->current_label = labels[enum_value];

    char buffer[128]; init_empty_cstr(buffer);
    [[maybe_unused]] s32 written = snprintf(buffer, lengthof(buffer), "%s##%zu-%zu", this->current_label, this->rand_1, this->rand_2);
    assert(written > 0);

    if (this->name != nullptr && !strempty(this->name)) {
        imgui::AlignTextToFramePadding();
        imgui::TextUnformatted(this->name);
        imgui::SameLine();
    }

    bool activated = imgui::Button(buffer);

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
        case basic_dirent::kind::directory:            return ImVec4(1, 1, 0, 1);         // yellow
        case basic_dirent::kind::file:                 return ImVec4(0.85f, 1, 0.85f, 1); // pale_green
        case basic_dirent::kind::symlink_to_directory: return ImVec4(1, 1, 0, 1);         // yellow
        case basic_dirent::kind::symlink_to_file:      return ImVec4(0.85f, 1, 0.85f, 1); // pale_green
        case basic_dirent::kind::invalid_symlink:      return ImVec4(1, 0, 0, 1);         // red
        default:                                       return ImVec4(1, 1, 1, 1);         // white
    }
}

void imgui::Spacing(u64 n) noexcept
{
    for (u64 i = 0; i < n; ++i) {
        ImGui::Spacing();
    }
}

void imgui::SameLineSpaced(u64 num_spacing_calls) noexcept
{
    ImGui::SameLine();
    for (u64 i = 0; i < num_spacing_calls; ++i) {
        ImGui::Spacing();
        ImGui::SameLine();
    }
}
