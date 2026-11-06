// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <nau/assets/asset_manager.h>
#include <atomic>
#include <imgui.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "graphics_impl.h"
#include "nau/gui/dag_imgui.h"
#include "nau/gui/imguiInput.h"
#include "nau/ui.h"
#include "render_window_impl.h"
#include "dof_controls.h"
#include "nau/3d/dag_drv3dCmd.h"
#include "nau/gpu_profiler/gpu_profiler.h"

namespace
{
    constexpr uint32_t DofThreadGroupSize = 8;

    // UI and render execute on different threads. Only scalar settings cross here.
    std::atomic<float> dofFocusDistance{25.0f};
    std::atomic<float> dofFocusRange{6.0f};

    void clearDofComputeBindings()
    {
        for (uint32_t slot = 0; slot < 8; ++slot)
        {
            d3d::set_tex(STAGE_CS, slot, nullptr);
            d3d::set_rwtex(STAGE_CS, slot, nullptr, 0, 0);
        }
    }

    void dispatchDofPass(nau::MaterialAssetView& material, eastl::string_view pipeline, BaseTexture* output)
    {
        TextureInfo info = {};
        output->getinfo(info, 0);
        material.bindPipeline(pipeline);
        material.dispatch((info.w + DofThreadGroupSize - 1) / DofThreadGroupSize,
                          (info.h + DofThreadGroupSize - 1) / DofThreadGroupSize,
                          1);
        clearDofComputeBindings();
    }
}


namespace nau::render
{

    void drawDofControls()
    {
        float focusDistance = dofFocusDistance.load();
        float focusRange = dofFocusRange.load();
        ImGui::SetNextWindowSize(ImVec2(330.0f, 110.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("DOF"))
        {
            if (ImGui::SliderFloat("Focus distance", &focusDistance, 0.1f, 200.0f, "%.2f"))
                dofFocusDistance.store(focusDistance);
            if (ImGui::SliderFloat("Focus range", &focusRange, 0.1f, 50.0f, "%.2f"))
                dofFocusRange.store(focusRange);
        }
        ImGui::End();
    }

    async::Task<> RenderWindowImpl::createRenderGraph()
    {
        const auto openMaterialAsset = [](MaterialAssetRef ref, MaterialAssetView::Ptr& destination) -> async::Task<>
        {
            destination = co_await ref.getAssetViewTyped<MaterialAssetView>();
            co_return;
        };
        const auto openTextureAsset = [](eastl::string_view path, ReloadableAssetView::Ptr& destination) -> async::Task<>
        {
            IAssetDescriptor::Ptr asset = getServiceProvider().get<IAssetManager>().openAsset(path);
            destination = co_await asset->getReloadableAssetView(rtti::getTypeInfo<TextureAssetView>());
            co_return;
        };
        const auto openShaderAsset = [](eastl::string_view path, ShaderAssetView::Ptr& destination) -> async::Task<>
        {
            IAssetDescriptor::Ptr asset = getServiceProvider().get<IAssetManager>().openAsset(path);
            destination = co_await asset->getAssetView(rtti::getTypeInfo<ShaderAssetView>());
            co_return;
        };

        NAU_LOG("NAU_DOF_HDR_REFERENCE_20260928: HDR pipeline, reference PatchMatch, default DOF OFF");
        async::TaskCollection loadTasks;

        MaterialAssetView::Ptr matPPTonemap;
        loadTasks.push(openMaterialAsset({"file:/res/materials/pp_tonemap.nmat_json"}, matPPTonemap));

        loadTasks.push(openMaterialAsset({"file:/res/materials/pp_depth_of_field_compute.nmat_json"}, m_depthOfFieldMaterial));


        loadTasks.push(openMaterialAsset({"file:/res/materials/grid.nmat_json"}, m_gridMaterial));

        ShaderAssetView::Ptr panoramaToCubemapComputeShader;
        loadTasks.push(openShaderAsset("file:/res/shaders/cache/shader_cache.nsbc+[cs_panorama_to_cubemap.cs.csmain]", panoramaToCubemapComputeShader));
        ShaderAssetView::Ptr genIrradianceMapComputeShader;
        loadTasks.push(openShaderAsset("file:/res/shaders/cache/shader_cache.nsbc+[cs_gen_irradiance_map.cs.csmain]", genIrradianceMapComputeShader));
        ShaderAssetView::Ptr genReflectionMapComputeShader;
        loadTasks.push(openShaderAsset("file:/res/shaders/cache/shader_cache.nsbc+[cs_gen_reflection_map.cs.csmain]", genReflectionMapComputeShader));
        ReloadableAssetView::Ptr hdrPanorama;
        loadTasks.push(openTextureAsset("file:/res/textures/hdri/default_cubemap_2k.hdr", hdrPanorama));

        loadTasks.push(openMaterialAsset({"file:/res/materials/pixel_data_extraction.nmat_json"}, m_pixelDataExtractionMaterial));

        MaterialAssetView::Ptr outlineTonemap;
        loadTasks.push(openMaterialAsset({"file:/res/materials/outline_calculation.nmat_json"}, outlineTonemap));

        MaterialAssetView::Ptr matSkybox;
        loadTasks.push(openMaterialAsset({"file:/res/materials/skybox.nmat_json"}, matSkybox));

        MaterialAssetView::Ptr matPPResolve;
        loadTasks.push(openMaterialAsset({"file:/res/materials/pp_deferred_resolve.nmat_json"}, matPPResolve));

        co_await loadTasks.awaitCompletion();

        matPPResolve->enableAutoSetTextures(false);
        matPPTonemap->enableAutoSetTextures(false);
        m_depthOfFieldMaterial->enableAutoSetTextures(false);

        m_environmentRenderer = eastl::make_unique<render::EnvironmentRenderer>(
            matSkybox, panoramaToCubemapComputeShader, genIrradianceMapComputeShader, genReflectionMapComputeShader);
        m_environmentRenderer->setPanoramaTexture(hdrPanorama);

        eastl::unique_ptr<render::PostFxRenderer> resolveShadingPass = eastl::make_unique<render::PostFxRenderer>(matPPResolve);
        eastl::unique_ptr<render::ShadingResolver> shadingResolver = eastl::make_unique<render::ShadingResolver>(std::move(resolveShadingPass));

        BufferDesc buffDesc;
        buffDesc.name = u8"pixel_data_extraction_result_buffer";
        buffDesc.elementSize = sizeof(PixelData);
        buffDesc.elementCount = 1;
        buffDesc.flags = 0;
        buffDesc.format = 0;

        m_pixelDataExtractionMaterial->createRwBuffer("default", "ResultBuffer", buffDesc);

        constexpr auto exposure = 0.4f;
        shader_globals::setVariable("exposure", &exposure);

        BaseTexture* backBuf = d3d::get_back_buffer_rt(m_swapchain);

        int32_t width, height;
        TextureInfo backInfo;
        backBuf->getinfo(backInfo, 0);
        width = backInfo.w;
        height = backInfo.h;

        m_gBuffer = eastl::make_unique<render::DeferredRenderTarget>(std::move(shadingResolver), nau::utils::format("{}_{}", "main", m_swapchain).c_str(), width, height, render::DeferredRT::StereoMode::MonoOrMultipass, TEXFMT_DEPTH32);

        {
            lock_(m_resizeMutex);
            m_width = width;
            m_height = height;
            resizeResolutions();
        }

        m_postFXRenderer = eastl::make_unique<render::PostFxRenderer>(matPPTonemap);
        m_outlineRenderer = eastl::make_unique<render::PostFxRenderer>(outlineTonemap);

        createGBufferNodes();

        m_environmentNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "render_envi", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "gbuffer_resolve", m_swapchain).c_str());

