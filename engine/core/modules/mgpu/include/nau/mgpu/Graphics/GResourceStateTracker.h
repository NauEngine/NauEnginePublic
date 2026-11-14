#pragma once
#include <map>
#include <mutex>
#include <ResourceUploadBatch.h>
#include <wrl/client.h>


#include "nau/mgpu/Allocator/MemoryAllocator.h"

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;

    class GCommandList;
    class GResource;

    using namespace Microsoft::WRL;

    class GResourceStateTracker

    {
    public:
        GResourceStateTracker();
        virtual ~GResourceStateTracker();

        static bool TryGetCurrentState(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES& currentState);

        static void AddCurrentState(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES currentState,
                                    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);

        void ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier);
        void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateAfter,
                                UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        void TransitionResource(const GResource& resource, D3D12_RESOURCE_STATES stateAfter,
                                UINT subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);


        void UAVBarrier(const GResource* resource = nullptr);


        void AliasBarrier(const GResource* resourceBefore = nullptr, const GResource* resourceAfter = nullptr);


        uint32_t FlushPendingResourceBarriers(const ComPtr<ID3D12GraphicsCommandList2>& commandList);


        void FlushResourceBarriers(const ComPtr<ID3D12GraphicsCommandList2>& commandList);


        void CommitFinalResourceStates();


        void Reset();


        static void Lock();


        static void Unlock();


        static void AddGlobalResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);


        static void RemoveGlobalResourceState(ID3D12Resource* resource);

    private:
        using ResourceBarriers = std::vector<D3D12_RESOURCE_BARRIER>;

        // Pending resource transitions are committed before a command list
        // is executed on the command queue. This guarantees that resources will
        // be in the expected state at the beginning of a command list.
        ResourceBarriers pendingResourceBarriers;

        // GResource barriers that need to be committed to the command list.
        ResourceBarriers resourceBarriers;

        // Tracks the state of a particular resource and all of its subresources.
        struct ResourceState
        {
            // Initialize all of the subresources within a resource to the given state.
            explicit ResourceState(const D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON)
                : State(state)
            {
            }

            // Set a subresource to a particular state.
            void SetSubresourceState(const UINT subresource, const D3D12_RESOURCE_STATES state)
            {
                if (subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES)
                {
                    State = state;
                    SubresourceState.clear();
                }
                else
                {
                    SubresourceState[subresource] = state;
                }
            }

            D3D12_RESOURCE_STATES GetSubresourceState(const UINT subresource) const
            {
                D3D12_RESOURCE_STATES state = State;
                const auto iter = SubresourceState.find(subresource);
                if (iter != SubresourceState.end())
                {
                    state = iter->second;
                }
                return state;
            }

            D3D12_RESOURCE_STATES State;
            std::map<UINT, D3D12_RESOURCE_STATES> SubresourceState;
        };

        using ResourceStateMap = std::unordered_map<ID3D12Resource*, ResourceState>;

        // The final (last known state) of the resources within a command list.
        // The final resource state is committed to the global resource state when the 
        // command list is closed but before it is executed on the command queue.
        ResourceStateMap finalResourceState;

        // The global resource state array (map) stores the state of a resource
        // between command list execution.
        static ResourceStateMap globalResourceState;

        // The mutex protects shared access to the GlobalResourceState map.
        static std::mutex globalMutex;
        static bool isLocked;
    };
}
