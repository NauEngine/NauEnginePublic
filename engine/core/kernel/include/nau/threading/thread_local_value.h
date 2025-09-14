// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

/**
 * @file thread_local_value.h
 * @brief Provides classes and functions for managing thread-local storage and resources.
 */

#pragma once

#include "nau/kernel/kernel_config.h"
#include "nau/diag/assertion.h"

#include <functional>
#include <mutex>
#include <limits>
#include <stack>

namespace nau
{
    /**
     * @brief A class to handle RAII for construction and destruction functions. Move only
     */
    class NAU_KERNEL_EXPORT RAIIFunction final
    {
    public:
        /**
         * @brief Constructor that initializes the RAII object.
         * 
         * @param construct The function to be called on construction.
         * @param destruct The function to be called on destruction.
         */
        RAIIFunction(std::function<void()> construct, std::function<void()> destruct);
        RAIIFunction(const RAIIFunction& other) = delete;
        RAIIFunction& operator=(const RAIIFunction& other) = delete;
        RAIIFunction(RAIIFunction&& other) = default;
        RAIIFunction& operator=(RAIIFunction&& other) = default;
        ~RAIIFunction();
    private:
        std::function<void()> m_destruct;
    };

    /**
     * @brief Retrieves the current live thread index.
     * 
     * @return The thread index.
     */
    NAU_KERNEL_EXPORT uint32_t liveThreadIndex();
    
    /**
     * @brief A template class for managing thread-local values. Move only
     * The provided class manages thread-local storage for objects of any type. 
     * It ensures that each thread has its own instance of a value, 
     * and it can construct and destruct these values appropriately. 
     * This is useful for cases where you need thread-specific data that is not shared between threads.
     * 
     * @tparam Type The type of the thread-local value.
     */
    template<class Type>
    class ThreadLocalValue final
    {
        struct Status
        {
            bool valid = false;
        };

    public:

        /**
         * @brief Constructs a ThreadLocalValue object.
         * You can create a ThreadLocalValue instance with an optional constructor function.
         * 
         * @param construct A function to be called on value construction.
         */
        ThreadLocalValue(std::function<void(Type&)> construct = nullptr) 
            : m_construct(construct)
        {
            m_lineSize = std::thread::hardware_concurrency();
            resizeLines(0);
        }

        ThreadLocalValue(const ThreadLocalValue&) = delete;
        ThreadLocalValue& operator=(const ThreadLocalValue&) = delete;
        ThreadLocalValue(ThreadLocalValue&& val)
        {
            m_lineSize = val.m_lineSize;
            m_numLines = val.m_numLines;
            m_memBlock = val.m_memBlock;
            val.m_memBlock = nullptr;
            val.m_numLines = 0; 
        }
        ThreadLocalValue& operator=(ThreadLocalValue&& val)
        {
            m_lineSize = val.m_lineSize;
            m_numLines = val.m_numLines;
            m_memBlock = val.m_memBlock;
            val.m_memBlock = nullptr;
            val.m_numLines = 0; 
            return *this;
        }


        ~ThreadLocalValue() 
        {
            for (size_t i = 0; i < m_numLines; i++)
            {
                for(size_t j = 0; j < m_lineSize; j++)
                {
                    auto val = getData(i, j);
                    if(val.first->valid)
                        val.second->~Type();
                }
                free(m_memBlock[i]);
            }
            free(m_memBlock);
        }

        /**
         * @brief Destroys all thread-local values. 
         * It's not thread safe operation make sure no other thread is using value
         */
        void destroyAll()
        {
            std::lock_guard lock(m_sync);
            for (size_t i = 0; i < m_numLines; i++)
            {
                for(size_t j = 0; j < m_lineSize; j++)
                {
                    auto val = getData(i, j);
                    if(val.first->valid)
                    {
                        val.second->~Type();
                        val.first->valid = false;
                    }
                }
            }
        }

