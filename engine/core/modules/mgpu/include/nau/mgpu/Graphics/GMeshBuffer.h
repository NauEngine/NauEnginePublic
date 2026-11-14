#pragma once

#include "nau/mgpu/Graphics/GBuffer.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    class GCommandList;

    class GMeshBuffer : public GBuffer
    {
    protected:
        const DXGI_FORMAT IndexFormat = DXGI_FORMAT_R32_UINT;

        D3D12_INDEX_BUFFER_VIEW ibv = {};
        D3D12_VERTEX_BUFFER_VIEW vbv = {};

    public:
        static GMeshBuffer CreateBuffer(const std::shared_ptr<GCommandList>& cmdList, const void* data, UINT elementSize,
                                        UINT count, const std::wstring& name = L"");

        GMeshBuffer(const GMeshBuffer& rhs);

        GMeshBuffer& operator=(const GMeshBuffer& a);

        D3D12_INDEX_BUFFER_VIEW* IndexBufferView();

        D3D12_VERTEX_BUFFER_VIEW* VertexBufferView();

    private:
        GMeshBuffer(const std::shared_ptr<GCommandList>& cmdList,
                    UINT elementSize, UINT elementCount,
                    const void* data, const std::wstring& name = L"");
    };
}
