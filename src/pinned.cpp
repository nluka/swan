#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"
#include "util.hpp"

static std::vector<pinned_path> g_pinned_paths = {};

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

void swan_windows::render_pinned(
    [[maybe_unused]] std::array<explorer_window,
    global_constants::num_explorers> &explorers,
    bool &open,
    [[maybe_unused]] bool any_popups_open) noexcept
{
    if (imgui::Begin(swan_windows::get_name(swan_windows::id::pinned), &open)) {
        if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            global_state::focused_window_set(swan_windows::id::pinned);
        }

        std::vector<pinned_path> &pins = global_state::pinned_get();

        static pinned_path *s_context_target = nullptr;
        u64 const npos = u64(-1);
        u64 pin_to_delete_idx = npos;

        ImGuiListClipper clipper;
        assert(pins.size() <= (u64)INT32_MAX);
        clipper.Begin(s32(pins.size()));

        while (clipper.Step())
        for (u64 i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            auto &pin = pins[i];

            imgui::TextDisabled(ICON_CI_PIN);
            imgui::SameLine();
            {
                imgui::ScopedTextColor tc(pin.color);
                auto label = make_str_static<pinned_path::LABEL_MAX_LEN + 32>("%s ## %zu", pin.label.c_str(), i);
                imgui::Selectable(label.data(), false);
            }
            if (imgui::IsItemClicked(ImGuiMouseButton_Right)) {
                s_context_target = &pin;
                imgui::OpenPopup("## pinned context_menu");
            }
            if (imgui::BeginDragDropSource()) {
                imgui::TextColored(pin.color, pin.label.c_str());
                pin_drag_drop_payload payload = { i };
                imgui::SetDragDropPayload(typeid(pin_drag_drop_payload).name(), &payload, sizeof(payload), ImGuiCond_Once);
                imgui::EndDragDropSource();
            }
            if (imgui::BeginDragDropTarget()) {
                ImRect drop_target_rect = {};
                auto payload_wrapper = imgui::AcceptDragDropPayload(typeid(pin_drag_drop_payload).name(),
                                                                    ImGuiDragDropFlags_AcceptNoDrawDefaultRect|ImGuiDragDropFlags_AcceptBeforeDelivery,
                                                                    &drop_target_rect);

                if (payload_wrapper != nullptr) {
                    auto payload_data = (pin_drag_drop_payload *)payload_wrapper->Data;
                    assert(payload_wrapper->DataSize == sizeof(pin_drag_drop_payload));

                    u64 from = payload_data->pin_idx;
                    u64 to = i;

                    assert(from != to);

                    bool reorder_moving_down = from > to;
                    ImVec2 p1 = reorder_moving_down ? drop_target_rect.GetTL() : drop_target_rect.GetBL();
                    ImVec2 p2 = reorder_moving_down ? drop_target_rect.GetTR() : drop_target_rect.GetBR();
                    imgui::GetWindowDrawList()->AddLine(p1, p2, imgui::GetColorU32(ImGuiCol_DragDropTarget), 2.f);

                    if (imgui::IsMouseReleased(ImGuiMouseButton_Left)) {
                        bool reorder_success = change_element_position(pins, from, to);
                        print_debug_msg("change_element_position(pins, from:%zu, to:%zu): %d", from, to, reorder_success);

                        if (reorder_success) {
                            bool save_success = global_state::pinned_save_to_disk();
                            print_debug_msg("global_state::pinned_save_to_disk: %d", save_success);
                        }
                    }
                }

                imgui::EndDragDropTarget();
            }
        }

        if (imgui::BeginPopup("## pinned context_menu")) {
            if (imgui::Selectable("Edit")) {
                swan_popup_modals::open_edit_pin(s_context_target);
            }
            if (imgui::Selectable("Delete")) {
                imgui::OpenConfirmationModal(swan_id_confirm_delete_pin, []() noexcept {
                    imgui::TextUnformatted("Are you sure you want to delete the following pin?");
                    imgui::TextColored(s_context_target->color, s_context_target->label.c_str());
                    imgui::TextUnformatted("This action cannot be undone.");
                });
            }
            imgui::TextDisabled("(drag to reorder)");

            imgui::EndPopup();
        }

        auto status = imgui::GetConfirmationStatus(swan_id_confirm_delete_pin);

        if (status.value_or(false)) {
            pin_to_delete_idx = std::distance(&*pins.begin(), s_context_target);
        }

        if (pin_to_delete_idx != npos) {
            pins.erase(pins.begin() + pin_to_delete_idx);
            bool success = global_state::pinned_save_to_disk();
            print_debug_msg("delete pins[%zu], global_state::pinned_save_to_disk: %d", pin_to_delete_idx, success);
        }
    }

    imgui::End();
}

