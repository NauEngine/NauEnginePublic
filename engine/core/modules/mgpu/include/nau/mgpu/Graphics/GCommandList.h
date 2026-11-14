#pragma once
#include <array>
#include <d3d12.h>
#include <DirectXColors.h>
#include <mutex>
#include <wrl.h>
#include "nau/mgpu/Allocator/MemoryAllocator.h"
#include "nau/mgpu/Graphics/GDataUploader.h"
#include "nau/mgpu/Graphics/GRootSignature.h"


namespace nau::PEPEngine::Graphics
{
    using namespace Microsoft::WRL;

    class GBuffer;
    class GDevice;
    class GResource;
    class GTexture;
    class GDataUploader;
    class GDescriptor;
    class GResourceStateTracker;
    class GRootSignature;
    class GraphicPSO;
    class ComputePSO;
    class GRenderTexture;
    struct UploadAllocation;

    class GCommandList
    {
        friend class GCommandQueue;

        using TrackedObjects = std::vector<ComPtr<ID3D12Object>>;
        TrackedObjects trackedObject = std::vector<ComPtr<ID3D12Object>>();

        std::unique_ptr<GDataUploader> uploadBuffer;
        UploadAllocation UploadData(size_t sizeInBytes, const void* bufferData, size_t alignment) const;

        ComPtr<ID3D12GraphicsCommandList2> cmdList;
        ComPtr<ID3D12CommandAllocator> cmdAllocator;

        std::array<ID3D12DescriptorHeap*, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> cachedDescriptorHeaps;

        D3D12_COMMAND_LIST_TYPE type;
        std::unique_ptr<GResourceStateTracker> tracker;

        D3D12_PRIMITIVE_TOPOLOGY cachedPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ComPtr<ID3D12RootSignature> cachedRootSignature = nullptr;
        ComPtr<ID3D12PipelineState> cachedPSO = nullptr;


        void TrackResource(const ComPtr<ID3D12Object>& object);
        void TrackResource(const GResource& res);

        std::shared_ptr<GCommandQueue> queue;

    public:
        GCommandList(const std::shared_ptr<GCommandQueue>& queue, D3D12_COMMAND_LIST_TYPE type);
        std::shared_ptr<GDevice>& GetDevice() const;

        virtual ~GCommandList() = default;
        void BeginQuery(ID3D12QueryHeap* QueryHeap, UINT index) const;
        void EndQuery(ID3D12QueryHeap* QueryHeap, UINT index) const;
        void ResolveQuery(ID3D12QueryHeap* QueryHeap, ID3D12Resource* TimeStampResource, UINT index, UINT quriesCount, UINT64 aligned) const;
        void BeginQuery(UINT index) const;
        void EndQuery(UINT index) const;
        void ResolveQuery(UINT index, UINT quriesCount, UINT64 aligned) const;

        D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

        ComPtr<ID3D12GraphicsCommandList2> GetGraphicsCommandList() const;

        void UpdateDescriptorHeaps() const;
        void SetComputeRootShaderResourceView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetGraphicsRootShaderResourceView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetDescriptorsHeap(const GDescriptor* memory);


        void SetRootShaderResourceView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetComputeRootConstantBufferView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetGraphicsRootConstantBufferView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);

