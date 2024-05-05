#include "stdafx.hpp"
#include "imgui_extension.hpp"

namespace imgui_confirmation_global_state
{
    static s32                                                g_active_id = -1;
    static std::variant<std::string, std::function<void ()>>  g_content = "";
    static std::optional<bool>                                g_response = std::nullopt;
    static std::function<void ()>                             g_on_yes_callback = nullptr;
    static bool *                                             g_confirmation_enabled = nullptr;
}

bool imgui::HaveActiveConfirmationModal() noexcept
{
    using namespace imgui_confirmation_global_state;

    return g_active_id != -1;
}

bool imgui::OpenConfirmationModal(s32 confirmation_id, char const *content_message, bool *confirmation_enabled) noexcept
{
    using namespace imgui_confirmation_global_state;

    assert(g_active_id == -1 && "Don't call " __FUNCTIONW__ " when there is already an active modal");
    assert(g_active_id != confirmation_id && "Don't call " __FUNCTIONW__ " repeatedly for the same confirmation_id");

    assert(confirmation_id != -1 && "Don't pass `confirmation_id` == -1 to " __FUNCTIONW__);
    assert(content_message != nullptr && "Don't pass `content_message` == nullptr to " __FUNCTIONW__);

    if (confirmation_enabled != nullptr && *confirmation_enabled == false) {
        // don't setup the modal for rendering and tell caller to execute operation immediately
        return true;
    }
    else {
        g_active_id = confirmation_id;
        g_content = content_message;
        g_response = std::nullopt;
        g_confirmation_enabled = confirmation_enabled;
        return false;
    }
}

bool imgui::OpenConfirmationModal(s32 confirmation_id, std::function<void ()> content_render_fn, bool *confirmation_enabled) noexcept
{
    using namespace imgui_confirmation_global_state;

    assert(g_active_id == -1 && "Don't call " __FUNCTIONW__ " when there is already an active modal");
    assert(g_active_id != confirmation_id && "Don't call " __FUNCTIONW__ " repeatedly for the same confirmation_id");

    assert(confirmation_id != -1 && "Don't pass `confirmation_id` == -1 to " __FUNCTIONW__);
    assert(content_render_fn != nullptr && "Don't pass `content_render_fn` == nullptr to " __FUNCTIONW__);

    if (confirmation_enabled != nullptr && *confirmation_enabled == false) {
        // don't setup the modal for rendering and tell caller to execute operation immediately
        return true;
    }
    else {
        g_active_id = confirmation_id;
        g_content = content_render_fn;
        g_response = std::nullopt;
        g_confirmation_enabled = confirmation_enabled;
        return false;
    }
}

void imgui::OpenConfirmationModalWithCallback(s32 confirmation_id, char const *content_message, std::function<void ()> on_yes_callback, bool *confirmation_enabled) noexcept
{
    using namespace imgui_confirmation_global_state;

    assert(g_active_id == -1 && "Don't call " __FUNCTIONW__ " when there is already an active modal");
    assert(g_active_id != confirmation_id && "Don't call " __FUNCTIONW__ " repeatedly for the same confirmation_id");

    assert(confirmation_id != -1 && "Don't pass `confirmation_id` == -1 to " __FUNCTIONW__);
    assert(content_message != nullptr && "Don't pass `content_message` == nullptr to " __FUNCTIONW__);
    assert(on_yes_callback != nullptr && "Don't pass `on_yes_callback` == nullptr to " __FUNCTIONW__);

    if (confirmation_enabled != nullptr && *confirmation_enabled == false) {
        // don't setup the modal for rendering and execute operation immediately
        on_yes_callback();
    } else {
        g_active_id = confirmation_id;
        g_content = content_message;
        g_response = std::nullopt;
        g_on_yes_callback = on_yes_callback;
        g_confirmation_enabled = confirmation_enabled;
    }
}

void imgui::OpenConfirmationModalWithCallback(
    s32 confirmation_id,
    std::function<void ()> content_render_fn,
    std::function<void ()> on_yes_callback,
    bool *confirmation_enabled) noexcept
{
    using namespace imgui_confirmation_global_state;

    assert(g_active_id == -1 && "Don't call " __FUNCTIONW__ " when there is already an active modal");
    assert(g_active_id != confirmation_id && "Don't call " __FUNCTIONW__ " repeatedly for the same confirmation_id");

    assert(confirmation_id != -1 && "Don't pass `confirmation_id` == -1 to " __FUNCTIONW__);
    assert(content_render_fn != nullptr && "Don't pass `content_render_fn` == nullptr to " __FUNCTIONW__);
    assert(on_yes_callback != nullptr && "Don't pass `on_yes_callback` == nullptr to " __FUNCTIONW__);

    if (confirmation_enabled != nullptr && *confirmation_enabled == false) {
        // don't setup the modal for rendering and execute operation immediately
        on_yes_callback();
    } else {
        g_active_id = confirmation_id;
        g_content = content_render_fn;
        g_response = std::nullopt;
        g_on_yes_callback = on_yes_callback;
        g_confirmation_enabled = confirmation_enabled;
    }
}

