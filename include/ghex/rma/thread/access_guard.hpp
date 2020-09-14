/*
 * GridTools
 *
 * Copyright (c) 2014-2020, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef INCLUDED_GHEX_RMA_THREAD_ACCESS_GUARD_HPP
#define INCLUDED_GHEX_RMA_THREAD_ACCESS_GUARD_HPP

#include <mutex>
#include <condition_variable>
#include <memory>
#include "../access_mode.hpp"
#include "../locality.hpp"
#include "./handle.hpp"

namespace gridtools {
namespace ghex {
namespace rma {
namespace thread {

struct access_state
{
    access_mode m_mode = access_mode::local;
    std::mutex m_mtx;
    std::condition_variable m_cv;
};

struct local_access_guard
{
    struct impl
    {
        access_state m_state;
        local_data_holder m_handle;
        
        impl()
        : m_state{}
        , m_handle(&m_state, sizeof(access_state), false)    
        {}
    };

    struct info
    {
        ::gridtools::ghex::rma::thread::info m_info;
    };

    std::unique_ptr<impl> m_impl;

    local_access_guard()
    : m_impl{std::make_unique<impl>()}
    {}
    
    local_access_guard(local_access_guard&&) = default;

    info get_info() const
    {
        return { m_impl->m_handle.get_info()  };
    }

    void start_target_epoch()
    {
        std::unique_lock<std::mutex> lk{m_impl->m_state.m_mtx};
        m_impl->m_state.m_cv.wait(lk, [this] { return m_impl->m_state.m_mode == access_mode::local; });
    }

    void end_target_epoch()
    {
        {
        std::lock_guard<std::mutex> lk{m_impl->m_state.m_mtx};
        m_impl->m_state.m_mode = access_mode::remote;
        }
        m_impl->m_state.m_cv.notify_one();
    }
};


struct remote_access_guard
{
    std::unique_ptr<remote_data_holder> m_handle;

    remote_access_guard(typename local_access_guard::info info_, locality loc)
    : m_handle{std::make_unique<remote_data_holder>(info_.m_info, loc)}
    {}
    remote_access_guard() = default;
    remote_access_guard(remote_access_guard&&) = default;
    remote_access_guard& operator=(remote_access_guard&&) = default;

    access_state* get_ptr()
    {
        return (access_state*)(m_handle->get_ptr());
    } 

    void start_source_epoch()
    {
        std::unique_lock<std::mutex> lk{get_ptr()->m_mtx};
        get_ptr()->m_cv.wait(lk, [this] { return get_ptr()->m_mode == access_mode::remote; });
    }

    void end_source_epoch()
    {
        {
        std::lock_guard<std::mutex> lk{get_ptr()->m_mtx};
        get_ptr()->m_mode = access_mode::local;
        }
        get_ptr()->m_cv.notify_one();
    }
};

} // namespace thread
} // namespace rma
} // namespace ghex
} // namespace gridtools

#endif /* INCLUDED_GHEX_RMA_THREAD_ACCESS_GUARD_HPP */
