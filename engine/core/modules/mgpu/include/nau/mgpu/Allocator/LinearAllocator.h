#pragma once

#include "nau/mgpu/Allocator/pch.h"
#include "nau/mgpu/Allocator/Allocator.h"

namespace nau::PEPEngine::Allocator
{
    class LinearAllocator : public Allocator
    {
    protected:
        void* startPtr = nullptr;
        std::size_t offset;

    public:
        LinearAllocator(std::size_t totalSize);

        ~LinearAllocator() override;

        void* Allocate(std::size_t size, std::size_t alignment = 0) override;

        void Free(void* ptr) override;

        void Init() override;

        virtual void Reset();
    };
}
