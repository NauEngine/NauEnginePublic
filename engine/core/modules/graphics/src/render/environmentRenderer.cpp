// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#include "nau/render/environmentRenderer.h"

#include <graphics_assets/material_asset.h>
#include <nau/graphics/core_graphics.h>

#include "nau/3d/dag_drv3d.h"
#include "nau/shaders/dag_shaderCommon.h"
#include "nau/shaders/shader_defines.h"
#include "nau/shaders/shader_globals.h"

namespace nau::render
{
    namespace details
    {
        static void submitEnvironmentComputeBatch()
        {
            // Keep expensive cubemap faces in separate GPU submissions so that
            // the initial lighting bake does not monopolize a single DMA batch.
            auto *query = d3d::create_event_query();
            d3d::issue_event_query(query);
            d3d::get_event_query_status(query, true);
            d3d::release_event_query(query);
        }

        static const std::vector<uint16_t> kEnvironmentIndices{0, 3, 1, 0, 2, 3, 4, 2, 0, 4, 6, 2, 5, 6, 4, 5, 7, 6, 1, 7,
                                                               5, 1, 3, 7, 5, 0, 1, 5, 4, 0, 7, 3, 2, 7, 2, 6};
        static const uint32_t kEnvironmentIndexCount = static_cast<uint32_t>(kEnvironmentIndices.size());
        static const uint32_t kEnvironmentPrimsCount = kEnvironmentIndexCount / 3;

        static math::ivec3 calculateWorkGroupCount(const int texSize, const math::vec3& workGroupSize)
        {
            const auto calculate = [](uint32_t dimension, uint32_t groupSize)
            {
                return dimension / groupSize + std::min(dimension % groupSize, 1u);
            };

            const math::ivec3 groupCount = {
                int(calculate(texSize, workGroupSize.getX())),
                int(calculate(texSize, workGroupSize.getY())),
                int(calculate(1, workGroupSize.getZ()))};

            return groupCount;
        }

        static uint32_t calculateMipLevelCount(uint32_t maxImageSize)
        {
            const float maxSize = static_cast<float>(maxImageSize);
            return 1 + static_cast<uint32_t>(std::floor(std::log2f(maxSize)));
        }

        static uint32_t calculateMipSize(uint32_t texSize, uint32_t mipLevel)
        {
            return std::max(static_cast<uint32_t>(texSize * std::pow(0.5f, mipLevel)), 1u);
        }
    }  // namespace details

    EnvironmentRenderer::~EnvironmentRenderer() = default;

    EnvironmentRenderer::EnvironmentRenderer(MaterialAssetView::Ptr envCubemapMaterial, ShaderAssetView::Ptr panoramaToCubemapComputeShader, ShaderAssetView::Ptr genIrradianceMapComputeShader, ShaderAssetView::Ptr genReflectionMapComputeShader) :
        m_envCubemapMaterial(envCubemapMaterial),
        m_panoramaToCubemapCS(panoramaToCubemapComputeShader),
        m_genIrradianceMapCS(genIrradianceMapComputeShader),
        m_genReflectionMapCS(genReflectionMapComputeShader)
    {
        createSkyboxIndexBuffer();

        m_envCubemapTexture = d3d::create_cubetex(CUBEMAP_ENV_FACE_SIZE, TEXFMT_A32B32G32R32F | TEXCF_RTARGET | TEXCF_UNORDERED, 0);

        m_irradianceMap = d3d::create_cubetex(IRRADIANCE_MAP_FACE_SIZE, TEXFMT_A32B32G32R32F | TEXCF_RTARGET | TEXCF_UNORDERED, 1);  // no mips
        m_reflectionMap = d3d::create_cubetex(REFLECTION_MAP_FACE_SIZE, TEXFMT_A32B32G32R32F | TEXCF_RTARGET | TEXCF_UNORDERED, 0);

        d3d::SamplerInfo csTexSamplerInfo;
        m_csTexSampler = d3d::create_sampler(csTexSamplerInfo);

        m_panoramaToCubemapCSProgram = ShaderAssetView::makeShaderProgram(make_span(&m_panoramaToCubemapCS, 1));
        m_genIrradianceMapCSProgram = ShaderAssetView::makeShaderProgram(make_span(&m_genIrradianceMapCS, 1));
        m_genReflectionMapCSProgram = ShaderAssetView::makeShaderProgram(make_span(&m_genReflectionMapCS, 1));
    }

    void EnvironmentRenderer::setPanoramaTexture(ReloadableAssetView::Ptr panoramaTex)
    {
        m_panoramaTextureView = panoramaTex;
        m_envCubemapsDirty = true;
    }

    CubeTexture* EnvironmentRenderer::getEnvCubemap() const
    {
        return m_envCubemapTexture;
    }

    CubeTexture* EnvironmentRenderer::getIrradianceMap() const
    {
        return m_irradianceMap;
    }
    CubeTexture* EnvironmentRenderer::getReflectionMap() const
    {
        return m_reflectionMap;
    }

    void EnvironmentRenderer::createSkyboxIndexBuffer()
    {
        const uint32_t bufferSize = details::kEnvironmentIndexCount * 2;

        m_envCubemapIndexBuffer = d3d::create_ib(bufferSize, SBCF_DYNAMIC, u8"IndexBuf");

        uint16_t* uint16_data = nullptr;
        m_envCubemapIndexBuffer->lock(0, bufferSize, reinterpret_cast<void**>(&uint16_data), VBLOCK_WRITEONLY);

        std::memcpy(uint16_data, details::kEnvironmentIndices.data(), bufferSize);

        m_envCubemapIndexBuffer->unlock();
    }

    void EnvironmentRenderer::setEnvCubemapsDirty(bool value)
    {
        m_envCubemapsDirty = value;
    }

