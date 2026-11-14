#pragma once
#include <memory>

#include "nau/mgpu/Graphics/GRootSignature.h"
#include "nau/mgpu/Graphics/GShader.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    class GDevice;

    enum class PSOType : uint8_t
    {
        Graphics,
        Compute,
        Count
    };

    class PSO
    {
    protected:
        ComPtr<ID3D12PipelineState> nativePSO;
        GRootSignature rs;

    public:
        PSO(const GRootSignature& RS) : rs(RS)
        {
        }

        PSO()
        {
        }

        bool IsInitialized() const { return nativePSO != nullptr; }

        virtual PSOType GetType() = 0;

        virtual void Initialize(const std::shared_ptr<GDevice>& device) = 0;

        ComPtr<ID3D12PipelineState> GetPSO() const
        {
            return nativePSO;
        }

        const GRootSignature& GetRootSignature() const
        {
            return rs;
        }

        void virtual SetRootSignature(const GRootSignature& rs)
        {
            this->rs = rs;
        }
    };
}
