#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

void swan_windows::render_icon_font_browser(
    swan_windows::id window_id,
    icon_font_browser_state &browser,
    bool &open,
    char const *icon_lib_name,
    char const *icon_prefix,
    std::vector<icon_font_glyph> const &(*get_all_icons)() noexcept) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(window_id), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::focused_window_set(window_id);
    }

    bool update_list = false;
    {
        imgui::ScopedItemWidth width(imgui::CalcTextSize("1234567890").x * 3);
        auto label = make_str_static<256>(" Search##%s", icon_lib_name);
        update_list = imgui::InputText(label.data(), browser.search_input, sizeof(browser.search_input));
    }

    if (update_list) {
        browser.matches.clear();

        if (strempty(browser.search_input)) {
            browser.matches = get_all_icons();
        }
        else {
            for (auto const &icon : get_all_icons()) {
                char icon_name_copy[256];
                (void) strncpy(icon_name_copy, icon.name, lengthof(icon_name_copy));

                char *icon_name_no_prefix = icon_name_copy + strlen(icon_prefix) - 1;

                boost::container::static_vector<char const *, 16> words = {};
                {
                    char const *word = strtok(icon_name_no_prefix, "_");
                    while (word != nullptr) {
                        words.push_back(word);
                        word = strtok(nullptr, "_");
                    }
                }

                bool any_words_matched = std::any_of(words.begin(), words.end(),
                    [&](char const *word) noexcept { return StrCmpIA(word, browser.search_input) == 0; });

                if (any_words_matched) {
                    browser.matches.push_back(icon);
                }
            }
        }
    }

    {
        imgui::ScopedItemWidth width(imgui::CalcTextSize("1234567890").x * 3);
        auto label = make_str_static<256>(" Grid Width##%s", icon_lib_name);
        imgui::SliderInt(label.data(), &browser.grid_width, 1, 50);
    }

    imgui::Separator();

    auto label_child = make_str_static<256>("icons_matched_child_%s", icon_lib_name);
    auto label_table = make_str_static<256>("icons_matched_tbl_%s", icon_lib_name);

    if (imgui::BeginChild(label_child.data())) {
        if (imgui::BeginTable(label_table.data(), browser.grid_width, ImGuiTableFlags_SizingFixedFit)) {
            for (auto const &icon : browser.matches) {
                imgui::TableNextColumn();

                imgui::Button(icon.content);

                if (imgui::IsItemClicked()) {
                    imgui::SetClipboardText(icon.name);
                }
                if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                    imgui::SetTooltip("%s", icon.name);
                }
            }
            imgui::EndTable();
        }
    }
    imgui::EndChild();

    imgui::End();
}
