#pragma once

#include <wrl.h>
#include <d3d12.h>
#include <dxgi.h>
#include "nau/mgpu/Utils/d3dx12.h"
#include <string>
#include <exception>

using namespace Microsoft::WRL;

// Hashers for view descriptions.
namespace std
{
    // Source: https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
    template <typename T>
    void hash_combine(std::size_t& seed, const T& v)
    {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template <>
    struct hash<D3D12_SHADER_RESOURCE_VIEW_DESC>
    {
        std::size_t operator()(const D3D12_SHADER_RESOURCE_VIEW_DESC& srvDesc) const noexcept
        {
            std::size_t seed = 0;

            hash_combine(seed, srvDesc.Format);
            hash_combine(seed, srvDesc.ViewDimension);
            hash_combine(seed, srvDesc.Shader4ComponentMapping);

            switch (srvDesc.ViewDimension)
            {
            case D3D12_SRV_DIMENSION_BUFFER:
                hash_combine(seed, srvDesc.Buffer.FirstElement);
                hash_combine(seed, srvDesc.Buffer.NumElements);
                hash_combine(seed, srvDesc.Buffer.StructureByteStride);
                hash_combine(seed, srvDesc.Buffer.Flags);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE1D:
                hash_combine(seed, srvDesc.Texture1D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture1D.MipLevels);
                hash_combine(seed, srvDesc.Texture1D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
                hash_combine(seed, srvDesc.Texture1DArray.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture1DArray.MipLevels);
                hash_combine(seed, srvDesc.Texture1DArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture1DArray.ArraySize);
                hash_combine(seed, srvDesc.Texture1DArray.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2D:
                hash_combine(seed, srvDesc.Texture2D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture2D.MipLevels);
                hash_combine(seed, srvDesc.Texture2D.PlaneSlice);
                hash_combine(seed, srvDesc.Texture2D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
                hash_combine(seed, srvDesc.Texture2DArray.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture2DArray.MipLevels);
                hash_combine(seed, srvDesc.Texture2DArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture2DArray.ArraySize);
                hash_combine(seed, srvDesc.Texture2DArray.PlaneSlice);
                hash_combine(seed, srvDesc.Texture2DArray.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMS:
                //                hash_combine(seed, srvDesc.Texture2DMS.UnusedField_NothingToDefine);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
                hash_combine(seed, srvDesc.Texture2DMSArray.FirstArraySlice);
                hash_combine(seed, srvDesc.Texture2DMSArray.ArraySize);
                break;
            case D3D12_SRV_DIMENSION_TEXTURE3D:
                hash_combine(seed, srvDesc.Texture3D.MostDetailedMip);
                hash_combine(seed, srvDesc.Texture3D.MipLevels);
                hash_combine(seed, srvDesc.Texture3D.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURECUBE:
                hash_combine(seed, srvDesc.TextureCube.MostDetailedMip);
                hash_combine(seed, srvDesc.TextureCube.MipLevels);
                hash_combine(seed, srvDesc.TextureCube.ResourceMinLODClamp);
                break;
            case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
                hash_combine(seed, srvDesc.TextureCubeArray.MostDetailedMip);
                hash_combine(seed, srvDesc.TextureCubeArray.MipLevels);
                hash_combine(seed, srvDesc.TextureCubeArray.First2DArrayFace);
                hash_combine(seed, srvDesc.TextureCubeArray.NumCubes);
                hash_combine(seed, srvDesc.TextureCubeArray.ResourceMinLODClamp);
                break;
                // TODO: Update Visual Studio?
                //case D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE:
                //    hash_combine(seed, srvDesc.RaytracingAccelerationStructure.Location);
                //    break;
            }

            return seed;
        }
    };

    template <>
    struct hash<D3D12_UNORDERED_ACCESS_VIEW_DESC>
    {
        std::size_t operator()(const D3D12_UNORDERED_ACCESS_VIEW_DESC& uavDesc) const noexcept
        {
            std::size_t seed = 0;

            hash_combine(seed, uavDesc.Format);
            hash_combine(seed, uavDesc.ViewDimension);

            switch (uavDesc.ViewDimension)
            {
            case D3D12_UAV_DIMENSION_BUFFER:
                hash_combine(seed, uavDesc.Buffer.FirstElement);
                hash_combine(seed, uavDesc.Buffer.NumElements);
                hash_combine(seed, uavDesc.Buffer.StructureByteStride);
                hash_combine(seed, uavDesc.Buffer.CounterOffsetInBytes);
                hash_combine(seed, uavDesc.Buffer.Flags);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE1D:
                hash_combine(seed, uavDesc.Texture1D.MipSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
                hash_combine(seed, uavDesc.Texture1DArray.MipSlice);
                hash_combine(seed, uavDesc.Texture1DArray.FirstArraySlice);
                hash_combine(seed, uavDesc.Texture1DArray.ArraySize);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE2D:
                hash_combine(seed, uavDesc.Texture2D.MipSlice);
                hash_combine(seed, uavDesc.Texture2D.PlaneSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
                hash_combine(seed, uavDesc.Texture2DArray.MipSlice);
                hash_combine(seed, uavDesc.Texture2DArray.FirstArraySlice);
                hash_combine(seed, uavDesc.Texture2DArray.ArraySize);
                hash_combine(seed, uavDesc.Texture2DArray.PlaneSlice);
                break;
            case D3D12_UAV_DIMENSION_TEXTURE3D:
                hash_combine(seed, uavDesc.Texture3D.MipSlice);
                hash_combine(seed, uavDesc.Texture3D.FirstWSlice);
                hash_combine(seed, uavDesc.Texture3D.WSize);
                break;
            }

            return seed;
        }
    };
}

namespace nau::PEPEngine
{
    namespace Math
    {
        constexpr float PI = 3.1415926535897932384626433832795f;
        constexpr float _2PI = 2.0f * PI;
        // Convert radians to degrees.
        constexpr float Degrees(const float radians)
        {
            return radians * (180.0f / PI);
        }

        // Convert degrees to radians.
        constexpr float Radians(const float degrees)
        {
            return degrees * (PI / 180.0f);
        }

        template <typename T>
        T Deadzone(T val, T deadzone)
        {
            if (std::abs(val) < deadzone)
            {
                return T(0);
            }

            return val;
        }

        // Normalize a value in the range [min - max]
        template <typename T, typename U>
        T NormalizeRange(U x, U min, U max)
        {
            return T(x - min) / T(max - min);
        }

        // Shift and bias a value into another range.
        template <typename T, typename U>
        T ShiftBias(U x, U shift, U bias)
        {
            return T(x * bias) + T(shift);
        }

        /***************************************************************************
        * These functions were taken from the MiniEngine.
        * Source code available here:
        * https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/MiniEngine/Core/Math/Common.h
        * Retrieved: January 13, 2016
        **************************************************************************/
        template <typename T>
        T AlignUpWithMask(T value, const size_t mask)
        {
            return static_cast<T>((static_cast<size_t>(value) + mask) & ~mask);
        }

        template <typename T>
        T AlignDownWithMask(T value, const size_t mask)
        {
            return static_cast<T>(static_cast<size_t>(value) & ~mask);
        }

        template <typename T>
        T AlignUp(T value, const size_t alignment)
        {
            return AlignUpWithMask(value, alignment - 1);
        }

        template <typename T>
        T AlignDown(T value, const size_t alignment)
        {
            return AlignDownWithMask(value, alignment - 1);
        }

        template <typename T>
        bool IsAligned(T value, const size_t alignment)
        {
            return 0 == (static_cast<size_t>(value) & (alignment - 1));
        }

        template <typename T>
        T DivideByMultiple(T value, size_t alignment)
        {
            return static_cast<T>((value + alignment - 1) / alignment);
        }

        /***************************************************************************/

        /**
        * Round up to the next highest power of 2.
        * @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        * @retrieved: January 16, 2016
        */
        inline uint32_t NextHighestPow2(uint32_t v)
        {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v++;

            return v;
        }

        /**
        * Round up to the next highest power of 2.
        * @source: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
        * @retrieved: January 16, 2016
        */
        inline uint64_t NextHighestPow2(uint64_t v)
        {
            v--;
            v |= v >> 1;
            v |= v >> 2;
            v |= v >> 4;
            v |= v >> 8;
            v |= v >> 16;
            v |= v >> 32;
            v++;

            return v;
        }
    }

    namespace Utils
    {
        constexpr int globalCountFrameResources = 3;


        static inline UINT Align(const UINT size, const UINT alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)
        {
            return (size + alignment - 1) & ~(alignment - 1);
        }


        template <typename T>
        T DivideByMultiple(T value, size_t alignment)
        {
            return static_cast<T>((value + alignment - 1) / alignment);
        }


        inline HRESULT ReadDataFromFile(const LPCWSTR filename, byte** data, UINT* size)
        {
            using namespace Microsoft::WRL;

            CREATEFILE2_EXTENDED_PARAMETERS extendedParams = {};
            extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
            extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
            extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
            extendedParams.lpSecurityAttributes = nullptr;
            extendedParams.hTemplateFile = nullptr;

            Wrappers::FileHandle file(CreateFile2(filename, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                                  &extendedParams));
            if (file.Get() == INVALID_HANDLE_VALUE)
            {
                throw std::exception();
            }

            FILE_STANDARD_INFO fileInfo = {};
            if (!GetFileInformationByHandleEx(file.Get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
            {
                throw std::exception();
            }

            if (fileInfo.EndOfFile.HighPart != 0)
            {
                throw std::exception();
            }

            *data = reinterpret_cast<byte*>(malloc(fileInfo.EndOfFile.LowPart));
            *size = fileInfo.EndOfFile.LowPart;

            if (!ReadFile(file.Get(), *data, fileInfo.EndOfFile.LowPart, nullptr, nullptr))
            {
                throw std::exception();
            }

            return S_OK;
        }


        inline void d3dSetDebugName(IDXGIObject* obj, const char* name)
        {
            if (obj)
            {
                obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
            }
        }

        inline void d3dSetDebugName(ID3D12Device* obj, const char* name)
        {
            if (obj)
            {
                obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
            }
        }

        inline void d3dSetDebugName(ID3D12DeviceChild* obj, const char* name)
        {
            if (obj)
            {
                obj->SetPrivateData(WKPDID_D3DDebugObjectName, lstrlenA(name), name);
            }
        }

        inline DXGI_FORMAT GetSRGBFormat(const DXGI_FORMAT format)
        {
            DXGI_FORMAT srgbFormat = format;

            return srgbFormat;
            switch (format)
            {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
                srgbFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
                break;
            case DXGI_FORMAT_BC1_UNORM:
                srgbFormat = DXGI_FORMAT_BC1_UNORM_SRGB;
                break;
            case DXGI_FORMAT_BC2_UNORM:
                srgbFormat = DXGI_FORMAT_BC2_UNORM_SRGB;
                break;
            case DXGI_FORMAT_BC3_UNORM:
                srgbFormat = DXGI_FORMAT_BC3_UNORM_SRGB;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
                srgbFormat = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
                break;
            case DXGI_FORMAT_B8G8R8X8_UNORM:
                srgbFormat = DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
                break;
            case DXGI_FORMAT_BC7_UNORM:
                srgbFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
                break;
            }

            return srgbFormat;
        }

        inline std::wstring AnsiToWString(const std::string& str)
        {
            WCHAR buffer[512];
            MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
            return std::wstring(buffer);
        }


#if defined(_DEBUG)
#ifndef Assert
#define Assert(x, description)                                  \
    {                                                               \
        static bool ignoreAssert = false;                           \
        if(!ignoreAssert && !(x))                                   \
        {                                                           \
            Debug::AssertResult result = Debug::ShowAssertDialog(   \
            (L""#x), description, AnsiToWString(__FILE__), __LINE__); \
        if(result == Debug::AssertIgnore)                           \
        {                                                           \
            ignoreAssert = true;                                    \
        }                                                           \
                    else if(result == Debug::AssertBreak)           \
        {                                                           \
            __debugbreak();                                         \
        }                                                           \
        }                                                           \
    }
#endif
#else
#ifndef Assert
#define Assert(x, description)
#endif
#endif


        class d3dUtil
        {
        public:
            static bool IsKeyDown(int vkeyCode);

            static std::string ToString(HRESULT hr);

            static UINT CalcConstantBufferByteSize(const UINT byteSize)
            {
                // Constant buffers must be a multiple of the minimum hardware
                // allocation size (usually 256 bytes).  So round up to nearest
                // multiple of 256.  We do this by adding 255 and then masking off
                // the lower 2 bytes which store all bits < 256.
                // Example: Suppose byteSize = 300.
                // (300 + 255) & ~255
                // 555 & ~255
                // 0x022B & ~0x00ff
                // 0x022B & 0xff00
                // 0x0200
                // 512
                return (byteSize + 255) & ~255;
            }

            static UINT CalcStructedCounteredBufferByteSize(UINT byteSize)
            {
                constexpr UINT ali = D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT;
                byteSize = (byteSize + ali - 1) & ~(ali - 1); // alignment the whole buffer to 256 bytes
                byteSize += sizeof(UINT); // add counter at the end of buffer
                return byteSize;
            }
        };

        class DxException
        {
        public:
            DxException() = default;
            DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber,
                        const std::wstring& message = L"");
            DxException(const std::wstring& message);

            std::wstring ToString() const;

            HRESULT ErrorCode = S_OK;
            std::wstring FunctionName;
            std::wstring Filename;
            int LineNumber = -1;
            std::wstring message;
        };


#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L""#x, wfn, __LINE__); } \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif
    }
}
