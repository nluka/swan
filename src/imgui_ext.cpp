#include "imgui_ext.hpp"

#undef min
#undef max

static s32 s_current_confirm_id = -1;
static std::variant<std::string, std::function<void ()>> s_confirmation_content = "";
static std::optional<bool> s_confirmation_response = std::nullopt;

bool imgui::HaveActiveConfirmationModal() noexcept
{
    return s_current_confirm_id != -1;
}

void imgui::OpenConfirmationModal(s32 confirmation_id, char const *message) noexcept
{
    assert(s_current_confirm_id == -1 && "Don't call OpenConfirmationModal when there is already an active modal");
    assert(s_current_confirm_id != confirmation_id && "Don't call OpenConfirmationModal repeatedly for the same confirmation_id");
    assert(message != nullptr && "Don't pass message == nullptr to OpenConfirmationModal");

    s_current_confirm_id = confirmation_id;
    s_confirmation_content = message;
    s_confirmation_response = std::nullopt;
}

void imgui::OpenConfirmationModal(s32 confirmation_id, std::function<void ()> content_render_fn) noexcept
{
    assert(s_current_confirm_id == -1 && "Don't call OpenConfirmationModal when there is already an active modal");
    assert(s_current_confirm_id != confirmation_id && "Don't call OpenConfirmationModal repeatedly for the same confirmation_id");
    assert(content_render_fn != nullptr && "Don't pass content_render_fn == nullptr to OpenConfirmationModal");

    s_current_confirm_id = confirmation_id;
    s_confirmation_content = content_render_fn;
    s_confirmation_response = std::nullopt;
}

std::optional<bool> imgui::GetConfirmationStatus(s32 confirmation_id) noexcept
{
    if (confirmation_id != s_current_confirm_id) {
        return std::nullopt;
    } else {
        auto retval = s_confirmation_response;
        if (retval.has_value()) {
            s_current_confirm_id = -1;
            s_confirmation_content = "";
            s_confirmation_response = std::nullopt;
        }
        return retval;
    }
}

void imgui::RenderConfirmationModal() noexcept
{
    if (s_current_confirm_id == -1) {
        return;
    }

    imgui::OpenPopup("Confirm ## RenderConfirmationModal");

    imgui::SetNextWindowSize({ std::min(500.f, imgui::GetMainViewport()->Size.x), -1 });

    if (imgui::BeginPopupModal("Confirm ## RenderConfirmationModal", 0, ImGuiWindowFlags_NoResize)) {
        auto cleanup_and_close_popup = []() noexcept {
            //? defer cleanup to GetConfirmationStatus
            // s_current_confirm_id = -1;
            // s_confirmation_content = "";
            // s_confirmation_response = std::nullopt;

            imgui::CloseCurrentPopup();
        };

        if (std::holds_alternative<std::string>(s_confirmation_content)) {
            imgui::TextWrapped(std::get<std::string>(s_confirmation_content).c_str());
        } else {
            auto user_defined_fn = std::get<std::function<void ()>>(s_confirmation_content);
            user_defined_fn();
        }

        imgui::Spacing();

        if (imgui::Button("OK")) {
            s_confirmation_response = true;
            cleanup_and_close_popup();
        }

        imgui::SameLine();

        if (imgui::Button("Cancel")) {
            s_confirmation_response = false;
            cleanup_and_close_popup();
        }

        imgui::SameLine();

        imgui::TextUnformatted("(?)");
        if (imgui::IsItemHovered()) {
            imgui::SetTooltip("[Enter]   OK\n"
                              "[Escape]  Cancel");
        }

        if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
            s_confirmation_response = false;
            cleanup_and_close_popup();
        }
        if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Enter)) {
            s_confirmation_response = true;
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
        auto const &style = imgui::GetStyle();

        f32 text_width = imgui::CalcTextSize(possibly_truncated_text).x;
        f32 column_width = imgui::GetColumnWidth(table_column_index);
        // f32 space_width = imgui::CalcTextSize(" ").x;
        // f32 total_content_width = text_width + style.ItemSpacing.x; // + (style.CellPadding.x * 2);

        if (text_width > column_width) {
            tooltip_content = tooltip_content ? tooltip_content : possibly_truncated_text;
            imgui::SetTooltip(tooltip_content);
        }
    }
}
