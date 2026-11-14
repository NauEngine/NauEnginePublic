#include "nau/mgpu/Graphics/GCommandQueue.h"


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GCommandList.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GResource.h"
#include "nau/mgpu/Graphics/GResourceStateTracker.h"
#include "pix3.h"

namespace nau::PEPEngine::Graphics
{
    GCommandQueue::GCommandQueue(const std::shared_ptr<GDevice>& device, const D3D12_COMMAND_LIST_TYPE type) :
        type(type)
        , device(device)
        , FenceValue(0), IsExecutorAlive(true)
    {
        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = type;
        desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        desc.NodeMask = device->GetNodeMask();

        ThrowIfFailed(device->GetDXDevice()->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)));

        ThrowIfFailed(device->GetDXDevice()->CreateFence(FenceValue, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&fence)));

        switch (this->type)
        {
        case D3D12_COMMAND_LIST_TYPE_COPY:
            commandQueue->SetName(L"Copy Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_COMPUTE:
            commandQueue->SetName(L"Compute Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_DIRECT:
            commandQueue->SetName(L"Direct Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_BUNDLE:
            commandQueue->SetName(L"Bundle Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_VIDEO_DECODE:
            commandQueue->SetName(L"VDecode Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_VIDEO_PROCESS:
            commandQueue->SetName(L"VProcess Command Queue");
            break;
        case D3D12_COMMAND_LIST_TYPE_VIDEO_ENCODE:
            commandQueue->SetName(L"VEncode Command Queue");
            break;
        }

        GetTimestampFreq();

        // Two timestamps for each frame.
        constexpr UINT resultCount = 2 * globalCountFrameResources;
        constexpr UINT resultBufferSize = resultCount * sizeof(UINT64);


        timestampResultBuffer = Lazy<GResource>([resultBufferSize, this]
        {
            return GResource(this->device, CD3DX12_RESOURCE_DESC::Buffer(resultBufferSize),
                             this->device->GetName() + L" TimestampBuffer", nullptr, D3D12_RESOURCE_STATE_COPY_DEST,
                             CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK));
        });

        timestampQueryHeap = Lazy<ComPtr<ID3D12QueryHeap>>([this, resultCount]
        {
            D3D12_QUERY_HEAP_DESC timestampHeapDesc = {};
            timestampHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            timestampHeapDesc.Count = resultCount;
            timestampHeapDesc.NodeMask = this->device->GetNodeMask();

            ComPtr<ID3D12QueryHeap> heap;
            ThrowIfFailed(this->device->GetDXDevice()->CreateQueryHeap(&timestampHeapDesc, IID_PPV_ARGS(&heap)));
            return heap;
        });


        CommandListExecutorThread = std::thread(&GCommandQueue::ProccessInFlightCommandLists, this);
    }

    UINT64 GCommandQueue::GetTimestamp(const UINT index)
    {
        if (type != D3D12_COMMAND_LIST_TYPE_DIRECT && type != D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            return 0;
        }

        readRange.Begin = 2 * index * sizeof(UINT64);
        readRange.End = readRange.Begin + 2 * sizeof(UINT64);

        (timestampResultBuffer.value().GetD3D12Resource()->Map(0, &readRange, &mappedData));

        const UINT64* pTimestamps = reinterpret_cast<UINT64*>(static_cast<UINT8*>(mappedData) + readRange.Begin);
        const UINT64 timeStampDelta = pTimestamps[1] - pTimestamps[0];

        // Unmap with an empty range (written range).
        timestampResultBuffer.value().GetD3D12Resource()->Unmap(0, &emptyRange);

        // Calculate the GPU execution time in microseconds.
        return (timeStampDelta * 1000000) / GetTimestampFreq();
    }

    ComPtr<ID3D12Fence> GCommandQueue::GetFence() const
    {
        return fence;
    }


    GCommandQueue::~GCommandQueue()
    {
        HardStop();
    }

    uint64_t GCommandQueue::Signal()
    {
        const auto fenceValue = ++FenceValue;
        Signal(fence, fenceValue);
        return fenceValue;
    }

    void GCommandQueue::Signal(const ComPtr<ID3D12Fence>& otherFence, const UINT64 fenceValue) const
    {
        commandQueue->Signal(otherFence.Get(), fenceValue);
    }

    void GCommandQueue::SignalWithNewFenceValue(const UINT64 newFenceValue)
    {
        FenceValue = newFenceValue;
        Signal(fence, FenceValue);
    }

    bool GCommandQueue::IsFinish(const uint64_t fenceValue) const
    {
        return fence->GetCompletedValue() >= fenceValue;
    }

    void GCommandQueue::WaitForFenceValue(const uint64_t fenceValue) const
    {
        if (IsFinish(fenceValue))
            return;

        auto event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
        assert(event && "Failed to create fence event handle.");

        fence->SetEventOnCompletion(fenceValue, event);
        WaitForSingleObject(event, DWORD_MAX);
        CloseHandle(event);
    }

    void GCommandQueue::Flush()
    {
        std::unique_lock<std::mutex> lock(commandQueueExecutorMutex);
        CommandListExecutorCondition.wait(lock, [this] { return InFlightCommandLists.Empty(); });

        WaitForFenceValue(Signal());
    }

    std::shared_ptr<GCommandList> GCommandQueue::GetCommandList()
    {
        std::shared_ptr<GCommandList> commandList;

        if (availableCommandLists.TryPop(commandList))
        {
            return commandList;
        }

        commandList = std::make_shared<GCommandList>(shared_from_this(), this->type);
        createdCommandList.push_back(commandList);
        availableCommandLists.Push(commandList);
        return GetCommandList();
    }

    uint64_t GCommandQueue::ExecuteCommandList(const std::shared_ptr<GCommandList>& commandList)
    {
        return ExecuteCommandLists(&commandList, 1);
    }

    uint64_t GCommandQueue::ExecuteCommandLists(const std::shared_ptr<GCommandList>* commandLists, const size_t size)
    {
        GResourceStateTracker::Lock();

        // Command lists that need to put back on the command list queue.
        std::vector<std::shared_ptr<GCommandList>> toBeQueued;
        toBeQueued.reserve(size * 2); // 2x since each command list will have a pending command list.


        // Command lists that need to be executed.
        std::vector<ID3D12CommandList*> d3d12CommandLists;
        d3d12CommandLists.reserve(size * 2); // 2x since each command list will have a pending command list.

        for (size_t i = 0; i < size; ++i)
        {
            auto commandList = commandLists[i];

            auto pendingCommandList = GetCommandList();

            bool hasPendingBarriers = commandList->Close(pendingCommandList);
            if (pendingCommandList != nullptr)
                pendingCommandList->Close();
            // If there are no pending barriers on the pending command list, there is no reason to 
            // execute an empty command list on the command queue.
            if (hasPendingBarriers)
            {
                d3d12CommandLists.push_back(pendingCommandList->GetGraphicsCommandList().Get());
            }
            d3d12CommandLists.push_back(commandList->GetGraphicsCommandList().Get());

            if (pendingCommandList != nullptr)
                toBeQueued.emplace_back(pendingCommandList);

            toBeQueued.push_back(commandList);
        }

        const UINT count = static_cast<UINT>(d3d12CommandLists.size());
        commandQueue->ExecuteCommandLists(count, d3d12CommandLists.data());
        const uint64_t fenceValue = Signal();

        GResourceStateTracker::Unlock();

        // Queue command lists for reuse.
        for (const auto& commandList : toBeQueued)
        {
            InFlightCommandLists.Push({fenceValue, commandList});
        }

        return fenceValue;
    }

    void GCommandQueue::Wait(const GCommandQueue& other) const
    {
        commandQueue->Wait(other.fence.Get(), other.FenceValue);
    }

    void GCommandQueue::Wait(const std::shared_ptr<GCommandQueue>& other) const
    {
        commandQueue->Wait(other->fence.Get(), other->FenceValue);
    }

    void GCommandQueue::Wait(const ComPtr<ID3D12Fence>& otherFence, const UINT64 otherFenceValue) const
    {
        commandQueue->Wait(otherFence.Get(), otherFenceValue);
    }

    ComPtr<ID3D12CommandQueue>& GCommandQueue::GetD3D12CommandQueue()
    {
        return commandQueue;
    }

    void GCommandQueue::StartPixEvent(const std::wstring& message) const
    {
        PIXBeginEvent(commandQueue.Get(), 0, message.c_str());
    }

    void GCommandQueue::EndPixEvent() const
    {
        PIXEndEvent(commandQueue.Get());
    }

    uint64_t GCommandQueue::GetFenceValue() const
    {
        return FenceValue;
    }

    UINT64 GCommandQueue::GetTimestampFreq()
    {
        if (type != D3D12_COMMAND_LIST_TYPE_DIRECT && type != D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            return 0;
        }

        if (queueTimestampFrequencies == 0)
        {
            (commandQueue->GetTimestampFrequency(&queueTimestampFrequencies));

            if (queueTimestampFrequencies == 0)
            {
                queueTimestampFrequencies = 1;
            }
        }


        return queueTimestampFrequencies;
    }

    void GCommandQueue::HardStop()
    {
        if (IsExecutorAlive)
        {
            IsExecutorAlive = false;
            CommandListExecutorThread.join();
        }
    }


    void GCommandQueue::ProccessInFlightCommandLists()
    {
        std::unique_lock<std::mutex> lock(commandQueueExecutorMutex, std::defer_lock);

        while (IsExecutorAlive)
        {
            CommandListEntry commandListEntry;

            lock.lock();

            while (InFlightCommandLists.TryPop(commandListEntry))
            {
                try
                {
                    auto fenceValue = commandListEntry.fenceValue;
                    auto commandList = commandListEntry.commandList;

                    WaitForFenceValue(fenceValue);

                    commandList->Reset();

                    availableCommandLists.Push(commandList);
                }
                catch (...)
                {
                    commandListEntry.commandList.reset();
                    commandListEntry.commandList = nullptr;
                }
            }


            lock.unlock();
            CommandListExecutorCondition.notify_one();

            std::this_thread::yield();
        }
    }
}
