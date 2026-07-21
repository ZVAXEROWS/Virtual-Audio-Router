#pragma once

// =============================================================================
// ThreadPool.h — Virtual Audio Router
// =============================================================================
// Fixed-size general-purpose thread pool for non-audio background work.
//
// RESPONSIBILITY:
//   Execute general background tasks (device monitoring, reconnect logic,
//   settings save) without creating/destroying threads on the fly.
//
// IMPORTANT DISTINCTION:
//   This pool is NOT used for audio threads. Audio threads (capture, per-device
//   output) are created explicitly by AudioEngine with custom priority settings
//   (THREAD_PRIORITY_TIME_CRITICAL). Those threads must be controlled precisely.
//   This pool handles everything else.
//
// INPUTS:
//   - std::function<void()> tasks posted via Submit().
//
// OUTPUTS:
//   - Tasks are executed on pool threads in FIFO order.
//   - Returns a std::future<T> so callers can optionally wait for results.
//
// DESIGN:
//   Classic condition_variable + queue implementation. Simple and correct.
//   If we need work-stealing or per-thread queues later, the API surface
//   does not change — only the implementation.
//
// THREADING:
//   - Submit() is thread-safe.
//   - Workers share a single std::deque<Task> under a std::mutex.
// =============================================================================

#include <thread>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdexcept>
#include "var/Constants.h"

namespace var {

class ThreadPool {
public:
    /// Create pool with `threadCount` workers (default: kThreadPoolSize).
    explicit ThreadPool(uint32_t threadCount = constants::kThreadPoolSize);
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&)                 = delete;
    ThreadPool& operator=(ThreadPool&&)      = delete;

    // -------------------------------------------------------------------------
    // Task submission
    // -------------------------------------------------------------------------

    /// Submit a callable and return a future for its result.
    /// Throws std::runtime_error if the pool is shutting down.
    template<typename F, typename... Args>
    auto Submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
        using ReturnType = std::invoke_result_t<F, Args...>;

        if (m_shutdown.load()) {
            throw std::runtime_error("ThreadPool: Submit called after Shutdown");
        }

        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard lock(m_queueMutex);
            m_tasks.push([task]() { (*task)(); });
        }
        m_cv.notify_one();

        return result;
    }

    // -------------------------------------------------------------------------
    // Control
    // -------------------------------------------------------------------------

    /// Block until all currently queued tasks complete.
    void WaitAll();

    /// Stop accepting new tasks and wait for running tasks to finish.
    void Shutdown();

    uint32_t ThreadCount() const { return static_cast<uint32_t>(m_workers.size()); }
    uint32_t PendingCount() const;

private:
    void WorkerFunc();

    std::vector<std::thread>        m_workers;
    std::queue<std::function<void()>> m_tasks;
    mutable std::mutex              m_queueMutex;
    std::condition_variable         m_cv;
    std::atomic<bool>               m_shutdown { false };
};

} // namespace var
