#ifndef SWAN_PINNED_WINDOW_CPP
#define SWAN_PINNED_WINDOW_CPP

#include "common.hpp"
#include "util.hpp"

using namespace swan;

void render_pinned_window(
    std::array<explorer_window, 4> &explorers,
    windows_options const &win_opts,
    explorer_options const &expl_opts) noexcept(true)
{
    auto render_pin_item = [&](path_t const &pin, u64 pin_idx) {
        u64 num_buttons_rendered = 0;

        auto render_pin_button = [&](u64 expl_win_num, explorer_window &expl) {
            ImGui::BeginDisabled(path_loosely_same(expl.cwd, pin));

            char buffer[32];
            snprintf(buffer, lengthof(buffer), "%zu##%zu", expl_win_num, pin_idx);

            if (ImGui::Button(buffer)) {
                debug_log("setting Explorer %zu cwd to [%s]", expl_win_num, pin.data());
                expl.cwd = pin;
                new_history_from(expl, pin);
                update_cwd_entries(full_refresh, &expl, pin.data(), expl_opts);
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                ImGui::SetTooltip("Click here to set this pin as the cwd for Explorer %zu", expl_win_num);
            }

            ImGui::EndDisabled();
            ++num_buttons_rendered;
        };

        if (win_opts.show_explorer_0) {
            render_pin_button(1, explorers[0]);
        }
        if (win_opts.show_explorer_1) {
            ImGui::SameLine();
            render_pin_button(2, explorers[1]);
        }
        if (win_opts.show_explorer_2) {
            ImGui::SameLine();
            render_pin_button(3, explorers[2]);
        }
        if (win_opts.show_explorer_3) {
            ImGui::SameLine();
            render_pin_button(4, explorers[3]);
        }

        if (num_buttons_rendered > 0) {
            ImGui::SameLine();
        }
        ImGui::TextColored(basic_dir_ent::get_color(basic_dir_ent::kind::directory), pin.data());
    };

    if (ImGui::Begin("Pinned")) {
        std::vector<path_t> const &pins = get_pins();

        for (u64 i = 0; i < pins.size(); ++i) {
            render_pin_item(pins[i], i);
        }
    }

    ImGui::End();
}

#endif // SWAN_PINNED_WINDOW_CPP
