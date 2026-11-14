#include "nau/mgpu/Graphics/GCommandList.h"

#include <pix3.h>

#include "nau/mgpu/Graphics/ComputePSO.h"
#include "nau/mgpu/Graphics/GDataUploader.h"
#include "nau/mgpu/Graphics/GResource.h"
#include "nau/mgpu/Graphics/GResourceStateTracker.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Graphics/GraphicPSO.h"
#include "nau/mgpu/Graphics/GRootSignature.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GTexture.h"
#include "nau/mgpu/Graphics/GDescriptorHeap.h"
#include "nau/mgpu/Graphics/GBuffer.h"
#include "nau/mgpu/Graphics/GRenderTarger.h"

namespace nau::PEPEngine::Graphics
{
    void GCommandList::TrackResource(const ComPtr<ID3D12Object>& object)
    {
        trackedObject.push_back(object);
    }

    void GCommandList::TrackResource(const GResource& res)
    {
        TrackResource(res.GetD3D12Resource());
    }

    std::shared_ptr<GDevice>& GCommandList::GetDevice() const
    {
        return queue->device;
    }

    GCommandList::GCommandList(const std::shared_ptr<GCommandQueue>& queue, const D3D12_COMMAND_LIST_TYPE type)
        : type(type), queue(queue)
    {
        ThrowIfFailed(queue->device->GetDXDevice()->CreateCommandAllocator(type, IID_PPV_ARGS(&cmdAllocator)));

        ThrowIfFailed(
            queue->device->GetDXDevice()->CreateCommandList(queue->device->GetNodeMask(), type, cmdAllocator.Get(),
                nullptr, IID_PPV_ARGS(&cmdList)));

        uploadBuffer = std::make_unique<GDataUploader>(queue->device);

        tracker = std::make_unique<GResourceStateTracker>();

        for (int i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            cachedDescriptorHeaps[i] = nullptr;
        }
    }

    void GCommandList::BeginQuery(ID3D12QueryHeap* QueryHeap, UINT index) const
    {
        cmdList->BeginQuery(QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void GCommandList::EndQuery(ID3D12QueryHeap* QueryHeap, const UINT index) const
    {
        cmdList->EndQuery(QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, index);
    }

    void GCommandList::ResolveQuery(ID3D12QueryHeap* QueryHeap, ID3D12Resource* TimeStampResource, const UINT index,
                                    const UINT quriesCount, const UINT64 aligned) const
    {
        cmdList->ResolveQueryData(QueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, index,
                                  quriesCount, TimeStampResource, aligned);
    }

    void GCommandList::BeginQuery(const UINT index) const
    {
        BeginQuery(queue->timestampQueryHeap.value().Get(), index);
    }

    void GCommandList::EndQuery(const UINT index) const
    {
        EndQuery(queue->timestampQueryHeap.value().Get(), index);
    }

    void GCommandList::ResolveQuery(const UINT index, const UINT quriesCount, const UINT64 aligned) const
    {
        ResolveQuery(queue->timestampQueryHeap.value().Get(),
                     queue->timestampResultBuffer.value().GetD3D12Resource().Get(), index,
                     quriesCount, aligned);
    }


    D3D12_COMMAND_LIST_TYPE GCommandList::GetCommandListType() const
    {
        return type;
    }

    ComPtr<ID3D12GraphicsCommandList2> GCommandList::GetGraphicsCommandList() const
    {
        return cmdList;
    }

    void GCommandList::SetDescriptorsHeap(const GDescriptor* memory)
    {
        if (memory == nullptr || memory->IsNull())
            assert(memory == nullptr || memory->IsNull() && "Memory Descriptor Heap is null");

        const auto type = memory->GetType();
        const auto heap = memory->GetDescriptorHeap()->GetDirectxHeap();

        if (cachedDescriptorHeaps[type] != heap)
        {
            cachedDescriptorHeaps[type] = heap;
            UpdateDescriptorHeaps();
        }
    }

    void GCommandList::UpdateDescriptorHeaps() const
    {
        UINT numDescriptorHeaps = 0;
        ID3D12DescriptorHeap* descriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

        for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            if (ID3D12DescriptorHeap* descriptorHeap = cachedDescriptorHeaps[i])
            {
                descriptorHeaps[numDescriptorHeaps++] = descriptorHeap;
            }
        }

        cmdList->SetDescriptorHeaps(numDescriptorHeaps, descriptorHeaps);
    }

    void GCommandList::SetComputeRootShaderResourceView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                        const UINT offset)
    {
        assert(resource.IsValid() && "Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetComputeRootShaderResourceView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetGraphicsRootShaderResourceView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                         const UINT offset)
    {
        assert(resource.IsValid() && "Resource is invalid");
        assert(type == D3D12_COMMAND_LIST_TYPE_DIRECT);

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetGraphicsRootShaderResourceView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetRootShaderResourceView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                 const UINT offset)
    {
        //assert(cachedRootSignature != nullptr && "Root Signature not set into the CommandList");

        assert(resource.IsValid() && "Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRootShaderResourceView(rootSignatureSlot, res);
        }
        else
        {
            cmdList->SetGraphicsRootShaderResourceView(rootSignatureSlot, res);
        }

        TrackResource(resource);
    }

    void GCommandList::SetComputeRootConstantBufferView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                        const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetComputeRootConstantBufferView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetGraphicsRootConstantBufferView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                         const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetGraphicsRootConstantBufferView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetRootConstantBufferView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                 const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRootConstantBufferView(rootSignatureSlot, res);
        }
        else
        {
            cmdList->SetGraphicsRootConstantBufferView(rootSignatureSlot, res);
        }

        TrackResource(resource);
    }

