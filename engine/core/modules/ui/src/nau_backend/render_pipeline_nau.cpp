// Copyright 2024 N-GINN LLC. All rights reserved.
/****************************************************************************
 Copyright (c) 2018-2019 Xiamen Yaji Software Co., Ltd.

 http://www.cocos2d-x.org

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/


#include "render_pipeline_nau.h"

#include <assert.h>

#include "depth_stencil_state_nau.h"
#include "program_nau.h"
#include "texture_nau.h"
#include "utils_nau.h"

DAGOR_CC_BACKEND_BEGIN

void RenderPipelineNau::update(const cocos2d::PipelineDescriptor& pipelineDescirptor, const cocos2d::backend::RenderPassDescriptor& renderpassDescriptor)
{
    m_pipelineDescriptor = pipelineDescirptor;
    m_renderPassDescriptor = renderpassDescriptor;
}

void RenderPipelineNau::apply(shaders::RenderState& renderState)
{
    renderState.colorWr = cocos_utils::toNauWriteMask(m_pipelineDescriptor.blendDescriptor.writeMask);

    if (m_pipelineDescriptor.blendDescriptor.blendEnabled)
    {
        for (auto& blendParam : renderState.blendParams)
        {
            blendParam.ablend = 1;
            blendParam.blendOp = cocos_utils::toNauBlendOperation(m_pipelineDescriptor.blendDescriptor.rgbBlendOperation);
            blendParam.sepablendOp = cocos_utils::toNauBlendOperation(m_pipelineDescriptor.blendDescriptor.alphaBlendOperation);

            blendParam.ablendFactors.src = cocos_utils::toNauBlendFactor(m_pipelineDescriptor.blendDescriptor.sourceRGBBlendFactor);
            blendParam.ablendFactors.dst = cocos_utils::toNauBlendFactor(m_pipelineDescriptor.blendDescriptor.destinationRGBBlendFactor);
            blendParam.sepablendFactors.src = cocos_utils::toNauBlendFactor(m_pipelineDescriptor.blendDescriptor.sourceAlphaBlendFactor);
            blendParam.sepablendFactors.dst = cocos_utils::toNauBlendFactor(m_pipelineDescriptor.blendDescriptor.destinationAlphaBlendFactor);

            // UI layer (graphics/src/render_window/render_graph.cpp, ui_layer): the alpha channel of the target has to
            // hold the coverage of the UI, a = src.a + dst.a * (1 - src.a), so the layer can be composited over the
            // scene later. Color factors stay as cocos sets them: SRC_ALPHA / INV_SRC_ALPHA writes premultiplied color
            // into a transparent target, ONE / INV_SRC_ALPHA (premultiplied textures) keeps it premultiplied.
            // Additive draws (dst = ONE) add light and do not cover the scene. The alpha of the back buffer is never
            // displayed, so drawing straight into the back buffer looks the same as before.
            blendParam.sepablend = 1;
            if (blendParam.ablendFactors.dst == BLEND_ONE)
            {
                blendParam.sepablendFactors.src = BLEND_ZERO;
                blendParam.sepablendFactors.dst = BLEND_ONE;
            }
            else
            {
                blendParam.sepablendFactors.src = BLEND_ONE;
                blendParam.sepablendFactors.dst = BLEND_INVSRCALPHA;
            }
            blendParam.sepablendOp = BLENDOP_ADD;
        }
    }

    ProgramNau* program = dynamic_cast<ProgramNau*>(m_pipelineDescriptor.programState->getProgram());
    NAU_ASSERT(program != nullptr);
    auto inputLayout = m_pipelineDescriptor.programState->getVertexLayout();

    inputLayout->getStride();

    eastl::vector<VSDTYPE> ilDefVec;

    char* buffer;
    std::size_t size;
    m_pipelineDescriptor.programState->getVertexUniformBuffer(&buffer, size);

    if (!consBuffer)
    {
        consBuffer = d3d::create_cb(size, SBCF_DYNAMIC);
    }
    consBuffer->updateData(0, size, buffer, VBLOCK_WRITEONLY);

    d3d::set_const_buffer(STAGE_PS, 1, consBuffer);
    d3d::set_const_buffer(STAGE_VS, 1, consBuffer);

    for (auto& [i, texture] : m_pipelineDescriptor.programState->getVertexTextureInfos())
    {
        ((Texture2DNau*)texture.textures[0])->apply(texture.slot[0]);
    }

    auto& layoutAttributes = inputLayout->getAttributes();

    ilDefVec.resize(layoutAttributes.size() + 1);
    ilDefVec[layoutAttributes.size()] = VSD_END;

    for (auto& [name, attribute] : layoutAttributes)
    {
        VSDR semanticIndex = VSDR(-1);
        if (attribute.name == "a_position")
        {
            semanticIndex = VSDR_POS;
        }
        if (attribute.name == "a_color")
        {
            semanticIndex = VSDR_DIFF;
        }
        if (attribute.name == "a_texCoord")
        {
            semanticIndex = VSDR_TEXC0;
        }
        NAU_ASSERT(semanticIndex != VSDR(-1));

        ilDefVec[attribute.index] = VSD_REG(semanticIndex, cocos_utils::toNauAttributeFormat(attribute.format, attribute.needToBeNormallized));
    }

    d3d::set_program(program->getHandler(d3d::create_vdecl(ilDefVec.data())));
}

RenderPipelineNau::~RenderPipelineNau()
{
    if (consBuffer)
    {
        consBuffer->destroy();
    }
}

DAGOR_CC_BACKEND_END
