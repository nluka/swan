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

    static bool s_show_codicons = true;
    static bool s_show_font_awesome = true;

    enum class library_id : s8
    {
        nil = -1,
        codicons,
        font_awesome_5,
        count
    };

    struct icon_library
    {
        char const *library_name = nullptr;
        char const *prefix = nullptr;
        std::vector<icon_font_glyph> glyphs;
        bool *show = nullptr;
    };
    static icon_library s_icon_libraries[(u64)library_id::count] = {
        { "Codicons", "ICON_CI_", global_constants::icon_font_glyphs_codicon(), &s_show_codicons },
        { "Font Awesome 5", "ICON_FA_", global_constants::icon_font_glyphs_font_awesome(), &s_show_font_awesome },
    };

    struct icon_match
    {
        u64 lev_edit_distance = 0;
        char const *library_name = nullptr;
        char const *icon_name = nullptr;
        char const *icon_content = nullptr;
        library_id lib_id = library_id::nil;
        bool has_word = false;
    };

    auto match_everything = []() noexcept {
        std::vector<icon_match> everything = {};

        for (u64 i = 0; i < lengthof(s_icon_libraries); ++i) {
            auto &icon_lib = s_icon_libraries[i];

            if (*icon_lib.show) {
                for (auto &icon_glyph : icon_lib.glyphs) {
                    everything.emplace_back(0, icon_lib.library_name, icon_glyph.name, icon_glyph.content, library_id(i), false);
                }
            }
        }

        return everything;
    };

    static std::vector<icon_match> s_matches = match_everything();

    static char s_search_input[64] = {};
    bool recompute_matches = false;
    {
        imgui::ScopedAvailWidth w = {};
        recompute_matches |= imgui::InputTextWithHint("## icon_font_finder search", "Search", s_search_input, lengthof(s_search_input));
    }

    static bool s_prioritize_sub_word_matching = true;

    imgui::Checkbox("Codicons", &s_show_codicons);
    imgui::SameLineSpaced(1);
    imgui::Checkbox("Font Awesome 5", &s_show_font_awesome);

    imgui::SameLineSpaced(3);

    recompute_matches |= imgui::Checkbox("Prioritize Sub Words", &s_prioritize_sub_word_matching);
    imgui::SameLine();
    imgui::TextUnformatted("(?)");
    if (imgui::IsItemHovered()) {
        imgui::SetTooltip("When enabled, icon names with exact word matches will appear first,\n"
                          "icon names that don't have a matching word will appear as the second partition.\n"
                          "Levenshtein edit distance is used to sort the two partitions.\n"
                          "\n"
                          "When disabled, only Levenshtein edit distance is used to sort matches.");
    }

    if (!strempty(s_search_input)) {
        imgui::SameLineSpaced(3);
        imgui::TextDisabled("Icons appear in order of closest to furthest match");
    }

    if (recompute_matches) {
        s_matches.clear();

        u64 search_input_len = strnlen(s_search_input, lengthof(s_search_input));

        char search_input_upper[64] = {};

        std::transform(s_search_input, s_search_input + search_input_len,
                       search_input_upper, [](char ch) noexcept { return (char)toupper(ch); });

        if (search_input_len == 0) {
            for (u64 i = 0; i < lengthof(s_icon_libraries); ++i) {
                auto &icon_lib = s_icon_libraries[i];

                for (auto &icon_glyph : icon_lib.glyphs) {
                    s_matches.emplace_back(0, icon_lib.library_name, icon_glyph.name, icon_glyph.content, library_id(i), false);
                }
            }
        }
        else { // search_input_len > 0
            for (u64 i = 0; i < lengthof(s_icon_libraries); ++i) {
                auto &icon_lib = s_icon_libraries[i];

                if (*icon_lib.show == false) {
                    continue;
                }

                u64 prefix_len = strlen(icon_lib.prefix);

                for (auto &icon_glyph : icon_lib.glyphs) {
                    char const *icon_name_no_prefix     = icon_glyph.name + prefix_len;
                    u64         icon_name_no_prefix_len = strlen(icon_name_no_prefix);

                    bool sub_word_found = false;

                    if (s_prioritize_sub_word_matching) {
                        auto  icon_name_copy = make_str_static<256>(icon_glyph.name);
                        char *icon_name_copy_no_prefix = icon_name_copy.data() + strlen(icon_lib.prefix) - 1;

                        boost::container::static_vector<char const *, 16> words = {};
                        {
                            char const *word = strtok(icon_name_copy_no_prefix, "_");
                            while (word != nullptr) {
                                words.push_back(word);
                                word = strtok(nullptr, "_");
                            }
                        }

                        sub_word_found = std::any_of(words.begin(), words.end(), [&](char const *word) noexcept {
                            return StrCmpIA(word, s_search_input) == 0;
                        });
                    }

                    u64 lev_edit_distance = compute_levenshtein_dist(icon_name_no_prefix, icon_name_no_prefix_len, search_input_upper, search_input_len);

                    s_matches.emplace_back(lev_edit_distance, icon_lib.library_name, icon_glyph.name, icon_glyph.content, library_id(i), sub_word_found);
                }
            }

            std::sort(s_matches.begin(), s_matches.end(), [](icon_match const &left, icon_match const &right) noexcept {
                if (left.lib_id != right.lib_id) {
                    return (u8)left.lib_id < (u8)right.lib_id;
                } else if (s_prioritize_sub_word_matching && left.has_word != right.has_word) {
                    return left.has_word > right.has_word;
                }
                return left.lev_edit_distance < right.lev_edit_distance;
            });
        }
    }

    if (imgui::BeginChild("icon_library_matches", {}, false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        library_id curr_lib_id = library_id::nil;

        for (auto const &match : s_matches) {
            if (*s_icon_libraries[(u64)match.lib_id].show == false) {
                continue;
            }
            if (match.lib_id != curr_lib_id) {
                imgui::NewLine();
                imgui::SeparatorText(match.library_name);
                curr_lib_id = match.lib_id;
            }

            f32 space_left = imgui::GetContentRegionAvail().x - imgui::GetStyle().ScrollbarSize - imgui::GetStyle().ItemSpacing.x - imgui::GetStyle().WindowPadding.x;
            f32 button_size = imgui::CalcTextSize(match.icon_content).x + imgui::GetStyle().FramePadding.x*2 + imgui::GetStyle().ItemSpacing.x;

            imgui::Button(match.icon_content);
            if (button_size <= space_left) {
                imgui::SameLine();
            }

            if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
                if (imgui::BeginTooltip()) {
                    imgui::Separator();
                    imgui::TextUnformatted(match.icon_name);
                    imgui::SameLineSpaced(1);
                    imgui::TextDisabled("click to copy");

                    if (s_prioritize_sub_word_matching) {
                        imgui::Separator();
                        imgui::TextColored(match.has_word ? success_color() : error_color(), "Word match = %c", match.has_word ? 'Y' : 'N');
                    }

                    imgui::Separator();
                    imgui::Text("Lev edit dist = %zu", match.lev_edit_distance);

                    imgui::Separator();
                    imgui::EndTooltip();
                }
            }

            if (imgui::IsItemClicked()) {
                imgui::SetClipboardText(match.icon_name);
            }
        }
    }
    imgui::EndChild();

    imgui::End();
}
