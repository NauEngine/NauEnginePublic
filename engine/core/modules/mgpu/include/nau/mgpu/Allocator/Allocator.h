#pragma once

#include "nau/mgpu/Allocator/pch.h"
#include <cstddef> // size_t

namespace nau::PEPEngine::Allocator
{
    class Allocator
    {
    protected:
        std::size_t totalSize;
        std::size_t used;
        std::size_t peak;

    public:
        Allocator(std::size_t totalSize);

        virtual ~Allocator();

        virtual void* Allocate(std::size_t size, std::size_t alignment = 0) = 0;

        virtual void Free(void* ptr) = 0;

        virtual void Init() = 0;
    };
}
