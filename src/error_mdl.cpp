#include "imgui/imgui.h"

#include "common_fns.hpp"
#include "imgui_specific.hpp"

static bool s_error_open = false;
static std::string s_error_action = {};
static std::string s_error_failure = {};

void swan_popup_modals::open_error(char const *action, char const *failure) noexcept
{
    s_error_open = true;

    assert(action != nullptr);
    s_error_action = action;

    assert(failure != nullptr);
    assert(strlen(failure) > 0);
    s_error_failure = failure;

    if (!s_error_action.empty()) {
        // capitalize first letter
        s_error_action.front() = (char)toupper(s_error_action.front());

        // ensure ends with period
        if (s_error_action.back() != '.') {
            s_error_action.push_back('.');
        }
    }

    // capitalize first letter
    s_error_failure.front() = (char)toupper(s_error_failure.front());

    // ensure ends with period
    if (s_error_failure.back() != '.') {
        s_error_failure.push_back('.');
    }
}

bool swan_popup_modals::is_open_error() noexcept
{
    return s_error_open;
}

void swan_popup_modals::render_error() noexcept
{
    if (s_error_open) {
        imgui::OpenPopup(swan_popup_modals::error);
    }
    if (!imgui::BeginPopupModal(swan_popup_modals::error, &s_error_open)) {
        return;
    }

    assert(!s_error_failure.empty());

    auto cleanup_and_close_popup = [&]() {
        s_error_open = false;
        s_error_action.clear();
        s_error_failure.clear();

        imgui::CloseCurrentPopup();
    };

    {
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        SCOPE_EXIT { imgui::PopTextWrapPos(); };

        if (s_error_action.empty()) {
            imgui::TextColored(red(), "%s", s_error_failure.c_str());
        }
        else {
            imgui::TextUnformatted("Failed:");
            imgui::TextColored(orange(), "%s", s_error_action.c_str());
            imgui::TextUnformatted("Reason:");
            imgui::TextColored(red(), "%s", s_error_failure.c_str());
        }
    }

    if (imgui::IsWindowFocused() && imgui::IsKeyPressed(ImGuiKey_Escape)) {
        cleanup_and_close_popup();
    }

    imgui::EndPopup();
}
