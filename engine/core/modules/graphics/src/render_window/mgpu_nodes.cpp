// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <algorithm>
#include <cstring>

#include "nau/3d/dag_drv3d.h"
#include "nau/3d/dag_drv3dCmd.h"
#include "nau/diag/logging.h"
#include "render_window_impl.h"

namespace nau::render
{
#if defined(NAU_ENABLE_MGPU)
    // Initial state of the Mgpu path of the render pipeline: the second GPU draws Nau's shadow cascades (see
    // MgpuShadowsState). The value is applied once, when the first render graph is created;
    // nau::mgpu::bridge::setEnabled() switches the path at run time (SceneBaseSample: key M).
    // It only has an effect in a build with NAU_ENABLE_MGPU (preset win_vs2022_x64_mgpu) on a machine with two distinct
    // GPUs, while the directional light casts shadows; with one GPU Nau draws the cascades itself.
    constexpr bool MGPU_SHADOWS_ENABLED = true;

    namespace
    {
        namespace shadows = nau::mgpu::shadows;

        // Matrices go to the second GPU in the memory layout Nau's shaders read them in.
        static_assert(sizeof(math::Matrix4) == 16 * sizeof(float));

        // drv3d_DX12 (render_state.h): D3D12 DepthBias = zBias / MINIMUM_REPRESENTABLE_D16, truncated to INT.
        constexpr double DX12_MINIMUM_REPRESENTABLE_D16 = 2e-5;

        void copyText(char* destination, const size_t capacity, const char* source)
        {
            std::strncpy(destination, source != nullptr ? source : "", capacity - 1);
            destination[capacity - 1] = '\0';
        }

        void appendMatrices(eastl::vector<float>& matrices, const math::Matrix4* source, const size_t count)
        {
            const auto* values = reinterpret_cast<const float*>(source);
            matrices.insert(matrices.end(), values, values + count * 16);
        }

        bool toAtlasFormat(const uint32_t format, shadows::AtlasFormat& out)
        {
            switch (format)
            {
                case TEXFMT_DEPTH16:
                    out = shadows::AtlasFormat::Depth16;
                    return true;
                case TEXFMT_DEPTH32:
                    out = shadows::AtlasFormat::Depth32;
                    return true;
                default:
                    return false;
            }
        }

        bool sameAtlas(const csm::CascadeShadows::AtlasInfo& a, const csm::CascadeShadows::AtlasInfo& b)
        {
            return a.width == b.width && a.height == b.height && a.format == b.format;
        }

        // Read-only lock of a Nau mesh buffer. Mesh buffers are SBCF_DYNAMIC and filled on the CPU once, so this reads
        // their mapped memory without waiting for the GPU (StaticMesh::createFromStaticMeshAccessor reads them back the
        // same way).
        class BufferRead
        {
        public:
            explicit BufferRead(Sbuffer* buffer) :
                m_buffer(buffer)
            {
                if (m_buffer != nullptr && !m_buffer->lock(0, 0, &m_data, VBLOCK_READONLY))
                {
                    m_data = nullptr;
                }
            }

            ~BufferRead()
            {
                if (m_data != nullptr)
                {
                    m_buffer->unlock();
                }
            }

            BufferRead(const BufferRead&) = delete;
            BufferRead& operator=(const BufferRead&) = delete;

            const void* data() const
            {
                return m_data;
            }

            int size() const
            {
                return m_buffer != nullptr ? m_buffer->ressize() : 0;
            }

        private:
            Sbuffer* m_buffer;
            void* m_data = nullptr;
        };
    }
#endif

    void RenderWindowImpl::initializeMgpu()
    {
#if defined(NAU_ENABLE_MGPU)
        static const bool switchApplied = (nau::mgpu::bridge::setEnabled(MGPU_SHADOWS_ENABLED), true);
        static_cast<void>(switchApplied);
#endif
    }

#if defined(NAU_ENABLE_MGPU)

