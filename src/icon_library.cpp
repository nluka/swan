#include "stdafx.hpp"
#include "common_functions.hpp"
#include "imgui_dependent_functions.hpp"

u64 compute_levenshtein_dist(char const *word1, u64 size1, char const *word2, u64 size2) {
    // int size1 = word1.size();
    // int size2 = word2.size();

    // int verif[size1 + 1][size2 + 1]; // Verification matrix i.e. 2D array which will store the calculated distance.
    static static_vector< static_vector<u64, 64>, 64 > s_verif;
    s_verif.clear();
    s_verif.resize(size1 + 1);
    for (auto &row : s_verif)
        row.resize(size2 + 1);

    // If one of the words has zero length, the distance is equal to the size of the other word.
    if (size1 == 0)
        return size2;
    if (size2 == 0)
        return size1;

    // Sets the first row and the first column of the verification matrix with the numerical order from 0 to the length of each word.
    for (int i = 0; i <= size1; i++)
        s_verif[i][0] = i;
    for (int j = 0; j <= size2; j++)
        s_verif[0][j] = j;

    // Verification step / matrix filling.
    for (int i = 1; i <= size1; i++) {
        for (int j = 1; j <= size2; j++) {
            // Sets the modification cost.
            // 0 means no modification (i.e. equal letters) and 1 means that a modification is needed (i.e. unequal letters).
            int cost = (word2[j - 1] == word1[i - 1]) ? 0 : 1;

            // Sets the current position of the matrix as the minimum value between a (deletion), b (insertion) and c (substitution).
            // a = the upper adjacent value plus 1: verif[i - 1][j] + 1
            // b = the left adjacent value plus 1: verif[i][j - 1] + 1
            // c = the upper left adjacent value plus the modification cost: verif[i - 1][j - 1] + cost
            s_verif[i][j] = std::min(
                std::min(s_verif[i - 1][j] + 1, s_verif[i][j - 1] + 1),
                s_verif[i - 1][j - 1] + cost
            );
        }
    }

    // The last position of the matrix will contain the Levenshtein distance.
    return s_verif[size1][size2];
}

void swan_windows::render_icon_library(bool &open) noexcept
{
    if (!imgui::Begin(swan_windows::get_name(swan_windows::id::icon_library), &open)) {
        imgui::End();
        return;
    }

    if (imgui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
        global_state::focused_window_set(swan_windows::id::icon_library);
    }

    struct icon_library
    {
        char const *library_name;
        char const *prefix;
        std::vector<icon_font_glyph> glyphs;
    };
    static icon_library s_icon_libraries[] = {
        { "Codicons", "ICON_CI_", global_constants::icon_font_glyphs_codicon() },
        { "Font Awesome", "ICON_FA_", global_constants::icon_font_glyphs_font_awesome() },
        { "Material Design", "ICON_MD_", global_constants::icon_font_glyphs_material_design() },
    };

    struct icon_match
    {
        u64 lev_edit_distance;
        char const *library_name;
        char const *icon_name;
        char const *icon_content;
    };

    auto match_everything = []() noexcept {
        std::vector<icon_match> everything = {};

        for (auto &icon_lib : s_icon_libraries) {
            for (auto &icon_glyph : icon_lib.glyphs) {
                everything.emplace_back(0, icon_lib.library_name, icon_glyph.name, icon_glyph.content);
            }
        }

        return everything;
    };

    static std::vector<icon_match> s_matches = match_everything();

    static char s_search_input[64] = {};
    bool search_update = false;
    {
        imgui::ScopedAvailWidth w = {};
        search_update = imgui::InputTextWithHint("## icon_font_finder search", "Search", s_search_input, lengthof(s_search_input));
    }

    if (search_update) {
        s_matches.clear();

        u64 search_input_len = strnlen(s_search_input, lengthof(s_search_input));

        char search_input_upper[64] = {};

        std::transform(s_search_input, s_search_input + search_input_len,
                       search_input_upper, [](char ch) noexcept { return (char)toupper(ch); });

        if (search_input_len == 0) {
            for (auto &icon_lib : s_icon_libraries) {
                for (auto &icon_glyph : icon_lib.glyphs) {
                    s_matches.emplace_back(0, icon_lib.library_name, icon_glyph.name, icon_glyph.content);
                }
            }
        }
        else { // search_input_len > 0
            for (auto &icon_lib : s_icon_libraries) {
                u64 prefix_len = strlen(icon_lib.prefix);

                for (auto &icon_glyph : icon_lib.glyphs) {
                    char const *icon_name_no_prefix     = icon_glyph.name + prefix_len;
                    u64         icon_name_no_prefix_len = strlen(icon_name_no_prefix);

                    u64 lev_edit_distance = compute_levenshtein_dist(icon_name_no_prefix, icon_name_no_prefix_len, search_input_upper, search_input_len);

                    s_matches.emplace_back(lev_edit_distance, icon_lib.library_name, icon_glyph.name, icon_glyph.content);
                }
            }

            std::sort(s_matches.begin(), s_matches.end(), [](icon_match const &left, icon_match const &right) noexcept {
                return left.lev_edit_distance < right.lev_edit_distance;
            });
        }
    }

    if (imgui::BeginChild("icon_library_matches")) {
        for (auto const &match : s_matches) {
            f32 space_left = imgui::GetContentRegionAvail().x - imgui::GetStyle().ScrollbarSize - imgui::GetStyle().ItemSpacing.x - imgui::GetStyle().WindowPadding.x;
            f32 button_size = imgui::CalcTextSize(match.icon_content).x + imgui::GetStyle().FramePadding.x*2 + imgui::GetStyle().ItemSpacing.x;

            imgui::Button(match.icon_content);
            if (button_size <= space_left) {
                imgui::SameLine();
            }

            if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                imgui::SetTooltip("%s %zu", match.icon_name, match.lev_edit_distance);
            }

            if (imgui::IsItemClicked()) {
                imgui::SetClipboardText(match.icon_name);
            }
        }
    }
    imgui::EndChild();

    imgui::End();
}
