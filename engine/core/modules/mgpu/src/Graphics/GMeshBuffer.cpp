#include "nau/mgpu/Graphics/GMeshBuffer.h"

namespace nau::PEPEngine::Graphics
{
    using namespace Utils;

    D3D12_INDEX_BUFFER_VIEW* GMeshBuffer::IndexBufferView()
    {
        return &ibv;
    }

    D3D12_VERTEX_BUFFER_VIEW* GMeshBuffer::VertexBufferView()
    {
        return &vbv;
    }


    GMeshBuffer GMeshBuffer::CreateBuffer(const std::shared_ptr<GCommandList>& cmdList, const void* data,
                                          const UINT elementSize,
                                          const UINT count,
                                          const std::wstring& name)
    {
        GMeshBuffer buffer(cmdList, elementSize, count, data, name);
        return buffer;
    }

    GMeshBuffer::GMeshBuffer(const GMeshBuffer& rhs) : GBuffer(rhs)
    {
        this->ibv = rhs.ibv;
        this->vbv = rhs.vbv;
    }

    GMeshBuffer& GMeshBuffer::operator=(const GMeshBuffer& a)
    {
        GBuffer::operator=(a);
        this->ibv = a.ibv;
        this->vbv = a.vbv;
        return *this;
    }

    GMeshBuffer::GMeshBuffer(const std::shared_ptr<GCommandList>& cmdList, const UINT elementSize,
                             const UINT elementCount,
                             const void* data, const std::wstring& name) : GBuffer(
        cmdList, elementSize, elementCount, data, name)
    {
        ibv.BufferLocation = dxResource->GetGPUVirtualAddress();
        ibv.Format = IndexFormat;
        ibv.SizeInBytes = bufferSize;

        vbv.BufferLocation = dxResource->GetGPUVirtualAddress();
        vbv.StrideInBytes = stride;
        vbv.SizeInBytes = bufferSize;
    }
}