    RenderWindowImpl::MgpuShadowsState::~MgpuShadowsState()
    {
        if (!releaseEventsCreated)
        {
            return;
        }

        for (size_t slot = 0; slot < releaseEvents.size(); ++slot)
        {
            // The driver signals the event when the frame with the copy has finished; it must not be closed earlier.
            if (releasePending[slot])
            {
                os_event_wait(&releaseEvents[slot], 1000);
            }
            os_event_destroy(&releaseEvents[slot]);
        }
        // The renderer is destroyed after this and waits for the second GPU.
    }

    void RenderWindowImpl::publishMgpuStatus() const
    {
        const auto& state = m_mgpuShadows;
        const shadows::RendererStats& stats = state.renderer.stats();

        nau::mgpu::bridge::Status status;
        status.ready = state.renderer.isReady();
        copyText(status.primaryAdapter, sizeof(status.primaryAdapter), state.renderer.primaryAdapterName());
        copyText(status.secondaryAdapter, sizeof(status.secondaryAdapter), state.renderer.secondaryAdapterName());
        copyText(status.lastError, sizeof(status.lastError), state.renderer.lastError());
        status.sceneFramesPerSecond = state.renderFramesPerSecond;
        status.shadowsFromSecondGpu = state.hasAtlas;
        status.atlasAgeFrames = state.hasAtlas ? static_cast<uint32_t>(state.frame - state.atlasFrame) : 0;
        status.atlasesPerSecond = state.atlasesPerSecond;
        status.latencyMs = stats.lastLatencyMs;
        status.drawCount = stats.lastDrawCount;
        status.atlasesCompleted = stats.completed;
        status.skippedBusy = stats.skippedBusy;
        status.failed = stats.failed + state.copyFailures;
        status.dofEnabled = m_depthOfFieldEnabled;
        status.dofSecondGpuRequested = m_dofMultiGpuEnabled;
        status.dofFromSecondGpu = m_depthOfFieldEnabled && m_dofMultiGpuUsed;
        nau::mgpu::bridge::publishStatus(status);
    }

    bool RenderWindowImpl::releaseMgpuShadowSlots()
    {
        auto& state = m_mgpuShadows;

        bool waiting = false;
        for (uint32_t slot = 0; slot < shadows::SLOT_COUNT; ++slot)
        {
            if (!state.releasePending[slot])
            {
                continue;
            }

            if (os_event_wait(&state.releaseEvents[slot], 0) == OS_WAIT_OK)
            {
                state.releasePending[slot] = false;
                state.renderer.release(slot);
            }
            else
            {
                waiting = true;
            }
        }
        return !waiting;
    }

    void RenderWindowImpl::releaseMgpuShadows()
    {
        auto& state = m_mgpuShadows;

        // Waits for the second GPU. Nau's copies have executed already (releaseMgpuShadowSlots).
        state.renderer.shutdown();
        state.meshes.clear();
        state.pendingFrames.clear();
        state.releasePending = {};
        state.initAttempted = false;
        state.hasAtlas = false;
        state.publishedHasAtlas = false;
        state.reportedFailures = 0;
        state.copyFailures = 0;
        state.atlasesSinceReport = 0;
        state.atlasesPerSecond = 0;
        state.framesSinceReport = 0;
        state.cpuSecondsSinceReport = 0;
    }

