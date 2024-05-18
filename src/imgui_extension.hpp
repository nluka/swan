/*
    Custom functions and structures to add convenience features to ImGui,
    masquerading as if they were part of ImGui all along...
*/

#pragma once

#include "stdafx.hpp"

namespace ImGui
{
    ImVec4 RGBA_to_ImVec4(s32 r, s32 g, s32 b, s32 a) noexcept;

    ImU32 ImVec4_to_ImU32(ImVec4 vec, bool attempt_denormalization = false) noexcept;

    f32 CalcLineLength(ImVec2 const &p1, ImVec2 const &p2) noexcept;

    [[deprecated( "Prefer DrawBestLineBetweenRectCorners" )]]
    bool DrawBestLineBetweenContextMenuAndTarget(ImRect const &target_rect, ImVec2 const &menu_TL, ImU32 const &color,
                                                 f32 circle_radius_target_corner = 2, f32 circle_radius_menu_TL = 2) noexcept;

    bool DrawBestLineBetweenRectCorners(ImRect const &rect1, ImRect const &rect2, ImVec4 const &color,
                                        bool draw_border_for_rect1 = false, bool draw_border_for_rect2 = false,
                                        f32 rect1_corner_circle_radius = 0, f32 rect2_corner_circle_radius = 0) noexcept;

    void Spacing(u64 n) noexcept;

    void SameLineSpaced(u64 num_spacing_calls) noexcept;

    ImVec4 RGBA_to_ImVec4(s32 r, s32 g, s32 b, s32 a) noexcept;

    void SetInitialFocusOnNextWidget() noexcept;

    void TableDrawCellBorderTop(ImVec2 cell_rect_min, f32 cell_width) noexcept;

    void HighlightTextRegion(ImVec2 const &text_rect_min, char const *text, u64 highlight_start_idx, u64 highlight_len, ImVec4 color) noexcept;

    bool IsColumnTextVisuallyTruncated(s32 table_column_index, char const *column_text, f32 column_text_offset_x = 0) noexcept;

    bool RenderTooltipWhenColumnTextTruncated(s32 table_column_index, char const *possibly_truncated_text, f32 possibly_truncated_text_offset_x = 0, char const *tooltip_content = nullptr) noexcept;

    bool OpenConfirmationModal(s32 confirmation_id, char const *             content_message, bool *confirmation_enabled = nullptr) noexcept;
    bool OpenConfirmationModal(s32 confirmation_id, std::function<void ()> content_render_fn, bool *confirmation_enabled = nullptr) noexcept;

    void OpenConfirmationModalWithCallback(s32 confirmation_id, char const *             content_message, std::function<void ()> on_yes_callback, bool *confirmation_enabled = nullptr) noexcept;
    void OpenConfirmationModalWithCallback(s32 confirmation_id, std::function<void ()> content_render_fn, std::function<void ()> on_yes_callback, bool *confirmation_enabled = nullptr) noexcept;

    std::optional<bool> GetConfirmationStatus(s32 confirmation_id) noexcept;

    bool HaveActiveConfirmationModal() noexcept;

    void RenderConfirmationModal() noexcept;

    struct EnumButton
    {
        struct RenderResult
        {
            bool hovered;
            bool activated;
            bool right_clicked;
        };

        EnumButton(char const *id) noexcept : id(id) { assert(id != nullptr); }

        RenderResult Render(s32 &enum_value, s32 enum_first, s32 enum_count, char const *labels[], u64 num_labels) noexcept;

    private:
        char const *id = nullptr;
        char const *current_label = nullptr;
    };

    std::pair<u64, u64> SelectRange(u64 prev_select_idx, u64 curr_select_idx) noexcept;

    struct ScopedAvailWidth
    {
        ScopedAvailWidth(f32 subtract_amt = 0) noexcept
        {
            f32 avail_width = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(std::max(avail_width - subtract_amt, 0.f));
        }
        ~ScopedAvailWidth() noexcept { ImGui::PopItemWidth(); }
    };

    struct ScopedItemWidth
    {
        ScopedItemWidth(f32 width) noexcept { ImGui::PushItemWidth(width); }
        ~ScopedItemWidth()         noexcept { ImGui::PopItemWidth(); }
    };

    struct ScopedDisable
    {
        bool state;
        ScopedDisable(bool disabled) noexcept : state(disabled) { ImGui::BeginDisabled(disabled); }
        ~ScopedDisable()             noexcept                   { ImGui::EndDisabled(); }
    };

    struct ScopedTextColor
    {
        bool m_condition;

        ScopedTextColor(ImVec4 const &color, bool condition = true) noexcept
            : m_condition(condition)
        {
            if (m_condition)
                ImGui::PushStyleColor(ImGuiCol_Text, color);
        }

        ~ScopedTextColor() noexcept { if (m_condition) ImGui::PopStyleColor(); }
    };

    struct ScopedColor
    {
        bool m_condition;

        ScopedColor(ImGuiCol which, ImVec4 const &color, bool condition = true) noexcept
            : m_condition(condition)
        {
            if (m_condition)
                ImGui::PushStyleColor(which, color);
        }

        ~ScopedColor() noexcept { if (m_condition) ImGui::PopStyleColor(); }
    };

    template <typename Ty>
    struct ScopedStyle
    {
        bool m_condition;
        Ty &m_attr;
        Ty m_original_value;

        ScopedStyle() = delete;

        ScopedStyle(Ty &attr, Ty const &override_value, bool condition = true) noexcept
            : m_condition(condition)
            , m_attr(attr)
            , m_original_value(attr)
        {
            if (m_condition)
                m_attr = override_value;
        }

        ~ScopedStyle() noexcept
        {
            if (m_condition)
                m_attr = m_original_value;
        }
    };

} // namespace imgui
