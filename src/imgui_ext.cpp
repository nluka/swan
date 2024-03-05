#include "imgui_ext.hpp"

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

void imgui::RenderTooltipWhenColumnTextTruncated(s32 table_column_index, char const *possibly_truncated_text, char const *tooltip_content) noexcept
{
    // if hovering the path with at least 1 character's worth of content truncated, display full path as tooltip
    if (imgui::IsItemHovered(ImGuiHoveredFlags_DelayNormal) && imgui::TableGetHoveredColumn() == table_column_index) {
        f32 text_width = imgui::CalcTextSize(possibly_truncated_text).x;
        f32 column_width = imgui::GetColumnWidth(table_column_index);

        if (text_width > column_width) {
            tooltip_content = tooltip_content ? tooltip_content : possibly_truncated_text;
            imgui::SetTooltip(tooltip_content);
        }
    }
}