    void GCommandList::SetComputeRootUnorderedAccessView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                         const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetComputeRootUnorderedAccessView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetGraphicsRootUnorderedAccessView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                          const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        cmdList->SetGraphicsRootUnorderedAccessView(rootSignatureSlot, res);

        TrackResource(resource);
    }

    void GCommandList::SetRootUnorderedAccessView(const UINT rootSignatureSlot, const GBuffer& resource,
                                                  const UINT offset)
    {
        if (cachedRootSignature == nullptr)
            assert("Root Signature not set into the CommandList");

        if (!resource.IsValid())
            assert("Resource is invalid");

        const auto res = resource.GetElementResourceAddress(offset);

        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRootUnorderedAccessView(rootSignatureSlot, res);
        }
        else
        {
            cmdList->SetGraphicsRootUnorderedAccessView(rootSignatureSlot, res);
        }

        TrackResource(resource);
    }

    void GCommandList::SetComputeRoot32BitConstants(const UINT rootSignatureSlot, const UINT Count32BitValueToSet,
                                                    const void* data,
                                                    const UINT DestOffsetIn32BitValueToSet) const
    {
        cmdList->SetComputeRoot32BitConstants(rootSignatureSlot, Count32BitValueToSet, data,
                                              DestOffsetIn32BitValueToSet);
    }

    void GCommandList::SetGraphicsRoot32BitConstants(const UINT rootSignatureSlot, const UINT Count32BitValueToSet,
                                                     const void* data,
                                                     const UINT DestOffsetIn32BitValueToSet) const
    {
        cmdList->SetGraphicsRoot32BitConstants(rootSignatureSlot, Count32BitValueToSet, data,
                                               DestOffsetIn32BitValueToSet);
    }

    void GCommandList::SetRoot32BitConstants(const UINT rootSignatureSlot, const UINT Count32BitValueToSet,
                                             const void* data,
                                             const UINT DestOffsetIn32BitValueToSet) const
    {
        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRoot32BitConstants(rootSignatureSlot, Count32BitValueToSet, data,
                                                  DestOffsetIn32BitValueToSet);
        }
        else
        {
            cmdList->SetGraphicsRoot32BitConstants(rootSignatureSlot, Count32BitValueToSet, data,
                                                   DestOffsetIn32BitValueToSet);
        }
    }

    void GCommandList::SetComputeRoot32BitConstant(const UINT shaderRegister, const UINT value, const UINT offset) const
    {
        cmdList->SetComputeRoot32BitConstant(shaderRegister, value, offset);
    }

    void GCommandList::SetGraphicsRoot32BitConstant(const UINT shaderRegister, const UINT value,
                                                    const UINT offset) const
    {
        cmdList->SetGraphicsRoot32BitConstant(shaderRegister, value, offset);
    }

    void GCommandList::SetRoot32BitConstant(const UINT shaderRegister, const UINT value, const UINT offset) const
    {
        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRoot32BitConstant(shaderRegister, value, offset);
        }
        else
        {
            cmdList->SetGraphicsRoot32BitConstant(shaderRegister, value, offset);
        }
    }

    void GCommandList::SetComputeRootDescriptorTable(const UINT rootSignatureSlot, const GDescriptor* memory,
                                                     const UINT offset) const
    {
        if (memory == nullptr || memory->IsNull())
        {
            assert("Memory null");
        }

        cmdList->SetComputeRootDescriptorTable(rootSignatureSlot, memory->GetGPUHandle(offset));
    }

    void GCommandList::SetGraphicsRootDescriptorTable(const UINT rootSignatureSlot, const GDescriptor* memory,
                                                      const UINT offset) const
    {
        if (memory == nullptr || memory->IsNull())
        {
            assert("Memory null");
        }

        cmdList->SetGraphicsRootDescriptorTable(rootSignatureSlot, memory->GetGPUHandle(offset));
    }

    void GCommandList::SetRootDescriptorTable(const UINT rootSignatureSlot, const GDescriptor* memory,
                                              const size_t offset) const
    {
        if (memory == nullptr || memory->IsNull())
        {
            assert("Memory null");
        }

        if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
        {
            cmdList->SetComputeRootDescriptorTable(rootSignatureSlot, memory->GetGPUHandle(offset));
        }
        else
        {
            cmdList->SetGraphicsRootDescriptorTable(rootSignatureSlot, memory->GetGPUHandle(offset));
        }
    }

    UploadAllocation GCommandList::UploadData(const size_t sizeInBytes, const void* bufferData,
                                              const size_t alignment = 0) const
    {
        const auto allocation = uploadBuffer->Allocate(sizeInBytes, alignment);
        if (bufferData != nullptr)
            memcpy(allocation.CPU, bufferData, sizeInBytes);
        return allocation;
    }

    void GCommandList::UpdateSubresource(const GResource& destResource, const D3D12_SUBRESOURCE_DATA* subresources,
                                         const size_t countSubresources)
    {
        UpdateSubresource(destResource.GetD3D12Resource(), subresources, countSubresources);
    }

    void GCommandList::UpdateSubresource(const ComPtr<ID3D12Resource>& destResource,
                                         const D3D12_SUBRESOURCE_DATA* subresources,
                                         const size_t countSubresources)
    {
        const auto res = destResource;

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(res.Get(),
                                                                     0, static_cast<UINT>(countSubresources));

        auto upload = UploadData(uploadBufferSize, nullptr, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        tracker->TransitionResource(res.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
        FlushResourceBarriers();

        UpdateSubresources(cmdList.Get(),
                           res.Get(), &upload.d3d12Resource,
                            upload.Offset, 0, static_cast<UINT>(countSubresources),
                           subresources);

        TrackResource(res.Get());
    }

    void GCommandList::StartMark(const std::wstring& message) const
    {
        PIXBeginEvent(cmdList.Get(), 0, message.c_str());
    }

    void GCommandList::EndMark() const
    {
        PIXEndEvent(cmdList.Get());
    }

    void GCommandList::SetViewports(const D3D12_VIEWPORT* viewports, const size_t count) const
    {
        assert(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        cmdList->RSSetViewports(static_cast<UINT>(count), viewports);
    }

    void GCommandList::SetScissorRects(const D3D12_RECT* scissorRects, const size_t count) const
    {
        assert(count < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
        cmdList->RSSetScissorRects(static_cast<UINT>(count), scissorRects);
    }

    void GCommandList::SetComputeRootSignature(const GRootSignature& rs)
    {
        const auto d3d12RootSignature = rs.GetNativeSignature();
        if (cachedRootSignature != d3d12RootSignature)
        {
            cachedRootSignature = d3d12RootSignature;
            cmdList->SetComputeRootSignature(cachedRootSignature.Get());
            TrackResource(cachedRootSignature);
        }
    }

    void GCommandList::SetGraphicsRootSignature(const GRootSignature& rs)
    {
        const auto d3d12RootSignature = rs.GetNativeSignature();
        if (cachedRootSignature != d3d12RootSignature)
        {
            cachedRootSignature = d3d12RootSignature;
            cmdList->SetGraphicsRootSignature(cachedRootSignature.Get());
            TrackResource(cachedRootSignature);
        }
    }

    void GCommandList::SetRootSignature(const GRootSignature& rs)
    {
        const auto d3d12RootSignature = rs.GetNativeSignature();
        if (cachedRootSignature != d3d12RootSignature)
        {
            cachedRootSignature = d3d12RootSignature;
            if (type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
                cmdList->SetComputeRootSignature(cachedRootSignature.Get());
            else
                cmdList->SetGraphicsRootSignature(cachedRootSignature.Get());

            TrackResource(cachedRootSignature);
        }
    }

    void GCommandList::SetPipelineState(const GraphicPSO& pso)
    {
        const auto pipeState = pso.GetPSO();

        if (pipeState != cachedPSO)
        {
            cachedPSO = pipeState;
            cmdList->SetPipelineState(cachedPSO.Get());
            TrackResource(pipeState);
        }

        const auto d3d12RootSignature = pso.GetRootSignature().GetNativeSignature();
        if (cachedRootSignature != d3d12RootSignature)
        {
            cachedRootSignature = d3d12RootSignature;
            cmdList->SetGraphicsRootSignature(cachedRootSignature.Get());
            TrackResource(cachedRootSignature);
        }
    }

    void GCommandList::SetPipelineState(const ComputePSO& pso)
    {
        const auto pipeState = pso.GetPSO();

        if (pipeState != cachedPSO)
        {
            cachedPSO = pipeState;
            cmdList->SetPipelineState(cachedPSO.Get());
            TrackResource(pipeState);
        }

        const auto d3d12RootSignature = pso.GetRootSignature().GetNativeSignature();
        if (cachedRootSignature != d3d12RootSignature)
        {
            cachedRootSignature = d3d12RootSignature;
            cmdList->SetComputeRootSignature(cachedRootSignature.Get());
            TrackResource(cachedRootSignature);
        }
    }

    void GCommandList::SetVBuffer(const UINT slot, const UINT count, const D3D12_VERTEX_BUFFER_VIEW* views) const
    {
        cmdList->IASetVertexBuffers(slot, count, views);
    }

    void GCommandList::SetIBuffer(const D3D12_INDEX_BUFFER_VIEW* views) const
    {
        cmdList->IASetIndexBuffer(views);
    }


    void GCommandList::TransitionBarrier(const ComPtr<ID3D12Resource>& resource, const D3D12_RESOURCE_STATES stateAfter,
                                         const UINT subresource, const bool flushBarriers) const
    {
        if (resource)
        {
            const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COMMON,
                                                                      stateAfter, subresource);
            tracker->ResourceBarrier(barrier);
        }

        if (flushBarriers)
        {
            FlushResourceBarriers();
        }
    }

    void GCommandList::TransitionBarrier(const GRenderTexture* render, const D3D12_RESOURCE_STATES stateAfter,
                                         const UINT subresource,
                                         const bool flushBarriers) const
    {
        assert(render != nullptr);
        TransitionBarrier(render->GetRenderTexture()->GetD3D12Resource(), stateAfter, subresource, flushBarriers);
    }

    void GCommandList::UAVBarrier(const GResource& resource, const bool flushBarriers) const
    {
        UAVBarrier(resource.GetD3D12Resource(), flushBarriers);
    }

    void GCommandList::UAVBarrier(const ComPtr<ID3D12Resource>& resource, const bool flushBarriers) const
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.Get());

        tracker->ResourceBarrier(barrier);

        if (flushBarriers)
        {
            FlushResourceBarriers();
        }
    }

    void GCommandList::AliasingBarrier(const GResource& beforeResource, const GResource& afterResource,
                                       const bool flushBarriers) const
    {
        AliasingBarrier(beforeResource.GetD3D12Resource(), afterResource.GetD3D12Resource(), flushBarriers);
    }

    void GCommandList::AliasingBarrier(const ComPtr<ID3D12Resource>& beforeResource,
                                       const ComPtr<ID3D12Resource>& afterResource, const bool flushBarriers) const
    {
        const auto barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(beforeResource.Get(), afterResource.Get());

        tracker->ResourceBarrier(barrier);

        if (flushBarriers)
        {
            FlushResourceBarriers();
        }
    }

    void GCommandList::FlushResourceBarriers() const
    {
        tracker->FlushResourceBarriers(cmdList);
    }

    void GCommandList::TransitionBarrier(const GResource& resource, const D3D12_RESOURCE_STATES stateAfter,
                                         const UINT subresource,
                                         const bool flushBarriers) const
    {
        TransitionBarrier(resource.GetD3D12Resource(), stateAfter, subresource, flushBarriers);
    }

    void GCommandList::CopyTextureRegion(const ComPtr<ID3D12Resource>& dstRes, const UINT DstX,
                                         const UINT DstY,
                                         const UINT DstZ, const ComPtr<ID3D12Resource>& srcRes, const D3D12_BOX* srcBox)
    {
        TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushResourceBarriers();

        const auto& Dest = CD3DX12_TEXTURE_COPY_LOCATION(dstRes.Get());
        const auto& Src = CD3DX12_TEXTURE_COPY_LOCATION(srcRes.Get());
        cmdList->CopyTextureRegion(&Dest, DstX, DstY, DstZ,
                                   &Src, srcBox);

        TrackResource(dstRes.Get());
        TrackResource(srcRes.Get());
    }

    void GCommandList::CopyCounter(const ComPtr<ID3D12Resource>& dest_resource, const UINT dest_offset,
                                   const ComPtr<ID3D12Resource>& source_resource) const
    {
        TransitionBarrier(dest_resource, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(source_resource, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushResourceBarriers();
        cmdList->CopyBufferRegion(dest_resource.Get(), dest_offset, source_resource.Get(), 0, 4);
    }

    void GCommandList::CopyTextureRegion(const GResource& dstRes, const UINT DstX,
                                         const UINT DstY,
                                         const UINT DstZ, const GResource& srcRes, const D3D12_BOX* srcBox)
    {
        CopyTextureRegion(dstRes.GetD3D12Resource(), DstX, DstY, DstZ, srcRes.GetD3D12Resource(), srcBox);
    }

    void GCommandList::CopyBufferRegion(const GBuffer& dstRes, const UINT DstOffset,
                                        const GBuffer& srcRes, const UINT SrcOffset, const UINT numBytes,
                                        const bool copyBarier)
    {
        CopyBufferRegion(dstRes.GetD3D12Resource(), DstOffset, srcRes.GetD3D12Resource(), SrcOffset, numBytes,
                         copyBarier);
    }

    void GCommandList::CopyBufferRegion(const ComPtr<ID3D12Resource>& dstRes, const UINT DstOffset,
                                        const ComPtr<ID3D12Resource>& srcRes, const UINT SrcOffset, const UINT numBytes,
                                        const bool copyBarier)
    {
        if (copyBarier)
        {
            TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
            TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);
            FlushResourceBarriers();
        }
        cmdList->CopyBufferRegion(dstRes.Get(), DstOffset, srcRes.Get(), SrcOffset, numBytes);
        TrackResource(dstRes.Get());
        TrackResource(srcRes.Get());
    }

    void GCommandList::CopyResource(const ComPtr<ID3D12Resource>& dstRes, const ComPtr<ID3D12Resource>& srcRes)
    {
        TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_COPY_SOURCE);

        FlushResourceBarriers();

        cmdList->CopyResource(dstRes.Get(), srcRes.Get());

        TrackResource(dstRes.Get());
        TrackResource(srcRes.Get());
    }

    void GCommandList::CopyResource(const GResource& dstRes, const GResource& srcRes)
    {
        CopyResource(dstRes.GetD3D12Resource(), srcRes.GetD3D12Resource());
    }

    void GCommandList::CopyResourceToCubeMap(const GResource& dstCube, const GResource& srcTex, UINT faceIndex)
    {
        CopyResourceToCubeMap(dstCube.GetD3D12Resource(), srcTex.GetD3D12Resource(), faceIndex);
    }

    void GCommandList::CopyResourceToCubeMap(const ComPtr<ID3D12Resource>& dstCube,
                                             const ComPtr<ID3D12Resource>& srcTex,
                                             UINT faceIndex)
    {
        assert(cmdList);
        assert(dstCube);
        assert(srcTex);
        assert(faceIndex < 6);

        TransitionBarrier(dstCube, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(srcTex, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushResourceBarriers();


        auto dstLoc = CD3DX12_TEXTURE_COPY_LOCATION(dstCube.Get());
        auto srcLoc = CD3DX12_TEXTURE_COPY_LOCATION(srcTex.Get());
        
        
        dstLoc.SubresourceIndex = D3D12CalcSubresource(
            0, // mip slice
            faceIndex, // array slice = face cubemap
            0, // plane slice
            1, // mip levels
            6 // array size
        );

        
        srcLoc.SubresourceIndex = D3D12CalcSubresource(
            0, // mip slice
            0, // array slice
            0, // plane slice
            1, // mip levels
            1 // array size
        );

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        TrackResource(dstCube);
        TrackResource(srcTex);
    }

    void GCommandList::CopyResourceFromCubeMap(const GResource& dstTex, const GResource& srcCube, UINT faceIndex)
    {
        CopyResourceFromCubeMap(dstTex.GetD3D12Resource(), srcCube.GetD3D12Resource(), faceIndex);
    }

    void GCommandList::CopyCubeMapFace(const GResource& dstCube, const GResource& srcCube, const UINT faceIndex)
    {
        assert(cmdList);
        assert(faceIndex < 6);

        const auto dstResource = dstCube.GetD3D12Resource();
        const auto srcResource = srcCube.GetD3D12Resource();
        assert(dstResource && srcResource);

        TransitionBarrier(dstResource, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(srcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushResourceBarriers();

        const UINT subresource = D3D12CalcSubresource(0, faceIndex, 0, 1, 6);
        const auto dstLocation = CD3DX12_TEXTURE_COPY_LOCATION(dstResource.Get(), subresource);
        const auto srcLocation = CD3DX12_TEXTURE_COPY_LOCATION(srcResource.Get(), subresource);
        cmdList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

        TrackResource(dstResource);
        TrackResource(srcResource);
    }

    void GCommandList::CopyResourceFromCubeMap(const ComPtr<ID3D12Resource>& dstTex,
                                               const ComPtr<ID3D12Resource>& srcCube,
                                               UINT faceIndex)
    {
        assert(cmdList);
        assert(srcCube);
        assert(dstTex);
        assert(faceIndex < 6);

        TransitionBarrier(dstTex, D3D12_RESOURCE_STATE_COPY_DEST);
        TransitionBarrier(srcCube, D3D12_RESOURCE_STATE_COPY_SOURCE);
        FlushResourceBarriers();


        auto dstLoc = CD3DX12_TEXTURE_COPY_LOCATION(dstTex.Get());
        auto srcLoc = CD3DX12_TEXTURE_COPY_LOCATION(srcCube.Get());
        
        dstLoc.SubresourceIndex = D3D12CalcSubresource(
            0, // mip slice
            0, // array slice
            0, // plane slice
            1, // mip levels
            1 // destination is a 2D texture, not a cubemap
        );
        
        srcLoc.SubresourceIndex = D3D12CalcSubresource(
            0, // mip slice
            faceIndex, // array slice
            0, // plane slice
            1, // mip levels
            6 // source is a six-face cubemap
        );

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

        TrackResource(dstTex);
        TrackResource(srcCube);
    }

    void GCommandList::ResolveSubresource(const GResource& dstRes, const GResource& srcRes,
                                          const uint32_t dstSubresource,
                                          const uint32_t srcSubresource)
    {
        TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_RESOLVE_DEST, dstSubresource);
        TransitionBarrier(srcRes, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, srcSubresource);

        FlushResourceBarriers();

        cmdList->ResolveSubresource(dstRes.GetD3D12Resource().Get(), dstSubresource,
                                    srcRes.GetD3D12Resource().Get(),
                                    srcSubresource, dstRes.GetD3D12ResourceDesc().Format);

        TrackResource(srcRes);
        TrackResource(dstRes);
    }

    void GCommandList::Draw(const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t startVertex,
                            const uint32_t startInstance) const
    {
        FlushResourceBarriers();

        cmdList->DrawInstanced(vertexCount, instanceCount, startVertex, startInstance);
    }

    void GCommandList::DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount, const uint32_t startIndex,
                                   const int32_t baseVertex,
                                   const uint32_t startInstance) const
    {
        FlushResourceBarriers();

        cmdList->DrawIndexedInstanced(indexCount, instanceCount, startIndex, baseVertex, startInstance);
    }

    void GCommandList::Dispatch(const uint32_t numGroupsX, const uint32_t numGroupsY, const uint32_t numGroupsZ) const
    {
        FlushResourceBarriers();
        cmdList->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
    }

    void GCommandList::ReleaseTrackedObjects()
    {
        trackedObject.clear();
    }

    void GCommandList::Reset(const GraphicPSO* pso)
    {
        ThrowIfFailed(cmdAllocator->Reset());

        tracker->Reset();
        uploadBuffer->Reset();

        ReleaseTrackedObjects();

        cachedRootSignature = nullptr;
        cachedPSO = nullptr;
        cachedPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

        for (auto& cachedDescriptorHeap : cachedDescriptorHeaps)
        {
            cachedDescriptorHeap = nullptr;
        }

        if (pso != nullptr)
        {
            cachedPSO = pso->GetPSO();
            TrackResource(cachedPSO);
        }

        ThrowIfFailed(cmdList->Reset(cmdAllocator.Get(), cachedPSO.Get()));
    }

    bool GCommandList::Close(const std::shared_ptr<GCommandList>& pendingCommandList) const
    {
        // Flush any remaining barriers.
        FlushResourceBarriers();

        cmdList->Close();

        uint32_t numPendingBarriers = 0;

        if (pendingCommandList != nullptr)
        {
            const auto peddingCmdList = pendingCommandList->GetGraphicsCommandList();

            // Flush pending resource barriers.
            numPendingBarriers = tracker->FlushPendingResourceBarriers(peddingCmdList);
        }
        // Commit the final resource state to the global state.
        tracker->CommitFinalResourceStates();

        return numPendingBarriers > 0;
    }

    void GCommandList::Close() const
    {
        FlushResourceBarriers();
        cmdList->Close();
    }

    void GCommandList::SetPrimitiveTopology(const D3D_PRIMITIVE_TOPOLOGY primitiveTopology)
    {
        if (cachedPrimitiveTopology == primitiveTopology) return;

        cachedPrimitiveTopology = primitiveTopology;
        cmdList->IASetPrimitiveTopology(cachedPrimitiveTopology);
    }

    void GCommandList::ClearRenderTarget(const GDescriptor* memory, const size_t offset, const FLOAT rgba[4],
                                         const D3D12_RECT* rects,
                                         const size_t rectCount) const
    {
        if (memory == nullptr || memory->IsNull())
        {
            assert("Bad Clear Render Target");
        }
        cmdList->ClearRenderTargetView(memory->GetCPUHandle(static_cast<uint32_t>(offset)), rgba,
                                       static_cast<UINT>(rectCount), rects);
    }

    void GCommandList::ClearRenderTarget(const GRenderTexture& target, const FLOAT rgba[4], const D3D12_RECT* rects,
                                         const size_t rectCount) const
    {
        cmdList->ClearRenderTargetView(target.GetRTVCpu(), rgba, static_cast<UINT>(rectCount), rects);
    }

    void GCommandList::SetRenderTargets(const size_t RTCount, const GDescriptor* rtvMemory, const size_t rtvOffset,
                                        const GDescriptor* dsvMemory,
                                        const size_t dsvOffset) const
    {
        const auto* rtvPtr = (rtvMemory == nullptr || rtvMemory->IsNull())
                                 ? nullptr
                                 : &rtvMemory->GetCPUHandle(static_cast<uint32_t>(rtvOffset));

        const auto* dsvPtr = (dsvMemory == nullptr || dsvMemory->IsNull())
                                 ? nullptr
                                 : &dsvMemory->GetCPUHandle(static_cast<uint32_t>(dsvOffset));

        cmdList->OMSetRenderTargets(static_cast<UINT>(RTCount), rtvPtr, true, dsvPtr);
    }

    void GCommandList::SetRenderTarget(const GRenderTexture& target, const GDescriptor* dsvMemory,
                                       const size_t dsvOffset) const
    {
        auto* dsvPtr = (dsvMemory == nullptr || dsvMemory->IsNull())
                           ? nullptr
                           : &dsvMemory->GetCPUHandle(static_cast<uint32_t>(dsvOffset));

        cmdList->OMSetRenderTargets(1, &target.GetRTVCpu(), true, dsvPtr);
    }

    void GCommandList::SetRenderTargets(const GRenderTexture* targets, const UINT targetsSize,
                                        const GDescriptor* dsvMemory,
                                        const size_t dsvOffset, const BOOL isSingleHandle) const
    {
        assert(targetsSize != 0);

        auto* dsvPtr = (dsvMemory == nullptr || dsvMemory->IsNull())
                           ? nullptr
                           : &dsvMemory->GetCPUHandle(static_cast<uint32_t>(dsvOffset));

        if (isSingleHandle)
        {
            cmdList->OMSetRenderTargets(targetsSize, &targets[0].GetRTVCpu(), true, dsvPtr);
        }
        else
        {
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handlers;
            handlers.reserve(targetsSize);

            for (size_t i = 0; i < targetsSize; ++i)
            {
                handlers.push_back(targets[i].GetRTVCpu());
            }

            cmdList->OMSetRenderTargets(static_cast<UINT>(handlers.size()), handlers.data(), false, dsvPtr);
        }
    }

    void GCommandList::ClearDepthStencil(const GDescriptor* dsvMemory, const size_t dsvOffset,
                                         const D3D12_CLEAR_FLAGS flags,
                                         const FLOAT depthValue,
                                         const UINT stencilValue, const D3D12_RECT* rects, const size_t rectCount) const
    {
        cmdList->ClearDepthStencilView(dsvMemory->GetCPUHandle(static_cast<uint32_t>(dsvOffset)), flags, depthValue, stencilValue,
                                       static_cast<UINT>(rectCount),
                                       rects);
    }
}
