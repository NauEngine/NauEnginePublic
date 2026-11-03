// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "render_window_impl.h"

#include "nau/graphics/core_graphics.h"
#include "nau/input.h"

namespace nau::render
{
    void RenderWindowImpl::setWorld(nau::Uid worldUid)
    {
        lock_(m_readWriteMutex);
        m_world = worldUid;
    }

    nau::Uid RenderWindowImpl::getWorld()
    {
        lock_(m_readWriteMutex);
        return m_world;
    }

    async::Task<> RenderWindowImpl::requestViewportResize(int32_t width, int32_t height)
    {
        lock_(m_resizeMutex);
        m_width = width;
        m_height = height;
        m_resizeFrameCounter = REQUEST_RESIZE;
        co_return;
    }

    async::Task<> RenderWindowImpl::requestViewportResizeImmediate(int32_t width, int32_t height)
    {
        lock_(m_resizeMutex);
        m_width = width;
        m_height = height;
        m_resizeFrameCounter = PERFORM_RESIZE;

        co_return;
    }

    void RenderWindowImpl::resizeResolutions()
    {
        d3d::set_screen_size(m_width, m_height, m_swapchain);

        dabfg::set_resolution(m_resolutionName.c_str(), {m_width, m_height});
        dabfg::set_resolution(m_displayName.c_str(), {m_width, m_height});
        m_mainPass.gBuffer->changeResolution(m_width, m_height);

        if (m_swapchain == DEFAULT_SWAPID)
        {
            nau::input::setScreenResolution(m_width, m_height);
        }
    }

    void RenderWindowImpl::getViewportSize(int32_t& width, int32_t& height)
    {
        lock_(m_resizeMutex);

        width = m_width;
        height = m_height;
    }

    eastl::string_view RenderWindowImpl::getName() const
    {
        return m_name;
    }

    void RenderWindowImpl::setName(eastl::string_view name)
    {
        m_name = name;
        m_displayName = nau::utils::format("{}_view", name);
        m_resolutionName = nau::utils::format("{}_display", name);
    }

    int32_t RenderWindowImpl::getMainCameraIndex()
    {
        lock_(m_readWriteMutex);
        if (m_activeCamera)
        {
            return *m_activeCamera;
        }
        return -1;
    }

    void RenderWindowImpl::setMainCameraIndex(int32_t ind)
    {
        lock_(m_readWriteMutex);
        m_activeCamera = ind;
    }

    void RenderWindowImpl::render()
    {
        {
            lock_(m_resizeMutex);

            if (m_resizeFrameCounter == PERFORM_RESIZE)
            {
                resizeResolutions();
                m_resizeFrameCounter = NO_RESIZE_REQUESTED;
            }

            if (m_resizeFrameCounter != NO_RESIZE_REQUESTED)
            {
                m_resizeFrameCounter--;
            }
        }

        m_mainPass.scene = m_graphicsScene.get();
        const bool hasMainView = m_mainPass.scene && m_mainPass.scene->prepareView(
            m_mainPass.view, NullUid, m_mainPass.gBuffer->getWidth(), m_mainPass.gBuffer->getHeight());
        for (auto& [_, group] : m_graphStages)
        {
            group.resetState();
        }
        if (!hasMainView)
        {
            m_mainPass.view = {};
            for (auto* group : {&m_gBufferNodes, &m_environmentNodes, &m_uidNodes, &m_outlineNodes})
            {
                group->disableNodesWeak();
            }
            for (auto& request : m_viewportRequests)
            {
                request.promise.resolve(NullUid);
            }
            m_viewportRequests.clear();
        }

        Uid cameraUid;
        std::optional<math::Transform> worldPose;
        {
            lock_(m_readWriteMutex);
            cameraUid = m_previewCamera;
            worldPose = m_previewPose;
            m_previewPass.excludedMeshes = m_excludedMeshes;
        }
        m_previewPass.scene = m_graphicsScene.get();
        const bool showPreview = cameraUid && m_previewPass.scene && m_previewPass.scene->prepareView(
            m_previewPass.view, cameraUid, PreviewWidth, PreviewHeight, m_previewPass.excludedMeshes, true, worldPose ? &*worldPose : nullptr);
        if (showPreview && !m_previewPass.gBuffer)
        {
            createPreviewGraph();
        }
        if (!showPreview && !m_previewPass.view.views.empty())
        {
            m_previewPass.view = {};
        }
        m_previewNodes.setEnabled(showPreview);
    }

