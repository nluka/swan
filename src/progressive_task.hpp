#pragma once

#include "stdafx.hpp"
#include "data_types.hpp"
#include "common_fns.hpp"

enum class progressive_task_status
{
    nil,
    active,
    // paused,
    cancelled,
    finished,
    // count,
};

inline char const *progressive_task_status_cstr(progressive_task_status status) noexcept
{
    switch (status) {
        default:
        case progressive_task_status::nil: return "nil";
        case progressive_task_status::active: return "active";
        case progressive_task_status::cancelled: return "cancelled";
        case progressive_task_status::finished: return "finished";
    }
}

template <typename Result>
struct progressive_task
{
    // template <typename F, typename... A>
    // void launch(swan_thread_pool_t &thread_pool, F && task, A &&... task_args) noexcept
    // {
    //     m_future = thread_pool.push_task(task, std::forward(task_args)...);
    // }

    // void request_pause() noexcept
    // {
    //     assert(m_task_status == progressive_task_status::active);
    //     assert(m_cancellation_token.load() == false);
    //     assert(this->pause_token.load() == false);

    //     this->pause_token.store(true);
    // }

    // void request_unpause() noexcept
    // {
    //     assert(m_task_status == progressive_task_status::paused);
    //     assert(m_cancellation_token.load() == false);
    //     assert(this->pause_token.load() == true);

    //     this->pause_token.store(false);
    // }

    Result result = {};
    std::mutex result_mutex = {};
    std::atomic_bool cancellation_token = false;
    std::atomic_bool active_token = false;
    // std::atomic_bool pause_token = false;
};