    uint64_t RenderWindowImpl::registerMgpuShadowMesh(const RenderEntity& entity)
    {
        auto& state = m_mgpuShadows;

        const auto bufferKey = eastl::make_pair<const Sbuffer*, const Sbuffer*>(entity.positionBuffer, entity.indexBuffer);
        const int positionsSize = entity.positionBuffer->ressize();
        const int indicesSize = entity.indexBuffer->ressize();

        if (const auto found = state.meshes.find(bufferKey); found != state.meshes.end() &&
                                                             found->second.positionsSize == positionsSize &&
                                                             found->second.indicesSize == indicesSize)
        {
            return found->second.key; // 0: the mesh could not be registered, it is not retried
        }

        // A new mesh, or a buffer address reused by another one: the old mesh stays on the second GPU until the Mgpu
        // path is switched off (CascadeRenderer::shutdown).
        MgpuShadowsState::MeshRecord& record = state.meshes[bufferKey];
        record = {0, positionsSize, indicesSize};

        const bool skinned = entity.boneWeightsBuffer != nullptr && entity.boneIndicesBuffer != nullptr;
        const BufferRead positions(entity.positionBuffer);
        const BufferRead indices(entity.indexBuffer);
        const BufferRead boneWeights(skinned ? entity.boneWeightsBuffer : nullptr);
        const BufferRead boneIndices(skinned ? entity.boneIndicesBuffer : nullptr);

        shadows::MeshDesc mesh;
        mesh.positions = static_cast<const float*>(positions.data());
        mesh.vertexCount = static_cast<uint32_t>(positionsSize / (3 * sizeof(float)));
        mesh.indices = indices.data();
        mesh.indices32 = (entity.indexBuffer->getFlags() & SBCF_INDEX32) != 0;
        mesh.indexCount = static_cast<uint32_t>(indicesSize / (mesh.indices32 ? sizeof(uint32_t) : sizeof(uint16_t)));
        if (skinned)
        {
            // Nau's skinned vertices: float4 weights + uint4 joints.
            const size_t skinSize = static_cast<size_t>(mesh.vertexCount) * 4 * sizeof(float);
            if (static_cast<size_t>(boneWeights.size()) >= skinSize && static_cast<size_t>(boneIndices.size()) >= skinSize)
            {
                mesh.boneWeights = static_cast<const float*>(boneWeights.data());
                mesh.boneIndices = static_cast<const uint32_t*>(boneIndices.data());
            }
            else
            {
                mesh.positions = nullptr;
            }
        }

        if (mesh.positions == nullptr || mesh.indices == nullptr || (skinned && (mesh.boneWeights == nullptr || mesh.boneIndices == nullptr)))
        {
            NAU_LOG_ERROR("mgpu shadows: could not read a shadow caster mesh ({} vertices, {} indices), Nau draws without it",
                          mesh.vertexCount, mesh.indexCount);
            return 0;
        }

        const uint64_t key = state.nextMeshKey++;
        if (!state.renderer.registerMesh(key, mesh))
        {
            NAU_LOG_ERROR("mgpu shadows: could not register a shadow caster mesh on the second GPU: {}", state.renderer.lastError());
            return 0;
        }

        record.key = key;
        return key;
    }

