#pragma once
#include <mutex>
#include <condition_variable>
#include <deque>
#include <optional>

namespace taskhub {

template<typename T>
class BlockingQueue {
public:
    BlockingQueue() = default;

    // 禁止拷贝，允许移动（可选）
    BlockingQueue(const BlockingQueue&) = delete;
    BlockingQueue& operator=(const BlockingQueue&) = delete;

    // 生产者：推入元素
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            if (m_stopped) {
                // 已经 stop 的队列，通常不再接收任务
                return;
            }
            m_queue.push_back(std::move(value));
        }
        //ccy: notify_one 比 notify_all 更高效
        m_cv.notify_one();
    }

    // 消费者：阻塞直到取到一个元素，或队列关闭
    // 返回 std::nullopt 表示队列已关闭且无数据
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(m_mtx);
        m_cv.wait(lock, [this] {
            return m_stopped || !m_queue.empty();
        });

        if (m_queue.empty()) {
            // stopped && empty
            return std::nullopt;
        }

        T value = std::move(m_queue.front());
        m_queue.pop_front();
        return value;
    }

    // 关闭队列：唤醒所有等待线程，之后 pop() 都返回 nullopt
    void shutdown() {
        {
            std::lock_guard<std::mutex> lock(m_mtx);
            m_stopped = true;
        }
        m_cv.notify_all();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mtx);
        return m_queue.size();
    }

private:
    mutable std::mutex m_mtx;
    std::condition_variable m_cv;
    std::deque<T> m_queue;
    bool m_stopped{false};
};

} // namespace taskhub