#include "stdafx.hpp"
#include "common.hpp"
#include "imgui_specific.hpp"
#include "util.hpp"

static std::vector<pinned_path> s_pins = {};

template <typename Ty>
bool change_element_position(std::vector<Ty> &vec, u64 elem_idx, u64 new_elem_idx) noexcept
{
    if (elem_idx >= vec.size() || new_elem_idx >= vec.size()) {
        return false;
    }
    else if (new_elem_idx == elem_idx) {
        return true;
    }
    else {
        auto elem_copy = vec[elem_idx];
        vec.erase(vec.begin() + elem_idx);
        vec.insert(vec.begin() + new_elem_idx, elem_copy);
        return true;
    }
}

struct reorder_pin_payload
{
    u64 src_index;
};

void swan_render_window_pinned_directories([[maybe_unused]] std::array<explorer_window, 4> &explorers, bool &open) noexcept
{
    namespace imgui = ImGui;

    if (imgui::Begin(" Pinned ", &open)) {
        std::vector<pinned_path> &pins = get_pins();

        u64 const npos = u64(-1);
        u64 pin_to_delete_idx = npos;

        for (u64 i = 0; i < pins.size(); ++i) {
            auto &pin = pins[i];

            {
                char buffer[32];
                snprintf(buffer, lengthof(buffer), "Color##%zu", i);
                if (imgui::ColorEdit4(buffer, &pin.color.x, ImGuiColorEditFlags_NoInputs|ImGuiColorEditFlags_NoLabel/*|ImGuiColorEditFlags_NoBorder*/)) {
                    bool save_success = save_pins_to_disk();
                    debug_log("save_pins_to_disk: %d", save_success);
                }
            }

            imgui::SameLine();

            {
                char buffer[32];
                snprintf(buffer, lengthof(buffer), ICON_CI_EDIT "##pin%zu", i);
                if (imgui::Button(buffer)) {
                    swan_open_popup_modal_edit_pin(&pin);
                }
            }

            imgui::SameLine();

            {
                char buffer[32];
                snprintf(buffer, lengthof(buffer), ICON_CI_TRASH "##pin%zu", i);
                if (imgui::Button(buffer)) {
                    pin_to_delete_idx = i;
                }
            }

            imgui_sameline_spacing(0);

            imgui::Text("%zu.", i+1);

            imgui_sameline_spacing(0);

            {
                char buffer[pinned_path::LABEL_MAX_LEN + 16];
                snprintf(buffer, lengthof(buffer), "%s##%zu", pin.label.c_str(), i);

                imgui_scoped_text_color tc(pin.color);
                imgui::Selectable(buffer, false/*, ImGuiSelectableFlags_SpanAllColumns*/);
            }
            if (imgui::BeginDragDropSource()) {
                imgui::Text("%zu.", i+1);
                imgui_sameline_spacing(1);
                imgui::TextColored(pin.color, pin.label.c_str());

                reorder_pin_payload payload = { i };
                imgui::SetDragDropPayload("REORDER_PINS", &payload, sizeof(payload), ImGuiCond_Once);
                imgui::EndDragDropSource();
            }
            if (imgui::BeginDragDropTarget()) {
                auto imgui_payload = imgui::AcceptDragDropPayload("REORDER_PINS");

                if (imgui_payload != nullptr) {
                    assert(imgui_payload->DataSize == sizeof(reorder_pin_payload));
                    auto actual_payload = (reorder_pin_payload *)imgui_payload->Data;

                    u64 from = actual_payload->src_index;
                    u64 to = i;

                    bool reorder_success = change_element_position(pins, from, to);
                    debug_log("change_element_position(pins, from:%zu, to:%zu): %d", from, to, reorder_success);

                    if (reorder_success) {
                        bool save_success = save_pins_to_disk();
                        debug_log("save_pins_to_disk: %d", save_success);
                    }
                }

                imgui::EndDragDropTarget();
            }
        }

        imgui_spacing(1);

        if (imgui::Button(ICON_CI_REPO_CREATE " Create Pin")) {
            swan_open_popup_modal_new_pin({}, true);
        }

        // if (imgui::Button("Reload Pins")) {
        //     (void) load_pins_from_disk(get_explorer_options().dir_separator_utf8());
        // }

        if (pin_to_delete_idx != npos) {
            pins.erase(pins.begin() + pin_to_delete_idx);
            bool success = save_pins_to_disk();
            debug_log("delete pins[%zu], save_pins_to_disk: %d", pin_to_delete_idx, success);
        }
    }

    imgui::End();
}

std::vector<pinned_path> &get_pins() noexcept
{
    return s_pins;
}

bool pin(ImVec4 color, char const *label, swan_path_t &path, char dir_separator) noexcept
{
    path_force_separator(path, dir_separator);

    try {
        s_pins.emplace_back(color, label, path);
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
        if (path_loosely_same(s_pins[i].path, path)) {
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
            out
                << pin.color.x << ' '
                << pin.color.y << ' '
                << pin.color.z << ' '
                << pin.color.w << ' '
                << pin.label.length() << ' '
                << pin.label.c_str() << ' '
                << path_length(pin.path) << ' '
                << pin.path.data() << '\n';
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
        path_force_separator(pin.path, new_dir_separator);
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

        std::string line = {};
        line.reserve(get_page_size() - 1);

        u64 num_loaded_successfully = 0;

        while (std::getline(in, line)) {
            try {
                std::istringstream out(line);

                ImVec4 color;
                char label[pinned_path::LABEL_MAX_LEN + 1] = {};
                u64 label_len = 0;
                swan_path_t path = {};
                u64 path_len = 0;

                out >> (f32 &)color.x;
                out >> (f32 &)color.y;
                out >> (f32 &)color.z;
                out >> (f32 &)color.w;

                out >> (u64 &)label_len;
                out.ignore(1);
                out.read(label, label_len);

                out >> (u64 &)path_len;
                out.ignore(1);
                out.read(path.data(), path_len);

                pin(color, label, path, dir_separator);
                ++num_loaded_successfully;

                line.clear();
            }
            catch (...) {}
        }

        return { true, num_loaded_successfully };
    }
    catch (...) {
        return { false, 0 };
    }
}
