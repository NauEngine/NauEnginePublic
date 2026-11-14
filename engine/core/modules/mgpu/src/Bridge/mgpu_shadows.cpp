// Second-GPU renderer of Nau's cascaded shadow atlas, see include/nau/mgpu/bridge/mgpu_shadows.h.
// No Nau headers here: the Mgpu headers below leak global `using namespace` directives and macros.

#include "nau/mgpu/bridge/mgpu_shadows.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/GCommandList.h"
#include "nau/mgpu/Graphics/GCommandQueue.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GResource.h"
#include "nau/mgpu/Graphics/GRootSignature.h"
#include "nau/mgpu/Graphics/GTexture.h"
#include "nau/mgpu/Graphics/GraphicPSO.h"
#include "nau/mgpu/Common/GCrossAdapterResource.h"
#include "nau/gpu_profiler/d3d12_timestamp_source.h"

#include "bridge_common.h"

namespace nau::mgpu::shadows
{
    namespace gfx = nau::PEPEngine::Graphics;
    using bridge::detail::SourceShader;

    namespace
    {
        // Atlases in flight: the second GPU draws one, the copy queue of the first GPU moves another into local memory,
        // one is ready for the next frame, Nau copies the last one.
        constexpr uint32_t kSlotCount = SLOT_COUNT;

        // Root signature: b0 = 18 root constants (DrawParams), t0 = root SRV with the matrices of the frame.
        constexpr uint32_t kViewProjConstants = 16;
        constexpr uint32_t kDrawConstants = 2;
        constexpr UINT kConstantsParameter = 0;
        constexpr UINT kMatricesParameter = 1;