    // The same cascades and casters Nau draws in renderShadowsCascades (renderCascadeShadowDepth -> RenderView::renderZPrepass
    // of the csmView_<cascade> view), as a frame description for the second GPU.
    void RenderWindowImpl::collectMgpuShadowCasters(const int numCascades)
    {
        auto& state = m_mgpuShadows;
        state.cascades.clear();
        state.draws.clear();
        state.matrices.clear();
        state.bones.clear();
        state.skippedCasters = 0;

        for (int c = 0; c < numCascades; ++c)
        {
            shadows::CascadeDesc& cascade = state.cascades.emplace_back();
            std::memcpy(cascade.viewProj, &m_csm->getWorldRenderMatrix(c), sizeof(cascade.viewProj));

            const csm::CascadeShadows::CascadeViewport viewport = m_csm->getCascadeViewport(c);
            cascade.viewport[0] = static_cast<uint32_t>(viewport.x);
            cascade.viewport[1] = static_cast<uint32_t>(viewport.y);
            cascade.viewport[2] = static_cast<uint32_t>(viewport.width);
            cascade.viewport[3] = static_cast<uint32_t>(viewport.height);

            float constBias = 0;
            float slopeBias = 0;
            m_csm->getCascadeDepthBias(c, constBias, slopeBias);
            cascade.depthBias = static_cast<int32_t>(constBias / DX12_MINIMUM_REPRESENTABLE_D16);
            cascade.slopeScaledDepthBias = slopeBias;
        }

        for (auto& view : m_graphicsScene->getRenderScene()->getViews())
        {
            if (!view->containsTag(nau::RenderScene::Tags::shadowCascadeTag))
            {
                continue;
            }
            const int cascade = static_cast<int>(reinterpret_cast<intptr_t>(view->getUserData()));
            if (cascade < 0 || cascade >= numCascades)
            {
                continue;
            }

            for (const auto& list : view->getLists())
            {
                for (const RenderEntity& entity : list->getEntities())
                {
                    if (entity.positionBuffer == nullptr || entity.indexBuffer == nullptr || entity.endIndex <= entity.startIndex)
                    {
                        continue;
                    }

                    const uint64_t mesh = registerMgpuShadowMesh(entity);
                    if (mesh == 0)
                    {
                        ++state.skippedCasters;
                        continue;
                    }

                    shadows::DrawDesc draw;
                    draw.mesh = mesh;
                    draw.cascade = static_cast<uint32_t>(cascade);
                    draw.startIndex = entity.startIndex;
                    draw.indexCount = (entity.endIndex - entity.startIndex) / 3 * 3;
                    draw.firstMatrix = static_cast<uint32_t>(state.matrices.size() / 16);

                    // RenderView::renderZPrepass: instanced entities use the "default" pipeline, the others are skinned
                    // when they have bone buffers.
                    const bool instanced = entity.instancingSupported && entity.instancesCount != 1;
                    const bool skinned = !instanced && entity.boneWeightsBuffer != nullptr && entity.boneIndicesBuffer != nullptr;
                    if (skinned)
                    {
                        const auto bones = entity.cbStructsData.find("BonesTransforms");
                        if (bones == entity.cbStructsData.end() || bones->second.dataPtr == nullptr)
                        {
                            ++state.skippedCasters;
                            continue;
                        }

                        // Bones are already in world space; one copy per skinned instance, shared by its cascades.
                        draw.boneCount = std::min<uint32_t>(static_cast<uint32_t>(bones->second.size / sizeof(math::Matrix4)),
                                                            NAU_MAX_SKINNING_BONES_COUNT);
                        const auto known = std::find_if(state.bones.begin(), state.bones.end(), [&](const auto& item)
                        {
                            return item.first == bones->second.dataPtr;
                        });
                        if (known != state.bones.end())
                        {
                            draw.firstMatrix = known->second;
                        }
                        else
                        {
                            state.bones.emplace_back(bones->second.dataPtr, draw.firstMatrix);
                            appendMatrices(state.matrices, static_cast<const math::Matrix4*>(bones->second.dataPtr), draw.boneCount);
                        }
                    }
                    else
                    {
                        // The z-prepass shader reads instanceBuffer[startInstance + instance].worldMatrix; a non-instanced
                        // draw is instance 0.
                        const uint32_t requested = instanced ? entity.instancesCount : 1;
                        draw.instanceCount = std::min<uint32_t>(requested, static_cast<uint32_t>(entity.instanceData.size()));
                        if (draw.instanceCount == 0)
                        {
                            continue;
                        }
                        for (uint32_t i = 0; i < draw.instanceCount; ++i)
                        {
                            appendMatrices(state.matrices, &entity.instanceData[i].worldMatrix, 1);
                        }
                    }

                    state.draws.push_back(draw);
                }
            }
        }
    }