std::vector<pinned_path> &global_state::pinned_get() noexcept
{
    return g_pinned_paths;
}

bool global_state::pinned_add(ImVec4 color, char const *label, swan_path &path, char dir_separator) noexcept
{
    path_force_separator(path, dir_separator);

    try {
        g_pinned_paths.emplace_back(color, label, path);
        return true;
    } catch (...) {
        return false;
    }
}

void global_state::pinned_remove(u64 pin_idx) noexcept
{
    [[maybe_unused]] u64 last_idx = g_pinned_paths.size() - 1;

    assert(pin_idx <= last_idx);

    g_pinned_paths.erase(g_pinned_paths.begin() + pin_idx);
}

void global_state::pinned_swap(u64 pin1_idx, u64 pin2_idx) noexcept
{
    assert(pin1_idx != pin2_idx);

    if (pin1_idx > pin2_idx) {
        u64 temp = pin1_idx;
        pin1_idx = pin2_idx;
        pin2_idx = temp;
    }

    std::swap(*(g_pinned_paths.begin() + pin1_idx), *(g_pinned_paths.begin() + pin2_idx));
}

u64 global_state::pinned_find_idx(swan_path const &path) noexcept
{
    for (u64 i = 0; i < g_pinned_paths.size(); ++i) {
        if (path_loosely_same(g_pinned_paths[i].path, path)) {
            return i;
        }
    }
    return std::string::npos;
}

bool global_state::pinned_save_to_disk() noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\pinned.txt";

    std::ofstream out(full_path);

    if (!out) {
        return false;
    }

    auto const &pins = global_state::pinned_get();
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

    print_debug_msg("SUCCESS global_state::pinned_save_to_disk");
    return true;
}
catch (...) {
    print_debug_msg("FAILED global_state::pinned_save_to_disk");
    return false;
}

void global_state::pinned_update_directory_separators(char new_dir_separator) noexcept
{
    for (auto &pin : g_pinned_paths) {
        path_force_separator(pin.path, new_dir_separator);
    }
}

std::pair<bool, u64> global_state::pinned_load_from_disk(char dir_separator) noexcept
try {
    std::filesystem::path full_path = global_state::execution_path() / "data\\pinned.txt";

    std::ifstream in(full_path);

    if (!in) {
        return { false, 0 };
    }

    g_pinned_paths.clear();

    std::string line = {};
    line.reserve(global_state::page_size() - 1);

    u64 num_loaded_successfully = 0;

    while (std::getline(in, line)) {
        std::istringstream out(line);

        ImVec4 color;
        char label[pinned_path::LABEL_MAX_LEN + 1] = {};
        u64 label_len = 0;
        swan_path path = {};
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

        global_state::pinned_add(color, label, path, dir_separator);
        ++num_loaded_successfully;

        line.clear();
    }

    print_debug_msg("SUCCESS global_state::pinned_load_from_disk, loaded %zu pins", num_loaded_successfully);
    return { true, num_loaded_successfully };
}
catch (...) {
    print_debug_msg("FAILED global_state::pinned_load_from_disk");
    return { false, 0 };
}
