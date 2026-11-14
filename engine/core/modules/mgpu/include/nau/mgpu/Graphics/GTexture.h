#pragma once
#include "nau/mgpu/Graphics/GCommandQueue.h"
#include "nau/mgpu/Graphics/ComputePSO.h"
#include "nau/mgpu/Graphics/GResource.h"

namespace nau::PEPEngine::Graphics
{
    enum class TextureUsage
    {
        Albedo,
        Diffuse = Albedo,
        // Treat Diffuse and Albedo textures the same.
        Heightmap,
        Depth = Heightmap,
        // Treat height and depth textures the same.
        Normalmap,
        RenderTarget,
        // Texture is used as a render target.
        MotionBlur
    };


    class GTexture : public GResource
    {
        TextureUsage usage;

        std::vector<ComPtr<ID3D12Resource>> track = std::vector<ComPtr<ID3D12Resource>>();

        std::wstring filePath;

    public:
        bool HasMipMap = false;

        std::wstring GetFilePath() const;

        static void Resize(GTexture& texture, uint32_t width, uint32_t height, uint32_t depthOrArraySize);


        static void GenerateMipMaps(std::shared_ptr<GCommandList>& cmdList, GTexture** textures, size_t count);
        TextureUsage GetTextureType() const;

        GTexture(const std::wstring& name = L"", TextureUsage use = TextureUsage::Diffuse);

        GTexture(const std::shared_ptr<GDevice>& device, const D3D12_RESOURCE_DESC& resourceDesc,
                 const std::wstring& name = L"", TextureUsage textureUsage = TextureUsage::Albedo,
                 const D3D12_CLEAR_VALUE* clearValue = nullptr);
        GTexture(const std::shared_ptr<GDevice>& device, ComPtr<ID3D12Resource> resource,
                 TextureUsage textureUsage = TextureUsage::Albedo,
                 const std::wstring& name = L"");

        GTexture(const GTexture& copy);
        GTexture(GTexture&& copy);

        GTexture& operator=(const GTexture& other);
        GTexture& operator=(GTexture&& other);


        ~GTexture() override;


        static std::shared_ptr<GTexture> LoadTextureFromFile(std::wstring filepath,
                                                             std::shared_ptr<GCommandList> commandList,
                                                             TextureUsage usage = TextureUsage::Diffuse);

        void ClearTrack();


        std::wstring& GetName();

        static DXGI_FORMAT GetUAVCompatableFormat(DXGI_FORMAT format);

        static bool IsUAVCompatibleFormat(DXGI_FORMAT format);

        static bool IsSRGBFormat(DXGI_FORMAT format);

        static bool IsBGRFormat(DXGI_FORMAT format);

        static bool IsDepthFormat(DXGI_FORMAT format);

        static DXGI_FORMAT GetTypelessFormat(DXGI_FORMAT format);
    };
}
