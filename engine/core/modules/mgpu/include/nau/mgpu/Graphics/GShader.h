#pragma once
#include <d3d12.h>
#include <d3dcommon.h>
#include <string>
#include <wrl/client.h>

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    enum ShaderType
    {
        VertexShader,
        ComputeShader,
        PixelShader,
        DomainShader,
        GeometryShader,
        HullShader
    };

    class GShader
    {
    protected:
        std::wstring FileName;
        ComPtr<ID3DBlob> shaderBlob = nullptr;
        ShaderType type;
        const D3D_SHADER_MACRO* defines;
        std::string entryPoint;
        std::string target;
        bool IsInited = false;

    public:
        GShader(const std::wstring& fileName, ShaderType type, const D3D_SHADER_MACRO* defines = nullptr,
                const std::string& entryPoint = "Main", const std::string& target = "ps_5_1");

        GShader();

        virtual void LoadAndCompile();

        ID3DBlob* GetShaderData() const;

        D3D12_SHADER_BYTECODE GetShaderResource() const;

        ShaderType GetType() const;
    };
}
