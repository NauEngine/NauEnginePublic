#pragma once
#include <cstddef>
#include <functional>
#include <list>
#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "nau/mgpu/Allocator/LinearAllocationStrategy.h"


namespace nau::PEPEngine::Allocator
{
    template <typename T, class AllocationStrategy>
    class STLCustomAllocator
    {
        static_assert(!std::is_same_v<T, void>, "Type of the allocator can not be void");

    public:
        using value_type = T;

        template <typename U, class AllocStrategy>
        friend class STLCustomAllocator;

        template <typename U>
        struct rebind
        {
            using other = STLCustomAllocator<U, AllocationStrategy>;
        };

        STLCustomAllocator();

        explicit STLCustomAllocator(AllocationStrategy& strategy) noexcept;

        STLCustomAllocator(const STLCustomAllocator& other) noexcept;

        template <typename U>
        STLCustomAllocator(const STLCustomAllocator<U, AllocationStrategy>& other) noexcept;

        T* allocate(std::size_t count_objects);

        void deallocate(void* memory_ptr, std::size_t count_objects);

        template <typename U, typename... Args>
        void construct(U* ptr, Args&&... args);

        template <typename U>
        void destroy(U* ptr);

        AllocationStrategy* GetStrategy() const;

    protected:
        AllocationStrategy* strategy = nullptr;
    };

    template <typename T, typename U, class AllocationStrategy>
    bool operator==(const STLCustomAllocator<T, AllocationStrategy>& lhs,
                    const STLCustomAllocator<U, AllocationStrategy>& rhs)
    {
        return lhs.GetStrategy() == rhs.GetStrategy();
    }

    template <typename T, typename U, class AllocationStrategy>
    bool operator!=(const STLCustomAllocator<T, AllocationStrategy>& lhs,
                    const STLCustomAllocator<U, AllocationStrategy>& rhs)
    {
        return !(lhs == rhs);
    }


    template <typename T, class AllocationStrategy>
    STLCustomAllocator<T, AllocationStrategy>::STLCustomAllocator() = default;

    template <typename T, class AllocationStrategy>
    STLCustomAllocator<T, AllocationStrategy
    >::STLCustomAllocator(AllocationStrategy& strategy) noexcept : strategy(&strategy)
    {
    }

    template <typename T, class AllocationStrategy>
    STLCustomAllocator<T, AllocationStrategy
    >::STLCustomAllocator(const STLCustomAllocator& other) noexcept : strategy(other.strategy)
    {
    }

    template <typename T, class AllocationStrategy>
    template <typename U>
    STLCustomAllocator<T, AllocationStrategy>::STLCustomAllocator(
        const STLCustomAllocator<U, AllocationStrategy>& other) noexcept : strategy(other.strategy)
    {
    }

    template <typename T, class AllocationStrategy>
    T* STLCustomAllocator<T, AllocationStrategy>::allocate(const std::size_t count_objects)
    {
        assert(strategy && "Not initialized allocation strategy");
        return static_cast<T*>(strategy->Allocate(count_objects * sizeof(T)));
    }

    template <typename T, class AllocationStrategy>
    void STLCustomAllocator<T, AllocationStrategy>::deallocate(void* memory_ptr, const std::size_t count_objects)
    {
        assert(strategy && "Not initialized allocation strategy");
        strategy->Deallocate(memory_ptr, count_objects * sizeof(T));
    }

    template <typename T, class AllocationStrategy>
    template <typename U, typename... Args>
    void STLCustomAllocator<T, AllocationStrategy>::construct(U* ptr, Args&&... args)
    {
        new(reinterpret_cast<void*>(ptr)) U{std::forward<Args>(args)...};
    }

    template <typename T, class AllocationStrategy>
    template <typename U>
    void STLCustomAllocator<T, AllocationStrategy>::destroy(U* ptr)
    {
        ptr->~U();
    }

    template <typename T, class AllocationStrategy>
    AllocationStrategy* STLCustomAllocator<T, AllocationStrategy>::GetStrategy() const
    {
        return strategy;
    }


    template <typename T>
    using CustomAllocator = STLCustomAllocator<T, LinearAllocationStrategy<INIT_LINEAR_SIZE>>;

    template <typename T>
    using custom_vector = std::vector<T, CustomAllocator<T>>;

    template <typename T>
    using custom_list = std::list<T, CustomAllocator<T>>;

    template <typename T>
    using custom_deque = std::deque<T, CustomAllocator<T>>;

    template <typename T, class _Container = custom_deque<T>>
    using custom_queue = std::queue<T, _Container>;

    template <typename T>
    using custom_set = std::set<T, std::less<T>, CustomAllocator<T>>;

    template <typename T>
    using custom_unordered_set = std::unordered_set<T, std::hash<T>, std::equal_to<T>, CustomAllocator<T>>;

    template <typename K, typename V>
    using custom_map = std::map<K, V, std::less<K>, CustomAllocator<std::pair<const K, V>>>;

    template <typename K, typename V>
    using custom_multimap = std::multimap<K, V, std::less<K>, CustomAllocator<std::pair<const K, V>>>;

    template <typename K, typename V>
    using custom_unordered_map = std::unordered_map<
        K, V, std::hash<K>, std::equal_to<K>, CustomAllocator<std::pair<const K, V>>>;

    using custom_string = std::basic_string<char, std::char_traits<char>, CustomAllocator<char>>;

    using custom_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, CustomAllocator<wchar_t>>;

    template <typename T>
    using custom_unique_ptr = std::unique_ptr<T, std::function<void(T*)>>;

    template <class Allocator, typename T = typename Allocator::value_type, typename... Args>
    custom_unique_ptr<T> make_custom_unique(Allocator allocator, Args&&... args)
    {
        const auto custom_deleter = [allocator](T* ptr) mutable
        {
            allocator.destroy(ptr);
            allocator.Deallocate(ptr, 1u);
        };

        void* memory_block = allocator.Allocate(1u);
        if (memory_block)
        {
            T* object_block = static_cast<T*>(memory_block);
            allocator.construct(object_block, std::forward<Args>(args)...);
            return custom_unique_ptr<T>{object_block, custom_deleter};
        }

        return nullptr;
    }
}
