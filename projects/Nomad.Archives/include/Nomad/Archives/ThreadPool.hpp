//
// Created by troplo on 09/21/26.
//

#ifndef NOMADARCHIVES_THREADPOOL_H
#define NOMADARCHIVES_THREADPOOL_H

#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace Nomad::Archives {
    class ThreadPool {
      public:
        static unsigned int DefaultThreadCount() {
            return std::max(1u, std::thread::hardware_concurrency() - 1);
        }

        explicit ThreadPool(size_t threads) {
            if (threads == 0) {
                threads = DefaultThreadCount();
            }
            for (size_t i = 0; i < threads; ++i) {
                m_workers.emplace_back([this] {
                    while (true) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(m_queueMutex);
                            m_cv.wait(lock, [this] {
                                return m_stop || !m_tasks.empty();
                            });
                            if (m_stop && m_tasks.empty())
                                return;
                            task = std::move(m_tasks.front());
                            m_tasks.pop();
                        }
                        task();
                    }
                });
            }
        }

        template <class F, class... Args>
        auto Enqueue(F &&f, Args &&...args)
            -> std::future<std::invoke_result_t<F, Args...>> {
            using return_type = std::invoke_result_t<F, Args...>;

            auto task = std::make_shared<std::packaged_task<return_type()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<return_type> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                if (m_stop)
                    throw std::runtime_error("Enqueue on stopped ThreadPool");
                m_tasks.emplace([task]() { (*task)(); });
            }
            m_cv.notify_one();
            return res;
        }

        ~ThreadPool() {
            {
                std::unique_lock<std::mutex> lock(m_queueMutex);
                m_stop = true;
            }
            m_cv.notify_all();
            for (std::thread &worker : m_workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
        }

        [[nodiscard]] size_t GetThreadCount() const { return m_workers.size(); }

      private:
        std::vector<std::thread> m_workers;
        std::queue<std::function<void()>> m_tasks;
        std::mutex m_queueMutex;
        std::condition_variable m_cv;
        bool m_stop{false};
    };

    inline size_t GetDefaultThreadCount() {
        return ThreadPool::DefaultThreadCount();
    }

} // namespace Nomad::Archives

#endif // NOMADARCHIVES_THREADPOOL_H
