#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

namespace edit_pin_modal_global_state
{
    static bool             g_open = false;
    static pinned_path *    g_target_pin = nullptr;
}

void swan_popup_modals::open_edit_pin(pinned_path *pin) noexcept
{
    using namespace edit_pin_modal_global_state;

    g_open = true;
    bit_set(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_edit_pin);

    assert(pin != nullptr);
    g_target_pin = pin;
}

bool swan_popup_modals::is_open_edit_pin() noexcept
{
    using namespace edit_pin_modal_global_state;

    return g_open;
}

void swan_popup_modals::render_edit_pin() noexcept
{
    using namespace edit_pin_modal_global_state;

    center_window_and_set_size_when_appearing(800, 180);

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::label_edit_pin);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::label_edit_pin, nullptr)) {
        return;
    }

    assert(g_target_pin != nullptr);

    static char s_label_input[pinned_path::LABEL_MAX_LEN + 1] = {};
    static swan_path s_path_input = {};
    static ImVec4 s_color_input = directory_color();
    static std::string s_err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        bit_clear(global_state::popup_modals_open_bit_field(), swan_popup_modals::bit_pos_edit_pin);

        g_target_pin = nullptr;

        init_empty_cstr(s_label_input);
        init_empty_cstr(s_path_input.data());
        s_err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        // set initial focus on label input
        imgui::SetKeyboardFocusHere(0);

        // init input fields
        s_color_input = g_target_pin->color;
        s_path_input = g_target_pin->path;
        strncpy(s_label_input, g_target_pin->label.c_str(), lengthof(s_label_input));
    }
    {
        imgui::ScopedAvailWidth width(imgui::CalcTextSize(" 00/64").x);

        if (imgui::InputTextWithHint("##pin_label", "Label...", s_label_input, lengthof(s_label_input))) {
            s_err_msg.clear();
        }
    }
    imgui::SameLine();
    imgui::Text("%zu/%zu", strlen(s_label_input), pinned_path::LABEL_MAX_LEN);

    {
        [[maybe_unused]] imgui::ScopedAvailWidth width = {};

        if (imgui::InputTextWithHint("##pin_path", "Path...", s_path_input.data(), s_path_input.size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars()))
        {
            s_err_msg.clear();
        }
    }

    imgui::Spacing(1);

    auto apply_changes = [&]() noexcept {
        swan_path path = path_squish_adjacent_separators(s_path_input);
        path_force_separator(path, global_state::settings().dir_separator_utf8);

        g_target_pin->color = s_color_input;
        g_target_pin->label = s_label_input;
        g_target_pin->path = path;

        bool success = global_state::pinned_save_to_disk();
        print_debug_msg("pinned_save_to_disk: %d", success);
    };

    if (imgui::Button("Save##pin") && !strempty(s_path_input.data()) && !strempty(s_label_input)) {
        apply_changes();
        cleanup_and_close_popup();
    }

    imgui::SameLine();

    if (imgui::Button("Cancel##pin")) {
        cleanup_and_close_popup();
    }

    imgui::SameLineSpaced(1);

    imgui::ColorEdit4("Edit Color##pin", &s_color_input.x, ImGuiColorEditFlags_NoAlpha|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
    imgui::SameLine();
    imgui::TextColored(s_color_input, "Color");

    if (!s_err_msg.empty()) {
        imgui::TextColored(error_color(), "Error: %s", s_err_msg.c_str());
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }
    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
        apply_changes();
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
