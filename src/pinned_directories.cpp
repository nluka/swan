#include <fstream>

#include "common.hpp"
#include "imgui_specific.hpp"
#include "util.hpp"

static std::vector<swan_path_t> s_pins = {};

void swan_render_window_pinned_directories(std::array<explorer_window, 4> &explorers, windows_options const &win_opts) noexcept
{
    auto render_pin_item = [&](swan_path_t const &pin, u64 pin_idx) {
        u64 num_buttons_rendered = 0;

        auto render_pin_button = [&](u64 expl_win_num, explorer_window &expl) {
            ImGui::BeginDisabled(path_loosely_same(expl.cwd, pin));

            char buffer[32];
            snprintf(buffer, lengthof(buffer), "%zu##%zu", expl_win_num, pin_idx);

            if (ImGui::Button(buffer)) {
                debug_log("setting Explorer %zu cwd to [%s]", expl_win_num, pin.data());
                expl.cwd = pin;
                new_history_from(expl, pin);
                (void) update_cwd_entries(full_refresh, &expl, pin.data());
                (void) expl.save_to_disk();
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
        ImGui::TextColored(get_color(basic_dirent::kind::directory), pin.data());
    };

    if (ImGui::Begin("Pinned")) {
        std::vector<swan_path_t> const &pins = get_pins();

        for (u64 i = 0; i < pins.size(); ++i) {
            render_pin_item(pins[i], i);
        }
    }

    ImGui::End();
}

std::vector<swan_path_t> const &get_pins() noexcept
{
    return s_pins;
}

bool pin(swan_path_t &path, char dir_separator) noexcept
{
    path_force_separator(path, dir_separator);

    try {
        s_pins.push_back(path);
        return true;
    } catch (...) {
        return false;
    }
}

void unpin(u64 pin_idx) noexcept
{
    [[maybe_unused]] u64 last_idx = s_pins.size() - 1;

    assert(pin_idx <= last_idx);

    s_pins.erase(s_pins.begin() + pin_idx);
}

void swap_pins(u64 pin1_idx, u64 pin2_idx) noexcept
{
    assert(pin1_idx != pin2_idx);

    if (pin1_idx > pin2_idx) {
        u64 temp = pin1_idx;
        pin1_idx = pin2_idx;
        pin2_idx = temp;
    }

    std::swap(*(s_pins.begin() + pin1_idx), *(s_pins.begin() + pin2_idx));
}

u64 find_pin_idx(swan_path_t const &path) noexcept
{
    for (u64 i = 0; i < s_pins.size(); ++i) {
        if (path_loosely_same(s_pins[i], path)) {
            return i;
        }
    }
    return std::string::npos;
}

bool save_pins_to_disk() noexcept
{
    try {
        std::ofstream out("data/pins.txt");
        if (!out) {
            return false;
        }

        auto const &pins = get_pins();
        for (auto const &pin : pins) {
            out << pin.data() << '\n';
        }

        return true;
    }
    catch (...) {
        return false;
    }
}

void update_pin_dir_separators(char new_dir_separator) noexcept
{
    for (auto &pin : s_pins) {
        path_force_separator(pin, new_dir_separator);
    }
}

std::pair<bool, u64> load_pins_from_disk(char dir_separator) noexcept
{
    try {
        std::ifstream in("data/pins.txt");
        if (!in) {
            return { false, 0 };
        }

        s_pins.clear();

        std::string temp = {};
        temp.reserve(MAX_PATH);

        u64 num_loaded_successfully = 0;

        while (std::getline(in, temp)) {
            swan_path_t temp2;

            if (temp.length() < temp2.size()) {
                strcpy(temp2.data(), temp.c_str());
                pin(temp2, dir_separator);
                ++num_loaded_successfully;
            }

            temp.clear();
        }

        return { true, num_loaded_successfully };
    }
    catch (...) {
        return { false, 0 };
    }
}
