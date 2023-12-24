/*
    Code which has been retired, i.e. replaced with newer & better code which delivers a better solution.
    Kept here for reference as it may inform or inspire future code.
*/

#if 0

{
    time_point_t now = current_time();
    s64 diff_ms = compute_diff_ms(expl.last_refresh_time, now);
    s32 min_refresh_itv_ms = explorer_options::min_tolerable_refresh_interval_ms;

    if (diff_ms >= max(min_refresh_itv_ms, opts.auto_refresh_interval_ms.load())) {
        auto refresh_notif_time = expl.refresh_notif_time.load(std::memory_order::seq_cst);
        if (expl.last_refresh_time.time_since_epoch().count() < refresh_notif_time.time_since_epoch().count()) {
            print_debug_msg("[ %d ] refresh notif RECV", expl.id);
            refresh();
        }
    }
}

std::jthread expl_change_notif_thread_0([&]() { explorer_change_notif_thread_func(explorers[0], window_close_flag); });
std::jthread expl_change_notif_thread_1([&]() { explorer_change_notif_thread_func(explorers[1], window_close_flag); });
std::jthread expl_change_notif_thread_2([&]() { explorer_change_notif_thread_func(explorers[2], window_close_flag); });
std::jthread expl_change_notif_thread_3([&]() { explorer_change_notif_thread_func(explorers[3], window_close_flag); });

