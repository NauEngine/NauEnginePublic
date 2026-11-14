#pragma once

// Helpers shared by the Mgpu bridges (mgpu_bridge.cpp, mgpu_shadows.cpp). Internal to the Mgpu module: it includes
// Mgpu headers, which must not leak into Nau code.

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

#include <d3dcompiler.h>

#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GDeviceFactory.h"
#include "nau/mgpu/Graphics/GShader.h"

namespace nau::mgpu::bridge::detail
{
    namespace gfx = nau::PEPEngine::Graphics;

    inline std::string ToUtf8(const std::wstring& wide)
    {
        if (wide.empty())
        {
            return {};
        }

        const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0,
                                             nullptr, nullptr);
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), result.data(), size, nullptr,
                            nullptr);
        return result;
    }

    inline std::wstring ToWide(const char* utf8)
    {
        if (utf8 == nullptr || utf8[0] == '\0')
        {
            return {};
        }

        const int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
        if (size <= 1)
        {
            return {};
        }

        std::wstring result(static_cast<size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result.data(), size);
        return result;
    }

    // GShader compiles from a file relative to the working directory; the bridges compile their shaders from
    // memory instead, so nothing has to be deployed next to the executable.
    class SourceShader final : public gfx::GShader
    {
    public:
        SourceShader(const char* source, const gfx::ShaderType shaderType, const std::string& entry,
                     const std::string& profile) :
            gfx::GShader(L"MgpuBridge", shaderType, nullptr, entry, profile),
            m_source(source)
        {
        }

        void LoadAndCompile() override
        {
            if (IsInited)
            {
                return;
            }

            ComPtr<ID3DBlob> errors;
            const HRESULT hr = D3DCompile(m_source, std::strlen(m_source), entryPoint.c_str(), nullptr, nullptr,
                                          entryPoint.c_str(), target.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                                          shaderBlob.ReleaseAndGetAddressOf(), errors.GetAddressOf());
            if (FAILED(hr))
            {
                const std::string details = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no details";
                throw std::runtime_error("shader " + entryPoint + " compilation failed: " + details);
            }

            IsInited = true;
        }

    private:
        const char* m_source;
    };

    // The primary device is the Mgpu device on the adapter Nau renders on (nauAdapterName, d3d::get_device_name());
    // nullptr or "" picks the first hardware adapter (standalone tests). The secondary one is another hardware adapter.
    inline bool SelectDevices(const char* nauAdapterName, std::shared_ptr<gfx::GDevice>& primary,
                              std::shared_ptr<gfx::GDevice>& secondary, std::string& error)
    {
        // Inside a process that already owns a D3D12 device (Nau) the debug layer must not be enabled after the fact.
        if (nauAdapterName != nullptr)
        {
            gfx::GDeviceFactory::SetDebugLayerEnabled(false);
        }

        auto& devices = gfx::GDeviceFactory::GetAllDevices(false);
        if (devices.size() < 2)
        {
            error = "need two distinct hardware adapters, found " + std::to_string(devices.size());
            return false;
        }

        size_t primaryIndex = 0;
        const std::wstring wantedName = ToWide(nauAdapterName);
        bool matched = wantedName.empty();
        for (size_t i = 0; i < devices.size() && !matched; ++i)
        {
            const std::wstring name = devices[i]->GetName();
            if (name == wantedName || name.find(wantedName) != std::wstring::npos ||
                wantedName.find(name) != std::wstring::npos)
            {
                primaryIndex = i;
                matched = true;
            }
        }

        if (!matched)
        {
            error = "Nau adapter '" + ToUtf8(wantedName) + "' was not found among the Mgpu adapters";
            return false;
        }

        primary = devices[primaryIndex];
        secondary = devices[primaryIndex == 0 ? 1 : 0];
        return true;
    }
}