std::optional<bool> imgui::GetConfirmationStatus(s32 confirmation_id) noexcept
{
    using namespace imgui_confirmation_global_state;

    if (confirmation_id != g_active_id) {
        return std::nullopt;
    } else {
        auto retval = g_response;
        if (retval.has_value()) {
            g_active_id = -1;
            g_content = "";
            g_response = std::nullopt;
            g_confirmation_enabled = nullptr;
        }
        return retval;
    }
}

void imgui::RenderConfirmationModal() noexcept
{
    using namespace imgui_confirmation_global_state;

    if (!HaveActiveConfirmationModal()) {
        return;
    }

    imgui::OpenPopup("Confirm ## RenderConfirmationModal");

    imgui::SetNextWindowSize({ std::min(500.f, imgui::GetMainViewport()->Size.x), -1 });

    if (imgui::BeginPopupModal("Confirm ## RenderConfirmationModal", 0, ImGuiWindowFlags_NoResize)) {
        auto cleanup_and_close_popup = []() noexcept {
            if (g_on_yes_callback != nullptr) {
                g_on_yes_callback = nullptr;
                g_active_id = -1;
                g_content = "";
                g_response = std::nullopt;
                g_confirmation_enabled = nullptr;
            } else {
                //? defer cleanup to GetConfirmationStatus
                // g_active_id = -1;
                // g_content = "";
                // g_response = std::nullopt;
            }
        };

        if (std::holds_alternative<std::string>(g_content)) {
            imgui::TextWrapped(std::get<std::string>(g_content).c_str());
        } else {
            auto user_defined_render_fn = std::get<std::function<void ()>>(g_content);
            user_defined_render_fn();
        }

        imgui::Spacing();

        if (imgui::Button("Yes")) {
            g_response = true;
            if (g_on_yes_callback != nullptr) {
                g_on_yes_callback();
            }
            cleanup_and_close_popup();
        }

        imgui::SameLine();

        if (imgui::Button("No")) {
            g_response = false;
            cleanup_and_close_popup();
        }

        imgui::SameLine();

        imgui::TextUnformatted("(?)");
        if (imgui::IsItemHovered()) {
            imgui::SetTooltip("[Enter]   Yes\n"
                              "[Escape]  No");
        }

        imgui::SameLineSpaced(1);

        if (g_confirmation_enabled != nullptr) {
            imgui::Checkbox("Ask for this operation", g_confirmation_enabled);
        }

        if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
            g_response = false;
            cleanup_and_close_popup();
        }
        if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
            g_response = true;
            cleanup_and_close_popup();
        }

        imgui::EndPopup();
    }
}

void imgui::Spacing(u64 n) noexcept
{
    for (u64 i = 0; i < n; ++i) {
        ImGui::Spacing();
    }
}

void imgui::SameLineSpaced(u64 num_spacing_calls) noexcept
{
    ImGui::SameLine();
    for (u64 i = 0; i < num_spacing_calls; ++i) {
        ImGui::Spacing();
        ImGui::SameLine();
    }
}

void imgui::SetInitialFocusOnNextWidget() noexcept
{
    if (imgui::IsWindowAppearing() && !imgui::IsAnyItemActive() && !imgui::IsMouseClicked(0)) {
        imgui::SetKeyboardFocusHere(0);
    }
}

bool imgui::IsColumnTextVisuallyTruncated(s32 table_column_index, char const *column_text, f32 column_text_offset_x) noexcept
{
    f32 text_width = imgui::CalcTextSize(column_text).x;
    f32 column_width = imgui::GetColumnWidth(table_column_index);
    return text_width > (column_width - column_text_offset_x + imgui::GetStyle().CellPadding.x);
}

bool imgui::RenderTooltipWhenColumnTextTruncated(s32 table_column_index,
                                                 char const *possibly_truncated_text,
                                                 f32 possibly_truncated_text_offset_x,
                                                 char const *tooltip_content) noexcept
{
    // if hovering the path with at least 1 character's worth of content truncated, display full path as tooltip
    if (imgui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled, 0.7f) && imgui::TableGetHoveredColumn() == table_column_index) {
        f32 text_width = imgui::CalcTextSize(possibly_truncated_text).x;
        f32 column_width = imgui::GetColumnWidth(table_column_index);

        if (text_width > column_width - possibly_truncated_text_offset_x + imgui::GetStyle().CellPadding.x) {
            tooltip_content = tooltip_content ? tooltip_content : possibly_truncated_text;
            imgui::SetTooltip(tooltip_content);
            return true;
        }
    }

    return false;
}

