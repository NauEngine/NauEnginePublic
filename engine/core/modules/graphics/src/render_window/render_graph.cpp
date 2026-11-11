// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <nau/assets/asset_manager.h>

#include "graphics_impl.h"
#include "nau/gui/dag_imgui.h"
#include "nau/gui/imguiInput.h"
#include "nau/ui.h"
#include "render_window_impl.h"


namespace nau::render
{

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

        async::TaskCollection loadTasks;

        MaterialAssetView::Ptr matPPTonemap;
        loadTasks.push(openMaterialAsset({"file:/res/materials/pp_tonemap.nmat_json"}, matPPTonemap));

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

        loadTasks.push(openMaterialAsset({"file:/res/materials/pp_deferred_resolve.nmat_json"}, m_resolveMaterial));

        co_await loadTasks.awaitCompletion();

        m_resolveMaterial->enableAutoSetTextures(false);
        matPPTonemap->enableAutoSetTextures(false);

        m_environmentRenderer = eastl::make_unique<render::EnvironmentRenderer>(
            matSkybox, panoramaToCubemapComputeShader, genIrradianceMapComputeShader, genReflectionMapComputeShader);
        m_environmentRenderer->setPanoramaTexture(hdrPanorama);

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

        m_mainPass.initialize(m_resolveMaterial, width, height);

        {
            lock_(m_resizeMutex);
            m_width = width;
            m_height = height;
            resizeResolutions();
        }

        m_postFXRenderer = eastl::make_unique<render::PostFxRenderer>(matPPTonemap);
        m_outlineRenderer = eastl::make_unique<render::PostFxRenderer>(outlineTonemap);

        createGBufferNodes(m_mainPass, m_gBufferNodes);
        createEnvironmentNode();
        createSkyboxNode(m_mainPass, m_environmentNodes);

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
                    m_pixelDataExtractionMaterial->setRoTexture("default", "DepthTexture", m_mainPass.gBuffer->getDepth());
                    m_pixelDataExtractionMaterial->bind();
                    m_pixelDataExtractionMaterial->dispatch(1, 1, 1);

                    PixelData data;
                    m_pixelDataExtractionMaterial->readRwBuffer("default", "ResultBuffer", &data, sizeof(PixelData));

