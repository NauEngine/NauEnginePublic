#pragma once
#include <queue>
#include <mutex>

namespace nau::PEPEngine::Utils
{
    template <typename T>
    class LockThreadQueue final
    {
    public:
        LockThreadQueue();
        LockThreadQueue(const LockThreadQueue& copy);

        /**
             * Push a value into the back of the queue.
             */
        void Push(T value);

        /**
             * Try to pop a value from the front of the queue.
             * @returns false if the queue is empty.
             */
        bool TryPop(T& value);

        /**
             * Check to see if there are any items in the queue.
             */
        bool Empty() const;

        /**
             * Retrieve the number of items in the queue.
             */
        size_t Size() const;

    private:
        std::queue<T> m_Queue;
        mutable std::mutex m_Mutex;
    };

    template <typename T>
    LockThreadQueue<T>::LockThreadQueue()
    {
    }

    template <typename T>
    LockThreadQueue<T>::LockThreadQueue(const LockThreadQueue<T>& copy)
    {
        std::lock_guard<std::mutex> lock(copy.m_Mutex);
        m_Queue = copy.m_Queue;
    }

    template <typename T>
    void LockThreadQueue<T>::Push(T value)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Queue.emplace(value);
    }

    template <typename T>
    bool LockThreadQueue<T>::TryPop(T& value)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Queue.empty())
            return false;

        value = m_Queue.front();
        m_Queue.pop();

        return true;
    }

    template <typename T>
    bool LockThreadQueue<T>::Empty() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Queue.empty();
    }

    template <typename T>
    size_t LockThreadQueue<T>::Size() const
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Queue.size();
    }
}
