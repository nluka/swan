#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"

namespace edit_pin_modal_global_state
{
    static bool             g_open = false;
    static pinned_path *    g_target_pin = nullptr;
}

void swan_popup_modals::open_edit_pin(pinned_path *pin) noexcept
{
    using namespace edit_pin_modal_global_state;

    g_open = true;

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

    if (g_open) {
        imgui::OpenPopup(swan_popup_modals::edit_pin);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::edit_pin, nullptr)) {
        return;
    }

    assert(g_target_pin != nullptr);

    static char label_input[pinned_path::LABEL_MAX_LEN + 1] = {};
    static swan_path_t path_input = {};
    static ImVec4 color_input = dir_color();
    static std::string err_msg = {};

    auto cleanup_and_close_popup = [&]() noexcept {
        g_open = false;
        g_target_pin = nullptr;

        init_empty_cstr(label_input);
        init_empty_cstr(path_input.data());
        err_msg.clear();

        imgui::CloseCurrentPopup();
    };

    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        // set initial focus on label input
        imgui::SetKeyboardFocusHere(0);

        // init input fields
        color_input = g_target_pin->color;
        path_input = g_target_pin->path;
        strncpy(label_input, g_target_pin->label.c_str(), lengthof(label_input));
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
        [[maybe_unused]] imgui::ScopedAvailWidth width = {};

        if (imgui::InputTextWithHint("##pin_path", "Path...", path_input.data(), path_input.size(),
                                     ImGuiInputTextFlags_CallbackCharFilter, filter_chars_callback, (void *)windows_illegal_path_chars()))
        {
            err_msg.clear();
        }
    }

    imgui::ColorEdit4("Edit Color##pin", &color_input.x, ImGuiColorEditFlags_NoAlpha|ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel);
    imgui::SameLine();
    imgui::TextColored(color_input, "Color");

    imgui::SameLineSpaced(1);

    auto apply_changes = [&]() noexcept {
        swan_path_t path = path_squish_adjacent_separators(path_input);
        path_force_separator(path, global_state::settings().dir_separator_utf8);

        g_target_pin->color = color_input;
        g_target_pin->label = label_input;
        g_target_pin->path = path;

        bool success = global_state::save_pins_to_disk();
        print_debug_msg("save_pins_to_disk: %d", success);
    };

    if (imgui::Button("Save##pin") && !strempty(path_input.data()) && !strempty(label_input)) {
        apply_changes();
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
    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
        apply_changes();
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
