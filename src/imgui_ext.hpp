/*
    Custom functions and structures to add convenience features to ImGui,
    masquerading as if they were part of ImGui all along...
*/

#pragma once

#include "stdafx.hpp"

namespace ImGui
{
    void Spacing(u64 n) noexcept;

    void SameLineSpaced(u64 num_spacing_calls) noexcept;

    ImVec4 RGBA_to_ImVec4(s32 r, s32 g, s32 b, s32 a) noexcept;

    struct EnumButton
    {
    public:
        EnumButton(char const *name) noexcept : name(name) {}
        bool Render(s32 &enum_value, s32 enum_first, s32 enum_count, char const *labels[], u64 num_labels) noexcept;
    private:
        char const *name = nullptr;
        char const *current_label = nullptr;
        u64 rand_1 = {};
        u64 rand_2 = {};
    };

    struct ScopedAvailWidth
    {
        ScopedAvailWidth(f32 subtract_amt = 0) noexcept
        {
            f32 avail_width = ImGui::GetContentRegionAvail().x;
            ImGui::PushItemWidth(max(avail_width - subtract_amt, 0.f));
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
        ScopedDisable(bool disabled) noexcept { ImGui::BeginDisabled(disabled); }
        ~ScopedDisable()             noexcept { ImGui::EndDisabled(); }
    };

    struct ScopedTextColor
    {
        ScopedTextColor(ImVec4 const &color) noexcept { ImGui::PushStyleColor(ImGuiCol_Text, color); }
        ~ScopedTextColor()                   noexcept { ImGui::PopStyleColor(); }
    };

    struct ScopedColor
    {
        ScopedColor(ImGuiCol which, ImVec4 const &color) noexcept { ImGui::PushStyleColor(which, color); }
        ~ScopedColor()                                   noexcept { ImGui::PopStyleColor(); }
    };

    template <typename Ty>
    struct ScopedStyle
    {
        Ty &m_attr;
        Ty m_original_value;

        ScopedStyle() = delete;

        ScopedStyle(Ty &attr, Ty const &override_value) noexcept
            : m_attr(attr)
            , m_original_value(attr)
        {
            attr = override_value;
        }

        ~ScopedStyle() noexcept
        {
            m_attr = m_original_value;
        }
    };

} // namespace imgui
