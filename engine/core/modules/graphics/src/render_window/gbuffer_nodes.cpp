// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "graphics_assets/texture_asset.h"
#include "nau/gui/imguiInput.h"
#include "nau/image/dag_texPixel.h"
#include "nau/shaders/shader_globals.h"
#include "nau/shaders/dag_renderStateId.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/gpu_profiler/gpu_profiler.h"
#include "render/daBfg/bfg.h"
#include "render_window_impl.h"


namespace nau::render
{
    void RenderWindowImpl::createGBufferNodes()
    {
        m_gBufferNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "z-prepass", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeBefore(nau::utils::format("{}_{}", "fill_gbuffer", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            auto uidTarget = registry
                                 .createTexture2d(nau::utils::format("{}_{}", "uid_texture", m_swapchain).c_str(),
                                                  dabfg::History::ClearZeroOnFirstFrame,
                                                  dabfg::Texture2dCreateInfo{TEXFMT_A32B32G32R32UI | TEXCF_RTARGET,
                                                                             registry.getResolution(m_resolutionName.c_str())})
                                 .atStage(dabfg::Stage::POST_RASTER)
                                 .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                 .handle();

            return [=, this]()
            {
                d3d::set_render_target(const_cast<BaseTexture*>(uidTarget.get()), 0);
                d3d::set_depth(m_gBuffer->getDepth(), DepthAccess::RW);
                d3d::clearview(CLEAR_TARGET | CLEAR_ZBUFFER | CLEAR_STENCIL, nau::math::E3DCOLOR(0, 0, 0, 0), 0, 0);

                m_graphicsScene->renderDepth();
                d3d::set_depth(nullptr, DepthAccess::RW);
            };
        }));

