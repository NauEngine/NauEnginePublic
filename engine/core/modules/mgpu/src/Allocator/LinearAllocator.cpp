#include "nau/mgpu/Allocator/pch.h"
#include "nau/mgpu/Allocator/LinearAllocator.h"
#include "nau/mgpu/Allocator/Utils.h"  /* CalculatePadding */
#include <stdlib.h>     /* malloc, free */
#include <cassert>   /*assert		*/
#include <algorithm>    // max
#ifdef _DEBUG
#include <iostream>
#endif
namespace nau::PEPEngine::Allocator
{
    LinearAllocator::LinearAllocator(const std::size_t totalSize)
        : Allocator(totalSize)
    {
    }

    void LinearAllocator::Init()
    {
        if (startPtr != nullptr)
        {
            free(startPtr);
        }

        /*� ������ ��� ��� ����� �� �������� ��� ������.
         *� ����������� ���� ��������� �� �������� ������ �� VirtualAlloc (��� ����-�� ����� ��� �������� � ��������)*/
        startPtr = malloc(totalSize);
        offset = 0;
    }

    LinearAllocator::~LinearAllocator()
    {
        free(startPtr);
        startPtr = nullptr;
    }

    void* LinearAllocator::Allocate(const std::size_t size, const std::size_t alignment)
    {
        std::size_t padding = 0;
        const std::size_t currentAddress = (std::size_t)startPtr + offset;

        if (alignment != 0 && offset % alignment != 0)
        {
            // Alignment is required. Find the next aligned memory address and update offset
            padding = Utils::CalculatePadding(currentAddress, alignment);
        }

        if (offset + padding + size > totalSize)
        {
            return nullptr;
        }

        offset += padding;
        const std::size_t nextAddress = currentAddress + padding;
        offset += size;

#ifdef _DEBUG
        std::cout << "A" << "\t@C " << reinterpret_cast<void*>(currentAddress) << "\t@R " << reinterpret_cast<void*>
            (nextAddress) << "\tO " << offset << "\tP " << padding << std::endl;
#endif

        used = offset;
        peak = std::max(peak, used);

        return reinterpret_cast<void*>(nextAddress);
    }

    void LinearAllocator::Free(void* ptr)
    {
        assert(false && "Use Reset() method");
    }

    void LinearAllocator::Reset()
    {
        offset = 0;
        used = 0;
        peak = 0;
    }
}
