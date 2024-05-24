#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace new_pin_modal_global_state
{
    static bool         g_open = false;
    static bool         g_enable_path_text_input = true;
    static swan_path  g_initial_path_value = {};
}

void swan_popup_modals::open_new_pin(swan_path const &init_path, bool mutable_path) noexcept
{
    using namespace new_pin_modal_global_state;

    g_open = true;
    g_enable_path_text_input = mutable_path;
    g_initial_path_value = init_path;
}

void swan_popup_modals::render_new_pin() noexcept
{
    using namespace new_pin_modal_global_state;

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_new_pin);
        ImVec2 glyph_size = imgui::CalcTextSize("X");
        auto const &style = imgui::GetStyle();
        f32 window_height = glyph_size.y*4 + style.FramePadding.y*8 + style.WindowPadding.y*2 + style.ItemSpacing.y*2;
        center_window_and_set_size_when_appearing(800, window_height);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_new_pin, nullptr, ImGuiWindowFlags_NoResize)) {
        return;
    }

    static char s_label_input[pinned_path::LABEL_MAX_LEN + 1] = {};
    static swan_path s_path_input = {};
    static ImVec4 s_color_input = imgui::GetStyleColorVec4(ImGuiCol_Text);
    static std::string s_err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        g_enable_path_text_input = true;
        cstr_clear(g_initial_path_value.data());

        cstr_clear(s_label_input);
        cstr_clear(s_path_input.data());
        s_err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        // when modal just opened:
        imgui::SetKeyboardFocusHere(0); // set focus on label input

        s_path_input = g_initial_path_value;

        // set initial label input value to filename if it fits
        {
            char const *filename = path_find_filename(g_initial_path_value.data());
            u64 filename_len = strlen(filename);
            if (filename_len <= pinned_path::LABEL_MAX_LEN) {
                strncpy(s_label_input, filename, lengthof(s_label_input));
            } else {
                // leave it blank
            }
        }
    }

    {
        imgui::ScopedAvailWidth width(imgui::CalcTextSize(" 00/64").x);

        if (imgui::InputTextWithHint("## pin_label", "Label...", s_label_input, lengthof(s_label_input))) {
            s_err_msg.clear();
        }
    }
    imgui::SameLine();
    imgui::Text("%zu/%zu", strlen(s_label_input), pinned_path::LABEL_MAX_LEN);

    {
        [[maybe_unused]] imgui::ScopedDisable disabled(!g_enable_path_text_input);
        [[maybe_unused]] imgui::ScopedAvailWidth width = {};

        if (imgui::InputTextWithHint("## pin_path", "Path...", s_path_input.data(), s_path_input.size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars()))
        {
            s_err_msg.clear();
        }
    }

    // imgui::Spacing();

    if (imgui::Button("Create" "## pin") && !cstr_empty(s_path_input.data()) && !cstr_empty(s_label_input)) {
        swan_path path = path_squish_adjacent_separators(s_path_input);
        path_force_separator(path, global_state::settings().dir_separator_utf8);

        global_state::pinned_add(s_color_input, s_label_input, path, '\\');

        bool success = global_state::pinned_save_to_disk();
        print_debug_msg("pinned_save_to_disk: %d", success);

        cleanup_and_close_popup();
    }

    imgui::SameLine();

    if (imgui::Button("Cancel" "## pin")) {
        cleanup_and_close_popup();
    }

    imgui::SameLineSpaced(1);

    {
        imgui::ScopedDisable d(true);
        imgui::ColorEdit4("Edit Color" "## pin", &s_color_input.x, ImGuiColorEditFlags_NoAlpha|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
        imgui::SameLine();
        imgui::TextColored(s_color_input, "Color");
    }

    if (!s_err_msg.empty()) {
        imgui::TextColored(error_color(), "Error: %s", s_err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