    bool EnvironmentRenderer::isEnvCubemapsDirty() const
    {
        nau::Ptr<TextureAssetView> textureView;
        m_panoramaTextureView->getTyped<TextureAssetView>(textureView);
        return m_envCubemapsDirty || (textureView != m_panoramaTextureViewCached);
    }

    void EnvironmentRenderer::renderSkybox(Texture* renderTargetHDR, Texture* sceneDepth, const nau::math::Matrix4& viewMatrix, const nau::math::Matrix4& projMatrix) const
    {
        NAU_ASSERT(getServiceProvider().has<nau::ICoreGraphics>());

        nau::shader_globals::setVariable("viewMatrix", &viewMatrix);
        nau::shader_globals::setVariable("projectionMatrix", &projMatrix);

        m_envCubemapMaterial->bind();

        d3d::set_render_target(renderTargetHDR, 0);
        d3d::set_depth(sceneDepth, DepthAccess::RW);

        d3d::settex(0, m_envCubemapTexture);
        d3d::setind(m_envCubemapIndexBuffer);
        d3d::setvsrc(0, nullptr, 0);

        d3d::drawind(PRIM_TRILIST, 0, details::kEnvironmentPrimsCount, 0);
    }

    void EnvironmentRenderer::convertPanoramaToCubemap()
    {
        const math::vec3 workgroupSize{CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1};
        math::ivec3 csPanoramaToCubeMapGroupCount = details::calculateWorkGroupCount(CUBEMAP_ENV_FACE_SIZE, workgroupSize);

        d3d::set_program(m_panoramaToCubemapCSProgram);
        d3d::set_cs_constbuffer_size(4);

        m_panoramaTextureView->getTyped<TextureAssetView>(m_panoramaTextureViewCached);

        d3d::set_tex(STAGE_CS, 0, m_panoramaTextureViewCached->getTexture());
        d3d::set_sampler(STAGE_CS, 0, m_csTexSampler);

        for (int faceIndex = 0; faceIndex < CUBE_FACE_COUNT; ++faceIndex)
        {
            d3d::set_rwtex(STAGE_CS, 0, m_envCubemapTexture, faceIndex, 0, false);

            uint32_t cbData[4] = {uint32_t(faceIndex), CUBEMAP_ENV_FACE_SIZE, 0, 0};
            d3d::set_const(STAGE_CS, 0, &cbData[0], 1);

            d3d::dispatch(csPanoramaToCubeMapGroupCount.getX(), csPanoramaToCubeMapGroupCount.getY(), csPanoramaToCubeMapGroupCount.getZ(), GpuPipeline::ASYNC_COMPUTE);
        }
        m_envCubemapTexture->generateMips();
    }

    void EnvironmentRenderer::generateIrradianceMap()
    {
        const math::vec3 workgroupSize{CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1};
        math::ivec3 genIrradianceGroupCount = details::calculateWorkGroupCount(IRRADIANCE_MAP_FACE_SIZE, workgroupSize);

        d3d::set_program(m_genIrradianceMapCSProgram);
        d3d::set_cs_constbuffer_size(4);

        d3d::set_tex(STAGE_CS, 0, m_envCubemapTexture);
        d3d::set_sampler(STAGE_CS, 0, m_csTexSampler);

        for (int faceIndex = 0; faceIndex < CUBE_FACE_COUNT; ++faceIndex)
        {
            d3d::set_rwtex(STAGE_CS, 0, m_irradianceMap, faceIndex, 0, false);

            uint32_t cbData[4] = {uint32_t(faceIndex), IRRADIANCE_MAP_FACE_SIZE, 0, 0};
            d3d::set_const(STAGE_CS, 0, &cbData[0], 1);

            d3d::dispatch(genIrradianceGroupCount.getX(), genIrradianceGroupCount.getY(), genIrradianceGroupCount.getZ(), GpuPipeline::ASYNC_COMPUTE);
            details::submitEnvironmentComputeBatch();
        }
    }

    void EnvironmentRenderer::generateReflectionMap()
    {
        d3d::set_program(m_genReflectionMapCSProgram);
        d3d::set_cs_constbuffer_size(4);

        d3d::set_tex(STAGE_CS, 0, m_envCubemapTexture);
        d3d::set_sampler(STAGE_CS, 0, m_csTexSampler);

        const uint32_t reflectionMipLevelCount = details::calculateMipLevelCount(REFLECTION_MAP_FACE_SIZE);
        const float maxMipLevel = static_cast<float>(reflectionMipLevelCount - 1);

        const math::vec3 workgroupSize{CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1};
        std::byte cbData[16];

        for (uint32_t mipLevel = 0; mipLevel < reflectionMipLevelCount; ++mipLevel)
        {
            for (uint32_t faceIndex = 0; faceIndex < CUBE_FACE_COUNT; ++faceIndex)
            {
                const uint32_t faceSize = details::calculateMipSize(REFLECTION_MAP_FACE_SIZE, mipLevel);
                const math::ivec3 groupCount = details::calculateWorkGroupCount(faceSize, workgroupSize);
                const float roughness = static_cast<float>(mipLevel) / maxMipLevel;

                std::memcpy(&cbData[0], &faceIndex, 4);
                std::memcpy(&cbData[4], &faceSize, 4);
                std::memcpy(&cbData[8], &roughness, 4);

                d3d::set_rwtex(STAGE_CS, 0, m_reflectionMap, faceIndex, mipLevel, false);
                d3d::set_const(STAGE_CS, 0, &cbData[0], 1);

                d3d::dispatch(groupCount.getX(), groupCount.getY(), groupCount.getZ(), GpuPipeline::ASYNC_COMPUTE);
                details::submitEnvironmentComputeBatch();
            }
        }
    }
}  // namespace nau::render