    void RenderWindowImpl::setCameraPreview(Uid cameraUid, eastl::vector<Uid> excludedMeshes, std::optional<math::Transform> worldPose)
    {
        lock_(m_readWriteMutex);
        m_previewCamera = cameraUid;
        m_previewPose = std::move(worldPose);
        m_excludedMeshes = std::move(excludedMeshes);
    }

    void RenderWindowImpl::setRenderScene(eastl::shared_ptr<GraphicsScene> gScene)
    {
        m_graphicsScene = gScene;
    }

    async::Task<nau::Uid> RenderWindowImpl::requestUidByCoords(int32_t viewportX, int32_t viewportY)
    {
        using namespace nau::async;

        if (Executor::Ptr renderWorkQueue = m_preRenderWorkQueueRef ? m_preRenderWorkQueueRef.lock() : nullptr)
        {
            ASYNC_SWITCH_EXECUTOR(renderWorkQueue);
        }
        else
        {
            NAU_LOG_ERROR("Render work queue is not accessible");
            co_return NullUid;
        }

        Task<Uid> future = m_viewportRequests.emplace_back(viewportX, viewportY).promise.getTask();
        future.setContinueOnCapturedExecutor(false);

        Uid result = co_await future;
        co_return result;
    }

    void RenderWindowImpl::setWorkQueue(WorkQueue::Ptr workQueue)
    {
        m_preRenderWorkQueueRef = std::move(workQueue);
    }

    void RenderWindowImpl::setOutlineWidth(float newWidth)
    {
        m_outlineWidth = newWidth;
    }

    void RenderWindowImpl::setOutlineColor(const math::Color4& color)
    {
        m_outlineColor.store(color, std::memory_order_seq_cst);
    }

    async::Task<> RenderWindowImpl::setEnabledRenderStages(::nau::TypedFlag<NauRenderStage> stages, bool enabled)
    {
        if (async::Executor::Ptr renderWorkQueue = m_preRenderWorkQueueRef ? m_preRenderWorkQueueRef.lock() : nullptr)
        {
            ASYNC_SWITCH_EXECUTOR(renderWorkQueue);
        }
        else
        {
            NAU_LOG_ERROR("Render work queue is not accessible");
            co_return;
        }

        for (auto& [stage, nodes] : m_graphStages)
        {
            if (stages.has(stage))
            {
                nodes.setEnabled(enabled);
            }
        }
    }

    async::Task<> RenderWindowImpl::enableRenderStages(::nau::TypedFlag<NauRenderStage> stages)
    {
        co_await setEnabledRenderStages(stages, true);
    }

    async::Task<> RenderWindowImpl::disableRenderStages(::nau::TypedFlag<NauRenderStage> stages)
    {
        co_await setEnabledRenderStages(stages, false);
    }

    void RenderWindowImpl::setRenderTarget()
    {
        BaseTexture* backBuf = d3d::get_back_buffer_rt(m_swapchain);
        d3d::set_render_target();
        d3d::set_render_target(backBuf, 0);
    }

    void* RenderWindowImpl::getHwnd() const
    {
        return m_windowHandle;
    }

    void RenderWindowImpl::initialize(eastl::string_view name, SWAPID swapchain, void* hwnd)
    {
        setName(name);
        m_swapchain = swapchain;
        m_windowHandle = hwnd;

        m_mainPass.suffix = eastl::to_string(m_swapchain);
        m_mainPass.resolution = m_resolutionName;
    }

