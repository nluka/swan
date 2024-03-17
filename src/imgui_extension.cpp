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

    if (*confirmation_enabled == false) {
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

bool imgui::RenderTooltipWhenColumnTextTruncated(s32 table_column_index,
                                                 char const *possibly_truncated_text,
                                                 f32 possibly_truncated_text_offset_x,
                                                 char const *tooltip_content) noexcept
{
    // if hovering the path with at least 1 character's worth of content truncated, display full path as tooltip
    if (imgui::IsItemHovered({}, 0.7f) && imgui::TableGetHoveredColumn() == table_column_index) {
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

void imgui::HighlightTextRegion(ImVec2 const &text_rect_min, char const *text, u64 highlight_start_idx, u64 highlight_len) noexcept
{
    ImVec2 min = text_rect_min;
    ImVec2 max = min + imgui::CalcTextSize(text);

    // at this point min & max draw a rectangle around the entire path

    // move forward min.x to skip characters before highlight begins
    min.x += imgui::CalcTextSize(text, text + highlight_start_idx).x;

    // move backward max.x to eliminate characters after the highlight ends
    max.x -= imgui::CalcTextSize(text + highlight_start_idx + highlight_len).x;

    ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(255, 100, 0, 60));
}
