// =============================================================================
// ThreadPool.cpp — Virtual Audio Router
// =============================================================================

#include "var/ThreadPool.h"
#include <stdexcept>

namespace var {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------

ThreadPool::ThreadPool(uint32_t threadCount) {
    if (threadCount == 0) {
        throw std::invalid_argument("ThreadPool: threadCount must be > 0");
    }

    m_workers.reserve(threadCount);
    for (uint32_t i = 0; i < threadCount; ++i) {
        m_workers.emplace_back(&ThreadPool::WorkerFunc, this);
    }
}

ThreadPool::~ThreadPool() {
    Shutdown();
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void ThreadPool::WorkerFunc() {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock(m_queueMutex);
            m_cv.wait(lock, [this] {
                return m_shutdown.load() || !m_tasks.empty();
            });

            if (m_shutdown.load() && m_tasks.empty()) {
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        task();
    }
}

// ---------------------------------------------------------------------------
// Control
// ---------------------------------------------------------------------------

void ThreadPool::Shutdown() {
    {
        std::lock_guard lock(m_queueMutex);
        m_shutdown.store(true);
    }
    m_cv.notify_all();

    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::WaitAll() {
    // Simple approach: submit a barrier task per thread and wait for all.
    // Phase 5: replace with a proper latch or semaphore.
    std::vector<std::future<void>> barriers;
    for (uint32_t i = 0; i < m_workers.size(); ++i) {
        barriers.push_back(Submit([]{}));
    }
    for (auto& f : barriers) {
        f.get();
    }
}

uint32_t ThreadPool::PendingCount() const {
    std::lock_guard lock(m_queueMutex);
    return static_cast<uint32_t>(m_tasks.size());
}

} // namespace var
