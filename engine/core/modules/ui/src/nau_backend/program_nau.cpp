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


#include "program_nau.h"

#include "base/CCDirector.h"
#include "base/CCEventDispatcher.h"
#include "base/CCEventType.h"
#include "base/ccMacros.h"
#include "nau/assets/asset_manager.h"
#include "nau/service/service_provider.h"
#include "renderer/backend/Types.h"
#include "utils_nau.h"

DAGOR_CC_BACKEND_BEGIN
namespace
{
    std::string vsPreDefine("#version 100\n precision highp float;\n precision highp int;\n");
    std::string fsPreDefine("precision mediump float;\n precision mediump int;\n");
}  // namespace

ProgramNau::ProgramNau(const std::string& vertexShader, const std::string& fragmentShader) :
    Program(vertexShader, fragmentShader)
{
    const auto openShaderAsset = [](nau::string path) -> async::Task<ShaderAssetView::Ptr>
    {
        const AssetPath assetPath{path.tostring()};
        IAssetDescriptor::Ptr asset = nau::getServiceProvider().get<nau::IAssetManager>().openAsset(assetPath);
        ShaderAssetView::Ptr shader = co_await asset->getAssetView(rtti::getTypeInfo<ShaderAssetView>());

        co_return shader;
    };

    auto defaultShaderPath = u8"/res/ui/shaders/cache/shader_cache.nsbc";

    m_vertexShader = *waitResult(openShaderAsset(nau::string::format(u8"file:{}+[{}.vs.vsmain]", defaultShaderPath, vertexShader.c_str())));
    m_pixelShader = *waitResult(openShaderAsset(nau::string::format(u8"file:{}+[{}.ps.psmain]", defaultShaderPath, fragmentShader.c_str())));

    NAU_ASSERT(m_vertexShader);
    NAU_ASSERT(m_pixelShader);

    computeUniformInfos();
    computeLocations();
}

ProgramNau::~ProgramNau()
{
}

void ProgramNau::computeLocations()
{
    _shaderAttributeLocation.clear();
    auto& signatureParams = m_vertexShader->getShader()->reflection.signatureParams;

    for (int i = 0; i < signatureParams.size(); ++i)
    {
        cocos2d::backend::AttributeBindInfo attributeBindInfo;

        attributeBindInfo.location = i;
        attributeBindInfo.attributeName = signatureParams[i].semanticName.c_str();

        _shaderAttributeLocation.push_back(attributeBindInfo);
    }
}

/// built-in attribute names
eastl::unordered_map<eastl::string, const char*> ShadersToBuildInAttributeNames = {
    { "POSITION",  cocos2d::backend::ATTRIBUTE_NAME_POSITION},
    {    "COLOR",     cocos2d::backend::ATTRIBUTE_NAME_COLOR},
    { "TEXCOORD",  cocos2d::backend::ATTRIBUTE_NAME_TEXCOORD},
    {"TEXCOORD1", cocos2d::backend::ATTRIBUTE_NAME_TEXCOORD1},
    {"TEXCOORD2", cocos2d::backend::ATTRIBUTE_NAME_TEXCOORD2},
    {"TEXCOORD3", cocos2d::backend::ATTRIBUTE_NAME_TEXCOORD3}
};

std::unordered_map<std::string, cocos2d::backend::AttributeBindInfo> ProgramNau::getActiveAttributes() const
{
    std::unordered_map<std::string, cocos2d::backend::AttributeBindInfo> attributesOut;

    for (auto& attribute : _shaderAttributeLocation)
    {
        if (ShadersToBuildInAttributeNames.count(attribute.attributeName.c_str()))
        {
            attributesOut[ShadersToBuildInAttributeNames[attribute.attributeName.c_str()]] = attribute;
        }
    }

    return attributesOut;
}

void ProgramNau::computeUniformInfos()
{
    // We taking const buffers information only from vertex shader for now.
    _shaderUniformInfo.clear();
    auto& shaderInputBinds = m_vertexShader->getShader()->reflection.inputBinds;

    // Only oner const buffer for simplicity
    NAU_ASSERT(shaderInputBinds.size() <= 1);
    auto& bufferDesc = shaderInputBinds[0].bufferDesc;
    _totalBufferSize = bufferDesc.size;
    for (int i = 0; i < bufferDesc.variables.size(); ++i)
    {
        cocos2d::backend::UniformInfo uniformInfo;

        uniformInfo.count = bufferDesc.variables[i].type.elements;
        uniformInfo.location = shaderInputBinds[0].bindPoint;

        uniformInfo.size = bufferDesc.variables[i].size;
        uniformInfo.type = (unsigned int)(bufferDesc.variables[i].type.svc);
        uniformInfo.isArray = bufferDesc.variables[i].type.elements > 0;
        uniformInfo.bufferOffset = bufferDesc.variables[i].startOffset;

        uniformInfo.isMatrix = bufferDesc.variables[i].type.svc == ShaderVariableClass::MatrixColumns;
        uniformInfo.needConvert = false;

        _shaderUniformInfo[bufferDesc.variables[i].name] = uniformInfo;
    }

    auto& psShaderInputBinds = m_pixelShader->getShader()->reflection.inputBinds;
    for (int i = 0; i < psShaderInputBinds.size(); ++i)
    {
        if (psShaderInputBinds[i].type == ShaderInputType::Texture && psShaderInputBinds[i].dimension != SrvDimension::Buffer)
        {
            cocos2d::backend::UniformInfo uniformInfo;

            uniformInfo.location = psShaderInputBinds[i].bindPoint;
            uniformInfo.needConvert = false;

            _shaderUniformInfo[psShaderInputBinds[i].name] = uniformInfo;
        }
    }
}

/// built-in uniform names
const char* BuildInUniformNames[] = {
    cocos2d::backend::UNIFORM_NAME_MVP_MATRIX,
    cocos2d::backend::UNIFORM_NAME_TEXTURE,
    cocos2d::backend::UNIFORM_NAME_TEXTURE1,
    cocos2d::backend::UNIFORM_NAME_TEXTURE2,
    cocos2d::backend::UNIFORM_NAME_TEXTURE3,
    cocos2d::backend::UNIFORM_NAME_TEXT_COLOR,
    cocos2d::backend::UNIFORM_NAME_EFFECT_COLOR,
    cocos2d::backend::UNIFORM_NAME_EFFECT_TYPE};

cocos2d::backend::UniformLocation ProgramNau::getUniformLocation(cocos2d::backend::Uniform name) const
{
    return getUniformLocation(BuildInUniformNames[name]);
}

cocos2d::backend::UniformLocation ProgramNau::getUniformLocation(const std::string& uniform) const
{
    cocos2d::backend::UniformLocation uniformLocation;

    if (_shaderUniformInfo.count(uniform.c_str()))
    {
        auto& uniformInfo = _shaderUniformInfo.at(uniform.c_str());
        uniformLocation.location[0] = uniformInfo.location;
        uniformLocation.location[1] = uniformInfo.bufferOffset;
    }

    return uniformLocation;
}

std::size_t ProgramNau::getUniformBufferSize(cocos2d::backend::ShaderStage stage) const
{
    return _totalBufferSize;
}

PROGRAM ProgramNau::getHandler(VDECL vdecl)
{
    if (_shadersPool.contains(vdecl))
    {
        return _shadersPool[vdecl];
    }

    eastl::array shaders = {m_vertexShader, m_pixelShader};

    PROGRAM prog = ShaderAssetView::makeShaderProgram(shaders, vdecl);
    _shadersPool[vdecl] = prog;
    return prog;
}

DAGOR_CC_BACKEND_END
