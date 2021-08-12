#include "ThreadPool.h"

#include <cassert>

#include "spdlog/spdlog.h"

void Worker(std::shared_ptr<ThreadPool> pool, const size_t id) {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    log->debug("worker {} ready", id);

    while (true) {
        ThreadPool::Task fn;

        {
            std::unique_lock<std::mutex> lock(pool->queue_mutex_);
            pool->queue_cv_.wait(lock, [&]() {
                return !pool->queue_.empty() || !pool->work_;
            });
            if (pool->queue_.empty() && !pool->work_) {
                break;
            }
            fn = std::move(pool->queue_.front());
            pool->queue_.pop_front();
        }

        log->debug("worker {} executing job", id);
        fn();
    }

    log->debug("worker {} terminating", id);
}

void ThreadPool::Init(const size_t concurrency) {
    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    log->info("creating thread pool with {} workers", concurrency);
    for (auto i = 0; i < concurrency; i++) {
        auto self = shared_from_this();
        pool_.emplace_back([self, i]() {
            Worker(self, i);
        });
    }
}

ThreadPool::Future ThreadPool::Schedule(Function fn) {
    assert(!pool_.empty());

    auto log = spdlog::get("ogx");
    assert(log && "null logger");

    log->debug("queueing job");

    Task task(fn);
    auto f = task.get_future();

    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push_back(std::move(task));
    }

    queue_cv_.notify_one();
    return f;
}

void ThreadPool::Shutdown() {
    work_ = false;
    queue_cv_.notify_all();
    for (auto& t : pool_) {
        t.join();
    }
}
