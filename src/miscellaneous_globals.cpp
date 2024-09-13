#include "stdafx.hpp"
#include "common_functions.hpp"

namespace swan
{
    static s32                      g_page_size = 0;
    static swan_thread_pool_t       g_thread_pool(0);
    static bool                     g_move_dirents_payload_is_set = false;
    static std::filesystem::path    g_execution_path = {};
    static HWND                     g_hwnd = {};
    static std::vector<s64>         g_delete_icon_textures_queue = {};
};

s32 &global_state::page_size() noexcept { return swan::g_page_size; }

swan_thread_pool_t &global_state::thread_pool() noexcept { return swan::g_thread_pool; }

bool &global_state::move_dirents_payload_set() noexcept { return swan::g_move_dirents_payload_is_set; }

std::filesystem::path &global_state::execution_path() noexcept { return swan::g_execution_path; }

HWND &global_state::window_handle() noexcept { return swan::g_hwnd; }

std::vector<s64> &global_state::delete_icon_textures_queue() noexcept { return swan::g_delete_icon_textures_queue; };