void imgui::TableDrawCellBorderTop(ImVec2 cell_rect_min, f32 cell_width) noexcept
{
    ImGuiWindow *window = ImGui::GetCurrentWindow();
    if (window == NULL)
        return;

    ImDrawList *drawList = window->DrawList;
    if (drawList == NULL)
        return;

    ImVec2 const &min = cell_rect_min;
    ImVec2 max = min;
    max.x += cell_width;

    float thickness = 1.0f; // Border thickness
    ImU32 color = ImGui::GetColorU32(ImGuiCol_Border); // Border color

    drawList->AddLine(ImVec2(min.x, min.y), ImVec2(max.x, min.y), color, thickness);
}

void imgui::HighlightTextRegion(ImVec2 const &text_rect_min, char const *text, u64 highlight_start_idx, u64 highlight_len, ImVec4 color) noexcept
{
    ImVec2 min = text_rect_min;
    ImVec2 max = min + imgui::CalcTextSize(text);

    // at this point min & max draw a rectangle around the entire path

    // move forward min.x to skip characters before highlight begins
    min.x += imgui::CalcTextSize(text, text + highlight_start_idx).x;

    // move backward max.x to eliminate characters after the highlight ends
    max.x -= imgui::CalcTextSize(text + highlight_start_idx + highlight_len).x;

    auto border_color = color;
    border_color.w += 50;

    ImGui::GetWindowDrawList()->AddRectFilled(min, max, imgui::ImVec4_to_ImU32(color));
    ImGui::GetWindowDrawList()->AddRect(min, max, imgui::ImVec4_to_ImU32(border_color));
}

f32 imgui::CalcLineLength(ImVec2 const &p1, ImVec2 const &p2) noexcept
{
    f32 dx = p2.x - p1.x;
    f32 dy = p2.y - p1.y;
    return sqrt(dx * dx + dy * dy);
}

bool imgui::DrawBestLineBetweenContextMenuAndTarget(ImRect const &target_rect, ImVec2 const &menu_TL, ImU32 const &color,
                                                    f32 circle_radius_target_corner, f32 circle_radius_menu_TL) noexcept
{
    imgui::GetWindowDrawList()->AddRect(target_rect.GetTL(), target_rect.GetBR(), color);

    bool menu_TL_is_inside_target_rect = target_rect.Contains(menu_TL);

    if (!menu_TL_is_inside_target_rect) {
        ImVec2 target_TL = target_rect.GetTL();
        ImVec2 target_BR = target_rect.GetBR();
        ImVec2 target_TR = target_rect.GetTR();
        ImVec2 target_BL = target_rect.GetBL();

        struct line {
            ImVec2 m_name_corner_point;
            ImVec2 m_context_TL_point;
            f32 m_length;
            bool m_obstructed;

            line(ImVec2 const &name_corner_point, ImVec2 const &context_TL_point) noexcept
                : m_name_corner_point(name_corner_point)
                , m_context_TL_point(context_TL_point)
                , m_length(imgui::CalcLineLength(name_corner_point, context_TL_point))
                , m_obstructed(context_TL_point.x <= name_corner_point.x && context_TL_point.y <= name_corner_point.y)
            {}

            bool operator<(line const &other) noexcept { return m_length < other.m_length; }
        };

        std::array<line, 4> line_candidates = {{
            line(target_TL, menu_TL),
            line(target_BR, menu_TL),
            line(target_TR, menu_TL),
            line(target_BL, menu_TL),
        }};

        std::sort(line_candidates.begin(), line_candidates.end(), std::less());

        auto shortest_unobstructed_line_iter = std::find_if(line_candidates.begin(), line_candidates.end(),
                                                            [](line const &ln) { return !ln.m_obstructed; });

        line const &best_line = shortest_unobstructed_line_iter == line_candidates.end()
            ? *line_candidates.begin() // all lines obstructed, use the shortest one
            : *shortest_unobstructed_line_iter;

        imgui::GetWindowDrawList()->AddLine(best_line.m_name_corner_point, best_line.m_context_TL_point, color);

        if (circle_radius_target_corner > 0.f) {
            imgui::GetWindowDrawList()->AddCircleFilled(best_line.m_name_corner_point, circle_radius_target_corner, color);
        }
        if (circle_radius_menu_TL > 0.f) {
            imgui::GetWindowDrawList()->AddCircleFilled(best_line.m_context_TL_point, circle_radius_menu_TL, color);
        }
    }

    return !menu_TL_is_inside_target_rect;
}