    eastl::string RenderWindowImpl::ScenePass::name(const char* base) const
    {
        return nau::utils::format("{}_{}", base, suffix);
    }

    void RenderWindowImpl::ScenePass::initialize(MaterialAssetView::Ptr resolveMaterial, int width, int height)
    {
        auto resolver = eastl::make_unique<ShadingResolver>(eastl::make_unique<PostFxRenderer>(resolveMaterial));
        gBuffer = eastl::make_unique<DeferredRenderTarget>(std::move(resolver), name("gbuffer").c_str(), width, height,
            DeferredRT::StereoMode::MonoOrMultipass, TEXFMT_DEPTH32);
        nau::csm::CascadeShadows::Settings settings;
        settings.cascadeWidth = 1024;
        settings.splitsW = settings.splitsH = 2;
        shadows.reset(nau::csm::CascadeShadows::make(this, settings));
    }

    SWAPID RenderWindowImpl::getSwapchain() const
    {
        return m_swapchain;
    }
    void RenderWindowImpl::GraphNodes::addNode(dabfg::NodeHandle&& node)
    {
        m_frameGraphNodes.emplace_back(std::move(node));
    }

    void RenderWindowImpl::GraphNodes::setEnabled(bool enabled)
    {
        m_isEnabled = enabled;
        resetState();
    }

    void RenderWindowImpl::GraphNodes::disableNodesWeak()
    {
        for (auto& nodeId : m_frameGraphNodes)
        {
            dabfg::set_node_enabled(nodeId, false);
        }
    }

    void RenderWindowImpl::GraphNodes::resetState()
    {
        for (auto& nodeId : m_frameGraphNodes)
        {
            dabfg::set_node_enabled(nodeId, m_isEnabled);
        }
    }



    void RenderWindowImpl::ScenePass::renderDepth(BaseTexture* uidTarget)
    {
        d3d::set_render_target(uidTarget, 0);
        d3d::set_depth(gBuffer->getDepth(), DepthAccess::RW);
        d3d::setview(0, 0, gBuffer->getWidth(), gBuffer->getHeight(), 0.f, 1.f);
        d3d::clearview((uidTarget ? CLEAR_TARGET : 0) | CLEAR_ZBUFFER | CLEAR_STENCIL, math::E3DCOLOR(0, 0, 0, 0), 0, 0);
        scene->renderDepth(view);
        d3d::set_depth(nullptr, DepthAccess::RW);
    }

    void RenderWindowImpl::ScenePass::renderCascadeShadowDepth(int cascade, const math::Vector2&)
    {
        for (const auto& cascadeView : view.views)
        {
            if (cascadeView->containsTag(RenderScene::Tags::shadowCascadeTag) && cascade == int(cascadeView->getUserData()))
            {
                const auto& matrix = shadows->getWorldRenderMatrix(cascade);
                scene->prepareShadowView(*cascadeView, matrix, excludedMeshes);
                cascadeView->renderZPrepass(matrix, scene->getRenderScene()->getZPrepassMaterial().get());
                return;
            }
        }
    }

    void RenderWindowImpl::setDrawViewportGrid(bool isDrawGrid)
    {
        m_drawViewportGrid = isDrawGrid;
    }

    bool RenderWindowImpl::getDrawViewportGrid()
    {
        return m_drawViewportGrid;
    }

    void RenderWindowImpl::ScenePass::getCascadeShadowAnchorPoint(float, math::Vector3& anchor)
    {
        anchor = -view.camera.worldPosition;
    }

    void RenderWindowImpl::ScenePass::getCascadeShadowSparseUpdateParams(int, const math::NauFrustum&, float& dist, int& frame)
    {
        dist = 100000;
        frame = -1000;
    }
}  // namespace nau::render
