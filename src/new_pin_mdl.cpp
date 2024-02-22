#include "imgui/imgui.h"

#include "common_fns.hpp"
#include "imgui_specific.hpp"

static bool s_new_pin_open = false;
static bool s_new_pin_enable_path_input = true;
static swan_path_t s_new_pin_init_path = {};

void swan_popup_modals::open_new_pin(swan_path_t const &init_path, bool mutable_path) noexcept
{
    s_new_pin_open = true;
    s_new_pin_enable_path_input = mutable_path;
    s_new_pin_init_path = init_path;
}

bool swan_popup_modals::is_open_new_pin() noexcept
{
    return s_new_pin_open;
}

void swan_popup_modals::render_new_pin() noexcept
{
    if (s_new_pin_open) {
        imgui::OpenPopup(swan_popup_modals::new_pin);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::new_pin, nullptr)) {
        return;
    }

    static char label_input[pinned_path::LABEL_MAX_LEN + 1] = {};
    static swan_path_t path_input = {};
    static ImVec4 color_input = dir_color();
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() {
        s_new_pin_open = false;
        s_new_pin_enable_path_input = true;
        init_empty_cstr(s_new_pin_init_path.data());

        init_empty_cstr(label_input);
        init_empty_cstr(path_input.data());
        err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        // when modal just opened:
        imgui::SetKeyboardFocusHere(0); // set focus on label input

        path_input = s_new_pin_init_path;

        // set initial label input value to filename if it fits
        {
            char const *filename = get_file_name(s_new_pin_init_path.data());
            u64 filename_len = strlen(filename);
            if (filename_len <= pinned_path::LABEL_MAX_LEN) {
                strncpy(label_input, filename, lengthof(label_input));
            } else {
                // leave it blank
            }
        }
    }

    {
        imgui::ScopedAvailWidth width(imgui::CalcTextSize(" 00/64").x);

        if (imgui::InputTextWithHint("##pin_label", "Label...", label_input, lengthof(label_input))) {
            err_msg.clear();
        }
    }
    imgui::SameLine();
    imgui::Text("%zu/%zu", strlen(label_input), pinned_path::LABEL_MAX_LEN);

    {
        [[maybe_unused]] imgui::ScopedDisable disabled(!s_new_pin_enable_path_input);
        [[maybe_unused]] imgui::ScopedAvailWidth width = {};

        if (imgui::InputTextWithHint("##pin_path", "Path...", path_input.data(), path_input.size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars()))
        {
            err_msg.clear();
        }
    }

    imgui::ColorEdit4("Edit Color##pin", &color_input.x, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
    imgui::SameLine();
    imgui::TextColored(color_input, "Color");

    imgui::SameLineSpaced(1);

    if (imgui::Button("Create##pin") && !strempty(path_input.data()) && !strempty(label_input)) {
        swan_path_t path = path_squish_adjacent_separators(path_input);
        path_force_separator(path, global_state::settings().dir_separator_utf8);

        global_state::add_pin(color_input, label_input, path, '\\');

        bool success = global_state::save_pins_to_disk();
        print_debug_msg("save_pins_to_disk: %d", success);

        cleanup_and_close_popup();
    }

    imgui::SameLine();

    if (imgui::Button("Cancel##pin")) {
        cleanup_and_close_popup();
    }

    if (!err_msg.empty()) {
        imgui::TextColored(red(), "Error: %s", err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