    bool RenderWindowImpl::renderMgpuShadows()
    {
        auto& state = m_mgpuShadows;
        ++state.frame;

        // Render frame rate for the UI, in every mode.
        const auto frameTime = std::chrono::steady_clock::now();
        if (state.renderFrames++ == 0)
        {
            state.renderFramesStart = frameTime;
        }
        else if (const float elapsed = std::chrono::duration<float>(frameTime - state.renderFramesStart).count();
                 elapsed >= MGPU_REPORT_PERIOD_SECONDS)
        {
            state.renderFramesPerSecond = static_cast<float>(state.renderFrames - 1) / elapsed;
            state.renderFrames = 1;
            state.renderFramesStart = frameTime;
            publishMgpuStatus();
        }

        // DOF runs on its own switches (F9, F8 in the render graph), also while the shadow path is off. Its result of
        // the previous frame is known here: the CSM node runs before the DOF nodes.
        const bool dofFromSecondGpu = m_depthOfFieldEnabled && m_dofMultiGpuUsed;
        if (state.publishedDofEnabled != m_depthOfFieldEnabled || state.publishedDofFromSecondGpu != dofFromSecondGpu)
        {
            state.publishedDofEnabled = m_depthOfFieldEnabled;
            state.publishedDofFromSecondGpu = dofFromSecondGpu;
            publishMgpuStatus();
        }

        if (!nau::mgpu::bridge::isEnabled())
        {
            state.hasAtlas = false;
            // Shut down once Nau's copies from the shared heaps have executed; until then nothing new is started.
            if (state.initAttempted && releaseMgpuShadowSlots())
            {
                releaseMgpuShadows();
                publishMgpuStatus();
                NAU_LOG_INFO("mgpu shadows: switched off, Nau draws the shadow cascades");
            }
            return false;
        }

        if (!state.initAttempted)
        {
            state.initAttempted = true;
            state.lastReport = std::chrono::steady_clock::now();

            if (!state.releaseEventsCreated)
            {
                for (os_event_t& event : state.releaseEvents)
                {
                    os_event_create(&event);
                }
                state.releaseEventsCreated = true;
            }

            // The primary Mgpu device is the one on the adapter Nau renders on: the shared heaps are copied from there.
            if (state.renderer.initialize(d3d::get_device_name()))
            {
                NAU_LOG_INFO("mgpu shadows: the shadow cascades are drawn by '{}', the scene by '{}'",
                             state.renderer.secondaryAdapterName(), state.renderer.primaryAdapterName());
            }
            else
            {
                NAU_LOG_WARNING("mgpu shadows: second GPU not available, Nau draws the shadow cascades: {}", state.renderer.lastError());
            }
            publishMgpuStatus();
        }

        if (!state.renderer.isReady())
        {
            return false;
        }

        const auto workStart = std::chrono::steady_clock::now();
        ++state.framesSinceReport;
        releaseMgpuShadowSlots();

        const int numCascades = m_csm->getNumCascadesToRender();
        const csm::CascadeShadows::AtlasInfo atlas = m_csm->getAtlasInfo();
        shadows::AtlasFormat format = shadows::AtlasFormat::Depth16;
        if (numCascades <= 0 || !toAtlasFormat(atlas.format, format))
        {
            // No shadows this frame: Nau clears its atlas.
            state.hasAtlas = false;
            return false;
        }
        if (state.hasAtlas && !sameAtlas(state.atlasSnapshot.atlas, atlas))
        {
            state.hasAtlas = false; // Nau's atlas was recreated (another size or format)
        }

        // 1. The newest atlas the second GPU has finished goes into Nau's atlas, with the constants it was drawn with.
        shadows::ReadyAtlas ready;
        if (state.renderer.takeReady(ready))
        {
            const auto pending = std::find_if(state.pendingFrames.begin(), state.pendingFrames.end(), [&](const auto& item)
            {
                return item.token == ready.userToken;
            });

            bool copied = false;
            if (pending != state.pendingFrames.end() && sameAtlas(pending->snapshot.atlas, atlas))
            {
                BaseTexture* target = m_csm->getShadowsCascade();
                copied = d3d::driver_command(DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE, ready.resource, target, nullptr) != 0;
                if (copied)
                {
                    // Same state Nau leaves its atlas in after drawing it (CascadeShadows::renderShadowsCascadesCb).
                    d3d::resource_barrier({target, RB_RO_SRV | RB_STAGE_PIXEL, 0, 0});
                    d3d::driver_command(DRV3D_COMMAND_REGISTER_FRAME_COMPLETION_EVENT, &state.releaseEvents[ready.slot], nullptr, nullptr);
                    state.releasePending[ready.slot] = true;

                    state.hasAtlas = true;
                    state.atlasSnapshot = pending->snapshot;
                    state.atlasFrame = pending->frame;
                    ++state.atlasesSinceReport;
                }
                else if (state.copyFailures++ == 0)
                {
                    NAU_LOG_ERROR("mgpu shadows: the driver did not copy the atlas of the second GPU, Nau draws the shadow cascades");
                }
            }

            if (!copied)
            {
                // Drawn for an atlas Nau has recreated since, or refused by the driver: the slot is free right away.
                state.renderer.release(ready.slot);
            }

            // takeReady has dropped the older finished atlases.
            state.pendingFrames.erase(std::remove_if(state.pendingFrames.begin(), state.pendingFrames.end(), [&](const auto& item)
            {
                return item.token <= ready.userToken;
            }), state.pendingFrames.end());
        }

        // 2. The cascades of this frame go to the second GPU if it has a free slot; it never waits.
        collectMgpuShadowCasters(numCascades);

        shadows::FrameDesc frame;
        frame.atlasWidth = static_cast<uint32_t>(atlas.width);
        frame.atlasHeight = static_cast<uint32_t>(atlas.height);
        frame.format = format;
        frame.cascades = state.cascades.data();
        frame.cascadeCount = static_cast<uint32_t>(state.cascades.size());
        frame.draws = state.draws.data();
        frame.drawCount = static_cast<uint32_t>(state.draws.size());
        frame.matrices = state.matrices.data();
        frame.matrixCount = static_cast<uint32_t>(state.matrices.size() / 16);
        frame.userToken = state.frame;
        if (state.renderer.submit(frame))
        {
            state.pendingFrames.push_back({state.frame, state.frame, m_csm->snapshotShaderConstants()});
        }

        // 3. Log and UI.
        const auto now = std::chrono::steady_clock::now();
        state.cpuSecondsSinceReport += std::chrono::duration<double>(now - workStart).count();

        const shadows::RendererStats& stats = state.renderer.stats();
        if (stats.failed != state.reportedFailures)
        {
            NAU_LOG_ERROR("mgpu shadows: {} atlas(es) FAILED on the second GPU: {}", stats.failed - state.reportedFailures,
                          state.renderer.lastError());
            state.reportedFailures = stats.failed;
        }

        const float elapsed = std::chrono::duration<float>(now - state.lastReport).count();
        if (elapsed >= MGPU_REPORT_PERIOD_SECONDS)
        {
            state.atlasesPerSecond = static_cast<float>(state.atlasesSinceReport) / elapsed;
            const float framesPerSecond = static_cast<float>(state.framesSinceReport) / elapsed;
            const double cpuMsPerFrame = state.framesSinceReport != 0 ? state.cpuSecondsSinceReport * 1000.0 / state.framesSinceReport : 0.0;
            state.atlasesSinceReport = 0;
            state.framesSinceReport = 0;
            state.cpuSecondsSinceReport = 0;
            state.lastReport = now;
            publishMgpuStatus();
            state.publishedHasAtlas = state.hasAtlas;

            NAU_LOG_INFO("mgpu shadows: {:.1f} atlases/s from '{}', {} draws ({} casters skipped), latency {:.1f} ms "
                         "(transfer {:.1f} ms), atlas age {} frames, {} completed, {} skipped (busy), {} failed; scene {:.1f} fps, "
                         "{:.3f} ms CPU per frame",
                         state.atlasesPerSecond, state.renderer.secondaryAdapterName(), stats.lastDrawCount, state.skippedCasters,
                         stats.lastLatencyMs, stats.lastTransferMs,
                         state.hasAtlas ? state.frame - state.atlasFrame : 0, stats.completed, stats.skippedBusy,
                         stats.failed + state.copyFailures, framesPerSecond, cpuMsPerFrame);
        }
        else if (state.publishedHasAtlas != state.hasAtlas)
        {
            publishMgpuStatus();
            state.publishedHasAtlas = state.hasAtlas;
        }

        return state.hasAtlas;
    }

#endif
}  // namespace nau::render
