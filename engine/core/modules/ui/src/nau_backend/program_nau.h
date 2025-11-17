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


#pragma once

#include <graphics_assets/shader_asset.h>

#include <string>
#include <unordered_map>
#include <vector>

#include <EASTL/unordered_map.h>

#include "base/CCEventListenerCustom.h"
#include "base/CCRef.h"
#include "renderer/backend/Macros.h"
#include "renderer/backend/Program.h"
#include "renderer/backend/RenderPipelineDescriptor.h"
#include "renderer/backend/Types.h"
#include "nau/3d/dag_drv3d.h"

DAGOR_CC_BACKEND_BEGIN

/**
 * Store attribute information.
 */
struct AttributeInfo
{
    unsigned int location = 0;
    unsigned int size = 0;
    VSDT type = VSDT_UBYTE4;
    size_t stride = 0;
    size_t offset = 0;
    bool needToBeNormallized = false;
    eastl::string name;
};


/**
 * An OpenGL program.
 */
class ProgramNau : public cocos2d::backend::Program
{
public:
    /**
     * @param vertexShader Specifes the vertex shader source.
     * @param fragmentShader Specifes the fragment shader source.
     */
    ProgramNau(const std::string& vertexShader, const std::string& fragmentShader);

    ~ProgramNau();

    /**
     * Get program object.
     * @return Program object.
     */
    PROGRAM getHandler(VDECL vdecl);

    /**
     * Get uniform location by name.
     * @param uniform Specifies the uniform name.
     * @return The uniform location.
     */
    virtual cocos2d::backend::UniformLocation getUniformLocation(const std::string& uniform) const override;

    /**
     * Get uniform location by engine built-in uniform enum name.
     * @param name Specifies the engine built-in uniform enum name.
     * @return The uniform location.
     */
    virtual cocos2d::backend::UniformLocation getUniformLocation(cocos2d::backend::Uniform name) const override;

    /**
     * Get active vertex attributes.
     * @return Active vertex attributes. key is active attribute name, Value is corresponding attribute info.
     */
    virtual std::unordered_map<std::string, cocos2d::backend::AttributeBindInfo> getActiveAttributes() const override;

    /**
     * Get uniform buffer size in bytes that can hold all the uniforms.
     * @param stage Specifies the shader stage. The symbolic constant can be either VERTEX or FRAGMENT.
     * @return The uniform buffer size in bytes.
     */
    virtual std::size_t getUniformBufferSize(cocos2d::backend::ShaderStage stage) const override;

    /**
     * Get attribute location by attribute name.
     * Not used anywhere
     * @param name Specifies the attribute name.
     * @return The attribute location.
     */
    virtual int getAttributeLocation(const std::string& name) const override
    {
        NAU_FAILURE("Not used anywhere.");
        return 0;
    };

    /**
     * Get attribute location by engine built-in attribute enum name.
     * @param name Specifies the engine built-in attribute enum name.
     * @return The attribute location.
     */
    virtual int getAttributeLocation(cocos2d::backend::Attribute name) const override
    {
        return uint32_t(name);
    };

    /**
     * Get maximum vertex location.
     * Not used anywhere
     * @return Maximum vertex locaiton.
     */
    virtual int getMaxVertexLocation() const override
    {
        NAU_FAILURE("Not used anywhere.");
        return 0;
    };

    /**
     * Get maximum fragment location.
     * Not used anywhere
     * @return Maximum fragment location.
     */
    virtual int getMaxFragmentLocation() const override
    {
        NAU_FAILURE("Not used anywhere.");
        return 0;
    };

    /**
     * Get a uniformInfo in given location from the specific shader stage.
     * @param stage Specifies the shader stage. The symbolic constant can be either VERTEX or FRAGMENT.
     * @param location Specifies the uniform locaion.
     * @return The uniformInfo.
     */
    virtual const cocos2d::backend::UniformInfo& getActiveUniformInfo(cocos2d::backend::ShaderStage stage, int location) const override
    {
        static cocos2d::backend::UniformInfo ui;
        NAU_FAILURE("Not used anywhere.");
        return ui;
    };

    /**
     * Get all uniformInfos.
     * @return The uniformInfos.
     */
    virtual const std::unordered_map<std::string, cocos2d::backend::UniformInfo>& getAllActiveUniformInfo(cocos2d::backend::ShaderStage stage) const override
    {
        static std::unordered_map<std::string, cocos2d::backend::UniformInfo> arr;
        NAU_FAILURE("Not used anywhere.");
        return arr;
    };

private:
    void computeUniformInfos();
    void computeLocations();

    nau::ShaderAssetView::Ptr m_vertexShader;
    nau::ShaderAssetView::Ptr m_pixelShader;

    std::vector<AttributeInfo> _attributeInfos;  // Layout

    std::size_t _totalBufferSize = 0;
    eastl::unordered_map<eastl::string ,cocos2d::backend::UniformInfo> _shaderUniformInfo;
    eastl::vector<cocos2d::backend::AttributeBindInfo> _shaderAttributeLocation;
    std::unordered_map<int, int> _bufferOffset;
    std::unordered_map<VDECL, PROGRAM> _shadersPool;
};

DAGOR_CC_BACKEND_END
