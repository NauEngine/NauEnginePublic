#pragma once

#include <memory>

#include "nau/mgpu/Graphics/PSO.h"
#include "nau/mgpu/Graphics/GShader.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    enum class RenderMode : uint8_t
    {
        SkyBox,
        Opaque,
        DynamicOpaque,
        Wireframe,
        OpaqueAlphaDrop,
        AlphaSprites,
        Mirror,
        Reflection,
        ShadowMapOpaque,
        ShadowMapOpaqueDrop,
        Transparent,
        DrawNormalsOpaque,
        DrawNormalsOpaqueDrop,
        Ssao,
        SsaoBlur,
        Debug,
        Quad,
        UI,
        Particle,
        MBVelocity,
        Terrain, // AtmosphereApp
        AtmospherePostProcess, // AtmosphereApp
        Ssr,
        SsrDebug,
        Count
    };

    class GraphicPSO final : public PSO
    {
        RenderMode type;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};

    public:
        GraphicPSO(RenderMode type = RenderMode::Opaque);
        GraphicPSO(const GRootSignature& rs, RenderMode type = RenderMode::Opaque);

        void SetPsoDesc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC GetPsoDescription() const;

        void SetInputLayout(D3D12_INPUT_LAYOUT_DESC layoutDesc);

        void SetRasterizationState(const D3D12_RASTERIZER_DESC& rastState);

        void SetRenderTargetBlendState(UINT index, const D3D12_RENDER_TARGET_BLEND_DESC& desc);

        void SetBlendState(const D3D12_BLEND_DESC& blendDesc);

        void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& ddsDesc);

        void SetDSVFormat(DXGI_FORMAT format);

        void SetSampleMask(UINT sampleMask);

        void SetPrimitiveType(D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType);

        void SetRenderTargetsCount(UINT count);

        void SetRTVFormat(UINT index, DXGI_FORMAT format);

        void SetSampleCount(UINT count);

        void SetSampleQuality(UINT quality);

        void SetShader(const GShader* shader);

        RenderMode GetRenderMode() const;

        void Initialize(const std::shared_ptr<GDevice>& device) override;
        PSOType GetType() override { return PSOType::Graphics; }
        void SetRootSignature(const GRootSignature& rs) override;
    };
}
