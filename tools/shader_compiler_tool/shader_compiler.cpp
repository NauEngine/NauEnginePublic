// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "shader_compiler.h"
#include "utf8_path.h"

#include <format>
#include <fstream>
#include <unordered_set>

#include <d3d12shader.h>
#include <dxcapi.h>

#include <comdef.h>
#include <wrl.h>

#ifndef _UNICODE
    #define NAU_TCHAR_ERROR(handle)                                                         \
        {                                                                                   \
            return NauMakeError(_com_error(handle).ErrorMessage());                         \
        }
    #define NAU_TCHAR_ERROR_FORMAT(handle, format)                                          \
        {                                                                                   \
            return NauMakeError(format, _com_error(handle).ErrorMessage());                 \
        }
    #define NAU_VERIFY_SUCCEEDED(expr)                                                                                                                                                  \
        {                                                                                                                                                                               \
            const auto hRes = (expr);                                                                                                                                                   \
            static_assert(std::is_same_v<std::remove_const_t<decltype(hRes)>, ::HRESULT>, "Expected HRESULT");                                                                          \
            if(FAILED(hRes))                                                                                                                                                            \
            {                                                                                                                                                                           \
                std::cerr << std::format("Expression {} failed with error code {:#08x}: {}", (#expr), static_cast<unsigned long>(hRes), _com_error(hRes).ErrorMessage()) << std::flush; \
            }                                                                                                                                                                           \
        }
#else
    #define NAU_TCHAR_ERROR(handle)                                                         \
        {                                                                                   \
            const std::wstring error = _com_error(handle).ErrorMessage();                   \
            return NauMakeError(std::string(error.begin(), error.end()));                   \
        }
    #define NAU_TCHAR_ERROR_FORMAT(handle, format)                                          \
        {                                                                                   \
            const std::wstring error = _com_error(handle).ErrorMessage();                   \
            return NauMakeError(format, std::string(error.begin(), error.end()));           \
        }
    #define NAU_VERIFY_SUCCEEDED(expr)                                                                                                                                                          \
        {                                                                                                                                                                                       \
            const auto hRes = (expr);                                                                                                                                                           \
            static_assert(std::is_same_v<std::remove_const_t<decltype(hRes)>, ::HRESULT>, "Expected HRESULT");                                                                                  \
            const std::wstring error = _com_error(hRes).ErrorMessage();                                                                                                                         \
            if(FAILED(hRes))                                                                                                                                                                    \
            {                                                                                                                                                                                   \
                std::cerr << std::format("Expression {} failed with error code {:#08x}: {}", (#expr), static_cast<unsigned long>(hRes), std::string(error.begin(), error.end())) << std::flush; \
            }                                                                                                                                                                                   \
        }
#endif

template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

namespace nau
{
    namespace
    {
        constexpr LPCWSTR EntryPoints[] = {
            L"VSMain",
            L"PSMain",
            L"GSMain",
            L"HSMain",
            L"DSMain",
            L"CSMain",
        };
        static_assert(
            std::size(EntryPoints) == static_cast<size_t>(ShaderTarget::Count),
            "EntryPoints array size does not match the number of ShaderTarget enum values"
        );

        constexpr LPCWSTR TargetProfiles[] = {
            L"vs_6_0",
            L"ps_6_0",
            L"gs_6_0",
            L"hs_6_0",
            L"ds_6_0",
            L"cs_6_0",
        };
        static_assert(
            std::size(TargetProfiles) == static_cast<size_t>(ShaderTarget::Count),
            "TargetProfiles array size does not match the number of ShaderTarget enum values"
        );

        class IncludeHandler final : public IDxcIncludeHandler
        {
        public:
            HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource) override;
            HRESULT QueryInterface(const IID& riid, void** ppvObject) override;
            ULONG AddRef() override;
            ULONG Release() override;

            void SetUtils(IDxcUtils* utils);
            void ClearIncludedFiles();

        private:
            std::unordered_set<std::wstring> m_includedFiles;

            IDxcUtils* m_utils = nullptr;
        };

        HRESULT IncludeHandler::LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource)
        {
            NAU_ASSERT(m_utils);

            HRESULT result = E_FAIL;
            ComPtr<IDxcBlobEncoding> encoding = nullptr;

            const std::wstring path(pFilename);
            if (m_includedFiles.contains(path))
            {
                static constexpr char nullstr[] = " ";

                result = m_utils->CreateBlobFromPinned(nullstr, ARRAYSIZE(nullstr), DXC_CP_ACP, &encoding);
                if (SUCCEEDED(result))
                {
                    *ppIncludeSource = encoding.Detach();
                }

                return result;
            }

            result = m_utils->LoadFile(pFilename, nullptr, &encoding);
            if (SUCCEEDED(result))
            {
                m_includedFiles.insert(path);
                *ppIncludeSource = encoding.Detach();
            }

            return result;
        }

        HRESULT IncludeHandler::QueryInterface(const IID& riid, void** ppvObject)
        {
            return E_NOINTERFACE;
        }

        ULONG IncludeHandler::AddRef()
        {
            return 0;
        }

        ULONG IncludeHandler::Release()
        {
            return 0;
        }

        void IncludeHandler::SetUtils(IDxcUtils* utils)
        {
            m_utils = utils;
        }

        void IncludeHandler::ClearIncludedFiles()
        {
            m_includedFiles.clear();
        }

        Result<ShaderVariableTypeDescription> getVariableTypeDescription(ID3D12ShaderReflectionType* type)
        {
            if (!type)
            {
                return NauMakeError("Can not get variable type reflecton");
            }

            D3D12_SHADER_TYPE_DESC typeDesc = {};
            const HRESULT hr = type->GetDesc(&typeDesc);
            if (FAILED(hr))
            {
                NAU_TCHAR_ERROR(hr);
            }

            ShaderVariableTypeDescription varTypeDesc = {};
            varTypeDesc.svc = static_cast<ShaderVariableClass>(typeDesc.Class);
            varTypeDesc.svt = static_cast<ShaderVariableType>(typeDesc.Type);
            varTypeDesc.rows = typeDesc.Rows;
            varTypeDesc.columns = typeDesc.Columns;
            varTypeDesc.elements = typeDesc.Elements;
            varTypeDesc.name = typeDesc.Name;

            for (auto i = 0; i < typeDesc.Members; ++i)
            {
                const eastl::string memberName = type->GetMemberTypeName(i);
                ID3D12ShaderReflectionType* memberType = type->GetMemberTypeByIndex(i);

                auto memberTypeDesc = getVariableTypeDescription(memberType);
                NauCheckResult(memberTypeDesc);

                varTypeDesc.members[memberName] = std::move(*memberTypeDesc);
            }

            return {std::move(varTypeDesc)};
        }

        Result<> saveBlobToFile(IDxcBlob* blob, const wchar_t* filename)
        {
            if (!blob || !filename)
            {
                return NauMakeError("Blob or filenam is nullptr");
            }

            const void* data = blob->GetBufferPointer();
            SIZE_T size = blob->GetBufferSize();

            std::ofstream file(filename, std::ios::binary | std::ios::out);
            if (!file)
            {
                return NauMakeError("Failed to create file: {}", nau::shader_tools::pathToUtf8(fs::path(filename)));
            }

            file.write(static_cast<const char*>(data), size);
            if (!file)
            {
                return NauMakeError("Failed to write data to file: {}", nau::shader_tools::pathToUtf8(fs::path(filename)));
            }

            return ResultSuccess;
        }
    } // anonymous namespace

    class ShaderCompilerImpl final
    {
    public:
        ShaderCompilerImpl();

        Result<> loadFile(const fs::path& filename);
        Result<Shader> getResult() const;

        Result<> compile(
            ShaderTarget stage,
            std::string_view entry,
            const std::vector<std::wstring>& defines,
            const std::vector<std::wstring>& includeDirs,
            const std::optional<fs::path>& pdbFilename,
            bool needEmdedDebug);

        void reset();

        Result<BytesBuffer> getBytecode() const;
        Result<ShaderReflection> getReflection() const;

    private:
        ComPtr<IDxcBlobEncoding> m_source = nullptr;
        ComPtr<IDxcResult> m_compileResult = nullptr;
        ComPtr<IDxcUtils> m_utils = nullptr;

        std::optional<ShaderTarget> m_target = std::nullopt;
        std::string m_entry;
        fs::path m_filename;

        IncludeHandler m_includeHandler;
    };

    ShaderCompilerImpl::ShaderCompilerImpl()
    {
        NAU_VERIFY_SUCCEEDED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&m_utils)));

        m_includeHandler.SetUtils(m_utils.Get());
    }

    Result<> ShaderCompilerImpl::loadFile(const fs::path& filename)
    {
        NAU_ASSERT(m_utils);

        if (!exists(filename))
        {
            return NauMakeError("File not found");
        }

        const HRESULT result = m_utils->LoadFile(filename.c_str(), nullptr, &m_source);
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        m_filename = filename;

        return ResultSuccess;
    }

    Result<Shader> ShaderCompilerImpl::getResult() const
    {
        auto reflection = getReflection();
        if (reflection.isError())
        {
            return NauMakeError(reflection.getError()->getMessage());
        }

        auto bytecode = getBytecode();
        if (bytecode.isError())
        {
            return NauMakeError(bytecode.getError()->getMessage());
        }

        Shader shader;
        shader.srcName = nau::shader_tools::pathToUtf8(m_filename.filename()).c_str();
        shader.target = *m_target;
        shader.entryPoint = m_entry.c_str();
        shader.reflection = std::move(*reflection);
        shader.bytecode = std::move(*bytecode);

        return std::move(shader);
    }

    Result<> ShaderCompilerImpl::compile(
        ShaderTarget stage,
        std::string_view entry,
        const std::vector<std::wstring>& defines,
        const std::vector<std::wstring>& includeDirs,
        const std::optional<fs::path>& pdbFilename,
        bool needEmdedDebug)
    {
        if (!m_source || m_source->GetBufferSize() == 0)
        {
            return NauMakeError("Source file not loaded");
        }

        m_target = stage;
        m_entry = entry;

        ComPtr<IDxcCompiler3> compiler = nullptr;
        HRESULT result = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        std::vector<LPCWSTR> arguments;

        const std::wstring filename = m_filename.filename().c_str();
        arguments.push_back(filename.c_str());

        const std::wstring ep = {entry.begin(), entry.end()};
        arguments.push_back(L"-E");
        arguments.push_back(ep.c_str());

        arguments.push_back(L"-T");
        arguments.push_back(TargetProfiles[static_cast<size_t>(stage)]);

        for (const auto& define : defines)
        {
            arguments.push_back(L"-D");
            arguments.push_back(define.c_str());
        }

        for (const auto& includeDir : includeDirs)
        {
            arguments.push_back(L"-I");
            arguments.push_back(includeDir.c_str());
        }

        if (needEmdedDebug || pdbFilename.has_value())
        {
            arguments.push_back(DXC_ARG_DEBUG);

            if (needEmdedDebug)
            {
                arguments.push_back(L"-Qembed_debug");
            }
            else
            {
                arguments.push_back(L"-Qstrip_debug");
            }

            if (pdbFilename.has_value())
            {
                arguments.push_back(L"-Fd");
                arguments.push_back(pdbFilename->c_str());
            }
        }

        arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);

        const DxcBuffer sourceBuffer = {
            .Ptr = m_source->GetBufferPointer(),
            .Size = m_source->GetBufferSize(),
            .Encoding = DXC_CP_ACP
        };

        m_includeHandler.ClearIncludedFiles();

        result = compiler->Compile(&sourceBuffer, arguments.data(), arguments.size(), &m_includeHandler, IID_PPV_ARGS(&m_compileResult));
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        ComPtr<IDxcBlobUtf8> error = nullptr;
        result = m_compileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&error), nullptr);
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        // Here might be not only errors messages, but warnigs too. We don't need to return error if we have warning.
        // TODO(MaxWolf): Don't return here, only print message. Need return when check status.
        if (error && error->GetStringLength() > 0)
        {
            return NauMakeError("Shader compiled with errors:\n{}", error->GetStringPointer());
        }

        HRESULT status;
        result = m_compileResult->GetStatus(&status);

        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        if (FAILED(status))
        {
            NAU_TCHAR_ERROR_FORMAT(result, "Shader compiled with status: {}");
        }

        if (pdbFilename.has_value())
        {
            ComPtr<IDxcBlob> pdb = nullptr;
            ComPtr<IDxcBlobUtf16> filename = nullptr;
            result = m_compileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pdb), &filename);
            if (FAILED(result))
            {
                NAU_TCHAR_ERROR(result);
            }

            NauCheckResult(saveBlobToFile(pdb.Get(), filename->GetStringPointer()));
        }

        return ResultSuccess;
    }

    void ShaderCompilerImpl::reset()
    {
        m_source.Reset();
        m_compileResult.Reset();

        m_target.reset();

        m_entry.clear();
        m_filename.clear();
    }

    Result<BytesBuffer> ShaderCompilerImpl::getBytecode() const
    {
        if (!m_compileResult)
        {
            return NauMakeError("Shader not compiled");
        }

        ComPtr<IDxcBlob> shader = nullptr;

        const HRESULT result = m_compileResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shader), nullptr);
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        BytesBuffer bytecode(shader->GetBufferSize());
        memcpy(bytecode.data(), shader->GetBufferPointer(), shader->GetBufferSize());

        return bytecode;
    }

    Result<ShaderReflection> ShaderCompilerImpl::getReflection() const
    {
        NAU_ASSERT(m_utils);

        if (!m_compileResult)
        {
            return NauMakeError("Shader not compiled");
        }
        if (!m_target.has_value())
        {
            return NauMakeError("Invalid shader target");
        }

        ComPtr<IDxcBlob> reflectionData = nullptr;
        HRESULT result = m_compileResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&reflectionData), nullptr);
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        if (!reflectionData)
        {
            return NauMakeError("Cannot get shder reflection");
        }

        const DxcBuffer data = {
            .Ptr = reflectionData->GetBufferPointer(),
            .Size = reflectionData->GetBufferSize(),
            .Encoding = DXC_CP_ACP
        };

        ComPtr<ID3D12ShaderReflection> reflection = nullptr;
        result = m_utils->CreateReflection(&data, IID_PPV_ARGS(&reflection));
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        D3D12_SHADER_DESC shaderDesc = {};
        result = reflection->GetDesc(&shaderDesc);
        if (FAILED(result))
        {
            NAU_TCHAR_ERROR(result);
        }

        ShaderReflection shaderReflection = {};

        if (*m_target == ShaderTarget::Vertex)
        {
            shaderReflection.signatureParams.reserve(shaderDesc.InputParameters);

            for (auto i = 0; i < shaderDesc.InputParameters; ++i)
            {
                D3D12_SIGNATURE_PARAMETER_DESC d3d12Desc = {};
                result = reflection->GetInputParameterDesc(i, &d3d12Desc);
                if (FAILED(result))
                {
                    NAU_TCHAR_ERROR(result);
                }

                if (eastl::string_view(d3d12Desc.SemanticName).starts_with("SV_"))
                {
                    continue;
                }

                const SignatureParameterDescription desc = {
                    .semanticName = d3d12Desc.SemanticName,
                    .semanticIndex = d3d12Desc.SemanticIndex,
                    .registerIndex = d3d12Desc.Register,
                    .componentType = static_cast<RegisterComponentType>(d3d12Desc.ComponentType),
                    .mask = d3d12Desc.Mask,
                    .readWriteMask = d3d12Desc.ReadWriteMask,
                    .stream = d3d12Desc.Stream
                };

                shaderReflection.signatureParams.push_back(desc);
            }
        }

        shaderReflection.inputBinds.reserve(shaderDesc.BoundResources);

        for (auto i = 0; i < shaderDesc.BoundResources; ++i)
        {
            auto& inputBindDesc = shaderReflection.inputBinds.emplace_back();

            D3D12_SHADER_INPUT_BIND_DESC shaderInputBindDesc = {};
            result = reflection->GetResourceBindingDesc(i, &shaderInputBindDesc);
            if (FAILED(result))
            {
                NAU_TCHAR_ERROR(result);
            }

            inputBindDesc.name = shaderInputBindDesc.Name;
            inputBindDesc.type = static_cast<ShaderInputType>(shaderInputBindDesc.Type);
            inputBindDesc.bindPoint = shaderInputBindDesc.BindPoint;
            inputBindDesc.bindCount = shaderInputBindDesc.BindCount;
            inputBindDesc.flags = shaderInputBindDesc.uFlags;
            inputBindDesc.returnType = static_cast<ResourceReturnType>(shaderInputBindDesc.ReturnType);
            inputBindDesc.dimension = static_cast<SrvDimension>(shaderInputBindDesc.Dimension);
            inputBindDesc.numSamples = shaderInputBindDesc.NumSamples;
            inputBindDesc.space = shaderInputBindDesc.Space;

            if (inputBindDesc.type != ShaderInputType::CBuffer)
            {
                continue;
            }

            ID3D12ShaderReflectionConstantBuffer* constantBuffer = reflection->GetConstantBufferByIndex(i);
            if (!constantBuffer)
            {
                return NauMakeError("Cannot get constant buffer reflection");
            }

            D3D12_SHADER_BUFFER_DESC constantBufferDesc = {};
            result = constantBuffer->GetDesc(&constantBufferDesc);
            if (FAILED(result))
            {
                NAU_TCHAR_ERROR(result);
            }

            inputBindDesc.bufferDesc.name = constantBufferDesc.Name;
            inputBindDesc.bufferDesc.type = static_cast<CBufferType>(constantBufferDesc.Type);
            inputBindDesc.bufferDesc.size = constantBufferDesc.Size;
            inputBindDesc.bufferDesc.flags = constantBufferDesc.uFlags;

            inputBindDesc.bufferDesc.variables.reserve(constantBufferDesc.Variables);

            for (auto j = 0; j < constantBufferDesc.Variables; ++j)
            {
                auto& varDesc = inputBindDesc.bufferDesc.variables.emplace_back();

                ID3D12ShaderReflectionVariable* variable = constantBuffer->GetVariableByIndex(j);
                if (!variable)
                {
                    return NauMakeError("Cannot get variable reflection");
                }

                D3D12_SHADER_VARIABLE_DESC variableDesc = {};
                result = variable->GetDesc(&variableDesc);
                if (FAILED(result))
                {
                    NAU_TCHAR_ERROR(result);
                }

                varDesc.name = variableDesc.Name;
                varDesc.startOffset = variableDesc.StartOffset;
                varDesc.size = variableDesc.Size;
                varDesc.flags = variableDesc.uFlags;
                varDesc.startTexture = variableDesc.StartTexture;
                varDesc.textureSize = variableDesc.TextureSize;
                varDesc.startSampler = variableDesc.StartSampler;
                varDesc.samplerSize = variableDesc.SamplerSize;

                ID3D12ShaderReflectionType* type = variable->GetType();
                auto varDescType = getVariableTypeDescription(type);
                NauCheckResult(varDescType);

                varDesc.type = std::move(*varDescType);
            }
        }

        return shaderReflection;
    }

    ShaderCompiler::ShaderCompiler() :
        m_pimpl(std::make_unique<ShaderCompilerImpl>())
    {
    }

    ShaderCompiler::~ShaderCompiler() noexcept = default;

    Result<> ShaderCompiler::loadFile(const fs::path& filename)
    {
        return m_pimpl->loadFile(filename);
    }

    Result<Shader> ShaderCompiler::getResult() const
    {
        return m_pimpl->getResult();
    }

    Result<> ShaderCompiler::compile(
        ShaderTarget stage,
        std::string_view entry,
        const std::vector<std::wstring>& defines,
        const std::vector<std::wstring>& includeDirs,
        const std::optional<fs::path>& pdbFilename,
        bool needEmdedDebug)
    {
        return m_pimpl->compile(stage, entry, defines, includeDirs, pdbFilename, needEmdedDebug);
    }

    void ShaderCompiler::reset()
    {
        m_pimpl->reset();
    }

    Result<BytesBuffer> ShaderCompiler::getBytecode() const
    {
        return m_pimpl->getBytecode();
    }

    Result<ShaderReflection> ShaderCompiler::getReflection() const
    {
        return m_pimpl->getReflection();
    }
} // namespace nau