bool imgui::DrawBestLineBetweenRectCorners(ImRect const &rect1, ImRect const &rect2, ImVec4 const &color,
                                           bool draw_border_for_rect1, bool draw_border_for_rect2,
                                           f32 rect1_corner_circle_radius, f32 rect2_corner_circle_radius) noexcept
{
    struct line
    {
        ImVec2 m_p1;
        ImVec2 m_p2;
        f32 m_length;
        bool m_obstructed;

        line(ImVec2 const &p1, ImVec2 const &p2, ImRect const &forward_most_rect) noexcept
            : m_p1(p1)
            , m_p2(p2)
            , m_length(imgui::CalcLineLength(p1, p2))
            , m_obstructed(forward_most_rect.Contains(p1))
        {}

        bool operator<(line const &other) noexcept { return m_length < other.m_length; }
    };

    std::array<ImVec2, 4> p1_corners = { rect1.GetTL(), rect1.GetTR(), rect1.GetBL(), rect1.GetBR() };
    std::array<ImVec2, 4> p2_corners = { rect2.GetTL(), rect2.GetTR(), rect2.GetBL(), rect2.GetBR() };
    boost::container::static_vector<line, 4*4> possible_lines = {};

    for (auto const &p1_corner : p1_corners) {
        for (auto const &p2_corner : p2_corners) {
            possible_lines.emplace_back(p1_corner, p2_corner, rect2);
        }
    }

    std::sort(possible_lines.begin(), possible_lines.end(), std::less()); // shortest lines first

    auto shortest_unobstructed_line_iter = std::find_if(possible_lines.begin(), possible_lines.end(), [](line const &ln) { return !ln.m_obstructed; });

    if (shortest_unobstructed_line_iter == possible_lines.end()) {
        return false; // all possible lines to draw will be obstructed by rect2
    }

    ImU32 color_u32 = imgui::ImVec4_to_ImU32(color);

    line const &best_line = *shortest_unobstructed_line_iter;

    imgui::GetForegroundDrawList()->AddLine(best_line.m_p1, best_line.m_p2, color_u32);

    if (draw_border_for_rect1) {
        imgui::GetWindowDrawList()->AddRect(rect1.GetTL(), rect1.GetBR(), color_u32);
    }
    if (draw_border_for_rect2) {
        ImRect rect2_with_margin;
        ImVec2 margin = { 1, 1 };
        rect2_with_margin.Min = rect2.Min - margin;
        rect2_with_margin.Max = rect2.Max + margin;
        imgui::GetForegroundDrawList()->AddRect(rect2_with_margin.GetTL(), rect2_with_margin.GetBR(), color_u32);
    }

    ImVec2 rect_size = { 2, 2 };
    if (rect1_corner_circle_radius > 0.f) {
        imgui::GetForegroundDrawList()->AddRectFilled(best_line.m_p1 - rect_size, best_line.m_p1 + rect_size, color_u32);
        // imgui::GetForegroundDrawList()->AddCircleFilled(best_line.m_p1, rect1_corner_circle_radius, color_u32);
    }
    if (rect2_corner_circle_radius > 0.f) {
        imgui::GetForegroundDrawList()->AddRectFilled(best_line.m_p2 - rect_size, best_line.m_p2 + rect_size, color_u32);
        // imgui::GetForegroundDrawList()->AddCircleFilled(best_line.m_p2, rect2_corner_circle_radius, color_u32);
    }

    return true;
}

ImVec4 imgui::RGBA_to_ImVec4(s32 r, s32 g, s32 b, s32 a) noexcept
{
    f32 newr = f32(r) / 255.0f;
    f32 newg = f32(g) / 255.0f;
    f32 newb = f32(b) / 255.0f;
    f32 newa = f32(a);
    return ImVec4(newr, newg, newb, newa);
}

ImU32 imgui::ImVec4_to_ImU32(ImVec4 vec, bool attempt_denormalization) noexcept
{
    auto is_normalized = [&](f32 channel) noexcept { return channel >= 0.f && channel <= 1.f; };

    if (attempt_denormalization && is_normalized(vec.x) && is_normalized(vec.y) && is_normalized(vec.z) && is_normalized(vec.w)) {
        vec.x *= 255.f;
        vec.y *= 255.f;
        vec.z *= 255.f;
        vec.w *= 255.f;
    }

    ImU32 retval = IM_COL32(vec.x, vec.y, vec.z, vec.w);
    return retval;
}

std::pair<u64, u64> imgui::SelectRange(u64 prev_select_idx, u64 curr_select_idx) noexcept
{
    bool no_prev_selection = prev_select_idx == u64(-1);

    u64 first_idx = no_prev_selection ? 0 : prev_select_idx;
    u64 last_idx = curr_select_idx;

    if (first_idx > last_idx) {
        std::swap(first_idx, last_idx);
    }

    return { first_idx, last_idx };
}
