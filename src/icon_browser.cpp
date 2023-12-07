#include "stdafx.hpp"
#include "common_fns.hpp"
#include "imgui_specific.hpp"

void swan_windows::render_icon_font_browser(
    s32 window_code,
    icon_font_browser_state &browser,
    bool &open,
    char const *icon_lib_name,
    char const *icon_prefix,
    std::vector<icon_font_glyph> const &(*get_all_icons)() noexcept) noexcept
{
    {
        char buffer[256]; init_empty_cstr(buffer);
        snprintf(buffer, lengthof(buffer), " %s Icons ", icon_lib_name);
        if (!imgui::Begin(buffer, &open)) {
            imgui::End();
            return;
        }
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::save_focused_window(window_code);
    }

    bool update_list = false;
    {
        imgui_scoped_item_width width(imgui::CalcTextSize("1234567890").x * 3);
        char buffer[256]; init_empty_cstr(buffer);
        snprintf(buffer, lengthof(buffer), "##%s_search", icon_lib_name);
        update_list = imgui::InputTextWithHint(buffer, "Search", browser.search_input, sizeof(browser.search_input));
    }

    if (update_list) {
        browser.matches.clear();

        if (strempty(browser.search_input)) {
            browser.matches = get_all_icons();
        }
        else {
            for (auto const &icon : get_all_icons()) {
                char name_buf[256];
                (void) strncpy(name_buf, icon.name, lengthof(name_buf));

                char *name = name_buf + strlen(icon_prefix) - 1;

                boost::container::static_vector<char const *, 16> words = {};
                {
                    char const *word = strtok(name, "_");
                    while (word != nullptr) {
                        words.push_back(word);
                        word = strtok(nullptr, "_");
                    }
                }

                bool any_words_matched = std::any_of(words.begin(), words.end(),
                    [&](char const *word) { return StrCmpIA(word, browser.search_input) == 0; });

                if (any_words_matched) {
                    browser.matches.push_back(icon);
                }
            }
        }
    }

    {
        imgui_scoped_item_width width(imgui::CalcTextSize("1234567890").x * 3);
        char buffer[256]; init_empty_cstr(buffer);
        snprintf(buffer, lengthof(buffer), "##%s_grid_width", icon_lib_name);
        imgui::SliderInt(buffer, &browser.grid_width, 1, 50);
    }

    imgui::Separator();

    char buffer1[256]; init_empty_cstr(buffer1);
    char buffer2[256]; init_empty_cstr(buffer2);

    snprintf(buffer1, lengthof(buffer1), "icons_matched_child_%s", icon_lib_name);
    snprintf(buffer2, lengthof(buffer2), "icons_matched_tbl_%s", icon_lib_name);

    if (imgui::BeginChild(buffer1)) {
        if (imgui::BeginTable(buffer2, browser.grid_width, ImGuiTableFlags_SizingFixedFit)) {
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
