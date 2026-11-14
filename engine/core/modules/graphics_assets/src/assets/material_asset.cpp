// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "graphics_assets/material_asset.h"

#include <EASTL/hash_set.h>
#include <EASTL/unordered_map.h>

#include "nau/assets/asset_ref.h"
#include "nau/assets/material_asset_accessor.h"
#include "nau/shaders/dag_renderStateId.h"
#include "nau/shaders/shader_defines.h"
#include "nau/shaders/shader_globals.h"

#define LOAD_MATERIAL_ASYNC 0
#define PROGRAM_NULL -1

namespace nau
{
    namespace
    {
        constexpr auto GeneratedTextureWidth = 4;
        constexpr auto GeneratedTextureHeight = 4;

        ShaderStage getStage(ShaderTarget target)
        {
            switch (target)
            {
                case ShaderTarget::Vertex:
                case ShaderTarget::Geometry:
                case ShaderTarget::Hull:
                case ShaderTarget::Domain:
                    return STAGE_VS;
                case ShaderTarget::Pixel:
                    return STAGE_PS;
                case ShaderTarget::Compute:
                    return STAGE_CS;
                case ShaderTarget::Count:
                    NAU_FAILURE_ALWAYS("Invalid argument");
            }

            NAU_FAILURE_ALWAYS("Unreachable");
        }

        void fillTextureWithSolidColor(BaseTexture* tex, int texWidth, int texHeight, const math::Vector4& color)
        {
            void* data = nullptr;
            int stride;
            tex->lockimg(&data, stride, 0, TEXLOCK_WRITE);

            for (int row = 0; row < texHeight; ++row)
            {
                auto* rowData = reinterpret_cast<math::Vector4*>(reinterpret_cast<uint8_t*>(data) + row * stride);
                for (int col = 0; col < texWidth; ++col)
                {
                    rowData[col] = color;
                }
            }

            tex->unlockimg();
        }

        BaseTexture* generateSolidColorTexture(const math::Vector4& color)
        {
            BaseTexture* tex = d3d::create_tex(nullptr, GeneratedTextureWidth, GeneratedTextureHeight, TEXFMT_A32B32G32R32F, 1);

            fillTextureWithSolidColor(tex, GeneratedTextureWidth, GeneratedTextureHeight, color);

            return tex;
        }

    }  // anonymous namespace

    async::Task<MaterialAssetView::Ptr> MaterialAssetView::createFromAssetAccessor(nau::Ptr<> accessor)
    {
        auto& materialAccessor = accessor->as<IMaterialAssetAccessor&>();

        Material mat;
        materialAccessor.fillMaterial(mat).ignore();

        Ptr asset;
        if (mat.master.has_value())
        {
            asset = co_await MaterialInstanceAssetView::createFromMaterial(eastl::move(mat));
        }
        else
        {
            asset = co_await MasterMaterialAssetView::createFromMaterial(eastl::move(mat));
        }

        co_return asset;
    }

    eastl::unordered_set<eastl::string> MaterialAssetView::getPipelineNames() const
    {
        eastl::unordered_set<eastl::string> names;
        names.reserve(m_pipelines.size());

        for (const auto& [name, pipeline] : m_pipelines)
        {
            names.insert(name);
        }

        return names;
    }