void explorer_change_notif_thread_func(explorer_window &expl, std::atomic<s32> const &window_close_flag) noexcept
{
    // (void) set_thread_priority(THREAD_PRIORITY_BELOW_NORMAL);

    DWORD const notify_filter =
        FILE_NOTIFY_CHANGE_CREATION   |
        FILE_NOTIFY_CHANGE_DIR_NAME   |
        FILE_NOTIFY_CHANGE_FILE_NAME  |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_SIZE
    ;

    HANDLE watch_handle = {};
    wchar_t watch_target_utf16[2048] = {};
    swan_path_t watch_target_utf8 = {};
    time_point_t last_refresh_notif_sent = {};

    {
        std::scoped_lock lock(expl.latest_valid_cwd_mutex);
        watch_target_utf8 = expl.cwd;
    }

    s32 utf_written = utf8_to_utf16(watch_target_utf8.data(), watch_target_utf16, 2048);
    if (utf_written == 0) {
        print_debug_msg("[ %d ] utf8_to_utf16 failed during setup: watch_target_utf8 -> watch_target_utf16", expl.id);
    }

    watch_handle = FindFirstChangeNotificationW(watch_target_utf16, false, notify_filter);
    if (watch_handle == INVALID_HANDLE_VALUE) {
        print_debug_msg("[ %d ] FindFirstChangeNotificationW failed during setup: INVALID_HANDLE_VALUE", expl.id);
    } else {
        print_debug_msg("[ %d ] FindFirstChangeNotificationW initial setup success for [%s]", expl.id, watch_target_utf8.data());
    }

    while (!window_close_flag.load()) {
        expl.is_window_visible.wait(false); // wait for window to become visible

        swan_path_t latest_valid_cwd;
        {
            std::scoped_lock lock(expl.latest_valid_cwd_mutex);
            latest_valid_cwd = expl.latest_valid_cwd;
        }

        // check if watch target change, if yes then close previous watch_handle and reset it to new target
        if (!path_loosely_same(latest_valid_cwd, watch_target_utf8)) {
            watch_target_utf8 = latest_valid_cwd;

            if (watch_handle != INVALID_HANDLE_VALUE) {
                // stop watching old directory, if there was one
                FindCloseChangeNotification(watch_handle);
            }

            print_debug_msg("[ %d ] watch target changed, now: [%s]", expl.id, latest_valid_cwd.data());

            if (path_is_empty(latest_valid_cwd)) {
                watch_handle = INVALID_HANDLE_VALUE;
            }
            else {
                utf_written = utf8_to_utf16(latest_valid_cwd.data(), watch_target_utf16, 2048);
                if (utf_written == 0) {
                    print_debug_msg("[ %d ] utf8_to_utf16 failed: latest_valid_cwd -> watch_target_utf16", expl.id);
                } else {
                    watch_handle = FindFirstChangeNotificationW(watch_target_utf16, false, notify_filter);
                    if (watch_handle == INVALID_HANDLE_VALUE) {
                        print_debug_msg("[ %d ] FindFirstChangeNotificationW failed: INVALID_HANDLE_VALUE", expl.id);
                    }
                }
            }
        }

        if (watch_handle == INVALID_HANDLE_VALUE) {
            // latest_valid_cwd is invalid for some reason, wait for it to change.
            // during this time window visibility can change, but it doesn't matter.
            std::unique_lock lock(expl.latest_valid_cwd_mutex);
            expl.latest_valid_cwd_cond.wait(lock, []() { return true; });
        }
        else {
            // latest_valid_cwd is in fact a valid directory, so sit here and wait for a change notification.
            // there is a timeout because latest_valid_cwd may change while we wait, which invalidates the current watch_handle,
            // so we need to occasionally break and re-establish the watch_handle against the correct directory.
            DWORD wait_status = WaitForSingleObject(watch_handle, global_state::settings().auto_refresh_interval_ms.load());

            swan_path_t latest_target_utf8;
            {
                std::scoped_lock lock(expl.latest_valid_cwd_mutex);
                latest_target_utf8 = expl.cwd;
            }
            if (!path_loosely_same(watch_target_utf8, latest_target_utf8)) {
                // latest_valid_cwd changed as we were waiting for watch_handle,
                // invalidating this change notification setup. do nothing.
            }
            else {
                switch (wait_status) {
                    //? I believe the reason this code gets hit super frequently when a file operation is happening (e.g. copy big file)
                    //? is explained here: https://stackoverflow.com/a/14040978/16471560
                    // TODO: investigate why this is being called so many times when changes are happening, maybe this is a bug?
                    case WAIT_OBJECT_0: {
                        print_debug_msg("[ %d ] WAIT_OBJECT_0 h=%d target=[%s]", expl.id, watch_handle, watch_target_utf8.data());

                        if (expl.is_window_visible.load()) {
                            time_point_t now = current_time();

                            if (compute_diff_ms(last_refresh_notif_sent, now) >= global_state::settings().min_tolerable_refresh_interval_ms) {
                                print_debug_msg("[ %d ] refresh notif SEND", expl.id);

                                expl.refresh_notif_time.store(now, std::memory_order::seq_cst);
                                last_refresh_notif_sent = now;

                                if (!FindNextChangeNotification(watch_handle)) {
                                    print_debug_msg("[ %d ] FindNextChangeNotification failed", expl.id);
                                }
                            }
                        }

                        break;
                    }
                    case WAIT_TIMEOUT:
                        break;
                    case WAIT_FAILED:
                        print_debug_msg("[ %d ] WAIT_FAILED h=%d target=[%s]", expl.id, watch_handle, watch_target_utf8.data());
                        break;
                    default:
                        assert(false && "Unhandled wait_status");
                        break;
                }
            }
        }
    }

    BOOL closed = FindCloseChangeNotification(watch_handle);
    assert(closed && "FindCloseChangeNotification failed at program exit");
}

void read_dir_changes_callback(DWORD error_code, DWORD num_bytes_transferred, LPOVERLAPPED overlapped) noexcept
{
    if (error_code == ERROR_SUCCESS) {
        HANDLE handle_associated_with_callback = overlapped->hEvent;
        for (auto &expl : global_state::explorers()) {
            if (expl.read_dir_changes_handle == handle_associated_with_callback) {
                expl.read_dir_changes_in_flight.store(false);
                break;
            }
        }
    } else {
        print_debug_msg("read_dir_changes_callback::error_code indicates a failure: %d", error_code);
    }
}

#endif
