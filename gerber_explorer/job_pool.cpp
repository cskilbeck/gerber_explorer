#include "job_pool.h"
#include "gerber_log.h"

//////////////////////////////////////////////////////////////////////

job_pool::~job_pool()
{
    cv.notify_all();
}

//////////////////////////////////////////////////////////////////////

void job_pool::shut_down()
{
    abort_jobs(0xffffffff);

    for(auto &w : workers) {
        w.get_stop_source().request_stop();
    }

    cv.notify_all();
    workers.clear();
}

//////////////////////////////////////////////////////////////////////

void job_pool::start_workers(size_t thread_count)
{
    for(size_t i = 0; i < thread_count; ++i) {
        workers.emplace_back([this](std::stop_token stoken) { worker_loop(stoken); });
    }
}

//////////////////////////////////////////////////////////////////////

job_pool::pool_info job_pool::get_info()
{
    std::lock_guard lock(queue_mutex);
    return { active_jobs.size(), tasks.size() };
}

//////////////////////////////////////////////////////////////////////

void job_pool::abort_jobs(uint32_t mask)
{
    std::lock_guard lock(queue_mutex);

    auto [first, last] = std::ranges::remove_if(tasks, [mask](const job_item &item) {
        if((item.flags & mask) != 0) {
            item.stop_src.request_stop();
            return true;
        }
        return false;
    });
    tasks.erase(first, last);

    for(auto *active_job : active_jobs) {
        if((active_job->flags & mask) != 0) {
            active_job->stop_src.request_stop();
        }
    }
}

//////////////////////////////////////////////////////////////////////

void job_pool::worker_loop(std::stop_token pool_stoken)
{
    LOG_CONTEXT("worker", debug);

    while(!pool_stoken.stop_requested()) {
        std::shared_ptr<job_item> current_job;    // Use shared_ptr
        {
            std::unique_lock lock(queue_mutex);

            // Wait until task available OR stop requested
            if(!cv.wait(lock, pool_stoken, [this] { return !tasks.empty(); })) {
                return;    // Stop requested
            }

            // Move the job to a shared_ptr to keep it alive and trackable
            current_job = std::make_shared<job_item>(std::move(tasks.front()));
            tasks.pop_front();

            // Store the shared_ptr so abort_jobs can find it
            active_jobs.insert(current_job.get());
        }

        if(current_job->work) {
            LOG_INFO("job begins");
            current_job->work(current_job->stop_src.get_token());
            LOG_INFO("job complete");
        }

        {
            std::lock_guard lock(queue_mutex);
            active_jobs.erase(current_job.get());
        }
    }
}
