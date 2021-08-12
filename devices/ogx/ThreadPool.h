#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

/**
 * Enclave thread pool.
 */
class ThreadPool : public std::enable_shared_from_this<ThreadPool> {
public:
    using Value = std::vector<uint8_t>;
    using Function = std::function<Value()>;
    using Task = std::packaged_task<Value()>;
    using Future = std::future<Value>;

    friend void Worker(std::shared_ptr<ThreadPool> pool, size_t id);

    /**
     * Create a new thread pool.
     */
    explicit ThreadPool() : work_(true) {}

    /**
     * Initialize the thread pool.
     *
     * This needs to be separate from the constructor due to the use of shared_from_this<>.
     *
     * @param concurrency Concurrency level.
     */
    void Init(size_t concurrency = std::thread::hardware_concurrency());

    /**
     * Schedule a function for evaluation on the thread pool, returning a future.
     *
     * @param fn Function.
     * @return Future.
     */
    Future Schedule(Function fn);

    /**
     * Shutdown the pool.
     */
    void Shutdown();

private:
    /** Threads. */
    std::vector<std::thread> pool_;
    /** Work queue mutex. */
    std::mutex queue_mutex_;
    /** Work queue CV. */
    std::condition_variable queue_cv_;
    /** Work queue. */
    std::deque<Task> queue_;
    /** Work flag. */
    bool work_;
};