        // The vertex shaders repeat Nau's z-prepass (texturedInstanced.hlsl: zprepassInstancedVSMain and
        // zprepassSkinnedVSMain) that Nau uses for its cascades. Matrices keep Nau's memory layout (column-major
        // packing, mul(matrix, vector)). Skinned bones are already in world space, as in Nau.
        constexpr const char* kShadowShaderSource = R"(
cbuffer DrawParams : register(b0)
{
    float4x4 vp;
    uint matrixOffset;
    uint boneCount;
};

StructuredBuffer<float4x4> matrices : register(t0);

float4 ShadowStaticVS(float3 position : POSITION, uint instance : SV_InstanceID) : SV_Position
{
    return mul(mul(vp, matrices[matrixOffset + instance]), float4(position, 1.0));
}

float4 ShadowSkinnedVS(float3 position : POSITION, float4 boneWeights : BLENDWEIGHT,
                       uint4 boneIndices : BLENDINDICES) : SV_Position
{
    float4 skinned = 0.0;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        const uint bone = boneIndices[i];
        if (bone < boneCount)
        {
            skinned += boneWeights[i] * mul(matrices[matrixOffset + bone], float4(position, 1.0));
        }
    }
    return mul(vp, skinned);
}
)";

        DXGI_FORMAT TypelessFormat(const AtlasFormat format)
        {
            return format == AtlasFormat::Depth16 ? DXGI_FORMAT_R16_TYPELESS : DXGI_FORMAT_R32_TYPELESS;
        }

        DXGI_FORMAT DepthFormat(const AtlasFormat format)
        {
            return format == AtlasFormat::Depth16 ? DXGI_FORMAT_D16_UNORM : DXGI_FORMAT_D32_FLOAT;
        }

        uint32_t BytesPerTexel(const AtlasFormat format)
        {
            return format == AtlasFormat::Depth16 ? 2u : 4u;
        }

        std::shared_ptr<gfx::GResource> CreateUploadBuffer(const std::shared_ptr<gfx::GDevice>& device, const size_t size,
                                                           const std::wstring& name)
        {
            return std::make_shared<gfx::GResource>(device, CD3DX12_RESOURCE_DESC::Buffer(std::max<size_t>(size, 256)),
                                                    name, nullptr, D3D12_RESOURCE_STATE_GENERIC_READ,
                                                    CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));
        }

        void Upload(gfx::GResource& buffer, const void* data, const size_t size)
        {
            void* mapped = nullptr;
            const D3D12_RANGE noRead = {0, 0};
            ThrowIfFailed(buffer.GetD3D12Resource()->Map(0, &noRead, &mapped));
            std::memcpy(mapped, data, size);
            buffer.GetD3D12Resource()->Unmap(0, nullptr);
        }
    }

    struct CascadeRenderer::Impl
    {
        using Clock = std::chrono::steady_clock;

        // Shadow-caster geometry on the second GPU. Upload heap: the data is written once and read by the GPU directly.
        struct Mesh
        {
            std::shared_ptr<gfx::GResource> positions;
            std::shared_ptr<gfx::GResource> boneWeights;
            std::shared_ptr<gfx::GResource> boneIndices;
            std::shared_ptr<gfx::GResource> indices;
            std::array<D3D12_VERTEX_BUFFER_VIEW, 3> vertexViews = {};
            D3D12_INDEX_BUFFER_VIEW indexView = {};
            bool skinned = false;
        };

        enum class SlotState
        {
            Free,
            Rendering, // submitted to the second GPU
            Drawn, // finished on the second GPU, in the shared heap
            Transferring, // shared heap -> local memory of the first GPU, on its copy queue
            Ready, // in local memory of the first GPU, not handed out yet
            Handed // given to Nau, waits for release()
        };

        struct Slot
        {
            SlotState state = SlotState::Free;
            std::shared_ptr<GCrossAdapterResource> shared; // shared heap, placed resource on each GPU
            std::shared_ptr<gfx::GResource> local; // copy in local memory of the first GPU, handed to Nau
            uint32_t width = 0;
            uint32_t height = 0;
            AtlasFormat format = AtlasFormat::Depth16;
            std::shared_ptr<gfx::GResource> matrices; // upload heap, read by the second GPU while it draws the slot
            size_t matricesCapacity = 0;
            uint64_t fenceValue = 0; // graphics queue of the second GPU
            uint64_t transferFenceValue = 0; // copy queue of the first GPU
            uint64_t sequence = 0;
            uint64_t userToken = 0;
            Clock::time_point submitted;
            Clock::time_point drawn;
            float latencyMs = 0;
        };

        using PsoKey = std::tuple<bool, AtlasFormat, int32_t, float>; // skinned, format, depth bias, slope bias

        bool ready = false;
        std::string error;
        std::string primaryName;
        std::string secondaryName;

        std::shared_ptr<gfx::GDevice> primaryDevice;
        std::shared_ptr<gfx::GDevice> secondaryDevice;

        // Depth atlas on the second GPU. One is enough: the work of all slots is ordered on one queue.
        std::shared_ptr<gfx::GTexture> atlas;
        gfx::GDescriptor atlasDsv;
        uint32_t atlasWidth = 0;
        uint32_t atlasHeight = 0;
        AtlasFormat atlasFormat = AtlasFormat::Depth16;

        std::shared_ptr<gfx::GRootSignature> signature;
        std::shared_ptr<SourceShader> staticVS;
        std::shared_ptr<SourceShader> skinnedVS;
        std::map<PsoKey, std::shared_ptr<gfx::GraphicPSO>> psos;

        std::unordered_map<uint64_t, Mesh> meshes;
        std::array<Slot, kSlotCount> slots;
        uint64_t nextSequence = 0;
        RendererStats stats;

        // GPU profiler (NAU_GPU_PROFILER=1): timestamps on the graphics queue of the second GPU and on the copy queue of
        // the first one. They only signal their own fence after the queue's one; nothing waits for them.
        // The atlas transfer "csm_atlas" is matched by FrameDesc::userToken: drawn and sent in one frame, moved into
        // local memory of the first GPU frames later, used by Nau (markTransferConsumed in mgpu_nodes.cpp) later still.
        gpu_profiler::D3D12TimestampSource drawTimestamps;
        gpu_profiler::D3D12TimestampSource transferTimestamps;

        std::shared_ptr<gfx::GCommandQueue>& GraphicsQueue2() const
        {
            return secondaryDevice->GetCommandQueue(gfx::GQueueType::Graphics);
        }

        std::shared_ptr<gfx::GCommandQueue>& CopyQueue1() const
        {
            return primaryDevice->GetCommandQueue(gfx::GQueueType::Copy);
        }

        bool Initialize(const char* nauAdapterName)
        {
            try
            {
                if (!bridge::detail::SelectDevices(nauAdapterName, primaryDevice, secondaryDevice, error))
                {
                    return false;
                }
                primaryName = bridge::detail::ToUtf8(primaryDevice->GetName());
                secondaryName = bridge::detail::ToUtf8(secondaryDevice->GetName());

                // Does nothing unless the profiler is enabled.
                gpu_profiler::GpuProfiler::instance().setGpuName(gpu_profiler::SecondaryGpu, secondaryName.c_str());
                drawTimestamps.init(secondaryDevice->GetDXDevice().Get(), GraphicsQueue2()->GetD3D12CommandQueue().Get(),
                                    gpu_profiler::SecondaryGpu, "gfx1.csm");
                transferTimestamps.init(primaryDevice->GetDXDevice().Get(), CopyQueue1()->GetD3D12CommandQueue().Get(),
                                        gpu_profiler::PrimaryGpu, "copy0.csm");

                signature = std::make_shared<gfx::GRootSignature>();
                signature->AddConstantParameter(kViewProjConstants + kDrawConstants, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
                signature->AddShaderResourceView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
                signature->Initialize(secondaryDevice);

                staticVS = std::make_shared<SourceShader>(kShadowShaderSource, gfx::VertexShader, "ShadowStaticVS", "vs_5_1");
                staticVS->LoadAndCompile();
                skinnedVS = std::make_shared<SourceShader>(kShadowShaderSource, gfx::VertexShader, "ShadowSkinnedVS", "vs_5_1");
                skinnedVS->LoadAndCompile();

                stats = {};
                ready = true;
                error.clear();
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during initialize: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during initialize (see the D3D12 debug output)";
            }

            Release();
            return false;
        }

        // Depth-only pipeline with the render state Nau uses for its cascades: the cull mode of the z-prepass material
        // (static "Clockwise" -> D3D12 FRONT, skinned "CounterClockwise" -> BACK, FrontCounterClockwise = TRUE in
        // drv3d_DX12) and the cascade override (depth bias, Z_CLAMP_ENABLED -> no depth clip, LESS_EQUAL).
        gfx::GraphicPSO& GetPso(const bool skinned, const AtlasFormat format, const int32_t depthBias, const float slopeBias)
        {
            const PsoKey key{skinned, format, depthBias, slopeBias};
            if (auto found = psos.find(key); found != psos.end())
            {
                return *found->second;
            }

            static const D3D12_INPUT_ELEMENT_DESC staticLayout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };
            static const D3D12_INPUT_ELEMENT_DESC skinnedLayout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            };

            D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
            desc.pRootSignature = signature->GetNativeSignature().Get();
            desc.InputLayout = skinned ? D3D12_INPUT_LAYOUT_DESC{skinnedLayout, _countof(skinnedLayout)}
                                       : D3D12_INPUT_LAYOUT_DESC{staticLayout, _countof(staticLayout)};
            desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            desc.RasterizerState.CullMode = skinned ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_FRONT;
            desc.RasterizerState.FrontCounterClockwise = TRUE;
            desc.RasterizerState.DepthBias = depthBias;
            desc.RasterizerState.DepthBiasClamp = 0.0f;
            desc.RasterizerState.SlopeScaledDepthBias = slopeBias;
            desc.RasterizerState.DepthClipEnable = FALSE;
            desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            desc.SampleMask = UINT_MAX;
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            desc.NumRenderTargets = 0;
            desc.DSVFormat = DepthFormat(format);
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;

            auto pso = std::make_shared<gfx::GraphicPSO>(*signature);
            pso->SetPsoDesc(desc);
            pso->SetShader(skinned ? skinnedVS.get() : staticVS.get());
            pso->Initialize(secondaryDevice);
            return *psos.emplace(key, std::move(pso)).first->second;
        }

        bool RegisterMesh(const uint64_t key, const MeshDesc& desc)
        {
            if (!ready || desc.positions == nullptr || desc.indices == nullptr || desc.vertexCount == 0 ||
                desc.indexCount == 0)
            {
                return false;
            }

            try
            {
                const std::wstring name = L"MgpuShadows Mesh " + std::to_wstring(key);
                Mesh mesh;
                mesh.skinned = desc.boneWeights != nullptr && desc.boneIndices != nullptr;

                const size_t positionsSize = static_cast<size_t>(desc.vertexCount) * 3 * sizeof(float);
                mesh.positions = CreateUploadBuffer(secondaryDevice, positionsSize, name + L" Positions");
                Upload(*mesh.positions, desc.positions, positionsSize);
                mesh.vertexViews[0] = {mesh.positions->GetD3D12Resource()->GetGPUVirtualAddress(),
                                       static_cast<UINT>(positionsSize), 3 * sizeof(float)};

                if (mesh.skinned)
                {
                    const size_t weightsSize = static_cast<size_t>(desc.vertexCount) * 4 * sizeof(float);
                    mesh.boneWeights = CreateUploadBuffer(secondaryDevice, weightsSize, name + L" Bone Weights");
                    Upload(*mesh.boneWeights, desc.boneWeights, weightsSize);
                    mesh.vertexViews[1] = {mesh.boneWeights->GetD3D12Resource()->GetGPUVirtualAddress(),
                                           static_cast<UINT>(weightsSize), 4 * sizeof(float)};

                    const size_t indicesSize = static_cast<size_t>(desc.vertexCount) * 4 * sizeof(uint32_t);
                    mesh.boneIndices = CreateUploadBuffer(secondaryDevice, indicesSize, name + L" Bone Indices");
                    Upload(*mesh.boneIndices, desc.boneIndices, indicesSize);
                    mesh.vertexViews[2] = {mesh.boneIndices->GetD3D12Resource()->GetGPUVirtualAddress(),
                                           static_cast<UINT>(indicesSize), 4 * sizeof(uint32_t)};
                }

                const size_t indexSize = desc.indices32 ? sizeof(uint32_t) : sizeof(uint16_t);
                const size_t indicesSize = static_cast<size_t>(desc.indexCount) * indexSize;
                mesh.indices = CreateUploadBuffer(secondaryDevice, indicesSize, name + L" Indices");
                Upload(*mesh.indices, desc.indices, indicesSize);
                mesh.indexView = {mesh.indices->GetD3D12Resource()->GetGPUVirtualAddress(), static_cast<UINT>(indicesSize),
                                  desc.indices32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT};

                // A key can be re-registered (Nau reloaded the mesh): the old buffers may still be read by work in
                // flight, so wait for the second GPU first.
                if (meshes.count(key) != 0)
                {
                    GraphicsQueue2()->Flush();
                }
                meshes[key] = std::move(mesh);
                stats.meshCount = static_cast<uint32_t>(meshes.size());
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during registerMesh: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during registerMesh";
            }
            return false;
        }

        void ForgetMeshes()
        {
            if (ready)
            {
                GraphicsQueue2()->Flush();
            }
            meshes.clear();
            stats.meshCount = 0;
        }

        // The depth atlas of the second GPU; recreated when Nau changes the atlas size or format (rare: waits for the
        // second GPU, because the previous atlas may still be in use).
        void EnsureAtlas(const uint32_t width, const uint32_t height, const AtlasFormat format)
        {
            if (atlas && atlasWidth == width && atlasHeight == height && atlasFormat == format)
            {
                return;
            }

            if (atlas)
            {
                GraphicsQueue2()->Flush();
            }

            const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(TypelessFormat(format), width, height, 1, 1, 1, 0,
                                                                          D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            D3D12_CLEAR_VALUE clear = {};
            clear.Format = DepthFormat(format);
            clear.DepthStencil.Depth = 1.0f;
            atlas = std::make_shared<gfx::GTexture>(secondaryDevice, desc, L"MgpuShadows Atlas", gfx::TextureUsage::Depth,
                                                    &clear);

            if (atlasDsv.IsNull())
            {
                atlasDsv = secondaryDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1);
            }
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DepthFormat(format);
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            atlas->CreateDepthStencilView(&dsvDesc, &atlasDsv);

            atlasWidth = width;
            atlasHeight = height;
            atlasFormat = format;
        }

        void EnsureSlot(Slot& slot, const FrameDesc& frame)
        {
            if (!slot.shared || slot.width != frame.atlasWidth || slot.height != frame.atlasHeight ||
                slot.format != frame.format)
            {
                // Same description as the atlas; GCrossAdapterResource replaces the flags with ALLOW_CROSS_ADAPTER and
                // the layout with ROW_MAJOR (as for the shadow map of HybridShadowApp).
                D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(TypelessFormat(frame.format), frame.atlasWidth,
                                                                        frame.atlasHeight, 1, 1);
                const auto index = static_cast<size_t>(&slot - slots.data());
                slot.shared = std::make_shared<GCrossAdapterResource>(desc, primaryDevice, secondaryDevice,
                                                                      L"MgpuShadows Shared " + std::to_wstring(index));
                // Nau copies from local memory: reading the shared heap (system memory, over PCIe or Thunderbolt for an
                // external GPU) is left to the copy queue, so it does not take time from Nau's graphics queue.
                // A new description: GCrossAdapterResource has changed `desc`.
                const D3D12_RESOURCE_DESC localDesc = CD3DX12_RESOURCE_DESC::Tex2D(TypelessFormat(frame.format), frame.atlasWidth,
                                                                                   frame.atlasHeight, 1, 1);
                slot.local = std::make_shared<gfx::GResource>(primaryDevice, localDesc, L"MgpuShadows Local " + std::to_wstring(index));
                slot.width = frame.atlasWidth;
                slot.height = frame.atlasHeight;
                slot.format = frame.format;
            }

            const size_t matricesSize = static_cast<size_t>(frame.matrixCount) * 16 * sizeof(float);
            if (!slot.matrices || slot.matricesCapacity < matricesSize)
            {
                slot.matricesCapacity = std::max<size_t>(matricesSize * 2, 64 * 16 * sizeof(float));
                slot.matrices = CreateUploadBuffer(secondaryDevice, slot.matricesCapacity, L"MgpuShadows Matrices");
            }
            if (matricesSize != 0)
            {
                Upload(*slot.matrices, frame.matrices, matricesSize);
            }
        }

        Slot* FreeSlot()
        {
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Free)
                {
                    return &slot;
                }
            }
            return nullptr;
        }

        bool Submit(const FrameDesc& frame)
        {
            if (!ready || frame.atlasWidth == 0 || frame.atlasHeight == 0)
            {
                return false;
            }

            // Like HybridShadowApp, the second GPU starts a new atlas only when it has finished the previous one: a deeper
            // queue would only add latency, and the second GPU may also have to present (an integrated GPU driving the
            // display of a laptop with an external GPU).
            CollectFinished();
            const bool rendering = std::any_of(slots.begin(), slots.end(), [](const Slot& item)
            {
                return item.state == SlotState::Rendering;
            });
            Slot* slot = rendering ? nullptr : FreeSlot();
            if (slot == nullptr)
            {
                ++stats.skippedBusy;
                return false;
            }

            try
            {
                EnsureAtlas(frame.atlasWidth, frame.atlasHeight, frame.format);
                EnsureSlot(*slot, frame);

                auto queue = GraphicsQueue2();
                auto cmdList = queue->GetCommandList();
                auto* raw = cmdList->GetGraphicsCommandList().Get();
                drawTimestamps.setCommandList(raw);

                uint32_t drawn = 0;
                {
                    // GPU time of drawing the atlas on the second GPU (clear + cascades), without the copy to the shared heap.
                    NAU_GPU_TIMER_SRC(drawTimestamps.id(), "csm_atlas_draw");

                    cmdList->TransitionBarrier(*atlas, D3D12_RESOURCE_STATE_DEPTH_WRITE);
                    cmdList->FlushResourceBarriers();
                    cmdList->SetRenderTargets(0, nullptr, 0, &atlasDsv, 0);
                    cmdList->ClearDepthStencil(&atlasDsv, 0, D3D12_CLEAR_FLAG_DEPTH, 1.0f);

                    cmdList->SetRootSignature(*signature);
                    raw->SetGraphicsRootShaderResourceView(kMatricesParameter,
                                                           slot->matrices->GetD3D12Resource()->GetGPUVirtualAddress());
                    cmdList->SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                    for (uint32_t c = 0; c < frame.cascadeCount; ++c)
                    {
                        const CascadeDesc& cascade = frame.cascades[c];

                        const D3D12_VIEWPORT viewport = {static_cast<float>(cascade.viewport[0]),
                                                         static_cast<float>(cascade.viewport[1]),
                                                         static_cast<float>(cascade.viewport[2]),
                                                         static_cast<float>(cascade.viewport[3]), 0.0f, 1.0f};
                        const D3D12_RECT scissor = {static_cast<LONG>(cascade.viewport[0]), static_cast<LONG>(cascade.viewport[1]),
                                                    static_cast<LONG>(cascade.viewport[0] + cascade.viewport[2]),
                                                    static_cast<LONG>(cascade.viewport[1] + cascade.viewport[3])};
                        cmdList->SetViewports(&viewport, 1);
                        cmdList->SetScissorRects(&scissor, 1);
                        raw->SetGraphicsRoot32BitConstants(kConstantsParameter, kViewProjConstants, cascade.viewProj, 0);

                        for (uint32_t d = 0; d < frame.drawCount; ++d)
                        {
                            const DrawDesc& draw = frame.draws[d];
                            if (draw.cascade != c)
                            {
                                continue;
                            }

                            const auto found = meshes.find(draw.mesh);
                            if (found == meshes.end())
                            {
                                continue;
                            }
                            const Mesh& mesh = found->second;
                            const bool skinned = draw.boneCount != 0;
                            if (skinned && !mesh.skinned)
                            {
                                continue;
                            }

                            cmdList->SetPipelineState(GetPso(skinned, frame.format, cascade.depthBias,
                                                             cascade.slopeScaledDepthBias));
                            const uint32_t drawConstants[kDrawConstants] = {draw.firstMatrix, draw.boneCount};
                            raw->SetGraphicsRoot32BitConstants(kConstantsParameter, kDrawConstants, drawConstants,
                                                               kViewProjConstants);

                            cmdList->SetVBuffer(0, skinned ? 3u : 1u, mesh.vertexViews.data());
                            cmdList->SetIBuffer(&mesh.indexView);
                            cmdList->DrawIndexed(draw.indexCount, skinned ? 1u : draw.instanceCount, draw.startIndex, 0, 0);
                            ++drawn;
                        }
                    }
                }

                {
                    // Source half of the atlas transfer, on the second GPU.
                    NAU_GPU_TRANSFER_SRC("csm_atlas", gpu_profiler::SecondaryGpu, gpu_profiler::PrimaryGpu, frame.userToken,
                                         drawTimestamps.id(),
                                         uint64_t{frame.atlasWidth} * frame.atlasHeight * BytesPerTexel(frame.format));

                    // Depth atlas -> shared heap, then both back to COMMON (PopulateCopyResource of HybridShadowApp).
                    cmdList->CopyResource(slot->shared->GetSharedResource(), *atlas);
                    cmdList->TransitionBarrier(slot->shared->GetSharedResource(), D3D12_RESOURCE_STATE_COMMON);
                    cmdList->TransitionBarrier(*atlas, D3D12_RESOURCE_STATE_COMMON);
                    cmdList->FlushResourceBarriers();
                }

                drawTimestamps.resolve();
                drawTimestamps.setCommandList(nullptr);
                slot->fenceValue = queue->ExecuteCommandList(cmdList);
                // After the queue's own signal: the order of the bridge's fences is unchanged.
                drawTimestamps.submitted();
                slot->state = SlotState::Rendering;
                slot->sequence = ++nextSequence;
                slot->userToken = frame.userToken;
                slot->submitted = Clock::now();

                ++stats.submitted;
                stats.lastDrawCount = drawn;
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during submit: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during submit (see the D3D12 debug output)";
            }

            // The list is abandoned: its timestamps never arrive and the profiler drops that frame.
            drawTimestamps.setCommandList(nullptr);
            ++stats.failed;
            return false;
        }

        // HybridShadowApp: the first GPU copies the newest atlas of the second GPU when its copy queue is free.
        void StartTransfer(Slot& slot)
        {
            try
            {
                auto queue = CopyQueue1();
                auto cmdList = queue->GetCommandList();
                transferTimestamps.setCommandList(cmdList->GetGraphicsCommandList().Get());
                {
                    // Destination half of the atlas transfer, on the copy queue of the first GPU.
                    NAU_GPU_TRANSFER_DST("csm_atlas", gpu_profiler::SecondaryGpu, gpu_profiler::PrimaryGpu, slot.userToken,
                                         transferTimestamps.id(), uint64_t{slot.width} * slot.height * BytesPerTexel(slot.format));

                    const gfx::GResource& prime = slot.shared->GetPrimeResource();
                    cmdList->CopyResource(*slot.local, prime);
                    cmdList->TransitionBarrier(*slot.local, D3D12_RESOURCE_STATE_COMMON);
                    cmdList->TransitionBarrier(prime, D3D12_RESOURCE_STATE_COMMON);
                    cmdList->FlushResourceBarriers();
                }
                transferTimestamps.resolve();
                transferTimestamps.setCommandList(nullptr);
                slot.transferFenceValue = queue->ExecuteCommandList(cmdList);
                // After the queue's own signal: the order of the bridge's fences is unchanged.
                transferTimestamps.submitted();
                slot.state = SlotState::Transferring;
                slot.drawn = Clock::now();
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during the transfer to the first GPU: ") + e.what();
                transferTimestamps.setCommandList(nullptr);
                gpu_profiler::GpuProfiler::instance().markTransferDiscarded("csm_atlas", slot.userToken);
                slot.state = SlotState::Free;
                ++stats.failed;
            }
        }

        void CollectFinished()
        {
            auto graphics2 = GraphicsQueue2();
            auto copy1 = CopyQueue1();
            bool transferring = false;
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Rendering && graphics2->IsFinish(slot.fenceValue))
                {
                    slot.state = SlotState::Drawn;
                }
                else if (slot.state == SlotState::Transferring)
                {
                    if (copy1->IsFinish(slot.transferFenceValue))
                    {
                        const Clock::time_point now = Clock::now();
                        slot.state = SlotState::Ready;
                        slot.latencyMs = std::chrono::duration<float, std::milli>(now - slot.submitted).count();
                        stats.lastLatencyMs = slot.latencyMs;
                        stats.lastTransferMs = std::chrono::duration<float, std::milli>(now - slot.drawn).count();
                        ++stats.completed;
                    }
                    else
                    {
                        transferring = true;
                    }
                }
            }

            if (transferring)
            {
                return;
            }

            // Only the newest drawn atlas is worth the transfer; older ones are superseded.
            Slot* newest = nullptr;
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Drawn && (newest == nullptr || slot.sequence > newest->sequence))
                {
                    newest = &slot;
                }
            }
            if (newest == nullptr)
            {
                return;
            }
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Drawn && &slot != newest)
                {
                    // Superseded before its transfer: the profiler does not wait for a destination half.
                    gpu_profiler::GpuProfiler::instance().markTransferDiscarded("csm_atlas", slot.userToken);
                    slot.state = SlotState::Free;
                }
            }
            StartTransfer(*newest);
        }

        bool TakeReady(ReadyAtlas& out)
        {
            if (!ready)
            {
                return false;
            }

            CollectFinished();

            Slot* newest = nullptr;
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Ready && (newest == nullptr || slot.sequence > newest->sequence))
                {
                    newest = &slot;
                }
            }
            if (newest == nullptr)
            {
                return false;
            }

            // Older finished atlases are superseded by the newest one.
            for (Slot& slot : slots)
            {
                if (slot.state == SlotState::Ready && &slot != newest)
                {
                    gpu_profiler::GpuProfiler::instance().markTransferDiscarded("csm_atlas", slot.userToken);
                    slot.state = SlotState::Free;
                }
            }

            newest->state = SlotState::Handed;
            out.slot = static_cast<uint32_t>(newest - slots.data());
            out.resource = newest->local->GetD3D12Resource().Get();
            out.width = newest->width;
            out.height = newest->height;
            out.format = newest->format;
            out.userToken = newest->userToken;
            out.latencyMs = newest->latencyMs;
            return true;
        }

        void ReleaseSlot(const uint32_t index)
        {
            if (index < kSlotCount && slots[index].state == SlotState::Handed)
            {
                slots[index].state = SlotState::Free;
            }
        }

        bool WaitAndReadBack(std::vector<float>& depths, uint32_t& width, uint32_t& height)
        {
            if (!ready)
            {
                return false;
            }

            try
            {
                // Waits until the newest atlas has been drawn and moved into local memory of the first GPU.
                for (;;)
                {
                    CollectFinished();
                    if (std::any_of(slots.begin(), slots.end(), [](const Slot& item) { return item.state == SlotState::Ready; }))
                    {
                        break;
                    }

                    const auto busy = std::find_if(slots.begin(), slots.end(), [](const Slot& item)
                    {
                        return item.state == SlotState::Rendering || item.state == SlotState::Transferring;
                    });
                    if (busy == slots.end())
                    {
                        break;
                    }
                    if (busy->state == SlotState::Rendering)
                    {
                        GraphicsQueue2()->WaitForFenceValue(busy->fenceValue);
                    }
                    else
                    {
                        CopyQueue1()->WaitForFenceValue(busy->transferFenceValue);
                    }
                }

                ReadyAtlas readyAtlas;
                if (!TakeReady(readyAtlas))
                {
                    error = "no atlas was submitted";
                    return false;
                }
                const Slot& slot = slots[readyAtlas.slot];

                // First GPU: the local copy Nau would get -> read-back buffer.
                const gfx::GResource& local = *slot.local;
                const D3D12_RESOURCE_DESC localDesc = local.GetD3D12Resource()->GetDesc();
                D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
                UINT64 size = 0;
                primaryDevice->GetDXDevice()->GetCopyableFootprints(&localDesc, 0, 1, 0, &footprint, nullptr, nullptr, &size);
                gfx::GResource readback(primaryDevice, CD3DX12_RESOURCE_DESC::Buffer(size), L"MgpuShadows Readback", nullptr,
                                        D3D12_RESOURCE_STATE_COPY_DEST, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK));

                auto copyQueue = primaryDevice->GetCommandQueue(gfx::GQueueType::Copy);
                auto cmdList = copyQueue->GetCommandList();
                cmdList->TransitionBarrier(local, D3D12_RESOURCE_STATE_COPY_SOURCE);
                cmdList->FlushResourceBarriers();
                const CD3DX12_TEXTURE_COPY_LOCATION dst(readback.GetD3D12Resource().Get(), footprint);
                const CD3DX12_TEXTURE_COPY_LOCATION src(local.GetD3D12Resource().Get(), 0);
                cmdList->GetGraphicsCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
                cmdList->TransitionBarrier(local, D3D12_RESOURCE_STATE_COMMON);
                cmdList->FlushResourceBarriers();
                copyQueue->WaitForFenceValue(copyQueue->ExecuteCommandList(cmdList));

                width = slot.width;
                height = slot.height;
                depths.assign(static_cast<size_t>(width) * height, 0.0f);

                const uint8_t* mapped = nullptr;
                const D3D12_RANGE readRange = {0, static_cast<SIZE_T>(size)};
                ThrowIfFailed(readback.GetD3D12Resource()->Map(0, &readRange,
                                                               reinterpret_cast<void**>(const_cast<uint8_t**>(&mapped))));
                const uint32_t texel = BytesPerTexel(slot.format);
                for (uint32_t y = 0; y < height; ++y)
                {
                    const uint8_t* row = mapped + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch;
                    for (uint32_t x = 0; x < width; ++x)
                    {
                        float value = 0.0f;
                        if (texel == 2)
                        {
                            uint16_t raw = 0;
                            std::memcpy(&raw, row + x * texel, sizeof(raw));
                            value = static_cast<float>(raw) / 65535.0f;
                        }
                        else
                        {
                            std::memcpy(&value, row + x * texel, sizeof(value));
                        }
                        depths[static_cast<size_t>(y) * width + x] = value;
                    }
                }
                const D3D12_RANGE noWrite = {0, 0};
                readback.GetD3D12Resource()->Unmap(0, &noWrite);

                ReleaseSlot(readyAtlas.slot);
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during read-back: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during read-back (see the D3D12 debug output)";
            }
            return false;
        }

        void Release()
        {
            ready = false;

            // Waits for all work in flight on both GPUs.
            if (primaryDevice)
            {
                primaryDevice->Flush();
            }
            if (secondaryDevice)
            {
                secondaryDevice->Flush();
            }
            // Atlases still in flight are dropped with the renderer (the path switched off): never used.
            for (const Slot& slot : slots)
            {
                if (slot.state != SlotState::Free && slot.state != SlotState::Handed)
                {
                    gpu_profiler::GpuProfiler::instance().markTransferDiscarded("csm_atlas", slot.userToken);
                }
            }
            // Before the devices go away.
            drawTimestamps.shutdown();
            transferTimestamps.shutdown();

            for (Slot& slot : slots)
            {
                slot = Slot();
            }
            meshes.clear();
            psos.clear();
            staticVS.reset();
            skinnedVS.reset();
            signature.reset();
            atlasDsv = gfx::GDescriptor();
            atlas.reset();
            atlasWidth = 0;
            atlasHeight = 0;
            primaryDevice.reset();
            secondaryDevice.reset();
            stats.meshCount = 0;
        }
    };

    CascadeRenderer::CascadeRenderer() : m_impl(std::make_unique<Impl>())
    {
    }

    CascadeRenderer::~CascadeRenderer()
    {
        shutdown();
    }

    bool CascadeRenderer::initialize(const char* nauAdapterName)
    {
        if (m_impl->ready)
        {
            return true;
        }
        return m_impl->Initialize(nauAdapterName);
    }

    bool CascadeRenderer::isReady() const
    {
        return m_impl->ready;
    }

    void CascadeRenderer::shutdown()
    {
        if (m_impl)
        {
            m_impl->Release();
        }
    }

    bool CascadeRenderer::hasMesh(const uint64_t key) const
    {
        return m_impl->meshes.count(key) != 0;
    }

    bool CascadeRenderer::registerMesh(const uint64_t key, const MeshDesc& mesh)
    {
        return m_impl->RegisterMesh(key, mesh);
    }

    void CascadeRenderer::forgetMeshes()
    {
        m_impl->ForgetMeshes();
    }

    bool CascadeRenderer::submit(const FrameDesc& frame)
    {
        return m_impl->Submit(frame);
    }

    bool CascadeRenderer::takeReady(ReadyAtlas& out)
    {
        return m_impl->TakeReady(out);
    }

    void CascadeRenderer::release(const uint32_t slot)
    {
        m_impl->ReleaseSlot(slot);
    }

    bool CascadeRenderer::waitAndReadBack(std::vector<float>& depths, uint32_t& width, uint32_t& height)
    {
        return m_impl->WaitAndReadBack(depths, width, height);
    }

    const char* CascadeRenderer::lastError() const
    {
        return m_impl->error.c_str();
    }

    const char* CascadeRenderer::primaryAdapterName() const
    {
        return m_impl->primaryName.c_str();
    }

    const char* CascadeRenderer::secondaryAdapterName() const
    {
        return m_impl->secondaryName.c_str();
    }

    const RendererStats& CascadeRenderer::stats() const
    {
        return m_impl->stats;
    }
}