                    request.promise.resolve(data.uid);
                }

                m_viewportRequests.clear();
            };
        }));

        m_postFxNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "post_fx_nodes", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            auto resolveTexture = registry.readTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                                      .atStage(dabfg::Stage::PS_OR_CS)
                                      .useAs(dabfg::Usage::SHADER_RESOURCE)
                                      .handle();

            registry.orderMeAfter(nau::utils::format("{}_{}", "forward_translucent", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [=, this]()
            {
                setRenderTarget();
                d3d::clearview(CLEAR_TARGET, math::E3DCOLOR(0, 0, 0), 0, 0);
                if (m_graphicsScene && m_graphicsScene->hasMainCamera())
                {
                    tonemap(const_cast<BaseTexture*>(resolveTexture.get()));
                }
            };
        }));


        m_uidNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "billboard_render", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "post_fx_nodes", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            auto uidTarget = registry.modifyTexture(nau::utils::format("{}_{}", "uid_texture", m_swapchain).c_str())
                  .atStage(dabfg::Stage::POST_RASTER)
                  .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                  .handle();

            return [=, this]()
            {
                setRenderTarget();
                d3d::set_depth(m_mainPass.gBuffer->getDepth(), DepthAccess::RW);
                d3d::set_render_target(1, uidTarget.get(), 0);
                m_graphicsScene->renderBillboards(m_mainPass.view);
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
                    d3d::set_depth(m_mainPass.gBuffer->getDepth(), DepthAccess::RW);

                    const auto& activeCamera = m_mainPass.view.camera;
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

                if (m_graphicsScene && m_graphicsScene->hasMainCamera())
                {
                    d3d::set_depth(m_mainPass.gBuffer->getDepth(), DepthAccess::RW);
                    m_graphicsScene->renderSceneDebug(m_mainPass.view);
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
    }

    void RenderWindowImpl::createSkyboxNode(ScenePass& pass, GraphNodes& nodes)
    {
        nodes.addNode(
            dabfg::register_node(pass.name("render_envi").c_str(), DABFG_PP_NODE_SRC, [this, &pass](dabfg::Registry registry)
        {
            registry.orderMeAfter(pass.name("gbuffer_resolve").c_str());

            auto resolveTarget = registry.modifyTexture(pass.name("resolve_texture").c_str())
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            return [resolveTarget, this, &pass]()
            {
                const CameraNode& camera = pass.view.camera;
                m_environmentRenderer->renderSkybox(const_cast<BaseTexture*>(resolveTarget.get()), pass.gBuffer->getDepth(),
                    camera.getViewMatrix(), camera.getProjMatrix());
            };
        }));
    }

    void RenderWindowImpl::tonemap(BaseTexture* source)
    {
        d3d::set_srgb_backbuffer_write(true);
        d3d::settex(0, source);
        m_postFXRenderer->render("Regular");
        d3d::settex(0, nullptr);
        d3d::set_srgb_backbuffer_write(false);
    }

    void RenderWindowImpl::createPreviewGraph(int width, int height)
    {
        m_previewPass.suffix = m_mainPass.name("preview");
        m_previewPass.resolution = m_previewPass.name("display");
        dabfg::set_resolution(m_previewPass.resolution.c_str(), {width, height});
        m_previewPass.initialize(m_resolveMaterial, width, height);
        createGBufferNodes(m_previewPass, m_previewNodes);
        createSkyboxNode(m_previewPass, m_previewNodes);

        TextureInfo backInfo;
        d3d::get_back_buffer_rt(m_swapchain)->getinfo(backInfo);
        const auto format = backInfo.cflg & TEXFMT_MASK;
        m_previewNodes.addNode(dabfg::register_node(m_previewPass.name("tonemap").c_str(), DABFG_PP_NODE_SRC,
            [this, format](dabfg::Registry registry)
        {
            auto hdr = registry.readTexture(m_previewPass.name("resolve_texture").c_str())
                .atStage(dabfg::Stage::PS_OR_CS).useAs(dabfg::Usage::SHADER_RESOURCE).handle();
            const bool srgb = format == TEXFMT_A8R8G8B8 || format == TEXFMT_R8G8B8A8;
            auto color = registry.createTexture2d(m_previewPass.name("tonemapped").c_str(), dabfg::History::No,
                {format | TEXCF_RTARGET | (srgb ? TEXCF_SRGBWRITE : 0u), registry.getResolution(m_previewPass.resolution.c_str())})
                .atStage(dabfg::Stage::POST_RASTER).useAs(dabfg::Usage::COLOR_ATTACHMENT).handle();
            return [this, hdr, color]
            {
                d3d::set_render_target();
                d3d::set_render_target(color.get(), 0);
                d3d::clearview(CLEAR_TARGET, math::E3DCOLOR(0, 0, 0), 0, 0);
                tonemap(const_cast<BaseTexture*>(hdr.get()));
            };
        }));

        m_previewNodes.addNode(dabfg::register_node(m_previewPass.name("display_color").c_str(), DABFG_PP_NODE_SRC,
            [this, format](dabfg::Registry registry)
        {
            auto tonemapped = registry.readTexture(m_previewPass.name("tonemapped").c_str())
                .atStage(dabfg::Stage::TRANSFER).useAs(dabfg::Usage::COPY).handle();
            auto display = registry.createTexture2d(m_previewPass.name("display_texture").c_str(), dabfg::History::No,
                {format | TEXCF_RTARGET, registry.getResolution(m_previewPass.resolution.c_str())})
                .atStage(dabfg::Stage::TRANSFER).useAs(dabfg::Usage::COPY).handle();
            return [tonemapped, display]
            {
                // Preserve the display-encoded bytes for billboard rendering with sRGB writes disabled.
                display.get()->update(const_cast<BaseTexture*>(tonemapped.get()));
            };
        }));

        m_previewNodes.addNode(dabfg::register_node(m_previewPass.name("billboards").c_str(), DABFG_PP_NODE_SRC,
            [this](dabfg::Registry registry)
        {
            auto display = registry.modifyTexture(m_previewPass.name("display_texture").c_str())
                .atStage(dabfg::Stage::POST_RASTER).useAs(dabfg::Usage::COLOR_ATTACHMENT).handle();
            return [this, display]
            {
                d3d::set_render_target();
                d3d::set_render_target(display.get(), 0);
                d3d::set_depth(m_previewPass.gBuffer->getDepth(), DepthAccess::RW);
                m_previewPass.scene->renderBillboards(m_previewPass.view, m_previewPass.excludedComponents);
            };
        }));

        m_previewNodes.addNode(dabfg::register_node(m_previewPass.name("composite").c_str(), DABFG_PP_NODE_SRC,
            [this](dabfg::Registry registry)
        {
            registry.executionHas(dabfg::SideEffects::External);
            registry.orderMeBefore(m_mainPass.name("nau_gui").c_str());
            auto color = registry.readTexture(m_previewPass.name("display_texture").c_str())
                .atStage(dabfg::Stage::TRANSFER).useAs(dabfg::Usage::BLIT).handle();
            return [this, color]
            {
                BaseTexture* backBuffer = d3d::get_back_buffer_rt(m_swapchain);
                setRenderTarget();
                const auto& preview = *m_framePreview;
                math::RectInt dst{preview.x, preview.y, preview.x + preview.width, preview.y + preview.height};
                d3d::stretch_rect(const_cast<BaseTexture*>(color.get()), backBuffer, nullptr, &dst);
            };
        }));
    }
}  // namespace nau::render
