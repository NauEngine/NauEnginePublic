#include "nau/mgpu/Graphics/GResourceStateTracker.h"

#include <cassert>


#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/GResource.h"

namespace nau::PEPEngine::Graphics
{
    // Static definitions.
    std::mutex GResourceStateTracker::globalMutex;
    bool GResourceStateTracker::isLocked = false;
    GResourceStateTracker::ResourceStateMap GResourceStateTracker::globalResourceState = std::unordered_map<
        ID3D12Resource*, ResourceState>();


    GResourceStateTracker::GResourceStateTracker()
    = default;

    GResourceStateTracker::~GResourceStateTracker()
    = default;

    bool GResourceStateTracker::TryGetCurrentState(const ComPtr<ID3D12Resource>& resource,
                                                   D3D12_RESOURCE_STATES& currentState)
    {
        if (resource == nullptr) return false;

        auto it = globalResourceState.find(resource.Get());
        if (it == globalResourceState.end())
        {
            return false;
        }

        currentState = it->second.State;

        return true;
    }

    void GResourceStateTracker::AddCurrentState(const ComPtr<ID3D12Resource>& resource,
                                                const D3D12_RESOURCE_STATES currentState,
                                                const UINT subresource)
    {
        if (resource == nullptr) return;

        auto it = globalResourceState.find(resource.Get());
        if (it == globalResourceState.end())
        {
            globalResourceState[resource.Get()] = ResourceState(currentState);
            globalResourceState[resource.Get()].SetSubresourceState(subresource, currentState);
            return;
        }

        it->second.State = currentState;
        it->second.SetSubresourceState(subresource, currentState);
    }