        m_gBufferNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "fill_gbuffer", m_swapchain).c_str(), DABFG_PP_NODE_SRC,
                                                    [=, this](dabfg::Registry registry)
        {
            registry.executionHas(dabfg::SideEffects::External);

            const auto worldViewPos = math::Vector4{};
            shader_globals::setVariable("worldViewPos", &worldViewPos);

            return [this]()
            {
                if (!m_graphicsScene->hasCamera())
                {
                    return;
                }
                d3d::set_srgb_backbuffer_write(false);
                m_gBuffer->setRt();
                d3d::clearview(CLEAR_TARGET | CLEAR_STENCIL, nau::math::E3DCOLOR(0, 0, 0), 0, 0);
                
                const auto worldViewPos = math::Vector4(m_graphicsScene->getMainCamera().getProperties().getTranslation());
                shader_globals::setVariable("worldViewPos", &worldViewPos);

                m_graphicsScene->renderFrame(true);
                d3d::set_depth(nullptr, DepthAccess::RW);
            };
        }));


        m_csmNode = dabfg::register_node(nau::utils::format("{}_{}", "CSM", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "fill_gbuffer", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [this]() {
                // CPU time of the node on the render thread: with two GPUs it includes collecting the casters for the
                // second GPU; the GPU time of the node is measured by the framegraph executor.
                NAU_CPU_TIMER("CSM");

                if (!m_graphicsScene->hasMainCamera())
                {
                    return;
                }

                nau::CameraNode& camera = m_graphicsScene->getMainCamera();
                nau::csm::CascadeShadows::ModeSettings mode;
                mode.powWeight = 0.985;
                mode.maxDist = camera.cameraProperties->getClipFarPlane();
                mode.shadowStart = camera.cameraProperties->getClipNearPlane();
                mode.numCascades = 4;

                nau::math::Vector3 lightDir = nau::math::Vector3(1,1,1);

                if (m_graphicsScene->hasDirectionalLight())
                {
                    auto light = m_graphicsScene->getDirectionalLights()[0];
                    lightDir = -light.m_direction;
                    mode.numCascades = std::min(light.m_csmCascadesCount, 4u);
                    mode.powWeight = light.m_csmPowWeight;

                    m_csm->setCascadeWidth(light.m_csmSize);

                    if (!light.m_castShadows)
                    {
                        mode.numCascades = 0;
                    }
                }
                else
                {
                    mode.numCascades = 0;
                }

                if (length(lightDir) > MATH_SMALL_NUMBER)
                {
                    lightDir = Vectormath::SSE::normalize(lightDir);
                }

                nau::math::Vector3 cameraPos = camera.worldPosition;
                nau::math::Matrix4 view = camera.getViewMatrix();
                nau::math::Matrix4 proj = camera.getProjMatrix();
                nau::math::Matrix4 globtm = proj * view;

                auto nearZ = camera.cameraProperties->getClipNearPlane();
                auto farZ  = camera.cameraProperties->getClipFarPlane();
                nau::math::NauFrustum frustum;
                frustum.construct(globtm);

                m_csm->prepareShadowCascades(mode, lightDir, view, cameraPos, proj,
                    frustum, nau::math::Vector2(nearZ, farZ), nearZ);

#if defined(NAU_ENABLE_MGPU)
                // With two GPUs the second one draws the cascades and its newest atlas is already in Nau's atlas.
                bool atlasFromSecondGpu = false;
                {
                    NAU_CPU_TIMER("CSM/mgpu");
                    atlasFromSecondGpu = renderMgpuShadows();
                }
#else
                const bool atlasFromSecondGpu = false;
#endif
                if (!atlasFromSecondGpu)
                {
                    m_csm->renderShadowsCascades();
                }

                BaseTexture* backBuf = d3d::get_back_buffer_rt(m_swapchain);

                int32_t width, height;
                TextureInfo backInfo;
                backBuf->getinfo(backInfo, 0);
                d3d::setview(0, 0, backInfo.w, backInfo.h, 0.0f, 1.0f);
            };
        });

        m_gBufferNodes.addNode(
            dabfg::register_node(nau::utils::format("{}_{}", "gbuffer_resolve", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "CSM", m_swapchain).c_str());

            auto resolveTarget = registry
                                     .createTexture2d(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str(), dabfg::History::ClearZeroOnFirstFrame,
                                                      dabfg::Texture2dCreateInfo{
                                                          TEXFMT_A16B16G16R16F | TEXCF_RTARGET, registry.getResolution(m_resolutionName.c_str())})
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            return [resolveTarget, this]()
            {
                if (m_graphicsScene->hasCamera())
                {
                    const CameraNode& c = m_graphicsScene->getMainCamera();
                    if (m_graphicsScene->hasDirectionalLight())
                    {
                        auto lights = m_graphicsScene->getDirectionalLights();
                        
                        const auto lightDirection = math::Vector4(-lights[0].m_direction, 0.0f);
                        const auto lightColorIntensity = math::Vector4(lights[0].m_color.r, lights[0].m_color.g, lights[0].m_color.b, lights[0].m_intensity);
                        
                        shader_globals::setVariable("lightDirection", &lightDirection);
                        shader_globals::setVariable("lightColorIntensity", &lightColorIntensity);
                    }
                    else
                    {
                        const auto lightDirection = math::Vector4(1, 1, 1, 0);
                        const auto lightColorIntensity = math::Vector4(0);

                        shader_globals::setVariable("lightDirection", &lightDirection);
                        shader_globals::setVariable("lightColorIntensity", &lightColorIntensity);
                    }

                    float envIntensity = 1.0f;
                    if (m_graphicsScene->hasEnvironmentNode())
                    {
                        auto& envNode = m_graphicsScene->getEnvironmentNode();
                        envIntensity = envNode.envIntensity;
                    }
                    const auto envIntensityVec = math::Vector4(envIntensity, 0, 0, 0);
                    shader_globals::setVariable("envIntensity", &envIntensityVec);

#if defined(NAU_ENABLE_MGPU)
                    if (m_mgpuShadows.hasAtlas)
                    {
                        // The atlas of the second GPU goes with the cascade matrices of the frame it was drawn for.
                        m_csm->setCascadesToShader(m_gBuffer->getResolveShading()->getMaterial(), m_mgpuShadows.atlasSnapshot);
                    }
                    else
#endif
                    {
                        m_csm->setCascadesToShader(m_gBuffer->getResolveShading()->getMaterial());
                    }
                    m_gBuffer->resolve(resolveTarget.get(),
                                       m_environmentRenderer->getIrradianceMap(),
                                       m_environmentRenderer->getReflectionMap(),
                                       c.getViewProjectionMatrix());

                    TextureInfo info;
                    resolveTarget.get()->getinfo(info, 0);

                    auto screen_pos_to_texcoord =math::Vector4(1.f / info.w, 1.f / info.h, HALF_TEXEL_OFSF / info.w, HALF_TEXEL_OFSF / info.w);
                    shader_globals::setVariable("screen_pos_to_texcoord", &screen_pos_to_texcoord);

                    d3d::set_render_target(const_cast<BaseTexture*>(resolveTarget.get()), 0);
                    m_graphicsScene->renderLights();

                    m_gBuffer->flushResolve();

                }
            };
        }));

        m_gBufferNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "compute_env_cubemaps", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeBefore(nau::utils::format("{}_{}", "gbuffer_resolve", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [this]()
            {
                if (m_graphicsScene->hasEnvironmentNode())
                {
                    auto& node = m_graphicsScene->getEnvironmentNode();
                    if (node.isDirty)
                    {
                        node.isDirty = false;
                        m_environmentRenderer->setPanoramaTexture(node.textureView);
                    }
                }
                if (m_graphicsScene->hasCamera() && m_environmentRenderer->isEnvCubemapsDirty())
                {
                    m_environmentRenderer->convertPanoramaToCubemap();

                    m_environmentRenderer->generateIrradianceMap();
                    m_environmentRenderer->generateReflectionMap();

                    m_environmentRenderer->setEnvCubemapsDirty(false);
                }
            };
        }));

        m_gBufferNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "forward_translucent", m_swapchain).c_str(), DABFG_PP_NODE_SRC,
                                                    [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "render_envi", m_swapchain).c_str());

            auto resolveTarget = registry.modifyTexture(nau::utils::format("{}_{}", "resolve_texture", m_swapchain).c_str())
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            shaders::RenderState translucentRendState;
            shaders::DriverRenderStateId drvTranslucentRendStateId;

            translucentRendState.cull = CULL_CCW;
            translucentRendState.zwrite = false;
            translucentRendState.independentBlendEnabled = false;
            translucentRendState.blendParams[0].ablend = 1;
            translucentRendState.blendParams[0].blendOp = BLENDOP_ADD;
            translucentRendState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
            translucentRendState.blendParams[0].ablendFactors.dst = BLEND_INVSRCALPHA;

            drvTranslucentRendStateId = d3d::create_render_state(translucentRendState);

            shaders::RenderState additiveRendState;
            shaders::DriverRenderStateId drvAdditiveRendStateId;

            additiveRendState.cull = CULL_CCW;
            additiveRendState.zwrite = false;
            additiveRendState.independentBlendEnabled = false;
            additiveRendState.blendParams[0].ablend = 1;
            additiveRendState.blendParams[0].blendOp = BLENDOP_ADD;
            additiveRendState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
            additiveRendState.blendParams[0].ablendFactors.dst = BLEND_ONE;

            drvAdditiveRendStateId = d3d::create_render_state(additiveRendState);

            return [this, resolveTarget]()
            {
                d3d::set_render_target(const_cast<BaseTexture*>(resolveTarget.get()), 0);
                d3d::set_depth(m_gBuffer->getDepth(), DepthAccess::RW);

                m_graphicsScene->renderTranslucency();
            };
        }));
    }

}  // namespace nau::render