    void MaterialAssetView::setCullMode(eastl::string_view pipelineName, CullMode cullMode)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& ppipeline = m_pipelines[pipelineName.data()];
        ppipeline.cullMode = eastl::make_optional(cullMode);
        ppipeline.isRenderStateDirty = true;
    }

    CullMode MaterialAssetView::getCullMode(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        const auto& ppipeline = m_pipelines.at(pipelineName.data());
        return ppipeline.cullMode.has_value()
                   ? *ppipeline.cullMode
                   : CullMode::CounterClockwise;
    }

    void MaterialAssetView::setDepthMode(eastl::string_view pipelineName, DepthMode depthMode)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& ppipeline = m_pipelines[pipelineName.data()];
        ppipeline.depthMode = eastl::make_optional(depthMode);
        ppipeline.isRenderStateDirty = true;
    }

    DepthMode MaterialAssetView::getDepthMode(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        const auto& ppipeline = m_pipelines.at(pipelineName.data());
        return ppipeline.depthMode.has_value()
                   ? *ppipeline.depthMode
                   : DepthMode::Default;
    }

    void MaterialAssetView::setBlendMode(eastl::string_view pipelineName, BlendMode blendMode)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& ppipeline = m_pipelines[pipelineName.data()];
        ppipeline.blendMode = eastl::make_optional(blendMode);
        ppipeline.isRenderStateDirty = true;
    }

    BlendMode MaterialAssetView::getBlendMode(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        const auto& ppipeline = m_pipelines.at(pipelineName.data());
        return ppipeline.blendMode.has_value()
                   ? *ppipeline.blendMode
                   : BlendMode::Opaque;
    }

    void MaterialAssetView::setScissorsEnabled(eastl::string_view pipelineName, bool isEnabled)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& ppipeline = m_pipelines[pipelineName.data()];
        ppipeline.isScissorsEnabled = eastl::make_optional(isEnabled);
        ppipeline.isRenderStateDirty = true;
    }

    bool MaterialAssetView::isScissorsEnabled(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        const auto& ppipeline = m_pipelines.at(pipelineName.data());
        return ppipeline.isScissorsEnabled.has_value()
                   ? *ppipeline.isScissorsEnabled
                   : false;
    }

    void MaterialAssetView::setCBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, Sbuffer* cbuffer)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        auto& pipeline = m_pipelines[pipelineName.data()];

        NAU_ASSERT(pipeline.systemCBuffers.contains(bufferName));
        pipeline.systemCBuffers[bufferName.data()].buffer = cbuffer;
    }

    Sbuffer* MaterialAssetView::getCBuffer(eastl::string_view pipelineName, eastl::string_view bufferName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        auto& pipeline = m_pipelines[pipelineName.data()];

        NAU_ASSERT(pipeline.systemCBuffers.contains(bufferName));
        return pipeline.systemCBuffers[bufferName.data()].buffer;
    }

    void MaterialAssetView::setTexture(eastl::string_view pipelineName, eastl::string_view propertyName, BaseTexture* texture)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        auto& pipeline = m_pipelines[pipelineName.data()];

        NAU_ASSERT(pipeline.texProperties.contains(propertyName));
        auto& property = pipeline.texProperties[propertyName.data()];

        if (property.isMasterValue)
        {
            property.masterValue = nullptr;
            property.isMasterValue = false;
        }

        if (property.parentTexture->isOwned && property.currentValue != nullptr && property.currentValue->is<RuntimeReadonlyCollection>())
        {
            del_d3dres(property.parentTexture->texture);
        }

        property.parentTexture->textureView = nullptr;
        property.parentTexture->texture = texture;
        property.parentTexture->isOwned = false;
        property.currentValue = makeValueCopy(eastl::string{"Internal BaseTexture"});
        property.timestamp = std::chrono::steady_clock::now();
    }

    void MaterialAssetView::setSolidColorTexture(eastl::string_view pipelineName, eastl::string_view propertyName, math::E3DCOLOR color)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        auto& pipeline = m_pipelines[pipelineName.data()];

        NAU_ASSERT(pipeline.texProperties.contains(propertyName));
        auto& property = pipeline.texProperties[propertyName.data()];

        if (property.isMasterValue)
        {
            property.masterValue = nullptr;
            property.isMasterValue = false;
        }

        const math::Vector4 solidColor = {color.r / 255.0F, color.g / 255.0F, color.b / 255.0F, color.a / 255.0F};

        if (property.parentTexture->isOwned && property.currentValue != nullptr && property.currentValue->is<RuntimeReadonlyCollection>())
        {
            fillTextureWithSolidColor(
                property.parentTexture->texture,
                GeneratedTextureWidth,
                GeneratedTextureHeight,
                solidColor);
        }
        else
        {
            pipeline.samplerTextures[propertyName.data()].texture = generateSolidColorTexture(solidColor);
        }

        property.parentTexture->textureView = nullptr;
        property.parentTexture->isOwned = true;
        property.currentValue = makeValueCopy(solidColor);
        property.timestamp = std::chrono::steady_clock::now();
    }

    async::Task<> MaterialAssetView::setTextureFromAsset(eastl::string_view pipelineName, eastl::string_view propertyName, eastl::string_view textureView)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        auto& pipeline = m_pipelines[pipelineName.data()];

        NAU_ASSERT(pipeline.texProperties.contains(propertyName));
        auto& property = pipeline.texProperties[propertyName.data()];

        if (property.isMasterValue)
        {
            property.masterValue = nullptr;
            property.isMasterValue = false;
        }

        if (property.parentTexture->isOwned && property.currentValue != nullptr && property.currentValue->is<RuntimeReadonlyCollection>())
        {
            del_d3dres(pipeline.samplerTextures[propertyName.data()].texture);
        }

        TextureAssetRef assetRef = AssetPath{textureView};
        auto texAsset = co_await assetRef.getReloadableAssetViewTyped<TextureAssetView>();

        property.parentTexture->textureView = texAsset;
        property.parentTexture->isOwned = false;
        property.currentValue = makeValueCopy(eastl::string{textureView});
        property.timestamp = std::chrono::steady_clock::now();
    }

    void MaterialAssetView::createRwBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, const BufferDesc& desc)
    {
        // We ALWAYS use explicit specification of the element (or structure) size
        // and the element count instead of providing the total size.
        NAU_ASSERT(desc.elementCount > 0);
        NAU_ASSERT(desc.elementSize > 0);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];

        if (pipeline.rwBuffers.contains(bufferName))
        {
            auto& buf = pipeline.rwBuffers[bufferName.data()];
            if (buf.isOwned && buf.buffer != nullptr)
            {
                del_d3dres(buf.buffer);
            }

            pipeline.rwBuffers.erase(bufferName.data());
        }

        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    if (!pipeline.rwBuffers.contains(bufferName))
                    {
                        unsigned flags = desc.flags;

                        switch (bind.type)
                        {
                            case ShaderInputType::UavRwTyped:
                                flags |= SBCF_BIND_UNORDERED | SBCF_DYNAMIC;
                                break;

                            case ShaderInputType::UavRwStructured:
                                flags |= SBCF_UA_STRUCTURED | SBCF_DYNAMIC;
                                break;

                                // Currently, DXC returns D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER for ConsumeStructuredBuffer and AppendStructuredBuffer
                                // in reflection instead of D3D_SIT_UAV_CONSUME_STRUCTURED and D3D_SIT_UAV_APPEND_STRUCTURED, respectively.
                                // This may be related to the following code: https://github.com/microsoft/DirectXShaderCompiler/blob/9221570027d759bda093ae035a7cc68d6923fa13/lib/HLSL/DxilContainerReflection.cpp#L1690
                                // TODO: Further investigation is needed.

                            case ShaderInputType::UavRwStructuredWithCounter:
                                flags |= SBCF_UA_STRUCTURED | SBCF_BIND_SHADER_RES;
                                break;

                            default:
                                NAU_FAILURE_ALWAYS("Buffer '{}' has an unsupported type: '{}'", bufferName.data(), toString(bind.type));
                        }

                        pipeline.rwBuffers[bind.name] = {
                            .buffer = d3d::create_sbuffer(desc.elementSize, desc.elementCount, flags, desc.format, desc.name),
                            .slot = bind.bindPoint,
                            .isOwned = true};
                    }

                    pipeline.rwBuffers[bind.name].stages.insert(getStage(shader->target));
                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::writeRwBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, const void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer ||
                    bind.type != ShaderInputType::UavRwTyped && bind.type != ShaderInputType::UavRwStructured && bind.type != ShaderInputType::UavRwStructuredWithCounter)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwBuffers.contains(bufferName));

                    std::byte* ptr = nullptr;
                    auto& uav = pipeline.rwBuffers[bind.name];

                    uav.buffer->lock(0, size, reinterpret_cast<void**>(&ptr), VBLOCK_WRITEONLY);
                    NAU_ASSERT(ptr);

                    memcpy(ptr, data, size);
                    uav.buffer->unlock();

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::readRwBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer ||
                    bind.type != ShaderInputType::UavRwTyped && bind.type != ShaderInputType::UavRwStructured && bind.type != ShaderInputType::UavRwStructuredWithCounter)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwBuffers.contains(bufferName));

                    std::byte* ptr = nullptr;
                    auto& uav = pipeline.rwBuffers[bind.name];

                    uav.buffer->lock(0, size, reinterpret_cast<void**>(&ptr), VBLOCK_READONLY);
                    NAU_ASSERT(ptr);

                    memcpy(data, ptr, size);
                    uav.buffer->unlock();

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::setRwBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, Sbuffer* rwBuffer)
    {
        NAU_ASSERT(rwBuffer);
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    const auto flags = rwBuffer->getFlags();

                    switch (bind.type)
                    {
                        case ShaderInputType::UavRwTyped:
                            if (!(flags & SBCF_BIND_UNORDERED))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_BIND_UNORDERED flag is missing!");
                            }
                            if (!(flags & SBCF_DYNAMIC))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_DYNAMIC flag is missing!");
                            }
                            break;

                        case ShaderInputType::UavRwStructured:
                            if (!(flags & SBCF_UA_STRUCTURED))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_UA_STRUCTURED flag is missing!");
                            }
                            if (!(flags & SBCF_DYNAMIC))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_DYNAMIC flag is missing!");
                            }
                            break;

                        case ShaderInputType::UavRwStructuredWithCounter:
                            if (!(flags & SBCF_UA_STRUCTURED))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_UA_STRUCTURED flag is missing!");
                            }
                            if (!(flags & SBCF_BIND_SHADER_RES))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_BIND_SHADER_RES flag is missing!");
                            }
                            break;

                        default:
                            NAU_FAILURE_ALWAYS("Buffer '{}' has an unsupported type: '{}'", bufferName.data(), toString(bind.type));
                    }

                    auto& uav = pipeline.rwBuffers[bind.name];
                    if (uav.isOwned && uav.buffer != nullptr)
                    {
                        del_d3dres(uav.buffer);
                    }

                    uav.buffer = rwBuffer;
                    uav.slot = bind.bindPoint;
                    uav.stages.insert(getStage(shader->target));
                    uav.isOwned = false;

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    Sbuffer* MaterialAssetView::getRwBuffer(eastl::string_view pipelineName, eastl::string_view bufferName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwBuffers.contains(bufferName));
                    return pipeline.rwBuffers[bind.name].buffer;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::createRoBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, const BufferDesc& desc)
    {
        // We ALWAYS use explicit specification of the element (or structure) size
        // and the element count instead of providing the total size.
        NAU_ASSERT(desc.elementCount > 0);
        NAU_ASSERT(desc.elementSize > 0);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];

        if (pipeline.roBuffers.contains(bufferName))
        {
            auto& buf = pipeline.roBuffers[bufferName.data()];
            if (buf.isOwned && buf.buffer != nullptr)
            {
                del_d3dres(buf.buffer);
            }

            pipeline.roBuffers.erase(bufferName.data());
        }

        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    if (!pipeline.roBuffers.contains(bufferName))
                    {
                        unsigned flags = desc.flags;

                        switch (bind.type)
                        {
                            case ShaderInputType::Texture:
                                flags |= SBCF_BIND_SHADER_RES | SBCF_DYNAMIC;
                                break;

                            case ShaderInputType::Structured:
                                flags |= SBCF_MISC_STRUCTURED | SBCF_BIND_SHADER_RES | SBCF_DYNAMIC;
                                break;

                            default:
                                NAU_FAILURE_ALWAYS("Buffer '{}' has an unsupported type: '{}'", bufferName.data(), toString(bind.type));
                        }

                        pipeline.roBuffers[bind.name] = {
                            .buffer = d3d::create_sbuffer(desc.elementSize, desc.elementCount, flags, desc.format, desc.name),
                            .slot = bind.bindPoint,
                            .isOwned = true};
                    }

                    pipeline.roBuffers[bind.name].stages.insert(getStage(shader->target));
                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::writeRoBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, const void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer ||
                    bind.type != ShaderInputType::Structured && bind.type != ShaderInputType::Texture)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.roBuffers.contains(bufferName));

                    std::byte* ptr = nullptr;
                    auto& srv = pipeline.roBuffers[bind.name];

                    srv.buffer->lock(0, size, reinterpret_cast<void**>(&ptr), VBLOCK_WRITEONLY);
                    NAU_ASSERT(ptr);

                    memcpy(ptr, data, size);
                    srv.buffer->unlock();

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::setRoBuffer(eastl::string_view pipelineName, eastl::string_view bufferName, Sbuffer* roBuffer)
    {
        NAU_ASSERT(roBuffer);
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    const auto flags = roBuffer->getFlags();

                    switch (bind.type)
                    {
                        case ShaderInputType::Texture:
                            if (!(flags & SBCF_BIND_SHADER_RES))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_BIND_SHADER_RES flag is missing!");
                            }
                            if (!(flags & SBCF_DYNAMIC))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_DYNAMIC flag is missing!");
                            }
                            break;

                        case ShaderInputType::Structured:
                            if (!(flags & SBCF_MISC_STRUCTURED))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_MISC_STRUCTURED flag is missing!");
                            }
                            if (!(flags & SBCF_BIND_SHADER_RES))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_BIND_SHADER_RES flag is missing!");
                            }
                            if (!(flags & SBCF_DYNAMIC))
                            {
                                NAU_FAILURE_ALWAYS("SBCF_DYNAMIC flag is missing!");
                            }
                            break;

                        default:
                            NAU_FAILURE_ALWAYS("Buffer '{}' has an unsupported type: '{}'", bufferName.data(), toString(bind.type));
                    }

                    auto& srv = pipeline.roBuffers[bind.name];
                    if (srv.isOwned && srv.buffer != nullptr)
                    {
                        del_d3dres(srv.buffer);
                    }

                    srv.buffer = roBuffer;
                    srv.slot = bind.bindPoint;
                    srv.stages.insert(getStage(shader->target));
                    srv.isOwned = false;

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    Sbuffer* MaterialAssetView::getRoBuffer(eastl::string_view pipelineName, eastl::string_view bufferName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.dimension != SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.roBuffers.contains(bufferName));
                    return pipeline.roBuffers[bind.name].buffer;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Buffer '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::createRwTexture(eastl::string_view pipelineName, eastl::string_view bufferName, const TextureDesc& desc)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];

        if (pipeline.rwTextures.contains(bufferName))
        {
            auto& tex = pipeline.rwTextures[bufferName.data()];
            if (tex.isOwned && tex.texture != nullptr)
            {
                del_d3dres(tex.texture);
            }

            pipeline.rwTextures.erase(bufferName.data());
        }

        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    if (pipeline.rwTextures.contains(bufferName))
                    {
                        switch (bind.dimension)
                        {
                            case SrvDimension::Texture1D:
                            case SrvDimension::Texture1DArray:
                                NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                            case SrvDimension::Texture2D:
                                pipeline.rwTextures[bind.name] = {
                                    .texture = d3d::create_tex(desc.image, desc.width, desc.height, desc.flags | TEXCF_UNORDERED, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            case SrvDimension::Texture2DArray:
                                pipeline.rwTextures[bind.name] = {
                                    .texture = d3d::create_array_tex(desc.width, desc.height, desc.depthOrArraySize, desc.flags | TEXCF_UNORDERED, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            case SrvDimension::Texture3D:
                                pipeline.rwTextures[bind.name] = {
                                    .texture = d3d::create_voltex(desc.width, desc.height, desc.depthOrArraySize, desc.flags | TEXCF_UNORDERED, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            default:
                                NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                        }
                    }

                    pipeline.rwTextures[bind.name].stages.insert(getStage(shaderAsset->getShader()->target));
                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::writeRwTexture(eastl::string_view pipelineName, eastl::string_view bufferName, const void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwTextures.contains(bufferName));
                    auto& uav = pipeline.rwTextures[bind.name];

                    switch (bind.dimension)
                    {
                        case SrvDimension::Texture1D:
                        case SrvDimension::Texture1DArray:
                            NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                        case SrvDimension::Texture2D:
                        {
                            int stride;
                            void* ptr = nullptr;

                            uav.texture->lockimg(&ptr, stride, 0, TEXLOCK_WRITE);
                            NAU_ASSERT(ptr);

                            memcpy(ptr, data, size);
                            uav.texture->unlockimg();

                            break;
                        }

                        case SrvDimension::Texture2DArray:
                            NAU_FAILURE_ALWAYS("No lock function for Texture2DArray");

                        case SrvDimension::Texture3D:
                        {
                            int row;
                            int slice;
                            void* ptr = nullptr;

                            uav.texture->lockbox(&ptr, row, slice, 0, TEXLOCK_WRITE);
                            NAU_ASSERT(ptr);

                            memcpy(ptr, data, size);
                            uav.texture->unlockbox();

                            break;
                        }

                        default:
                            NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                    }

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::readRwTexture(eastl::string_view pipelineName, eastl::string_view bufferName, void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwTextures.contains(bufferName));
                    auto& uav = pipeline.rwTextures[bind.name];

                    switch (bind.dimension)
                    {
                        case SrvDimension::Texture1D:
                        case SrvDimension::Texture1DArray:
                            NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                        case SrvDimension::Texture2D:
                        {
                            int stride;
                            void* ptr = nullptr;

                            uav.texture->lockimg(&ptr, stride, 0, TEXLOCK_READ);
                            NAU_ASSERT(ptr);

                            memcpy(data, ptr, size);
                            uav.texture->unlockimg();

                            break;
                        }

                        case SrvDimension::Texture2DArray:
                            NAU_FAILURE_ALWAYS("No lock function for Texture2DArray");

                        case SrvDimension::Texture3D:
                        {
                            int row;
                            int slice;
                            void* ptr = nullptr;

                            uav.texture->lockbox(&ptr, row, slice, 0, TEXLOCK_READ);
                            NAU_ASSERT(ptr);

                            memcpy(data, ptr, size);
                            uav.texture->unlockbox();

                            break;
                        }

                        default:
                            NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                    }

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::setRwTexture(eastl::string_view pipelineName, eastl::string_view bufferName, BaseTexture* rwTexture)
    {
        NAU_ASSERT(rwTexture);
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    TextureInfo info = {};
                    rwTexture->getinfo(info);

                    if (!(info.cflg & TEXCF_UNORDERED))
                    {
                        NAU_FAILURE_ALWAYS("TEXCF_UNORDERED flag is missing!");
                    }

                    switch (bind.dimension)
                    {
                        case SrvDimension::Texture1D:
                        case SrvDimension::Texture1DArray:
                            NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                        case SrvDimension::Texture2D:
                            if (info.resType != RES3D_TEX)
                            {
                                NAU_FAILURE_ALWAYS("The texture type in the material does not match the provided texture. It should be RES3D_TEX");
                            }
                            break;

                        case SrvDimension::Texture2DArray:
                            if (info.resType != RES3D_ARRTEX)
                            {
                                NAU_FAILURE_ALWAYS("The texture type in the material does not match the provided texture. It should be RES3D_ARRTEX");
                            }
                            break;

                        case SrvDimension::Texture3D:
                            if (info.resType != RES3D_VOLTEX)
                            {
                                NAU_FAILURE_ALWAYS("The texture type in the material does not match the provided texture. It should be RES3D_VOLTEX");
                            }
                            break;

                        default:
                            NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                    }

                    auto& rwTex = pipeline.rwTextures[bind.name];
                    if (rwTex.isOwned && rwTex.texture != nullptr)
                    {
                        del_d3dres(rwTex.texture);
                    }

                    rwTex.textureView = nullptr;
                    rwTex.texture = rwTexture;
                    rwTex.slot = bind.bindPoint;
                    rwTex.stages.insert(getStage(shaderAsset->getShader()->target));

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    BaseTexture* MaterialAssetView::getRwTexture(eastl::string_view pipelineName, eastl::string_view bufferName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.rwTextures.contains(bufferName));
                    return pipeline.rwTextures[bind.name].getTexture();
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::createRoTexture(eastl::string_view pipelineName, eastl::string_view bufferName, const TextureDesc& desc)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];

        if (pipeline.roTextures.contains(bufferName))
        {
            auto& tex = pipeline.roTextures[bufferName.data()];
            if (tex.isOwned && tex.texture != nullptr)
            {
                del_d3dres(tex.texture);
            }

            pipeline.roTextures.erase(bufferName.data());
        }

        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type == ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    if (pipeline.roTextures.contains(bufferName))
                    {
                        switch (bind.dimension)
                        {
                            case SrvDimension::Texture1D:
                            case SrvDimension::Texture1DArray:
                                NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                            case SrvDimension::Texture2D:
                                pipeline.roTextures[bind.name] = {
                                    .texture = d3d::create_tex(desc.image, desc.width, desc.height, desc.flags, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            case SrvDimension::Texture2DArray:
                                pipeline.roTextures[bind.name] = {
                                    .texture = d3d::create_array_tex(desc.width, desc.height, desc.depthOrArraySize, desc.flags, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            case SrvDimension::Texture3D:
                                pipeline.roTextures[bind.name] = {
                                    .texture = d3d::create_voltex(desc.width, desc.height, desc.depthOrArraySize, desc.flags, desc.levels, desc.name),
                                    .slot = bind.bindPoint};
                                break;

                            default:
                                NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                        }
                    }

                    pipeline.roTextures[bind.name].stages.insert(getStage(shaderAsset->getShader()->target));
                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::writeRoTexture(eastl::string_view pipelineName, eastl::string_view bufferName, const void* data, size_t size)
    {
        NAU_ASSERT(data);
        NAU_ASSERT(size);

        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type == ShaderInputType::UavRwTyped)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    NAU_ASSERT(pipeline.roTextures.contains(bufferName));
                    auto& srv = pipeline.roTextures[bind.name];

                    switch (bind.dimension)
                    {
                        case SrvDimension::Texture1D:
                        case SrvDimension::Texture1DArray:
                            NAU_FAILURE_ALWAYS("Not supported in Dagor's render");

                        case SrvDimension::Texture2D:
                        {
                            int stride;
                            void* ptr = nullptr;

                            srv.texture->lockimg(&ptr, stride, 0, TEXLOCK_WRITE);
                            NAU_ASSERT(ptr);

                            memcpy(ptr, data, size);
                            srv.texture->unlockimg();

                            break;
                        }

                        case SrvDimension::Texture2DArray:
                            NAU_FAILURE_ALWAYS("No lock function for Texture2DArray");

                        case SrvDimension::Texture3D:
                        {
                            int row;
                            int slice;
                            void* ptr = nullptr;

                            srv.texture->lockbox(&ptr, row, slice, 0, TEXLOCK_WRITE);
                            NAU_ASSERT(ptr);

                            memcpy(ptr, data, size);
                            srv.texture->unlockbox();

                            break;
                        }

                        default:
                            NAU_FAILURE_ALWAYS("Texture '{}' has an unsupported dimension: '{}'", bufferName.data(), toString(bind.dimension));
                    }

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::setRoTexture(eastl::string_view pipelineName, eastl::string_view bufferName, BaseTexture* roTexture)
    {
        NAU_ASSERT(roTexture);
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];
        for (const auto& shaderAsset : pipeline.shaders)
        {
            auto* shader = shaderAsset->getShader();
            for (const auto& bind : shader->reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::Texture || bind.dimension == SrvDimension::Buffer)
                {
                    continue;
                }

                if (bind.name == bufferName)
                {
                    auto& srv = pipeline.roTextures[bind.name];
                    if (srv.isOwned && srv.texture != nullptr)
                    {
                        del_d3dres(srv.texture);
                    }

                    srv.textureView = nullptr;
                    srv.texture = roTexture;
                    srv.slot = bind.bindPoint;
                    srv.stages.insert(getStage(shaderAsset->getShader()->target));

                    return;
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    BaseTexture* MaterialAssetView::getRoTexture(eastl::string_view pipelineName, eastl::string_view bufferName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        if (m_pipelines.contains(pipelineName))
        {
            auto& pipeline = m_pipelines[pipelineName.data()];
            for (const auto& shaderAsset : pipeline.shaders)
            {
                auto* shader = shaderAsset->getShader();
                for (const auto& bind : shader->reflection.inputBinds)
                {
                    if (bind.type != ShaderInputType::Texture)
                    {
                        continue;
                    }

                    if (bind.name == bufferName)
                    {
                        NAU_ASSERT(pipeline.roTextures.contains(bufferName));
                        return pipeline.roTextures[bind.name].getTexture();
                    }
                }
            }
        }

        NAU_FAILURE_ALWAYS("Texture '{}' not found in pipeline '{}'", bufferName.data(), pipelineName.data());
    }

    void MaterialAssetView::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
    {
        NAU_ASSERT(hasComputeShader());
        d3d::dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
    }

    async::Task<MaterialAssetView::Pipeline> MaterialAssetView::makeMasterPipeline(eastl::string_view pipelineName, const MaterialPipeline& materialPipeline, eastl::span<ShaderAssetView::Ptr> shaders)
    {
        struct TextureLoadingResult
        {
            eastl::string bindName;
            TextureCache texture;
            ShaderAssetView* shader;
        };

        eastl::unordered_map<eastl::string, ConstantBufferVariable> properties;
        eastl::unordered_map<eastl::string, SampledTextureProperty> texProperties;
        eastl::unordered_map<eastl::string, BufferCache> constantBuffers;
        eastl::unordered_map<eastl::string, BufferCache> systemCBuffers;
        eastl::unordered_map<eastl::string, TextureCache> textures;
        eastl::unordered_map<eastl::string, SamplerCache> samplers;
        eastl::vector<async::Task<TextureLoadingResult>> textureLoaders;

        const auto addTexture = [&materialPipeline, &texProperties, &textures](const ShaderAssetView& shader, eastl::string inBindName, TextureCache&& inTexCache)
        {
            [[maybe_unused]] auto [iter, emplaceOk] = textures.emplace(std::move(inBindName), std::move(inTexCache));
            NAU_ASSERT(emplaceOk);

            auto& [bindName, texCache] = *iter;

            texCache.stages.insert(getStage(shader.getShader()->target));

            texProperties[bindName] = {
                .parentTexture = &texCache,
                .currentValue = eastl::move(materialPipeline.properties.at(bindName)),
                .masterValue = nullptr,
                .isMasterValue = false};
        };

        for (const ShaderAssetView::Ptr& shaderAsset : shaders)
        {
            const auto& reflection = shaderAsset->getShader()->reflection;
            for (const auto& bind : reflection.inputBinds)
            {
                switch (bind.type)
                {
                    case ShaderInputType::CBuffer:
                    {
                        if (shader_defines::isGlobalBuffer(bind.name))
                        {
                            // Skip global buffers.
                        }
                        else if (shader_defines::isSystemBuffer(bind.name))
                        {
                            if (!systemCBuffers.contains(bind.name))
                            {
                                systemCBuffers[bind.name] = {
                                    .reflection = &bind,
                                    .buffer = nullptr,
                                    .slot = bind.bindPoint,
                                    .isDirty = false};
                            }

                            systemCBuffers[bind.name].stages.insert(getStage(shaderAsset->getShader()->target));
                        }
                        else  // Property constant buffers.
                        {
                            if (!constantBuffers.contains(bind.name))
                            {
                                constantBuffers[bind.name] = {
                                    .reflection = &bind,
                                    .buffer = d3d::create_cb(bind.bufferDesc.size, SBCF_DYNAMIC),
                                    .slot = bind.bindPoint,
                                    .isDirty = true};
                            }

                            constantBuffers[bind.name].stages.insert(getStage(shaderAsset->getShader()->target));

                            for (const auto& var : bind.bufferDesc.variables)
                            {
                                NAU_ASSERT(materialPipeline.properties.contains(var.name));

                                properties[var.name] = {
                                    .reflection = &var,
                                    .parentBuffer = &constantBuffers[bind.name],
                                    .currentValue = eastl::move(materialPipeline.properties.at(var.name)),
                                    .masterValue = nullptr,
                                    .isMasterValue = false};
                            }
                        }
                        break;
                    }

                    case ShaderInputType::Sampler:
                    {
                        if (!samplers.contains(bind.name))
                        {
                            d3d::SamplerInfo sampInfo;
                            samplers[pipelineName.data()] = {
                                .handle = create_sampler(sampInfo),
                                .slot = bind.bindPoint};
                        }

                        samplers[bind.name].stages.insert(getStage(shaderAsset->getShader()->target));
                        break;
                    }

                    case ShaderInputType::Texture:
                    {
                        if (!materialPipeline.properties.contains(bind.name))
                        {
                            break;
                        }
                        if (bind.dimension != SrvDimension::Buffer)
                        {
                            auto property = materialPipeline.properties.at(bind.name);
                            if (property->is<RuntimeStringValue>())
                            {
                                textureLoaders.emplace_back([](eastl::string bindName, uint32_t bindPoint, eastl::string texName, ShaderAssetView* shader) -> async::Task<TextureLoadingResult>
                                {
                                    MaterialAssetRef texAssetRef = AssetPath{texName};
                                    auto texAsset = co_await texAssetRef.getReloadableAssetViewTyped<TextureAssetView>();

                                    co_return TextureLoadingResult{
                                        .bindName = std::move(bindName),

                                        .texture = TextureCache{
                                                                .textureView = texAsset,
                                                                .texture = nullptr,
                                                                .slot = bindPoint,
                                                                .isOwned = false},
                                        .shader = shader
                                    };
                                }(bind.name, bind.bindPoint, *runtimeValueCast<eastl::string>(property), shaderAsset.get()));

                                // continue;
                            }
                            else if (property->is<RuntimeReadonlyCollection>())
                            {
                                const auto color = *runtimeValueCast<math::Vector4>(property);
                                TextureCache texCache{
                                    .texture = generateSolidColorTexture(color),
                                    .slot = bind.bindPoint,
                                    .isOwned = true};

                                addTexture(*shaderAsset, bind.name, std::move(texCache));
                            }
                            else
                            {
                                NAU_FAILURE_ALWAYS("Invalid texture property");
                            }
                        }
                        break;
                    }

                    case ShaderInputType::Structured:
                    case ShaderInputType::UavRwTyped:
                    case ShaderInputType::UavRwStructured:
                    case ShaderInputType::UavRwStructuredWithCounter:
                        // Processed in other place.
                        break;

                    default:
                        NAU_FAILURE_ALWAYS("Not implemented");
                }
            }
        }

        if (!textureLoaders.empty())
        {
            co_await async::whenAll(textureLoaders);
            for (async::Task<TextureLoadingResult>& t : textureLoaders)
            {
                TextureLoadingResult textureResult = *std::move(t);
                addTexture(*textureResult.shader, std::move(textureResult.bindName), std::move(textureResult.texture));
            }
        }

        co_return Pipeline{
            .properties = eastl::move(properties),
            .texProperties = eastl::move(texProperties),
            .constantBuffers = eastl::move(constantBuffers),
            .systemCBuffers = eastl::move(systemCBuffers),
            .samplerTextures = eastl::move(textures),
            .samplers = eastl::move(samplers),
            .programID = PROGRAM_NULL,
            .renderStateId = eastl::nullopt,
            .cullMode = materialPipeline.cullMode,
            .depthMode = materialPipeline.depthMode,
            .blendMode = materialPipeline.blendMode,
            .isScissorsEnabled = materialPipeline.isScissorsEnabled,
            .stencilCmpFunc = materialPipeline.stencilCmpFunc,
            .isDirty = true,
            .isRenderStateDirty = true};
    }

    async::Task<MaterialAssetView::Pipeline> MaterialAssetView::makeInstancePipeline(eastl::string_view pipelineName, const MaterialPipeline& materialPipeline, Pipeline& masterPipeline)
    {
        eastl::unordered_map<eastl::string, ConstantBufferVariable> properties;
        properties.reserve(masterPipeline.properties.size());

        eastl::unordered_map<eastl::string, SampledTextureProperty> texProperties;
        texProperties.reserve(masterPipeline.texProperties.size());

        eastl::unordered_map<eastl::string, BufferCache> constantBuffers;
        constantBuffers.reserve(masterPipeline.constantBuffers.size());

        eastl::unordered_map<eastl::string, BufferCache> systemCBuffers;
        systemCBuffers.reserve(masterPipeline.systemCBuffers.size());

        eastl::unordered_map<eastl::string, TextureCache> textures;
        textures.reserve(masterPipeline.samplerTextures.size());

        for (auto& [name, property] : masterPipeline.properties)
        {
            if (materialPipeline.properties.contains(name))
            {
                properties[name] = {
                    .reflection = property.reflection,
                    .parentBuffer = nullptr,
                    .currentValue = eastl::move(materialPipeline.properties.at(name)),
                    .masterValue = nullptr,
                    .isMasterValue = false};
            }
            else
            {
                properties[name] = {
                    .reflection = property.reflection,
                    .parentBuffer = nullptr,
                    .currentValue = nullptr,
                    .masterValue = &property.currentValue,
                    .isMasterValue = true};
            }
        }

        for (auto& [name, property] : masterPipeline.texProperties)
        {
            if (materialPipeline.properties.contains(name))
            {
                texProperties[name] = {
                    .parentTexture = nullptr,
                    .currentValue = eastl::move(materialPipeline.properties.at(name)),
                    .masterValue = nullptr,
                    .isMasterValue = false};
            }
            else
            {
                texProperties[name] = {
                    .parentTexture = nullptr,
                    .currentValue = nullptr,
                    .masterValue = &property.currentValue,
                    .isMasterValue = true};
            }
        }

        for (const auto& [name, cb] : masterPipeline.constantBuffers)
        {
            constantBuffers[name] = {
                .stages = cb.stages,
                .reflection = cb.reflection,
                .buffer = d3d::create_cb(cb.reflection->bufferDesc.size, SBCF_DYNAMIC),
                .slot = cb.slot,
                .isDirty = true};

            for (const auto& var : cb.reflection->bufferDesc.variables)
            {
                auto& property = properties[var.name];
                property.parentBuffer = &constantBuffers[name];
            }
        }

        for (const auto& [name, cb] : masterPipeline.systemCBuffers)
        {
            systemCBuffers[name] = cb;
        }

        for (const auto& [name, tex] : masterPipeline.samplerTextures)
        {
            if (materialPipeline.properties.contains(name))
            {
                auto property = materialPipeline.properties.at(name);
                if (property->is<RuntimeStringValue>())
                {
                    auto texName = *runtimeValueCast<eastl::string>(property);
                    TextureAssetRef texAssetRef = AssetPath{texName};
                    auto texAsset = co_await texAssetRef.getReloadableAssetViewTyped<TextureAssetView>();
                    textures[name] = {
                        .textureView = texAsset,
                        .texture = nullptr,
                        .slot = tex.slot,
                        .isOwned = false};
                }
                else if (property->is<RuntimeReadonlyCollection>())
                {
                    auto color = *runtimeValueCast<math::Vector4>(property);
                    textures[name] = {
                        .texture = generateSolidColorTexture(color),
                        .slot = tex.slot,
                        .isOwned = true};
                }
                else
                {
                    NAU_FAILURE_ALWAYS("Invalid texture property");
                }

                textures[name].stages = tex.stages;
            }
            else
            {
                auto& t = textures[name];
                t.textureView = tex.textureView;
                t.texture = tex.texture;
                t.stages = tex.stages;
                t.slot = tex.slot;
                t.isOwned = false;
            }
        }

        co_return Pipeline{
            .properties = eastl::move(properties),
            .constantBuffers = eastl::move(constantBuffers),
            .samplerTextures = eastl::move(textures),
            .samplers = masterPipeline.samplers,
            .programID = PROGRAM_NULL,
            .renderStateId = eastl::nullopt,
            .cullMode = materialPipeline.cullMode.has_value() ? materialPipeline.cullMode : masterPipeline.cullMode,
            .depthMode = materialPipeline.depthMode.has_value() ? materialPipeline.depthMode : masterPipeline.depthMode,
            .blendMode = materialPipeline.blendMode.has_value() ? materialPipeline.blendMode : masterPipeline.blendMode,
            .isScissorsEnabled = materialPipeline.isScissorsEnabled.has_value() ? materialPipeline.isScissorsEnabled : masterPipeline.isScissorsEnabled,
            .stencilCmpFunc = materialPipeline.stencilCmpFunc.has_value() ? materialPipeline.stencilCmpFunc : masterPipeline.stencilCmpFunc,
            .isDirty = true,
            .isRenderStateDirty = true};
    }

    void MaterialAssetView::makeCullMode(CullMode cullMode, shaders::RenderState& renderState)
    {
        switch (cullMode)
        {
            case nau::CullMode::None:
                renderState.cull = CULL_NONE;
                break;
            case nau::CullMode::Clockwise:
                renderState.cull = CULL_CW;
                break;
            case nau::CullMode::CounterClockwise:
                renderState.cull = CULL_CCW;
                break;
            default:
                NAU_FAILURE_ALWAYS("Unreachable code");
        }
    }

    void MaterialAssetView::makeDepthMode(DepthMode depthMode, shaders::RenderState& renderState)
    {
        switch (depthMode)
        {
            case DepthMode::Default:
                // Nothing to do.
                break;
            case DepthMode::ReadOnly:
                renderState.zwrite = false;
                break;
            case DepthMode::WriteOnly:
                renderState.ztest = false;
                break;
            case DepthMode::Disabled:
                renderState.ztest = false;
                renderState.zwrite = false;
                break;
            default:
                NAU_FAILURE_ALWAYS("Unreachable code");
        }
    }

    void MaterialAssetView::makeBlendMode(BlendMode blendMode, shaders::RenderState& renderState)
    {
        switch (blendMode)
        {
            case BlendMode::Opaque:
            case BlendMode::Masked:
                // Nothing to do.
                break;
            case BlendMode::Translucent:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
                renderState.blendParams[0].ablendFactors.dst = BLEND_INVSRCALPHA;
                break;
            case BlendMode::Additive:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
                renderState.blendParams[0].ablendFactors.dst = BLEND_ONE;
                break;
            case BlendMode::PremultipliedAlpha:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].sepablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
                renderState.blendParams[0].ablendFactors.dst = BLEND_INVSRCALPHA;
                renderState.blendParams[0].sepablendFactors.src = BLEND_INVSRCALPHA;
                renderState.blendParams[0].sepablendFactors.dst = BLEND_ZERO;
                break;
            case BlendMode::InverseDestinationAlpha:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].sepablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_ONE;
                renderState.blendParams[0].ablendFactors.dst = BLEND_ZERO;
                renderState.blendParams[0].sepablendFactors.src = BLEND_SRCALPHA;
                renderState.blendParams[0].sepablendFactors.dst = BLEND_INVDESTALPHA;
                break;
            case BlendMode::AlphaBlend:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_SRCALPHA;
                renderState.blendParams[0].ablendFactors.dst = BLEND_INVSRCALPHA;
                break;
            case BlendMode::MaxBlend:
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].blendOp = BLENDOP_MAX;
                renderState.blendParams[0].ablendFactors.src = BLEND_ONE;
                renderState.blendParams[0].ablendFactors.dst = BLEND_ONE;
                break;
            case BlendMode::PremultipliedOver:
                // Porter-Duff "over" for premultiplied color: rgb = src.rgb + dst.rgb * (1 - src.a),
                // a = src.a + dst.a * (1 - src.a). Used by the UI layer: drawing into a transparent target with it and
                // compositing the target with it gives the same image as drawing straight into the back buffer.
                renderState.blendParams[0].ablend = true;
                renderState.blendParams[0].sepablend = true;
                renderState.blendParams[0].ablendFactors.src = BLEND_ONE;
                renderState.blendParams[0].ablendFactors.dst = BLEND_INVSRCALPHA;
                renderState.blendParams[0].sepablendFactors.src = BLEND_ONE;
                renderState.blendParams[0].sepablendFactors.dst = BLEND_INVSRCALPHA;
                break;
            default:
                NAU_FAILURE_ALWAYS("Unreachable code");
        }
    }

    void MaterialAssetView::makeStencilCmpFunc(ComparisonFunc cmpFunc, shaders::RenderState& renderState)
    {
        switch (cmpFunc)
        {
            case nau::ComparisonFunc::Disabled:
                renderState.stencil.func = 0;
                break;
            case nau::ComparisonFunc::Never:
                renderState.stencil.func = D3D_CMPF::CMPF_NEVER;
                break;
            case nau::ComparisonFunc::Less:
                renderState.stencil.func = D3D_CMPF::CMPF_LESS;
                break;
            case nau::ComparisonFunc::Equal:
                renderState.stencil.func = D3D_CMPF::CMPF_EQUAL;
                break;
            case nau::ComparisonFunc::LessEqual:
                renderState.stencil.func = D3D_CMPF::CMPF_LESSEQUAL;
                break;
            case nau::ComparisonFunc::Greater:
                renderState.stencil.func = D3D_CMPF::CMPF_GREATER;
                break;
            case nau::ComparisonFunc::NotEqual:
                renderState.stencil.func = D3D_CMPF::CMPF_NOTEQUAL;
                break;
            case nau::ComparisonFunc::GreaterEqual:
                renderState.stencil.func = D3D_CMPF::CMPF_GREATEREQUAL;
                break;
            case nau::ComparisonFunc::Always:
                renderState.stencil.func = D3D_CMPF::CMPF_ALWAYS;
                break;
            default:
                NAU_FAILURE_ALWAYS("Unreachable code");
        }
    }

    void MaterialAssetView::updateBuffers(eastl::string_view pipelineName)
    {
        auto& pipeline = m_pipelines[pipelineName.data()];

        for (auto& [name, cb] : pipeline.constantBuffers)
        {
            if (!cb.isDirty)
            {
                continue;
            }

            Sbuffer* buf = cb.buffer;

            std::byte* data = nullptr;
            buf->lock(0, cb.reflection->bufferDesc.size, reinterpret_cast<void**>(&data), VBLOCK_WRITEONLY | VBLOCK_DISCARD);
            NAU_ASSERT(buf);

            for (const auto& var : cb.reflection->bufferDesc.variables)
            {
                auto& property = pipeline.properties[var.name];

                switch (var.type.svc)
                {
                    case ShaderVariableClass::Scalar:
                    {
                        switch (var.type.svt)
                        {
                            case ShaderVariableType::Int:
                            {
                                auto value = property.isMasterValue
                                                 ? *runtimeValueCast<int32_t>(*property.masterValue)
                                                 : *runtimeValueCast<int32_t>(property.currentValue);

                                memcpy(data + var.startOffset, &value, sizeof(value));
                                break;
                            }
                            case ShaderVariableType::Uint:
                            {
                                auto value = property.isMasterValue
                                                 ? *runtimeValueCast<uint32_t>(*property.masterValue)
                                                 : *runtimeValueCast<uint32_t>(property.currentValue);

                                memcpy(data + var.startOffset, &value, sizeof(value));
                                break;
                            }
                            case ShaderVariableType::Float:
                            {
                                auto value = property.isMasterValue
                                                 ? *runtimeValueCast<float>(*property.masterValue)
                                                 : *runtimeValueCast<float>(property.currentValue);

                                memcpy(data + var.startOffset, &value, sizeof(value));
                                break;
                            }
                            default:
                                NAU_FAILURE_ALWAYS("Not implemented");
                        }
                        break;
                    }
                    case ShaderVariableClass::Vector:
                    {
                        switch (var.type.svt)
                        {
                            case ShaderVariableType::Float:
                            {
                                switch (var.type.columns)
                                {
                                    case 2:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::Vector2>(*property.masterValue)
                                                         : *runtimeValueCast<math::Vector2>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    case 3:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::Vector3>(*property.masterValue)
                                                         : *runtimeValueCast<math::Vector3>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    case 4:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::Vector4>(*property.masterValue)
                                                         : *runtimeValueCast<math::Vector4>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    default:
                                        NAU_FAILURE_ALWAYS("Not implemented");
                                }
                                break;
                            }
                            case ShaderVariableType::Int:
                            case ShaderVariableType::Uint:
                            {
                                switch (var.type.columns)
                                {
                                    case 2:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::IVector2>(*property.masterValue)
                                                         : *runtimeValueCast<math::IVector2>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    case 3:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::IVector3>(*property.masterValue)
                                                         : *runtimeValueCast<math::IVector3>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    case 4:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::IVector4>(*property.masterValue)
                                                         : *runtimeValueCast<math::IVector4>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    default:
                                        NAU_FAILURE_ALWAYS("Not implemented");
                                }
                                break;
                            }
                            default:
                                NAU_FAILURE_ALWAYS("Not implemented");
                        }
                        break;
                    }
                    case ShaderVariableClass::MatrixColumns:
                    {
                        NAU_ASSERT(var.type.columns == var.type.rows);

                        switch (var.type.svt)
                        {
                            case ShaderVariableType::Float:
                            {
                                switch (var.type.columns)
                                {
                                    case 3:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::Matrix3>(*property.masterValue)
                                                         : *runtimeValueCast<math::Matrix3>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    case 4:
                                    {
                                        auto value = property.isMasterValue
                                                         ? *runtimeValueCast<math::Matrix4>(*property.masterValue)
                                                         : *runtimeValueCast<math::Matrix4>(property.currentValue);

                                        memcpy(data + var.startOffset, &value, sizeof(value));
                                        break;
                                    }
                                    default:
                                        NAU_FAILURE_ALWAYS("Not implemented");
                                }
                                break;
                            }
                            default:
                                NAU_FAILURE_ALWAYS("Not implemented");
                        }
                        break;
                    }
                    default:
                        NAU_FAILURE_ALWAYS("Not implemented");
                }
            }

            buf->unlock();
            cb.isDirty = false;
        }

        pipeline.isDirty = false;
    }

    void MaterialAssetView::updateRenderState(eastl::string_view pipelineName)
    {
        auto& pipeline = m_pipelines[pipelineName.data()];

        shaders::RenderState renderState;

        bool needNewRenderState = false;
        if (pipeline.cullMode.has_value())
        {
            makeCullMode(*pipeline.cullMode, renderState);
            needNewRenderState = true;
        }
        if (pipeline.depthMode.has_value())
        {
            makeDepthMode(*pipeline.depthMode, renderState);
            needNewRenderState = true;
        }
        if (pipeline.blendMode.has_value())
        {
            makeBlendMode(*pipeline.blendMode, renderState);
            needNewRenderState = true;
        }
        if (pipeline.isScissorsEnabled.has_value())
        {
            renderState.scissorEnabled = *pipeline.isScissorsEnabled;
            needNewRenderState = true;
        }
        if (pipeline.stencilCmpFunc.has_value())
        {
            makeStencilCmpFunc(*pipeline.stencilCmpFunc, renderState);
            needNewRenderState = true;
        }

        if (needNewRenderState)
        {
            pipeline.renderStateId = eastl::make_optional(shaders::render_states::create(renderState));
        }

        pipeline.isRenderStateDirty = false;
    }

    bool MaterialAssetView::hasComputeShader() const
    {
        for (const auto& [name, pipeline] : m_pipelines)
        {
            for (const auto& shaderAsset : pipeline.shaders)
            {
                if (shaderAsset->getShader()->target == ShaderTarget::Compute)
                {
                    return true;
                }
            }
        }

        return false;
    }

    MasterMaterialAssetView::~MasterMaterialAssetView()
    {
        for (auto& [name, pipeline] : m_pipelines)
        {
            for (auto& [bufName, cb] : pipeline.constantBuffers)
            {
                if (cb.buffer != nullptr)
                {
                    cb.buffer->destroy();
                }
            }

            for (auto& [texName, tex] : pipeline.samplerTextures)
            {
                if (tex.isOwned && tex.texture != nullptr)
                {
                    del_d3dres(tex.texture);
                }
            }

            for (auto& [bufName, rwBuf] : pipeline.rwBuffers)
            {
                if (rwBuf.isOwned && rwBuf.buffer != nullptr)
                {
                    rwBuf.buffer->destroy();
                }
            }

            for (auto& [bufName, roBuf] : pipeline.roBuffers)
            {
                if (roBuf.isOwned && roBuf.buffer != nullptr)
                {
                    roBuf.buffer->destroy();
                }
            }

            d3d::delete_program(pipeline.programID);
        }
    }

    async::Task<Ptr<MasterMaterialAssetView>> MasterMaterialAssetView::createFromMaterial(Material&& material)
    {
        using namespace nau::async;

#if LOAD_MATERIAL_ASYNC
        ASYNC_SWITCH_EXECUTOR(async::Executor::getDefault());
#endif
        struct CreatePipelineResult
        {
            Pipeline pipeline;
            eastl::vector<ShaderAssetView::Ptr> shaders;
            eastl::string name;
        };

        const auto createMasterPipelineAsync = [](eastl::string name, const MaterialPipeline& pipeline) -> Task<CreatePipelineResult>
        {
            CreatePipelineResult result;
            result.shaders.reserve(pipeline.shaders.size());

            for (const eastl::string& shaderAssetPath : pipeline.shaders)
            {
                ShaderAssetRef shaderAssetRef{shaderAssetPath};
                result.shaders.push_back(co_await shaderAssetRef.getAssetViewTyped<ShaderAssetView>());
            }

            result.pipeline = co_await makeMasterPipeline(name, pipeline, result.shaders);
            result.name = std::move(name);

            co_return result;
        };

        eastl::vector<Task<CreatePipelineResult>> pipelineTasks;
        pipelineTasks.reserve(material.pipelines.size());

        for (const auto& [name, pipeline] : material.pipelines)
        {
            pipelineTasks.emplace_back(createMasterPipelineAsync(name, pipeline));
        }

        co_await whenAll(pipelineTasks);

        auto materialAssetView = rtti::createInstance<MasterMaterialAssetView>();
        materialAssetView->m_pipelines.reserve(material.pipelines.size());

        for (Task<CreatePipelineResult>& task : pipelineTasks)
        {
            CreatePipelineResult result = *std::move(task);

            materialAssetView->m_pipelines.emplace(result.name, std::move(result.pipeline));
            materialAssetView->m_pipelines[result.name].programID = ShaderAssetView::makeShaderProgram(result.shaders);
            materialAssetView->m_pipelines[result.name].shaders = eastl::move(result.shaders);

            materialAssetView->updateBuffers(result.name);
            materialAssetView->updateRenderState(result.name);
        }

        materialAssetView->m_defaultProgram = materialAssetView->m_pipelines.begin()->first;
        materialAssetView->m_name = eastl::move(material.name);
        materialAssetView->m_nameHash = nau::strings::constHash(materialAssetView->m_name.data());

        co_return materialAssetView;
    }

    void MasterMaterialAssetView::bind()
    {
        bindPipeline(m_defaultProgram);
    }

    void MasterMaterialAssetView::bindPipeline(eastl::string_view pipelineName)
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& pipeline = m_pipelines[pipelineName.data()];

        d3d::set_program(pipeline.programID);

        setGlobals(pipelineName);

        if (pipeline.isDirty)
        {
            updateBuffers(pipelineName);
        }

        if (pipeline.isRenderStateDirty)
        {
            updateRenderState(pipelineName);
        }

        for (const auto& [name, cb] : pipeline.constantBuffers)
        {
            for (const auto stage : cb.stages)
            {
                d3d::set_const_buffer(stage, cb.slot, cb.buffer);
            }
        }

        for (const auto& [name, cb] : pipeline.systemCBuffers)
        {
            if (cb.buffer != nullptr)
            {
                for (const auto stage : cb.stages)
                {
                    d3d::set_const_buffer(stage, cb.slot, cb.buffer);
                }
            }
        }

        if (m_autoSetTextures)
        {
            for (const auto& [name, tex] : pipeline.samplerTextures)
            {
                for (const auto stage : tex.stages)
                {
                    d3d::set_tex(stage, tex.slot, tex.getTexture());
                }
            }
        }

        for (const auto& [name, sampl] : pipeline.samplers)
        {
            for (const auto stage : sampl.stages)
            {
                set_sampler(stage, sampl.slot, sampl.handle);
            }
        }

        for (const auto& [name, rwBuf] : pipeline.rwBuffers)
        {
            for (const auto stage : rwBuf.stages)
            {
                d3d::set_rwbuffer(stage, rwBuf.slot, rwBuf.buffer);
            }
        }

        for (const auto& [name, roBuf] : pipeline.roBuffers)
        {
            for (const auto stage : roBuf.stages)
            {
                d3d::set_buffer(stage, roBuf.slot, roBuf.buffer);
            }
        }

        for (const auto& [name, rwTex] : pipeline.rwTextures)
        {
            for (const auto stage : rwTex.stages)
            {
                d3d::set_rwtex(stage, rwTex.slot, rwTex.getTexture(), 0, 0);
            }
        }

        for (const auto& [name, roTex] : pipeline.roTextures)
        {
            for (const auto stage : roTex.stages)
            {
                d3d::set_tex(stage, roTex.slot, roTex.getTexture());
            }
        }

        if (pipeline.renderStateId.has_value())
        {
            shaders::render_states::set(*pipeline.renderStateId);
        }
    }

    PROGRAM MasterMaterialAssetView::getPipelineProgram(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_pipelines.contains(pipelineName));
        return m_pipelines.at(pipelineName.data()).programID;
    }

    void MasterMaterialAssetView::setGlobals(eastl::string_view pipelineName)
    {
        static constexpr auto alignment = 16;
        auto& pipeline = m_pipelines[pipelineName.data()];

        for (const auto& shaderAsset : pipeline.shaders)
        {
            const auto& reflection = shaderAsset->getShader()->reflection;
            for (const auto& bind : reflection.inputBinds)
            {
                if (bind.type != ShaderInputType::CBuffer || !shader_defines::isGlobalBuffer(bind.name))
                {
                    continue;
                }

                eastl::vector<std::byte> buffer;
                buffer.resize(bind.bufferDesc.size);

                for (const auto& var : bind.bufferDesc.variables)
                {
                    if (var.type.elements > 0 || var.type.svc == ShaderVariableClass::Struct)
                    {
                        void* value = nullptr;
                        size_t size = 0;
                        shader_globals::getVariable(var.name, &size, &value);
                        memcpy(buffer.data() + var.startOffset, value, size);

                        continue;
                    }

                    switch (var.type.svc)
                    {
                        case ShaderVariableClass::Scalar:
                        {
                            switch (var.type.svt)
                            {
                                case ShaderVariableType::Int:
                                case ShaderVariableType::Uint:
                                case ShaderVariableType::Float:
                                {
                                    void* value = nullptr;
                                    size_t size = 0;
                                    shader_globals::getVariable(var.name, &size, &value);
                                    memcpy(buffer.data() + var.startOffset, value, size);
                                    break;
                                }
                                default:
                                    NAU_FAILURE_ALWAYS("Not implemented");
                            }
                            break;
                        }
                        case ShaderVariableClass::Vector:
                        {
                            switch (var.type.svt)
                            {
                                case ShaderVariableType::Float:
                                {
                                    switch (var.type.columns)
                                    {
                                        case 2:
                                        case 3:
                                        case 4:
                                        {
                                            void* value = nullptr;
                                            size_t size = 0;
                                            shader_globals::getVariable(var.name, &size, &value);
                                            memcpy(buffer.data() + var.startOffset, value, size);
                                            break;
                                        }
                                        default:
                                            NAU_FAILURE_ALWAYS("Not implemented");
                                    }
                                    break;
                                }
                                case ShaderVariableType::Int:
                                case ShaderVariableType::Uint:
                                {
                                    switch (var.type.columns)
                                    {
                                        case 2:
                                        case 3:
                                        case 4:
                                        {
                                            void* value = nullptr;
                                            size_t size = 0;
                                            shader_globals::getVariable(var.name, &size, &value);
                                            memcpy(buffer.data() + var.startOffset, value, size);
                                            break;
                                        }
                                        default:
                                            NAU_FAILURE_ALWAYS("Not implemented");
                                    }
                                    break;
                                }
                                default:
                                    NAU_FAILURE_ALWAYS("Not implemented");
                            }
                            break;
                        }
                        case ShaderVariableClass::MatrixColumns:
                        {
                            NAU_ASSERT(var.type.columns == var.type.rows);

                            switch (var.type.svt)
                            {
                                case ShaderVariableType::Float:
                                {
                                    switch (var.type.columns)
                                    {
                                        case 3:
                                        case 4:
                                        {
                                            void* value = nullptr;
                                            size_t size = 0;
                                            shader_globals::getVariable(var.name, &size, &value);
                                            memcpy(buffer.data() + var.startOffset, value, size);
                                            break;
                                        }
                                        default:
                                            NAU_FAILURE_ALWAYS("Not implemented");
                                    }
                                    break;
                                }
                                default:
                                    NAU_FAILURE_ALWAYS("Not implemented");
                            }
                            break;
                        }
                        default:
                            NAU_FAILURE_ALWAYS("Not implemented");
                    }
                }

                const ShaderTarget shaderTarget = shaderAsset->getShader()->target;
                const unsigned regCount = std::max(1U, static_cast<unsigned>(buffer.size() / alignment));

                if (shaderTarget == ShaderTarget::Vertex)
                {
                    d3d::set_vs_constbuffer_size(regCount);
                }
                else if (shaderTarget == ShaderTarget::Compute)
                {
                    d3d::set_cs_constbuffer_size(regCount);
                }

                d3d::set_const(getStage(shaderTarget), bind.bindPoint, buffer.data(), regCount);
            }
        }
    }

    MaterialInstanceAssetView::~MaterialInstanceAssetView()
    {
        for (auto& [name, pipeline] : m_pipelines)
        {
            for (auto& [bufName, cb] : pipeline.constantBuffers)
            {
                if (cb.buffer != nullptr)
                {
                    cb.buffer->destroy();
                }
            }

            for (auto& [texName, tex] : pipeline.samplerTextures)
            {
                if (tex.isOwned && tex.texture != nullptr)
                {
                    del_d3dres(tex.texture);
                }
            }

            for (auto& [bufName, rwBuf] : pipeline.rwBuffers)
            {
                if (rwBuf.isOwned && rwBuf.buffer != nullptr)
                {
                    rwBuf.buffer->destroy();
                }
            }

            for (auto& [bufName, roBuf] : pipeline.roBuffers)
            {
                if (roBuf.isOwned && roBuf.buffer != nullptr)
                {
                    roBuf.buffer->destroy();
                }
            }
        }
    }

    async::Task<Ptr<MaterialInstanceAssetView>> MaterialInstanceAssetView::createFromMaterial(Material&& material)
    {
#if LOAD_MATERIAL_ASYNC
        ASYNC_SWITCH_EXECUTOR(async::Executor::getDefault());
#endif
        auto materialAssetView = rtti::createInstance<MaterialInstanceAssetView>();

        if (!material.master.has_value())
        {
            NAU_FAILURE_ALWAYS("No master material reference in the material instance: {}", material.name);
        }

        MaterialAssetRef masterAssetRef = AssetPath{*material.master};
        materialAssetView->m_masterMaterial = co_await masterAssetRef.getAssetViewTyped<MaterialAssetView>();

        materialAssetView->m_pipelines.reserve(materialAssetView->m_masterMaterial->m_pipelines.size());
        for (auto& [name, pipeline] : materialAssetView->m_masterMaterial->m_pipelines)
        {
            if (material.pipelines.contains(name))
            {
                materialAssetView->m_pipelines.emplace(name, co_await makeInstancePipeline(name, material.pipelines[name], pipeline));
            }
            else
            {
                NAU_ASSERT(false);
                materialAssetView->m_pipelines[name] = pipeline;
            }

            materialAssetView->updateBuffers(name);
            materialAssetView->updateRenderState(name);
        }

        materialAssetView->m_name = eastl::move(material.name);
        materialAssetView->m_nameHash = nau::strings::constHash(materialAssetView->m_name.data());

        co_return materialAssetView;
    }

    void MaterialInstanceAssetView::bind()
    {
        bindPipeline(m_masterMaterial->m_defaultProgram);
    }

    void MaterialInstanceAssetView::bindPipeline(eastl::string_view pipelineName)
    {
        NAU_ASSERT(m_masterMaterial->m_pipelines.contains(pipelineName));
        NAU_ASSERT(m_pipelines.contains(pipelineName));

        auto& masterPipeline = m_masterMaterial->m_pipelines[pipelineName.data()];
        auto& instancePipeline = m_pipelines[pipelineName.data()];

        d3d::set_program(masterPipeline.programID);

        m_masterMaterial->setGlobals(pipelineName);

        syncBuffers(masterPipeline, instancePipeline);
        syncTextures(masterPipeline, instancePipeline);

        if (instancePipeline.isDirty)
        {
            updateBuffers(pipelineName);
        }

        if (instancePipeline.isRenderStateDirty)
        {
            updateRenderState(pipelineName);
        }

        // Constant buffers, textures, and samplers are always identical to those in the master material.
        // Therefore, no need to validate them as we do for SRVs and UAVs.

        for (const auto& [name, cb] : instancePipeline.constantBuffers)
        {
            for (const auto stage : cb.stages)
            {
                d3d::set_const_buffer(stage, cb.slot, cb.buffer);
            }
        }

        for (const auto& [name, cb] : instancePipeline.systemCBuffers)
        {
            if (cb.buffer != nullptr)
            {
                for (const auto stage : cb.stages)
                {
                    d3d::set_const_buffer(stage, cb.slot, cb.buffer);
                }
            }
        }

        if (m_autoSetTextures)
        {
            for (const auto& [name, tex] : instancePipeline.samplerTextures)
            {
                for (const auto stage : tex.stages)
                {
                    d3d::set_tex(stage, tex.slot, tex.getTexture());
                }
            }
        }

        for (const auto& [name, sampl] : instancePipeline.samplers)
        {
            for (const auto stage : sampl.stages)
            {
                set_sampler(stage, sampl.slot, sampl.handle);
            }
        }

        // Some SRV and UAV resources may exist only in the master material.
        // We must always consider this and process these resources correctly.

        for (const auto& [name, rwBuf] : masterPipeline.rwBuffers)
        {
            const auto& actualRwBuf = instancePipeline.rwBuffers.contains(name)
                                          ? instancePipeline.rwBuffers.at(name)
                                          : rwBuf;

            for (const auto stage : actualRwBuf.stages)
            {
                d3d::set_rwbuffer(stage, actualRwBuf.slot, actualRwBuf.buffer);
            }
        }

        for (const auto& [name, roBuf] : masterPipeline.roBuffers)
        {
            const auto& actualRoBuf = instancePipeline.roBuffers.contains(name)
                                          ? instancePipeline.roBuffers.at(name)
                                          : roBuf;

            for (const auto stage : actualRoBuf.stages)
            {
                d3d::set_buffer(stage, actualRoBuf.slot, actualRoBuf.buffer);
            }
        }

        for (const auto& [name, rwTex] : masterPipeline.rwTextures)
        {
            const auto& actualRwTex = instancePipeline.rwTextures.contains(name)
                                          ? instancePipeline.rwTextures.at(name)
                                          : rwTex;

            for (const auto stage : actualRwTex.stages)
            {
                d3d::set_rwtex(stage, actualRwTex.slot, actualRwTex.getTexture(), 0, 0);
            }
        }

        for (const auto& [name, roTex] : masterPipeline.roTextures)
        {
            const auto& actualRoTex = instancePipeline.roTextures.contains(name)
                                          ? instancePipeline.roTextures.at(name)
                                          : roTex;

            for (const auto stage : actualRoTex.stages)
            {
                d3d::set_tex(stage, actualRoTex.slot, actualRoTex.getTexture());
            }
        }

        if (instancePipeline.renderStateId.has_value())
        {
            shaders::render_states::set(*instancePipeline.renderStateId);
        }
        else if (masterPipeline.renderStateId.has_value())
        {
            shaders::render_states::set(*masterPipeline.renderStateId);
        }
    }

    PROGRAM MaterialInstanceAssetView::getPipelineProgram(eastl::string_view pipelineName) const
    {
        NAU_ASSERT(m_masterMaterial);
        return m_masterMaterial->getPipelineProgram(pipelineName);
    }

    void MaterialInstanceAssetView::syncBuffers(const Pipeline& masterPipeline, Pipeline& instancePipeline)
    {
        for (const auto& [name, property] : masterPipeline.properties)
        {
            auto& instProperty = instancePipeline.properties[name];

            if (instProperty.timestamp >= property.timestamp)
            {
                continue;
            }

            if (instProperty.isMasterValue)
            {
                instProperty.timestamp = property.timestamp;
                instProperty.parentBuffer->isDirty = true;

                instancePipeline.isDirty = true;
            }
        }
    }

    void MaterialInstanceAssetView::syncTextures(const Pipeline& masterPipeline, Pipeline& instancePipeline)
    {
        for (const auto& [name, property] : masterPipeline.texProperties)
        {
            auto& instProperty = instancePipeline.texProperties[name];

            if (instProperty.timestamp >= property.timestamp)
            {
                continue;
            }

            if (instProperty.isMasterValue)
            {
                instProperty.timestamp = property.timestamp;
                instProperty.parentTexture->textureView = property.parentTexture->textureView;
                instProperty.parentTexture->texture = property.parentTexture->texture;
            }
        }
    }
}  // namespace nau
