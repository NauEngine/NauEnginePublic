#pragma once
#include <cstdint>
#include <atomic>
#include <cassert>


namespace nau::PEPEngine::Utils
{
    // Source: Dmitry Vyukov's MPMC
    // http://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue

    template <typename T>
    class ConcurrentQueue
    {
    public:
        ConcurrentQueue(const size_t buffer_size)
            : buffer_(new cell_t[buffer_size])
              , buffer_mask_(buffer_size - 1)
        {
            assert((buffer_size >= 2) && ((buffer_size & (buffer_size - 1)) == 0));
            for (size_t i = 0; i != buffer_size; i += 1)
                buffer_[i].sequence_.store(i, std::memory_order_relaxed);
            enqueue_pos_.store(0, std::memory_order_relaxed);
            dequeue_pos_.store(0, std::memory_order_relaxed);
        }

        ~ConcurrentQueue()
        {
            delete[] buffer_;
        }

        bool enqueue(const T& data)
        {
            cell_t* cell;
            size_t pos = enqueue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                const size_t seq =
                    cell->sequence_.load(std::memory_order_acquire);
                const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
                if (dif == 0)
                {
                    if (enqueue_pos_.compare_exchange_weak
                        (pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = enqueue_pos_.load(std::memory_order_relaxed);
            }
            cell->data_ = data;
            cell->sequence_.store(pos + 1, std::memory_order_release);
            return true;
        }

        bool dequeue(T& data)
        {
            cell_t* cell;
            size_t pos = dequeue_pos_.load(std::memory_order_relaxed);
            for (;;)
            {
                cell = &buffer_[pos & buffer_mask_];
                const size_t seq =
                    cell->sequence_.load(std::memory_order_acquire);
                const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
                if (dif == 0)
                {
                    if (dequeue_pos_.compare_exchange_weak
                        (pos, pos + 1, std::memory_order_relaxed))
                        break;
                }
                else if (dif < 0)
                    return false;
                else
                    pos = dequeue_pos_.load(std::memory_order_relaxed);
            }
            data = cell->data_;
            cell->sequence_.store
                (pos + buffer_mask_ + 1, std::memory_order_release);
            return true;
        }

    private:
        struct cell_t
        {
            std::atomic<size_t> sequence_;
            T data_;
        };

        static constexpr size_t cacheline_size = 64;
        using cacheline_pad_t = char[cacheline_size];

        cacheline_pad_t pad0_;
        cell_t* const buffer_;
        const size_t buffer_mask_;
        cacheline_pad_t pad1_;
        std::atomic<size_t> enqueue_pos_;
        cacheline_pad_t pad2_;
        std::atomic<size_t> dequeue_pos_;
        cacheline_pad_t pad3_;
    };
}
