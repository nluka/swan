#include "stdafx.hpp"
#include "common_functions.hpp"

static u64 g_popup_modals_open = 0;

bool global_state::any_popup_modals_open() noexcept { return g_popup_modals_open != 0; }

u64 &global_state::popup_modals_open_bit_field() noexcept { return g_popup_modals_open; }