            auto resolveTarget = registry.modifyTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            return [resolveTarget, this]()
            {
                if (m_graphicsScene->hasCamera())
                {
                    const CameraNode& c = m_graphicsScene->getMainCamera();
                    m_environmentRenderer->renderSkybox(const_cast<BaseTexture*>(resolveTarget.get()), m_gBuffer->getDepth(), c.getViewMatrix(), c.getProjMatrix());
                }
            };
        }));

        m_uidNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "pixel_extraction", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "billboard_render", m_swapchain).c_str());
            registry.orderMeBefore(nau::utils::format("{}_{}", "grid_render", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            auto uidTex = registry.readTexture(nau::utils::format("{}_{}", "uid_texture", m_swapchain).c_str())
                              .atStage(dabfg::Stage::PS_OR_CS)
                              .useAs(dabfg::Usage::SHADER_RESOURCE)
                              .handle();

            return [this, uidTex]()
            {
                for (auto& request : m_viewportRequests)
                {
                    const auto viewportCoords = math::IVector2{request.viewportX, request.viewportY};
                    shader_globals::setVariable("viewportCoords", &viewportCoords);

                    m_pixelDataExtractionMaterial->setRoTexture("default", "UIDTexture", const_cast<BaseTexture*>(uidTex.get()));
                    m_pixelDataExtractionMaterial->setRoTexture("default", "DepthTexture", m_gBuffer->getDepth());
                    m_pixelDataExtractionMaterial->bind();
                    m_pixelDataExtractionMaterial->dispatch(1, 1, 1);

                    PixelData data;
                    m_pixelDataExtractionMaterial->readRwBuffer("default", "ResultBuffer", &data, sizeof(PixelData));

                    request.promise.resolve(data.uid);
                }

                m_viewportRequests.clear();
            };
        }));

        // Input precedes both the reference single-GPU path and the Mgpu path.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_controls", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "forward_translucent", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);
            return [this]()
            {
#if defined(_WIN32)
                const SHORT dofKey = GetAsyncKeyState(VK_F9);
                const bool dofToggleDown = (dofKey & 0x8000) != 0;
                if ((dofToggleDown && !m_depthOfFieldToggleWasDown) || ((dofKey & 1) && !m_depthOfFieldToggleWasDown))
                {
                    m_depthOfFieldEnabled = !m_depthOfFieldEnabled;
                    m_depthOfFieldDebugEnabled = false;
                    NAU_LOG("DOF enabled: {}", m_depthOfFieldEnabled);
                }
                m_depthOfFieldToggleWasDown = dofToggleDown;

                const SHORT debugKey = GetAsyncKeyState(VK_F10);
                const bool debugToggleDown = (debugKey & 0x8000) != 0;
                if ((debugToggleDown && !m_depthOfFieldDebugToggleWasDown) || ((debugKey & 1) && !m_depthOfFieldDebugToggleWasDown))
                {
                    m_depthOfFieldDebugEnabled = !m_depthOfFieldDebugEnabled;
                    if (m_depthOfFieldDebugEnabled) m_depthOfFieldEnabled = true;
                    NAU_LOG("DOF diagnostic: {}", m_depthOfFieldDebugEnabled);
                }
                m_depthOfFieldDebugToggleWasDown = debugToggleDown;

#if defined(NAU_ENABLE_MGPU)
                const bool multiDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
                if (multiDown && !m_dofMultiGpuToggleWasDown)
                {
                    m_dofMultiGpuEnabled = !m_dofMultiGpuEnabled;
                    m_dofMultiGpuReported = false;
                    NAU_LOG_INFO("DOF multi-GPU requested: {}", m_dofMultiGpuEnabled);
                }
                m_dofMultiGpuToggleWasDown = multiDown;
#endif

#endif
                m_depthOfFieldFocusDistance = dofFocusDistance.load();
                m_depthOfFieldFocusRange = dofFocusRange.load();
            };
        }));

        // GPU 0 preparation in the reference implementation: reverse-Z linearization.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_depth_prepare", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto linearDepth = registry
                                   .createTexture2d(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str(), dabfg::History::No,
                                                    dabfg::Texture2dCreateInfo{
                                                        TEXFMT_R32F | TEXCF_UNORDERED,
                                                        registry.getResolution(m_resolutionName.c_str())})
                                   .atStage(dabfg::Stage::COMPUTE)
                                   .useAs(dabfg::Usage::SHADER_RESOURCE)
                                   .handle();

            registry.orderMeAfter(nau::utils::format("{}_{}", "dof_controls", m_swapchain).c_str());

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled) return;
                d3d::set_render_target();
                d3d::resource_barrier({m_gBuffer->getDepth(), RB_RO_SRV | RB_STAGE_COMPUTE, 0, 0});

                float nearZ = 0.1f;
                float farZ = 1000.0f;
                if (m_graphicsScene->hasCamera())
                {
                    const auto& camera = m_graphicsScene->getMainCamera();
                    nearZ = camera.cameraProperties->getClipNearPlane();
                    farZ = camera.cameraProperties->getClipFarPlane();
                }

                m_depthOfFieldMaterial->setProperty("Depth", "nearZ", nearZ);
                m_depthOfFieldMaterial->setProperty("Depth", "farZ", farZ);
                m_depthOfFieldMaterial->setRoTexture("Depth", "NdcDepth", m_gBuffer->getDepth());
                m_depthOfFieldMaterial->setRwTexture("Depth", "LinearDepthOut", linearDepth.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Depth", linearDepth.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_prime_coc", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto linearDepth = registry.readTexture(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str())
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto coc = registry
                           .createTexture2d(nau::utils::format("{}_{}", "dof_prime_coc_texture", m_swapchain).c_str(), dabfg::History::No,
                                            dabfg::Texture2dCreateInfo{TEXFMT_R16F | TEXCF_UNORDERED,
                                                                       registry.getResolution(m_resolutionName.c_str())})
                           .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled) return;
                float farZ = 1000.0f;
                if (m_graphicsScene->hasCamera()) farZ = m_graphicsScene->getMainCamera().cameraProperties->getClipFarPlane();
                m_depthOfFieldMaterial->setProperty("CoC", "focusDistance", m_depthOfFieldFocusDistance);
                m_depthOfFieldMaterial->setProperty("CoC", "focusRange", m_depthOfFieldFocusRange);
                m_depthOfFieldMaterial->setProperty("CoC", "maxCoC", m_depthOfFieldMaxCoC);
                m_depthOfFieldMaterial->setProperty("CoC", "farZ", farZ);
                m_depthOfFieldMaterial->setRoTexture("CoC", "LinearDepth", const_cast<BaseTexture*>(linearDepth.get()));
                m_depthOfFieldMaterial->setRwTexture("CoC", "CoCOut", coc.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "CoC", coc.get());
            };
        }));

        // GPU0 retains scene/Depth/prime CoC/Gather. GPU1 runs the middle passes.
        // Separate outputs keep failed/inactive transfers away from the local fallback.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_multi_gpu", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "dof_prime_coc", m_swapchain).c_str());
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::COPY).handle();
            auto depth = registry.readTexture(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str())
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::COPY).handle();
            auto filled = registry.createTexture2d(nau::utils::format("{}_{}", "dof_mgpu_filled", m_swapchain).c_str(),
                dabfg::History::No, dabfg::Texture2dCreateInfo{TEXFMT_A16B16G16R16F | TEXCF_UNORDERED,
                    registry.getResolution(m_resolutionName.c_str())})
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::COPY).handle();
            auto filtered = registry.createTexture2d(nau::utils::format("{}_{}", "dof_mgpu_filtered", m_swapchain).c_str(),
                dabfg::History::No, dabfg::Texture2dCreateInfo{TEXFMT_R16F | TEXCF_UNORDERED,
                    registry.getResolution(m_resolutionName.c_str())})
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::COPY).handle();
            return [=, this]()
            {
                // CPU time of the node: the DOF bridge waits on the CPU for both GPUs (dof_bridge.cpp), so this is
                // where a synchronous second-GPU pass shows up in the frame time.
                NAU_CPU_TIMER("dof_multi_gpu");
                m_dofMultiGpuUsed = false;
#if defined(NAU_ENABLE_MGPU) && defined(_WIN32)
                if (!m_depthOfFieldEnabled || !m_dofMultiGpuEnabled || !d3d::get_driver_code().is(d3d::dx12)) return;
                TextureInfo info = {};
                color.get()->getinfo(info, 0);
                void* queue = nullptr;
                d3d::driver_command(DRV3D_COMMAND_GET_RENDERING_COMMAND_QUEUE, &queue, nullptr, nullptr);
                if (m_dofBridge.prepare(d3d::get_device(), queue, info.w, info.h))
                {
                    auto* colorTexture = const_cast<BaseTexture*>(color.get());
                    auto* depthTexture = const_cast<BaseTexture*>(depth.get());
                    // Flush Nau's deferred recording before submitting native commands on its queue.
                    clearDofComputeBindings();
                    d3d::set_render_target();
                    void* native[4] = {};
                    BaseTexture* textures[] = {colorTexture, depthTexture, filled.get(), filtered.get()};
                    bool prepared = true;
                    for (int i = 0; i < 4; ++i)
                    {
                        bool destination = i >= 2;
                        prepared = d3d::driver_command(DRV3D_COMMAND_PREPARE_TEXTURE_FOR_EXTERNAL_COPY,
                            textures[i], &destination, &native[i]) != 0 && prepared;
                    }
                    d3d::finish_render_commands();
                    if (!prepared)
                    {
                        NAU_LOG_WARNING("DOF single-GPU fallback: native texture access unavailable");
                        m_dofMultiGpuEnabled = false;
                        return;
                    }
                    nau::mgpu::bridge::DofParameters parameters;
                    parameters.focusDistance = m_depthOfFieldFocusDistance;
                    parameters.focusRange = m_depthOfFieldFocusRange;
                    parameters.maxCoC = m_depthOfFieldMaxCoC;
                    parameters.filterRadius = m_depthOfFieldFilterRadius;
                    parameters.taps = m_depthOfFieldTaps;
                    if (m_graphicsScene->hasCamera())
                        parameters.farZ = m_graphicsScene->getMainCamera().cameraProperties->getClipFarPlane();
                    const auto result = m_dofBridge.execute(native[0], native[1], native[2], native[3], parameters);
                    if (result == nau::mgpu::bridge::DofResult::PrimaryFailure)
                    {
                        // A lost primary is not recoverable by running more work on that same device.
                        m_depthOfFieldEnabled = false;
                        m_dofMultiGpuEnabled = false;
                        NAU_FAILURE("DOF primary GPU failure: {}", m_dofBridge.lastError());
                        return;
                    }
                    m_dofMultiGpuUsed = result == nau::mgpu::bridge::DofResult::Completed;
                }
                if (!m_dofMultiGpuReported || !m_dofMultiGpuUsed)
                {
                    if (m_dofMultiGpuUsed)
                        NAU_LOG_INFO("DOF multi-GPU active: GPU1 '{}', GPU-only transfers, synchronous frame",
                            m_dofBridge.secondaryAdapterName());
                    else
                        NAU_LOG_WARNING("DOF single-GPU fallback: {}", m_dofBridge.lastError());
                    m_dofMultiGpuReported = true;
                }
                if (!m_dofMultiGpuUsed) m_dofMultiGpuEnabled = false;
#endif
            };
        }));

        // Local GPU1-equivalent block is retained for fallback.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_second_coc", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "dof_multi_gpu", m_swapchain).c_str());
            auto linearDepth = registry.readTexture(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str())
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto coc = registry
                           .createTexture2d(nau::utils::format("{}_{}", "dof_second_coc_texture", m_swapchain).c_str(), dabfg::History::No,
                                            dabfg::Texture2dCreateInfo{TEXFMT_R16F | TEXCF_UNORDERED,
                                                                       registry.getResolution(m_resolutionName.c_str())})
                           .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                float farZ = 1000.0f;
                if (m_graphicsScene->hasCamera()) farZ = m_graphicsScene->getMainCamera().cameraProperties->getClipFarPlane();
                m_depthOfFieldMaterial->setProperty("CoC", "focusDistance", m_depthOfFieldFocusDistance);
                m_depthOfFieldMaterial->setProperty("CoC", "focusRange", m_depthOfFieldFocusRange);
                m_depthOfFieldMaterial->setProperty("CoC", "maxCoC", m_depthOfFieldMaxCoC);
                m_depthOfFieldMaterial->setProperty("CoC", "farZ", farZ);
                m_depthOfFieldMaterial->setRoTexture("CoC", "LinearDepth", const_cast<BaseTexture*>(linearDepth.get()));
                m_depthOfFieldMaterial->setRwTexture("CoC", "CoCOut", coc.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "CoC", coc.get());
            };
        }));

        // Remaining GPU 1 passes: Holes -> Pyramid -> Patch fill -> WMax -> WFilter.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_holes", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "dof_multi_gpu", m_swapchain).c_str());
            auto linearDepth = registry.readTexture(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str())
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto occlusion = registry
                                 .createTexture2d(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str(), dabfg::History::No,
                                                  dabfg::Texture2dCreateInfo{TEXFMT_R8UI | TEXCF_UNORDERED,
                                                                             registry.getResolution(m_resolutionName.c_str())})
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                float farZ = 1000.0f;
                if (m_graphicsScene->hasCamera()) farZ = m_graphicsScene->getMainCamera().cameraProperties->getClipFarPlane();
                m_depthOfFieldMaterial->setProperty("Holes", "focusDistance", m_depthOfFieldFocusDistance);
                m_depthOfFieldMaterial->setProperty("Holes", "focusRange", m_depthOfFieldFocusRange);
                m_depthOfFieldMaterial->setProperty("Holes", "farZ", farZ);
                m_depthOfFieldMaterial->setRoTexture("Holes", "LinearDepth", const_cast<BaseTexture*>(linearDepth.get()));
                m_depthOfFieldMaterial->setRwTexture("Holes", "OcclusionOut", occlusion.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Holes", occlusion.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_pyramid_1", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                             .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto occlusion = registry.readTexture(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str())
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto pyramid = registry
                               .createTexture2d(nau::utils::format("{}_{}", "dof_pyramid_1_texture", m_swapchain).c_str(), dabfg::History::No,
                                                dabfg::Texture2dCreateInfo{TEXFMT_A16B16G16R16F | TEXCF_UNORDERED,
                                                                           registry.getResolution(m_resolutionName.c_str(), 0.5f)})
                               .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setRoTexture("Pyramid1", "ColorIn", const_cast<BaseTexture*>(color.get()));
                m_depthOfFieldMaterial->setRoTexture("Pyramid1", "OcclusionMask", const_cast<BaseTexture*>(occlusion.get()));
                m_depthOfFieldMaterial->setRwTexture("Pyramid1", "PyramidOut", pyramid.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Pyramid1", pyramid.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_pyramid_2", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto pyramid1 = registry.readTexture(nau::utils::format("{}_{}", "dof_pyramid_1_texture", m_swapchain).c_str())
                                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto pyramid2 = registry
                                .createTexture2d(nau::utils::format("{}_{}", "dof_pyramid_2_texture", m_swapchain).c_str(), dabfg::History::No,
                                                 dabfg::Texture2dCreateInfo{TEXFMT_A16B16G16R16F | TEXCF_UNORDERED,
                                                                            registry.getResolution(m_resolutionName.c_str(), 0.25f)})
                                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setRoTexture("Pyramid2", "PyramidIn", const_cast<BaseTexture*>(pyramid1.get()));
                m_depthOfFieldMaterial->setRwTexture("Pyramid2", "PyramidOut", pyramid2.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Pyramid2", pyramid2.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_patch_2", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str()).atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto occlusion = registry.readTexture(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str())
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filled2 = registry
                               // Match the reference: Patch2 overwrites the quarter-resolution pyramid.
                               .renameTexture(nau::utils::format("{}_{}", "dof_pyramid_2_texture", m_swapchain).c_str(),
                                              nau::utils::format("{}_{}", "dof_filled_2", m_swapchain).c_str(), dabfg::History::No)
                               .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setRoTexture("Patch2", "ColorIn", const_cast<BaseTexture*>(color.get()));
                m_depthOfFieldMaterial->setRoTexture("Patch2", "OcclusionMask", const_cast<BaseTexture*>(occlusion.get()));
                m_depthOfFieldMaterial->setRwTexture("Patch2", "FilledOut", filled2.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Patch2", filled2.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_patch_1", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str()).atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filled2 = registry.readTexture(nau::utils::format("{}_{}", "dof_filled_2", m_swapchain).c_str())
                              .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto occlusion = registry.readTexture(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str())
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filled1 = registry
                               // Match the reference: Patch1 overwrites the half-resolution pyramid.
                               .renameTexture(nau::utils::format("{}_{}", "dof_pyramid_1_texture", m_swapchain).c_str(),
                                              nau::utils::format("{}_{}", "dof_filled_1", m_swapchain).c_str(), dabfg::History::No)
                               .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setRoTexture("Patch1", "ColorIn", const_cast<BaseTexture*>(color.get()));
                m_depthOfFieldMaterial->setRoTexture("Patch1", "CoarseFilled", const_cast<BaseTexture*>(filled2.get()));
                m_depthOfFieldMaterial->setRoTexture("Patch1", "OcclusionMask", const_cast<BaseTexture*>(occlusion.get()));
                m_depthOfFieldMaterial->setRwTexture("Patch1", "FilledOut", filled1.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Patch1", filled1.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_patch_0", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                             .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filled1 = registry.readTexture(nau::utils::format("{}_{}", "dof_filled_1", m_swapchain).c_str())
                              .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto occlusion = registry.readTexture(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str())
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto colorFilled = registry
                                   .createTexture2d(nau::utils::format("{}_{}", "dof_color_filled", m_swapchain).c_str(), dabfg::History::No,
                                                    dabfg::Texture2dCreateInfo{TEXFMT_A16B16G16R16F | TEXCF_UNORDERED,
                                                                               registry.getResolution(m_resolutionName.c_str())})
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setRoTexture("Patch0", "ColorIn", const_cast<BaseTexture*>(color.get()));
                m_depthOfFieldMaterial->setRoTexture("Patch0", "CoarseFilled", const_cast<BaseTexture*>(filled1.get()));
                m_depthOfFieldMaterial->setRoTexture("Patch0", "OcclusionMask", const_cast<BaseTexture*>(occlusion.get()));
                m_depthOfFieldMaterial->setRwTexture("Patch0", "FilledOut", colorFilled.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Patch0", colorFilled.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_wmax", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto occlusion = registry.readTexture(nau::utils::format("{}_{}", "dof_occlusion", m_swapchain).c_str())
                                 .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto coc = registry.readTexture(nau::utils::format("{}_{}", "dof_second_coc_texture", m_swapchain).c_str())
                           .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto wmax = registry
                            .createTexture2d(nau::utils::format("{}_{}", "dof_wmax_texture", m_swapchain).c_str(), dabfg::History::No,
                                             dabfg::Texture2dCreateInfo{TEXFMT_R16F | TEXCF_UNORDERED,
                                                                        registry.getResolution(m_resolutionName.c_str())})
                            .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setProperty("WMax", "maxCoC", m_depthOfFieldMaxCoC);
                m_depthOfFieldMaterial->setProperty("WMax", "dofTaps", m_depthOfFieldTaps);
                m_depthOfFieldMaterial->setRoTexture("WMax", "OcclusionMask", const_cast<BaseTexture*>(occlusion.get()));
                m_depthOfFieldMaterial->setRoTexture("WMax", "CoC", const_cast<BaseTexture*>(coc.get()));
                m_depthOfFieldMaterial->setRwTexture("WMax", "WMaxOut", wmax.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "WMax", wmax.get());
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "dof_wfilter", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto wmax = registry.readTexture(nau::utils::format("{}_{}", "dof_wmax_texture", m_swapchain).c_str())
                            .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filtered = registry
                                .createTexture2d(nau::utils::format("{}_{}", "dof_wfiltered", m_swapchain).c_str(), dabfg::History::No,
                                                 dabfg::Texture2dCreateInfo{TEXFMT_R16F | TEXCF_UNORDERED,
                                                                            registry.getResolution(m_resolutionName.c_str())})
                                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            return [=, this]()
            {
                if (!m_depthOfFieldEnabled || m_dofMultiGpuUsed) return;
                m_depthOfFieldMaterial->setProperty("WFilter", "filterRadius", m_depthOfFieldFilterRadius);
                m_depthOfFieldMaterial->setRoTexture("WFilter", "WMaxIn", const_cast<BaseTexture*>(wmax.get()));
                m_depthOfFieldMaterial->setRwTexture("WFilter", "WFilteredOut", filtered.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "WFilter", filtered.get());
            };
        }));

        // GPU 0 gather in the reference implementation. ColorFilled/WFiltered are
        // the two resources that the team's cross-adapter layer will return here.
        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "depth_of_field", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto color = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                             .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto linearDepth = registry.readTexture(nau::utils::format("{}_{}", "dof_linear_depth", m_swapchain).c_str())
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto coc = registry.readTexture(nau::utils::format("{}_{}", "dof_prime_coc_texture", m_swapchain).c_str())
                           .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto colorFilled = registry.readTexture(nau::utils::format("{}_{}", "dof_color_filled", m_swapchain).c_str())
                                   .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto filtered = registry.readTexture(nau::utils::format("{}_{}", "dof_wfiltered", m_swapchain).c_str())
                                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto dofTexture = registry
                                  .createTexture2d(nau::utils::format("{}_{}", "dof_texture", m_swapchain).c_str(), dabfg::History::No,
                                                   dabfg::Texture2dCreateInfo{TEXFMT_A16B16G16R16F | TEXCF_UNORDERED,
                                                                              registry.getResolution(m_resolutionName.c_str())})
                                  .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            auto multiFilled = registry.readTexture(nau::utils::format("{}_{}", "dof_mgpu_filled", m_swapchain).c_str())
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            auto multiFiltered = registry.readTexture(nau::utils::format("{}_{}", "dof_mgpu_filtered", m_swapchain).c_str())
                .atStage(dabfg::Stage::COMPUTE).useAs(dabfg::Usage::SHADER_RESOURCE).handle();

            auto executeGather = [=, this]()
            {
                if (!m_depthOfFieldEnabled) return;


                if (m_depthOfFieldDebugEnabled)
                {
                    float farZ = 1000.0f;
                    if (m_graphicsScene->hasCamera()) farZ = m_graphicsScene->getMainCamera().cameraProperties->getClipFarPlane();
                    m_depthOfFieldMaterial->setProperty("Debug", "focusDistance", m_depthOfFieldFocusDistance);
                    m_depthOfFieldMaterial->setProperty("Debug", "focusRange", m_depthOfFieldFocusRange);
                    m_depthOfFieldMaterial->setProperty("Debug", "maxCoC", m_depthOfFieldMaxCoC);
                    m_depthOfFieldMaterial->setProperty("Debug", "farZ", farZ);
                    m_depthOfFieldMaterial->setRoTexture("Debug", "LinearDepth", const_cast<BaseTexture*>(linearDepth.get()));
                    m_depthOfFieldMaterial->setRoTexture("Debug", "CoC", const_cast<BaseTexture*>(coc.get()));
                    m_depthOfFieldMaterial->setRoTexture("Debug", "WFiltered", const_cast<BaseTexture*>(
                        m_dofMultiGpuUsed ? multiFiltered.get() : filtered.get()));
                    m_depthOfFieldMaterial->setRwTexture("Debug", "DofOut", dofTexture.get());
                    dispatchDofPass(*m_depthOfFieldMaterial, "Debug", dofTexture.get());
                    return;
                }

                m_depthOfFieldMaterial->setProperty("Gather", "maxCoC", m_depthOfFieldMaxCoC);
                m_depthOfFieldMaterial->setProperty("Gather", "dofTaps", m_depthOfFieldTaps);
                m_depthOfFieldMaterial->setRoTexture("Gather", "ColorIn", const_cast<BaseTexture*>(color.get()));
                m_depthOfFieldMaterial->setRoTexture("Gather", "ColorFilled", const_cast<BaseTexture*>(
                    m_dofMultiGpuUsed ? multiFilled.get() : colorFilled.get()));
                m_depthOfFieldMaterial->setRoTexture("Gather", "WFiltered", const_cast<BaseTexture*>(
                    m_dofMultiGpuUsed ? multiFiltered.get() : filtered.get()));
                m_depthOfFieldMaterial->setRoTexture("Gather", "CoC", const_cast<BaseTexture*>(coc.get()));
                m_depthOfFieldMaterial->setRwTexture("Gather", "DofOut", dofTexture.get());
                dispatchDofPass(*m_depthOfFieldMaterial, "Gather", dofTexture.get());
            };
            // Eight texture handles exceed the graph's 128-byte inline callback storage.
            // Allocate once when the node is declared; each frame only invokes the callback.
            return [executeGather = eastl::make_unique<decltype(executeGather)>(eastl::move(executeGather))]()
            {
                (*executeGather)();
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "post_fx_nodes", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto resolveTexture = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                                      .atStage(dabfg::Stage::PS_OR_CS)
                                      .useAs(dabfg::Usage::SHADER_RESOURCE)
                                      .handle();
            auto dofTexture = registry.readTexture(nau::utils::format("{}_{}", "dof_texture", m_swapchain).c_str())
                                  .atStage(dabfg::Stage::PS_OR_CS)
                                  .useAs(dabfg::Usage::SHADER_RESOURCE)
                                  .handle();

            registry.orderMeAfter(nau::utils::format("{}_{}", "depth_of_field", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [=, this]()
            {
                setRenderTarget();
                d3d::clearview(CLEAR_TARGET | CLEAR_ZBUFFER | CLEAR_STENCIL, nau::math::E3DCOLOR(0, 0, 0), 0, 0);
                d3d::set_srgb_backbuffer_write(true);

                if (m_depthOfFieldEnabled)
                {
                    d3d::settex(0, const_cast<BaseTexture*>(dofTexture.get()));
                    m_postFXRenderer->render("Regular");
                }
                else
                {
                    // This is intentionally identical to Nau's original post-process.
                    // All DOF dispatches are skipped while disabled.
                    d3d::settex(0, const_cast<BaseTexture*>(resolveTexture.get()));
                    m_postFXRenderer->render("Regular");
                }

                d3d::set_srgb_backbuffer_write(false);
            };
        }));


        m_uidNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "billboard_render", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "post_fx_nodes", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            auto uidTarget = registry.readTexture(nau::utils::format("{}_{}", "uid_texture", m_swapchain).c_str())
                  .atStage(dabfg::Stage::PS_OR_CS)
                  .useAs(dabfg::Usage::SHADER_RESOURCE)
                  .handle();

            return [=, this]()
            {
                setRenderTarget();
                d3d::set_depth(m_gBuffer->getDepth(), DepthAccess::RW);
                d3d::set_render_target(1, const_cast<BaseTexture*>(uidTarget.get()), 0);
                m_graphicsScene->renderBillboards();
            };
        }));

        m_uidNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "grid_render", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "billboard_render", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [=, this]()
            {
                if (m_drawViewportGrid && m_graphicsScene->hasMainCamera())
                {
                    setRenderTarget();
                    d3d::set_depth(m_gBuffer->getDepth(), DepthAccess::RW);

                    auto& activeCamera = m_graphicsScene->getMainCamera();
                    const nau::math::Matrix4 viewProjectionMatrix = activeCamera.getViewProjectionMatrix();
                    shader_globals::setVariable("worldViewPos", &activeCamera.worldPosition);
                    shader_globals::setVariable("vp", &viewProjectionMatrix);
                    m_gridMaterial->bind();
                    d3d::draw(PRIM_TRISTRIP, 0, 2);
                }
            };
        }));

        m_debugNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "debug_render", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "grid_render", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [this]()
            {
                setRenderTarget();
                d3d::set_srgb_backbuffer_write(false);

                if (m_graphicsScene)
                {
                    d3d::set_depth(m_gBuffer->getDepth(), DepthAccess::RW);
                    m_graphicsScene->renderSceneDebug();
                }
            };
        }));

        m_nauGuiNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "nau_gui", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "debug_render", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [this]()
            {
                if (nau::getServiceProvider().has<nau::ui::UiManager>())
                {
                    BaseTexture* backBuf = d3d::get_back_buffer_rt(m_swapchain);
                    nau::getServiceProvider().get<nau::ui::UiManager>().render(backBuf);
                }
            };
        }));

        if (m_swapchain == DEFAULT_SWAPID)
        {
            m_debugNodes.addNode(
                dabfg::register_node(nau::utils::format("{}_{}", "debug_gui", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
            {
                registry.orderMeAfter(nau::utils::format("{}_{}", "nau_gui", m_swapchain).c_str());
                registry.executionHas(dabfg::SideEffects::External);

                imgui_switch_state();
                imgui_update();  // invoke init on demand
                imgui_endframe();
                return [this]()
                {
                    setRenderTarget();
                    d3d::set_srgb_backbuffer_write(false);

                    imgui_render_copied_data();
                };
            }));
        }

        createOutlineNodes();
        initializeMgpu();
    }

}  // namespace nau::render
