#pragma once

#include <condition_variable>
#include <mutex>
#include <deque>
#include <stop_token>
#include <thread>
#include <vector>
#include <memory>
#include <unordered_set>

//////////////////////////////////////////////////////////////////////

struct job_pool
{
    //////////////////////////////////////////////////////////////////////
    // A simple move-only wrapper since <move_only_function> is missing in MSVC

    struct task_wrapper
    {
        struct base
        {
            virtual ~base() = default;
            virtual void call(std::stop_token) = 0;
        };

        template <typename F> struct impl : base
        {
            F f;

            explicit impl(F &&f) : f(std::forward<F>(f))
            {
            }

            void call(std::stop_token st) override
            {
                f(st);
            }
        };

        std::unique_ptr<base> ptr;

        task_wrapper() = default;

        template <typename F> explicit task_wrapper(F &&f) : ptr(std::make_unique<impl<std::decay_t<F>>>(std::forward<F>(f)))
        {
        }

        void operator()(std::stop_token st) const
        {
            if(ptr) {
                ptr->call(st);
            }
        }

        explicit operator bool() const
        {
            return static_cast<bool>(ptr);
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct job_item
    {
        task_wrapper work;
        uint32_t flags;
        mutable std::stop_source stop_src;
    };

    //////////////////////////////////////////////////////////////////////

    struct pool_info
    {
        size_t active;
        size_t queued;
    };

    //////////////////////////////////////////////////////////////////////

    job_pool() = default;

    ~job_pool();

    void start_workers(size_t thread_count = std::thread::hardware_concurrency() - 1);

    pool_info get_info();

    void abort_jobs(uint32_t mask);

    void shut_down();

    void worker_loop(std::stop_token pool_stoken);

    bool get_active_job_count(uint32_t mask);

    template <typename F> void add_job(uint32_t flags, F &&func)
    {
        {
            std::lock_guard lock(queue_mutex);
            tasks.push_back({ task_wrapper(std::forward<F>(func)), flags, std::stop_source{} });
        }
        cv.notify_one();
    }

    //////////////////////////////////////////////////////////////////////

    std::vector<std::jthread> workers;
    std::deque<job_item> tasks;
    std::unordered_set<job_item *> active_jobs;
    std::mutex queue_mutex;
    std::condition_variable_any cv;
};
