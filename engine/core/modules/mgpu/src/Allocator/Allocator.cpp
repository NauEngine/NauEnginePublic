#include "nau/mgpu/Allocator/pch.h"
#include "nau/mgpu/Allocator/Allocator.h"

namespace nau::PEPEngine::Allocator
{
    Allocator::Allocator(const std::size_t totalSize)
        : totalSize(totalSize), used(0)
    {
    }

    Allocator::~Allocator()
    {
        totalSize = 0;
    }
}
