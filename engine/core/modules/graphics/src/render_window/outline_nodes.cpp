// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "graphics_assets/texture_asset.h"
#include "nau/gui/dag_imgui.h"
#include "nau/gui/imguiInput.h"
#include "nau/image/dag_texPixel.h"
#include "nau/osApiWrappers/dag_cpuJobs.h"
#include "nau/shaders/shader_defines.h"
#include "nau/service/service_provider.h"
#include "nau/shaders/shader_globals.h"
#include "nau/utils/performance_profiling.h"
#include "render/daBfg/bfg.h"
#include "render_window_impl.h"

namespace nau::render
{
    void RenderWindowImpl::createOutlineNodes()
    {
        setOutlineWidth(0);
        setOutlineColor(math::Color4(0.f, 0.f, 0.f, 0.f));

        m_outlineNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "outlineMask", m_swapchain).c_str(), DABFG_PP_NODE_SRC, [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "z-prepass", m_swapchain).c_str());

            auto outlineMaskTarget = registry
                                         .createTexture2d(nau::utils::format("{}_{}", "outline_mask", m_swapchain).c_str(),
                                                          dabfg::History::ClearZeroOnFirstFrame,
                                                          dabfg::Texture2dCreateInfo{TEXFMT_R8 | TEXCF_RTARGET,
                                                                                     registry.getResolution(m_resolutionName.c_str())})
                                         .atStage(dabfg::Stage::POST_RASTER)
                                         .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                         .handle();

            return [=, this]()
            {
                if (m_outlineWidth == 0)
                {
                    return;
                }

                d3d::set_render_target(const_cast<BaseTexture*>(outlineMaskTarget.get()), 0);
                d3d::set_depth(nullptr, DepthAccess::RW);
                d3d::clearview(CLEAR_TARGET, nau::math::E3DCOLOR(0, 0, 0), 0, 0);


                m_graphicsScene->renderOutlineMask(m_mainPass.view);
            };
        }));

        m_outlineNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "calculate_outline", m_swapchain).c_str(), DABFG_PP_NODE_SRC,
                                                    [this](dabfg::Registry registry)
        {
            auto outlineMaskId = registry.readTexture(nau::utils::format("{}_{}", "outline_mask", m_swapchain).c_str())
                                     .atStage(dabfg::Stage::PS_OR_CS)
                                     .useAs(dabfg::Usage::SHADER_RESOURCE)
                                     .handle();

            auto outlineJumpFoodLeftId = registry
                                             .createTexture2d(nau::utils::format("{}_{}", "outline_jump_food_left", m_swapchain).c_str(), dabfg::History::ClearZeroOnFirstFrame,
                                                              dabfg::Texture2dCreateInfo{
                                                                  TEXFMT_G16R16F | TEXCF_RTARGET, registry.getResolution(m_resolutionName.c_str())})
                                             .atStage(dabfg::Stage::POST_RASTER)
                                             .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                             .handle();

            auto outlineJumpFoodRightId = registry
                                              .createTexture2d(nau::utils::format("{}_{}", "outline_jump_food_right", m_swapchain).c_str(), dabfg::History::ClearZeroOnFirstFrame,
                                                               dabfg::Texture2dCreateInfo{
                                                                   TEXFMT_G16R16F | TEXCF_RTARGET, registry.getResolution(m_resolutionName.c_str())})
                                              .atStage(dabfg::Stage::POST_RASTER)
                                              .useAs(dabfg::Usage::COLOR_ATTACHMENT)
                                              .handle();

            nau::shader_globals::addVariable("screenWidth", sizeof(unsigned short));
            nau::shader_globals::addVariable("screenHeight", sizeof(unsigned short));

            constexpr auto jumpStep = 0.0F;
            nau::shader_globals::addVariable("jumpStepWidth", sizeof(jumpStep), &jumpStep);

            const auto color = math::Vector4{ 1, 1, 1, 1};
            nau::shader_globals::addVariable("outlineColor", sizeof(color), &color);

            return [=, this]()
            {
                float outlineWidth = m_outlineWidth.load(std::memory_order_acquire);
                if (outlineWidth == 0)
                {
                    return;
                }

                d3d::set_srgb_backbuffer_write(false);

                auto leftTexture = outlineJumpFoodLeftId.get();
                auto rightTexture = outlineJumpFoodRightId.get();

                setRenderTarget();
                d3d::set_render_target(0, leftTexture, 0);
                d3d::set_render_target(1, rightTexture, 0);
                d3d::clearview(CLEAR_TARGET, nau::math::E3DCOLOR(0, 0, 0), 0, 0);
                setRenderTarget();

                d3d::settex(0, const_cast<BaseTexture*>(outlineMaskId.get()));
                TextureInfo info;
                outlineMaskId.get()->getinfo(info, 0);

                nau::shader_globals::setVariable("screenWidth", &info.w);
                nau::shader_globals::setVariable("screenHeight", &info.h);

                d3d::set_render_target(0, leftTexture, 0);
                m_outlineRenderer->render("JumpFloodInit");

                uint32_t stepWidth = 1;

                while (stepWidth < outlineWidth)
                {
                    stepWidth *= 2;
                }

                nau::shader_globals::setVariable("screenWidth", &info.w);
                nau::shader_globals::setVariable("screenHeight", &info.h);

                while (stepWidth)
                {
                    const float width = stepWidth;
                    nau::shader_globals::setVariable("jumpStepWidth", &width);
                    setRenderTarget();

                    d3d::settex(0, const_cast<BaseTexture*>(leftTexture));

                    d3d::set_render_target(0, rightTexture, 0);
                    m_outlineRenderer->render("JumpFloodStep");

                    std::swap(leftTexture, rightTexture);

                    stepWidth = stepWidth >> 1ul;
                }
                if (leftTexture != outlineJumpFoodRightId.get())
                {
                    // Additional step, makes outline_jump_food_right always final.
                    const auto width = 1.0F;
                    nau::shader_globals::setVariable("jumpStepWidth", &width);
                    setRenderTarget();

                    d3d::settex(0, const_cast<BaseTexture*>(leftTexture));

                    d3d::set_render_target(0, rightTexture, 0);
                    m_outlineRenderer->render("JumpFloodStep");
                }
                nau::shader_globals::setVariable("jumpStepWidth", &outlineWidth);
            };
        }));

        m_outlineNodes.addNode(dabfg::register_node(nau::utils::format("{}_{}", "draw_outline", m_swapchain).c_str(), DABFG_PP_NODE_SRC,
                                                    [this](dabfg::Registry registry)
        {
            registry.orderMeAfter(nau::utils::format("{}_{}", "post_fx_nodes", m_swapchain).c_str());
            registry.orderMeBefore(nau::utils::format("{}_{}", "nau_gui", m_swapchain).c_str());
            registry.executionHas(dabfg::SideEffects::External);

            auto outlineMaskId = registry.readTexture(nau::utils::format("{}_{}", "outline_mask", m_swapchain).c_str())
                                     .atStage(dabfg::Stage::PS_OR_CS)
                                     .useAs(dabfg::Usage::SHADER_RESOURCE)
                                     .handle();

            auto outline_jump_food_resultId = registry.readTexture(nau::utils::format("{}_{}", "outline_jump_food_right", m_swapchain).c_str())
                                                  .atStage(dabfg::Stage::PS_OR_CS)
                                                  .useAs(dabfg::Usage::SHADER_RESOURCE)
                                                  .handle();

            return [=, this]()
            {
                if (m_outlineWidth.load(std::memory_order_acquire) == 0)
                {
                    return;
                }
                auto color = m_outlineColor.load(std::memory_order_acquire);
                auto outlineColor = math::Vector4{color.r, color.g, color.b, color.a};
                nau::shader_globals::setVariable("outlineColor", &outlineColor);

                d3d::settex(0, const_cast<BaseTexture*>(outline_jump_food_resultId.get()));
                d3d::settex(1, const_cast<BaseTexture*>(outlineMaskId.get()));

                setRenderTarget();

                m_outlineRenderer->render("JumpFloodResult");
            };
        }));
    }
}

