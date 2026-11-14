#include "nau/mgpu/Graphics/GShader.h"
#include "nau/mgpu/Graphics/D3DShaderLoad.h"

namespace nau::PEPEngine::Graphics
{
    GShader::GShader(const std::wstring& fileName, const ShaderType type, const D3D_SHADER_MACRO* defines,
                     const std::string& entryPoint, const std::string& target) : FileName(fileName), type(type),
                                                                                 defines(defines), entryPoint(entryPoint),
                                                                                 target(target)
    {
    }

    GShader::GShader()
    {
    }

    void GShader::LoadAndCompile()
    {
        if (IsInited) return;
        shaderBlob = CompileShader(FileName, defines, entryPoint, target);
        IsInited = true;
    }

    ID3DBlob* GShader::GetShaderData() const
    {
        return shaderBlob.Get();
    }

    D3D12_SHADER_BYTECODE GShader::GetShaderResource() const
    {
        D3D12_SHADER_BYTECODE info{shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize()};
        return info;
    }

    ShaderType GShader::GetType() const
    {
        return type;
    }
}