    void GResourceStateTracker::ResourceBarrier(const D3D12_RESOURCE_BARRIER& barrier)
    {
        if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
        {
            const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = barrier.Transition;

            // First check if there is already a known "final" state for the given resource.
            // If there is, the resource has been used on the command list before and
            // already has a known state within the command list execution.
            const auto it = finalResourceState.find(transitionBarrier.pResource);
            if (it != finalResourceState.end())
            {
                auto& resourceState = it->second;
                // If the known final state of the resource is different...
                if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
                    !resourceState.SubresourceState.empty())
                {
                    // First transition all of the subresources if they are different than the StateAfter.
                    for (const auto subresourceState : resourceState.SubresourceState)
                    {
                        if (transitionBarrier.StateAfter != subresourceState.second)
                        {
                            D3D12_RESOURCE_BARRIER newBarrier = barrier;
                            newBarrier.Transition.Subresource = subresourceState.first;
                            newBarrier.Transition.StateBefore = subresourceState.second;
                            resourceBarriers.push_back(newBarrier);
                        }
                    }
                }
                else
                {
                    const auto finalState = resourceState.GetSubresourceState(transitionBarrier.Subresource);
                    if (transitionBarrier.StateAfter != finalState)
                    {
                        // Push a new transition barrier with the correct before state.
                        D3D12_RESOURCE_BARRIER newBarrier = barrier;
                        newBarrier.Transition.StateBefore = finalState;
                        resourceBarriers.push_back(newBarrier);
                    }
                }
            }
            else // In this case, the resource is being used on the command list for the first time. 
            {
                pendingResourceBarriers.push_back(barrier);
            }

            // Push the final known state (possibly replacing the previously known state for the subresource).
            if (transitionBarrier.pResource)
            {
                if (finalResourceState.find(transitionBarrier.pResource) == finalResourceState.end())
                {
                    finalResourceState[transitionBarrier.pResource] = ResourceState();
                }

                finalResourceState[transitionBarrier.pResource].SetSubresourceState(
                    transitionBarrier.Subresource, transitionBarrier.StateAfter);
            }
        }
        else
        {
            // Just push non-transition barriers to the resource barriers array.
            resourceBarriers.push_back(barrier);
        }
    }

    void GResourceStateTracker::TransitionResource(ID3D12Resource* resource, const D3D12_RESOURCE_STATES stateAfter,
                                                   const UINT subResource)
    {
        if (resource)
        {
            ResourceBarrier(
                CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COMMON, stateAfter,
                                                     subResource));
        }
    }

    void GResourceStateTracker::TransitionResource(const GResource& resource, const D3D12_RESOURCE_STATES stateAfter,
                                                   const UINT subResource)
    {
        TransitionResource(resource.GetD3D12Resource().Get(), stateAfter, subResource);
    }

    void GResourceStateTracker::UAVBarrier(const GResource* resource)
    {
        auto* const pResource = resource != nullptr ? resource->GetD3D12Resource().Get() : nullptr;

        ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(pResource));
    }

    void GResourceStateTracker::AliasBarrier(const GResource* resourceBefore, const GResource* resourceAfter)
    {
        ID3D12Resource* pResourceBefore = resourceBefore != nullptr
                                              ? resourceBefore->GetD3D12Resource().Get()
                                              : nullptr;
        ID3D12Resource* pResourceAfter = resourceAfter != nullptr
                                             ? resourceAfter->GetD3D12Resource().Get()
                                             : nullptr;

        ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(pResourceBefore, pResourceAfter));
    }

    void GResourceStateTracker::FlushResourceBarriers(const ComPtr<ID3D12GraphicsCommandList2>& commandList)
    {
        FlushPendingResourceBarriers(commandList);


        UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
        if (numBarriers > 0)
        {
            commandList->ResourceBarrier(numBarriers, resourceBarriers.data());
            resourceBarriers.clear();
        }
    }

    uint32_t GResourceStateTracker::FlushPendingResourceBarriers(const ComPtr<ID3D12GraphicsCommandList2>& commandList)
    {
        // Resolve the pending resource barriers by checking the global state of the 
        // (sub)resources. Add barriers if the pending state and the global state do
        //  not match.
        ResourceBarriers resourceBarriers;
        // Reserve enough space (worst-case, all pending barriers).
        resourceBarriers.reserve(pendingResourceBarriers.size());

        for (auto pendingBarrier : pendingResourceBarriers)
        {
            if (pendingBarrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
            // Only transition barriers should be pending...
            {
                auto pendingTransition = pendingBarrier.Transition;

                const auto& iter = globalResourceState.find(pendingTransition.pResource);
                if (iter != globalResourceState.end())
                {
                    // If all subresources are being transitioned, and there are multiple
                    // subresources of the resource that are in a different state...
                    auto& resourceState = iter->second;
                    if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
                        !resourceState.SubresourceState.empty())
                    {
                        // Transition all subresources
                        for (auto subresourceState : resourceState.SubresourceState)
                        {
                            if (pendingTransition.StateAfter != subresourceState.second)
                            {
                                D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
                                newBarrier.Transition.Subresource = subresourceState.first;
                                newBarrier.Transition.StateBefore = subresourceState.second;
                                resourceBarriers.push_back(newBarrier);
                            }
                        }
                    }
                    else
                    {
                        // No (sub)resources need to be transitioned. Just add a single transition barrier (if needed).
                        auto globalState = (iter->second).GetSubresourceState(pendingTransition.Subresource);
                        if (pendingTransition.StateAfter != globalState)
                        {
                            // Fix-up the before state based on current global state of the resource.
                            pendingBarrier.Transition.StateBefore = globalState;
                            resourceBarriers.push_back(pendingBarrier);
                        }
                    }
                }
            }
        }

        UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
        if (numBarriers > 0)
        {
            commandList->ResourceBarrier(numBarriers, resourceBarriers.data());
        }

        pendingResourceBarriers.clear();


        return numBarriers;
    }

    void GResourceStateTracker::CommitFinalResourceStates()
    {
        assert(isLocked);

        // Commit final resource states to the global resource state array (map).
        for (auto&& resourceState : finalResourceState)
        {
            globalResourceState[resourceState.first] = resourceState.second;
        }

        finalResourceState.clear();
    }

    void GResourceStateTracker::Reset()
    {
        // Reset the pending, current, and final resource states.
        try
        {
            pendingResourceBarriers.clear();
            resourceBarriers.clear();
            finalResourceState.erase(nullptr);
            finalResourceState.clear();
        }
        catch (...)
        {
        }
    }

    void GResourceStateTracker::Lock()
    {
        globalMutex.lock();
        isLocked = true;
    }

    void GResourceStateTracker::Unlock()
    {
        globalMutex.unlock();
        isLocked = false;
    }

    void GResourceStateTracker::AddGlobalResourceState(ID3D12Resource* resource, const D3D12_RESOURCE_STATES state)
    {
        if (resource != nullptr)
        {
            std::lock_guard<std::mutex> lock(globalMutex);
            globalResourceState[resource].SetSubresourceState(D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state);
        }
    }

    void GResourceStateTracker::RemoveGlobalResourceState(ID3D12Resource* resource)
    {
        if (resource != nullptr)
        {
            std::lock_guard<std::mutex> lock(globalMutex);
            globalResourceState.erase(resource);
        }
    }
}