        /**
         * @brief Destroys the current thread's local value.
         */
        void destroy()
        {
            auto index = liveThreadIndex();
            auto line = index / m_lineSize;
            if(line >= m_numLines) 
                return;

            auto offset = index % m_lineSize;
            auto val = getData(line, offset);
            if(val.first->valid)
            {
                val.second->~Type();
                val.first->valid = false;
            }
        }

        /**
         * @brief Retrieves the current thread's local value.
         * 
         * @return Reference to the current thread's local value.
         */
        [[nodiscard]] Type& value()
        {
            auto index = liveThreadIndex();
            auto line = index / m_lineSize;
            resizeLines(line);
            auto offset = index % m_lineSize;

            auto val = getData(line, offset);
            if(!val.first->valid)
            {
                new (val.second) Type();
                val.first->valid = true;
                if(m_construct)
                    m_construct(*val.second); 
                return *val.second;
            }
            else
                return *val.second;
        }

        /**
         * @brief Visits all valid thread-local values and applies a visitor function.
         * It's not thread safe operation make sure no other thread is using value
         * 
         * @param visitor The function to apply to each valid thread-local value.
         */
        void visitAll(std::function<void(Type&)> visitor)
        {
            std::lock_guard lock(m_sync);
            for (size_t i = 0; i < m_numLines; i++)
            {
                for(size_t j = 0; j < m_lineSize; j++)
                {
                    auto val = getData(i, j);
                    if(val.first->valid)
                        visitor(*val.second);
                }
            }
        }

        /**
         * @brief Visits all valid thread-local values and applies a visitor function.
         * It's not thread safe operation make sure no other thread is using value
         * 
         * @param visitor The function to apply to each valid thread-local value.
         */
        void visitAll(std::function<void(const Type&)> visitor) const
        {
            std::lock_guard lock(m_sync);
            for (size_t i = 0; i < m_numLines; i++)
            {
                for (size_t j = 0; j < m_lineSize; j++)
                {
                    auto val = getData(i, j);
                    if (val.first->valid)
                        visitor(*val.second);
                }
            }
        }

    private:

        void resizeLines(size_t sizeReq) 
        {
            if(sizeReq >= m_numLines)
            {
                std::lock_guard lock(m_sync);
                if(sizeReq >= m_numLines)
                {
                    auto newSize = sizeReq + 1;
                    auto blockSize = (sizeof(Type) + sizeof(Status)) * m_lineSize + alignof(Type) - 1;

                    char** temp = static_cast<char**>(realloc(m_memBlock, sizeof(char*) * newSize));
                    if (!temp)
                    {
                        throw std::bad_alloc();
                    }
                    m_memBlock = temp;

                    for(size_t i = m_numLines; i < newSize; i++)
                    {
                        m_memBlock[i] = static_cast<char*>(malloc(blockSize));
                        auto statusLine = alingedPtr(m_memBlock[i]) + sizeof(Type) * m_lineSize;
                        for(size_t j = 0; j < m_lineSize; j++)
                        {
                            auto status = reinterpret_cast<Status*>(statusLine + sizeof(Status) * j);
                            status->valid = false;
                        }
                    }
                    m_numLines = newSize;
                }
            }
        }

        [[nodiscard]] std::pair<ThreadLocalValue::Status*, Type*> getData(size_t line, size_t offset) const
        {
            auto ptrAlinged = alingedPtr(m_memBlock[line]);
            auto statusLine = ptrAlinged + sizeof(Type) * m_lineSize;
            auto status = reinterpret_cast<Status*>(statusLine + sizeof(Status) * offset);
            auto valPtr = reinterpret_cast<Type*>(ptrAlinged + sizeof(Type) * offset);
            NAU_ASSERT(reinterpret_cast<uintptr_t>(valPtr) % alignof(Type) == 0);
            return { status, valPtr };
        }

        char* alingedPtr(char* startPtr) const
        {
            auto mask = ~(alignof(Type) - 1);
            return reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(startPtr + alignof(Type) - 1) & mask);
        }

        char** m_memBlock = nullptr;
        size_t m_lineSize = 0;
        size_t m_numLines = 0;
        mutable std::mutex m_sync; 
        std::function<void(Type&)> m_construct;
    };


} 
