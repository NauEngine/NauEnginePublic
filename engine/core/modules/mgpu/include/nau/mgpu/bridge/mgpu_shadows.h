#pragma once

// Second-GPU renderer of Nau's cascaded shadow atlas (the MGPU-ShadowMap scheme of Multi-GPU-Render-DX12 applied to
// nau::csm::CascadeShadows).
//
// Nau prepares the cascades as usual (matrices, atlas layout, depth bias) and hands the shadow casters of every cascade
// to submit(). The second GPU draws the depth atlas and copies it into a cross-adapter shared heap; the copy queue of
// the first GPU then moves it into local memory, and takeReady() returns that texture, which Nau copies into its own
// atlas. Like in HybridShadowApp the first GPU never waits: it takes the newest finished atlas together with the cascade
// matrices it was drawn with (userToken).
//
// Only plain C++ types here, see mgpu_bridge.h. Without NAU_ENABLE_MGPU this header provides inline stubs.

#include <cstdint>
#include <memory>
#include <vector>

namespace nau::mgpu::shadows
{
    /// Atlases in flight: drawn on the second GPU, moved to the first GPU, ready, or handed to Nau
    /// (ReadyAtlas::slot < SLOT_COUNT).
    constexpr uint32_t SLOT_COUNT = 4;

    enum class AtlasFormat : uint32_t
    {
        Depth16, // Nau TEXFMT_DEPTH16, transferred as R16_TYPELESS
        Depth32  // Nau TEXFMT_DEPTH32, transferred as R32_TYPELESS
    };

    // Shadow-caster geometry, copied by registerMesh(). Layout of Nau's mesh buffers.
    struct MeshDesc
    {
        const float* positions = nullptr; // 3 floats per vertex
        uint32_t vertexCount = 0;
        const void* indices = nullptr;
        uint32_t indexCount = 0;
        bool indices32 = false;
        const float* boneWeights = nullptr; // skinned meshes only: 4 floats per vertex
        const uint32_t* boneIndices = nullptr; // skinned meshes only: 4 uints per vertex
    };

    struct CascadeDesc
    {
        float viewProj[16] = {}; // Nau memory layout (nau::math::Matrix4), used like `vp` in Nau's z-prepass shader
        uint32_t viewport[4] = {}; // x, y, width, height in atlas pixels
        int32_t depthBias = 0; // D3D12 DepthBias; drv3d_DX12 uses zBias / 2e-5
        float slopeScaledDepthBias = 0; // D3D12 SlopeScaledDepthBias; drv3d_DX12 uses slopeZBias
    };

    struct DrawDesc
    {
        uint64_t mesh = 0; // key passed to registerMesh()
        uint32_t cascade = 0;
        uint32_t startIndex = 0;
        uint32_t indexCount = 0;
        // Static mesh: world matrices of the instances are FrameDesc::matrices[firstMatrix, firstMatrix + instanceCount).
        // Skinned mesh: bone matrices (already in world space, as in Nau) are
        // FrameDesc::matrices[firstMatrix, firstMatrix + boneCount), instanceCount is 1.
        uint32_t firstMatrix = 0;
        uint32_t instanceCount = 1;
        uint32_t boneCount = 0; // 0 = static mesh
    };

    struct FrameDesc
    {
        uint32_t atlasWidth = 0;
        uint32_t atlasHeight = 0;
        AtlasFormat format = AtlasFormat::Depth16;
        const CascadeDesc* cascades = nullptr;
        uint32_t cascadeCount = 0;
        const DrawDesc* draws = nullptr;
        uint32_t drawCount = 0;
        const float* matrices = nullptr; // 16 floats each, Nau memory layout
        uint32_t matrixCount = 0;
        uint64_t userToken = 0; // returned with the atlas, e.g. the id of the shader-constant snapshot of this frame
    };

    struct ReadyAtlas
    {
        uint32_t slot = 0;
        // ID3D12Resource* in local memory of Nau's adapter (on its device), D3D12_RESOURCE_STATE_COMMON, R16/R32_TYPELESS.
        // Stays valid until release(slot).
        void* resource = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        AtlasFormat format = AtlasFormat::Depth16;
        uint64_t userToken = 0;
        float latencyMs = 0; // submit() -> in local memory of the first GPU, observed on the CPU
    };

    struct RendererStats
    {
        uint64_t submitted = 0;
        uint64_t completed = 0;
        uint64_t skippedBusy = 0; // submit() calls while the second GPU was still drawing, or every slot was in use
        uint64_t failed = 0;
        uint32_t lastDrawCount = 0;
        uint32_t meshCount = 0;
        float lastLatencyMs = 0;
        float lastTransferMs = 0; // shared heap -> local memory of the first GPU (its copy queue)
    };

#if defined(NAU_ENABLE_MGPU)

    class CascadeRenderer
    {
    public:
        CascadeRenderer();
        ~CascadeRenderer();

        CascadeRenderer(const CascadeRenderer&) = delete;
        CascadeRenderer& operator=(const CascadeRenderer&) = delete;

        /// nauAdapterName: d3d::get_device_name() (nullptr in standalone tests). Needs two distinct hardware adapters.
        bool initialize(const char* nauAdapterName);
        bool isReady() const;
        /// Waits for the work in flight on both GPUs and releases everything.
        void shutdown();

        bool hasMesh(uint64_t key) const;
        bool registerMesh(uint64_t key, const MeshDesc& mesh);
        void forgetMeshes();

        /// Non-blocking. Starts drawing the atlas on the second GPU if it has finished the previous one and a slot is free;
        /// returns false (nothing submitted) while the second GPU is still busy.
        bool submit(const FrameDesc& frame);

        /// Non-blocking. The newest atlas that has finished on the second GPU and was not handed out yet; older finished
        /// ones are dropped. The slot stays reserved until release(): the caller's copy must have executed by then.
        bool takeReady(ReadyAtlas& out);
        void release(uint32_t slot);

        /// Test helper (blocking): waits for every submitted atlas, then reads the newest one back through the copy
        /// queue of the first GPU. depths: width * height values, 0..1.
        bool waitAndReadBack(std::vector<float>& depths, uint32_t& width, uint32_t& height);

        const char* lastError() const;
        const char* primaryAdapterName() const;
        const char* secondaryAdapterName() const;
        const RendererStats& stats() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

#else

    class CascadeRenderer
    {
    public:
        bool initialize(const char*) { return false; }
        bool isReady() const { return false; }
        void shutdown() {}
        bool hasMesh(uint64_t) const { return false; }
        bool registerMesh(uint64_t, const MeshDesc&) { return false; }
        void forgetMeshes() {}
        bool submit(const FrameDesc&) { return false; }
        bool takeReady(ReadyAtlas&) { return false; }
        void release(uint32_t) {}
        bool waitAndReadBack(std::vector<float>&, uint32_t&, uint32_t&) { return false; }
        const char* lastError() const { return "Mgpu is not linked (NAU_ENABLE_MGPU is off)"; }
        const char* primaryAdapterName() const { return ""; }
        const char* secondaryAdapterName() const { return ""; }
        const RendererStats& stats() const { return m_stats; }

    private:
        RendererStats m_stats;
    };

#endif
}
