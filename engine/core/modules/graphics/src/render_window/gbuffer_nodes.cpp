// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/shaders/shader_globals.h"
#include "render/daBfg/bfg.h"
#include "render_window_impl.h"


namespace nau::render
{
    void RenderWindowImpl::createGBufferNodes(ScenePass& pass, GraphNodes& nodes)
    {
        nodes.addNode(dabfg::register_node(pass.name("z-prepass").c_str(), DABFG_PP_NODE_SRC, [this, &pass](dabfg::Registry registry)
        {
            registry.orderMeBefore(pass.name("fill_gbuffer").c_str());
            registry.executionHas(dabfg::SideEffects::External);

            std::optional<dabfg::VirtualResourceHandle<BaseTexture, true, false>> uidTarget;
            if (&pass == &m_mainPass)
            {
                uidTarget = registry.createTexture2d(pass.name("uid_texture").c_str(), dabfg::History::ClearZeroOnFirstFrame,
                    {TEXFMT_A32B32G32R32UI | TEXCF_RTARGET, registry.getResolution(pass.resolution.c_str())})
                    .atStage(dabfg::Stage::POST_RASTER).useAs(dabfg::Usage::COLOR_ATTACHMENT).handle();
            }
            else
            {
                registry.orderMeAfter(m_mainPass.name("draw_outline").c_str());
                registry.orderMeAfter(m_mainPass.name("debug_render").c_str());
            }
            return [uidTarget, &pass]
            {
                pass.renderDepth(uidTarget ? uidTarget->get() : nullptr);
            };
        }));

        nodes.addNode(dabfg::register_node(pass.name("fill_gbuffer").c_str(), DABFG_PP_NODE_SRC,
            [&pass](dabfg::Registry registry)
        {
            registry.executionHas(dabfg::SideEffects::External);

            const auto worldViewPos = math::Vector4{};
            shader_globals::setVariable("worldViewPos", &worldViewPos);

            return [&pass]()
            {
                d3d::set_srgb_backbuffer_write(false);
                pass.gBuffer->setRt();
                d3d::clearview(CLEAR_TARGET | CLEAR_STENCIL, nau::math::E3DCOLOR(0, 0, 0), 0, 0);

                const auto worldViewPos = math::Vector4(pass.view.camera.worldPosition);
                shader_globals::setVariable("worldViewPos", &worldViewPos);

                pass.scene->renderFrame(pass.view);
                d3d::set_depth(nullptr, DepthAccess::RW);
            };
        }));


        nodes.addNode(dabfg::register_node(pass.name("CSM").c_str(), DABFG_PP_NODE_SRC, [this, &pass](dabfg::Registry registry)
        {
            registry.orderMeAfter(pass.name("fill_gbuffer").c_str());
            registry.executionHas(dabfg::SideEffects::External);

            return [this, &pass]()
            {
                nau::CameraNode& camera = pass.view.camera;
                nau::csm::CascadeShadows::ModeSettings mode;
                mode.powWeight = 0.985;
                mode.maxDist = camera.cameraProperties->getClipFarPlane();
                mode.shadowStart = camera.cameraProperties->getClipNearPlane();
                mode.numCascades = 4;

                nau::math::Vector3 lightDir = nau::math::Vector3(1,1,1);

                if (m_graphicsScene->hasDirectionalLight())
                {
                    const auto& light = m_graphicsScene->getDirectionalLights()[0];
                    lightDir = -light.m_direction;
                    mode.numCascades = std::min(light.m_csmCascadesCount, 4u);
                    mode.powWeight = light.m_csmPowWeight;

                    pass.shadows->setCascadeWidth(light.m_csmSize);

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

                pass.shadows->prepareShadowCascades(mode, lightDir, view, cameraPos, proj,
                    frustum, nau::math::Vector2(nearZ, farZ), nearZ);

                pass.shadows->renderShadowsCascades();

                d3d::setview(0, 0, pass.gBuffer->getWidth(), pass.gBuffer->getHeight(), 0.f, 1.f);
            };
        }));

        nodes.addNode(
            dabfg::register_node(pass.name("gbuffer_resolve").c_str(), DABFG_PP_NODE_SRC, [this, &pass](dabfg::Registry registry)
        {
            registry.orderMeAfter(pass.name("CSM").c_str());

            auto resolveTarget = registry
                                     .createTexture2d(pass.name("resolve_texture").c_str(), dabfg::History::ClearZeroOnFirstFrame,
                                                      dabfg::Texture2dCreateInfo{
                                                          TEXFMT_A16B16G16R16F | TEXCF_RTARGET, registry.getResolution(pass.resolution.c_str())})
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            return [resolveTarget, this, &pass]()
            {
                const CameraNode& c = pass.view.camera;
                const auto cameraPosition = math::Vector4(c.worldPosition);
                shader_globals::setVariable("worldViewPos", &cameraPosition);
                if (m_graphicsScene->hasDirectionalLight())
                {
                    const auto& lights = m_graphicsScene->getDirectionalLights();

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

                pass.shadows->setCascadesToShader(pass.gBuffer->getResolveShading()->getMaterial());
                pass.gBuffer->resolve(resolveTarget.get(),
                                     m_environmentRenderer->getIrradianceMap(),
                                     m_environmentRenderer->getReflectionMap(),
                                     c.getViewProjectionMatrix());

                TextureInfo info;
                resolveTarget.get()->getinfo(info, 0);

                auto screen_pos_to_texcoord = math::Vector4(1.f / info.w, 1.f / info.h, HALF_TEXEL_OFSF / info.w, HALF_TEXEL_OFSF / info.h);
                shader_globals::setVariable("screen_pos_to_texcoord", &screen_pos_to_texcoord);

                d3d::set_render_target(const_cast<BaseTexture*>(resolveTarget.get()), 0);
                pass.scene->renderLights(pass.view);

                pass.gBuffer->flushResolve();
            };
        }));

        nodes.addNode(dabfg::register_node(pass.name("forward_translucent").c_str(), DABFG_PP_NODE_SRC,
            [this, &pass](dabfg::Registry registry)
        {
            registry.orderMeAfter(pass.name("render_envi").c_str());

            auto resolveTarget = registry.modifyTexture(pass.name("resolve_texture").c_str())
                                     .atStage(dabfg::Stage::POST_RASTER)
                                     .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                     .handle();

            return [&pass, resolveTarget]()
            {
                d3d::set_render_target(const_cast<BaseTexture*>(resolveTarget.get()), 0);
                d3d::set_depth(pass.gBuffer->getDepth(), DepthAccess::RW);

                pass.scene->renderTranslucency(pass.view);
            };
        }));
    }

    void RenderWindowImpl::createEnvironmentNode()
    {
        m_gBufferNodes.addNode(dabfg::register_node(m_mainPass.name("compute_env_cubemaps").c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeBefore(m_mainPass.name("gbuffer_resolve").c_str());
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
    }

}  // namespace nau::render
