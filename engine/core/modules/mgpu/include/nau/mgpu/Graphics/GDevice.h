#pragma once
#include <array>
#include <cstddef>
#include <d3d12.h>
#include <wrl/client.h>
#include "dxgi1_6.h"
#include "nau/mgpu/Utils/Lazy.h"
#include "nau/mgpu/Allocator/MemoryAllocator.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;
    using namespace Utils;

    class GCommandQueue;
    class GResource;
    class GAllocator;
    class GDeviceFactory;
    class GDescriptor;

    enum class GQueueType
    {
        Graphics,
        Compute,
        Copy,
        Count
    };

    // B: public base, so make_shared sets the weak self-reference used by shared_from_this() (was private in A,
    // and GDevice.cpp created separate owners with std::shared_ptr<GDevice>(this): double delete at process exit).
    class GDevice : public std::enable_shared_from_this<GDevice>
    {
        ComPtr<ID3D12Device> device;
        ComPtr<IDXGIAdapter3> adapter;


        std::array<std::shared_ptr<GAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> graphicAllocators;
        std::array<std::shared_ptr<GCommandQueue>, static_cast<int>(GQueueType::Count)> queues;

        bool crossAdapterTextureSupport;

        // Get the timestamp values from the result buffers.
        D3D12_RANGE readRange = {};
        const D3D12_RANGE emptyRange = {};

        void* mappedData = nullptr;
        Lazy<GResource> timestampResultBuffer;
        Lazy<ComPtr<ID3D12QueryHeap>> timestampQueryHeap;

        DXGI_ADAPTER_DESC2 desc;
        std::wstring name;

        friend GDeviceFactory;


        void InitialDescriptorAllocator();
        void InitialCommandQueue();
        void InitialQueryTimeStamp();
        void InitialDevice();

        bool isInitialized = false;

    public:
        GDevice(const ComPtr<IDXGIAdapter3>& adapter);

        ~GDevice();

        bool IsInitialized() const { return isInitialized; }
        void Initialize();

        const DXGI_ADAPTER_DESC2& GetDesc() const { return desc; }

        HANDLE SharedHandle(const ComPtr<ID3D12DeviceChild>& deviceObject, const SECURITY_ATTRIBUTES* attributes,
                            DWORD access,
                            LPCWSTR name) const;

        void ShareResource(const GResource& resource, const std::shared_ptr<GDevice>& destDevice, GResource& destResource,
                           const SECURITY_ATTRIBUTES* attributes = nullptr,
                           DWORD access = GENERIC_ALL, LPCWSTR name = L"") const;


        void ReleaseSlateDescriptors(uint64_t frameCount) const;
        void ResetAllocators(uint64_t frameCount) const;

        GDescriptor AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE type, size_t descriptorCount = 1) const;

        UINT GetNodeMask() const;


        bool IsCrossAdapterTextureSupported() const;

        void SharedFence(ComPtr<ID3D12Fence>& primaryFence, const std::shared_ptr<GDevice>& sharedDevice,
                         ComPtr<ID3D12Fence>& sharedFence, UINT64 fenceValue = 0,
                         const SECURITY_ATTRIBUTES* attributes = nullptr,
                         DWORD access = GENERIC_ALL, LPCWSTR name = L"") const;

        /// Non-throwing variant for SharedFence; clears output pointers on failure.
        bool TrySharedFence(ComPtr<ID3D12Fence>& primaryFence, const std::shared_ptr<GDevice>& sharedDevice,
                            ComPtr<ID3D12Fence>& sharedFence, UINT64 fenceValue = 0,
                            const SECURITY_ATTRIBUTES* attributes = nullptr,
                            DWORD access = GENERIC_ALL, LPCWSTR name = L"") const;


        std::shared_ptr<GCommandQueue>& GetCommandQueue(GQueueType type = GQueueType::Graphics);

        UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

        void Flush() const;

        void TerminatedQueuesWorker() const;

        ComPtr<ID3D12Device> GetDXDevice() const;

        std::wstring GetName() const
        {
            return name;
        }

        bool TryGetAdapterDesc3(DXGI_ADAPTER_DESC3& outDesc) const;
    };
}
