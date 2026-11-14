#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "nau/mgpu/Graphics/GRootSignature.h"
#include "nau/mgpu/Graphics/DirectXBuffers.h"
#include "nau/mgpu/Graphics/GShader.h"
#include "nau/mgpu/Graphics/PSO.h"
#include "SimpleMath.h"

namespace nau::PEPEngine::Graphics
{
    struct GenerateMipsCB
    {
        DirectX::SimpleMath::Vector2 TexelSize; // 1.0 / OutMip1.Dimensions
    };


    class ComputePSO final : public PSO
    {
    public:
        ComputePSO();
        ComputePSO(const GRootSignature& RS);
        void SetShader(const GShader* shader);

        void Initialize(const std::shared_ptr<GDevice>& device) override;
        HRESULT TryInitialize(const std::shared_ptr<GDevice>& device);
        PSOType GetType() override { return PSOType::Compute; }
        void SetRootSignature(const GRootSignature& rs) override;

    private:
        D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    };
}