        void SetRootConstantBufferView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetComputeRootUnorderedAccessView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);

        void SetGraphicsRootUnorderedAccessView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetRootUnorderedAccessView(UINT rootSignatureSlot, const GBuffer& resource, UINT offset = 0);
        void SetComputeRoot32BitConstants(UINT rootSignatureSlot, UINT Count32BitValueToSet, const void* data, UINT DestOffsetIn32BitValueToSet) const;
        void SetGraphicsRoot32BitConstants(UINT rootSignatureSlot, UINT Count32BitValueToSet, const void* data, UINT DestOffsetIn32BitValueToSet) const;

        void SetRoot32BitConstants(UINT rootSignatureSlot, UINT Count32BitValueToSet, const void* data,
                                   UINT DestOffsetIn32BitValueToSet) const;
        void SetComputeRoot32BitConstant(UINT shaderRegister, UINT value, UINT offset = 0) const;
        void SetGraphicsRoot32BitConstant(UINT shaderRegister, UINT value, UINT offset = 0) const;

        void SetRoot32BitConstant(UINT shaderRegister, UINT value, UINT offset) const;
        void SetComputeRootDescriptorTable(UINT rootSignatureSlot, const GDescriptor* memory, UINT offset = 0) const;
        void SetGraphicsRootDescriptorTable(UINT rootSignatureSlot, const GDescriptor* memory, UINT offset = 0) const;

        void SetRootDescriptorTable(UINT rootSignatureSlot, const GDescriptor* memory, size_t offset = 0) const;

        void UpdateSubresource(const GResource& destResource, const D3D12_SUBRESOURCE_DATA* subresources,
                               size_t countSubresources);
        void UpdateSubresource(const ComPtr<ID3D12Resource>& destResource, const D3D12_SUBRESOURCE_DATA* subresources,
                               size_t countSubresources);
        void StartMark(const std::wstring& message) const;
        void EndMark() const;

        void SetViewports(const D3D12_VIEWPORT* viewports, size_t count) const;

        void SetScissorRects(const D3D12_RECT* scissorRects, size_t count) const;
        void SetComputeRootSignature(const GRootSignature& rs);
        void SetGraphicsRootSignature(const GRootSignature& rs);

        void SetRootSignature(const GRootSignature& rs);

        void SetPipelineState(const GraphicPSO& pso);

        void SetPipelineState(const ComputePSO& pso);

        void SetVBuffer(UINT slot = 0, UINT count = 0, const D3D12_VERTEX_BUFFER_VIEW* views = nullptr) const;
        void SetIBuffer(const D3D12_INDEX_BUFFER_VIEW* views = nullptr) const;

        void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY primitiveTopology);

        void ClearRenderTarget(const GDescriptor* memory, size_t offset = 0,
                               const FLOAT rgba[4] = DirectX::Colors::Black,
                               const D3D12_RECT* rects = nullptr, size_t rectCount = 0) const;

        void ClearRenderTarget(const GRenderTexture& target, const FLOAT rgba[4] = DirectX::Colors::Black,
                               const D3D12_RECT* rects = nullptr, size_t rectCount = 0) const;

        void SetRenderTargets(size_t RTCount = 0, const GDescriptor* rtvMemory = nullptr, size_t rtvOffset = 0,
                              const GDescriptor* dsvMemory = nullptr, size_t dsvOffset = 0) const;

        void SetRenderTarget(const GRenderTexture& target, const GDescriptor* dsvMemory = nullptr,
                             size_t dsvOffset = 0) const;

        void SetRenderTargets(const GRenderTexture* targets, UINT targetsSize, const GDescriptor* dsvMemory = nullptr,
                              size_t dsvOffset = 0, BOOL isSingleHandle = true) const;

        void ClearDepthStencil(const GDescriptor* dsvMemory, size_t dsvOffset,
                               D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                               FLOAT depthValue = 1.0f, UINT stencilValue = 0, const D3D12_RECT* rects = nullptr, size_t
                               rectCount = 0) const;

        void TransitionBarrier(const GResource& resource, D3D12_RESOURCE_STATES stateAfter,
                               UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               bool flushBarriers = false) const;

        void CopyTextureRegion(const ComPtr<ID3D12Resource>& dstRes, UINT DstX,
                               UINT DstY,
                               UINT DstZ, const ComPtr<ID3D12Resource>& srcRes, const D3D12_BOX* srcBox);
        void CopyCounter(const ComPtr<ID3D12Resource>& dest_resource, UINT dest_offset,
                         const ComPtr<ID3D12Resource>& source_resource) const;

        void CopyTextureRegion(const GResource& dstRes, UINT DstX, UINT DstY, UINT DstZ, const GResource& srcRes,
                               const D3D12_BOX* srcBox);

        void CopyBufferRegion(const GBuffer& dstRes, UINT DstOffset,
                              const GBuffer& srcRes, UINT SrcOffset, UINT numBytes, bool copyBarier = true);

        void TransitionBarrier(const ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES stateAfter,
                               UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               bool flushBarriers = false) const;

        void TransitionBarrier(const GRenderTexture* render, D3D12_RESOURCE_STATES stateAfter,
                               UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
                               bool flushBarriers = false) const;

        void UAVBarrier(const GResource& resource, bool flushBarriers = false) const;
        void UAVBarrier(const ComPtr<ID3D12Resource>& resource, bool flushBarriers = false) const;

        void AliasingBarrier(const GResource& beforeResource, const GResource& afterResource,
                             bool flushBarriers = false) const;
        void AliasingBarrier(const ComPtr<ID3D12Resource>& beforeResource, const ComPtr<ID3D12Resource>& afterResource,
                             bool flushBarriers = false) const;

        void FlushResourceBarriers() const;

        void CopyResource(const GResource& dstRes, const GResource& srcRes);
        void CopyResourceToCubeMap(const GResource& dstCube, const GResource& srcTex, UINT faceIndex);
        void CopyResourceToCubeMap(const ComPtr<ID3D12Resource>& dstCube, const ComPtr<ID3D12Resource>& srcTex, UINT faceIndex);
        void CopyCubeMapFace(const GResource& dstCube, const GResource& srcCube, UINT faceIndex);
        void CopyResourceFromCubeMap(const GResource& dstTex, const GResource& srcCube, UINT faceIndex);
        void CopyResourceFromCubeMap(const ComPtr<ID3D12Resource>& dstTex, const ComPtr<ID3D12Resource>& srcCube,
                                     UINT faceIndex);
        void CopyBufferRegion(const ComPtr<ID3D12Resource>& dstRes, UINT DstOffset,
                              const ComPtr<ID3D12Resource>& srcRes,
                              UINT SrcOffset,
                              UINT numBytes, bool copyBarier = true);
        void CopyResource(const ComPtr<ID3D12Resource>& dstRes, const ComPtr<ID3D12Resource>& srcRes);

        /**
             * Resolve a multisampled resource into a non-multisampled resource.
             */
        void ResolveSubresource(const GResource& dstRes, const GResource& srcRes, uint32_t dstSubresource = 0,
                                uint32_t srcSubresource = 0);

        void Draw(uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t startVertex = 0,
                  uint32_t startInstance = 0) const;
        void DrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t startIndex = 0,
                         int32_t baseVertex = 0,
                         uint32_t startInstance = 0) const;

        void Dispatch(uint32_t numGroupsX, uint32_t numGroupsY = 1, uint32_t numGroupsZ = 1) const;

        void ReleaseTrackedObjects();

        void Reset(const GraphicPSO* pso = nullptr);

        bool Close(const std::shared_ptr<GCommandList>& pendingCommandList) const;

        void Close() const;
    };
}
