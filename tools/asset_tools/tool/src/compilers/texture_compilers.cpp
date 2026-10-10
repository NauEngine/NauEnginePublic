// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/compilers/texture_compilers.h"

#include "nau/asset_tools/asset_compiler.h"
#include "nau/asset_tools/asset_info.h"
#include "nau/asset_tools/asset_utils.h"
#include "nau/assets/asset_container.h"
#include "nau/assets/asset_container_builder.h"
#include "nau/assets/asset_manager.h"
#include "nau/assets/asset_ref.h"
#include "nau/assets/texture_asset_accessor.h"
#include "nau/diag/logging.h"
#include "nau/io/file_system.h"
#include "nau/io/io_constants.h"
#include "nau/io/memory_stream.h"
#include "nau/io/stream.h"
#include "nau/io/stream_utils.h"
#include "nau/io/virtual_file_system.h"
#include "nau/service/service.h"
#include "nau/service/service_provider.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"

namespace nau
{
    namespace compilers
    {
        IAssetContainerLoader* getTextureLoader(eastl::string_view extension)
        {
            for (auto* const loader : getServiceProvider().getAll<IAssetContainerLoader>())
            {
                eastl::vector<eastl::string_view> supportedExtensions = loader->getSupportedAssetKind();
                if (std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end())
                {
                    return loader;
                }
            }

            return nullptr;
        }

        bool saveDdsTexture(nau::Ptr<nau::io::IFile> file, IAssetContainerLoader* textureLoader, std::string& out, const char* extension)
        {
            auto originalAssetContainerTask = textureLoader->loadFromStream(file->createStream(), {extension, "", textureLoader->getDefaultImportSettings()});
            async::wait(originalAssetContainerTask);
            auto originalAssetContainer = *originalAssetContainerTask;

            nau::Ptr<> asset = originalAssetContainer->getAsset();
            NAU_ASSERT(asset);

            IAssetContainerBuilder& builder = getServiceProvider().get<IAssetContainerBuilder>();

            std::replace(out.begin(), out.end(), '\\', '/');

            io::IStreamWriter::Ptr stream = io::createNativeFileStream(out.c_str(), io::AccessMode::Write, io::OpenFileMode::CreateAlways);
            builder.writeAssetToStream(stream, asset).ignore();

            return true;
        }

        nau::Result<AssetMetaInfo> compileTextureToDds(PXR_NS::UsdStageRefPtr stage, const std::string& path, const std::string& outputPath, const nau::UsdMetaInfo& metaInfo, const char* textureSourceExtension, int folderIndex, const std::string_view& ext)
        {
            FileSystem fs;
            auto& vfs = getServiceProvider().get<io::IFileSystem>();

            std::string sourceRelativePath = pathToUtf8(FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(path), true));
            std::string fileRelativePath = std::format("project/{}/{}.{}", getAssetsSubfolderDefaultName() , sourceRelativePath, textureSourceExtension);

            auto file = vfs.openFile(fileRelativePath, io::AccessMode::Read, io::OpenFileMode::OpenExisting);
            if (!file)
            {
                return NauMakeError("Failed to open texture {}", path);
            }
            auto loader = getTextureLoader(textureSourceExtension);

            if (!loader)
            {
                return NauMakeError("No texture loader found for extension {}!", textureSourceExtension);
            }

            const std::filesystem::path subPath = std::filesystem::u8path(outputPath) / std::to_string(folderIndex) / std::string(toString(metaInfo.uid) + ext.data());

            if (!std::filesystem::exists(subPath.parent_path()))
            {
                std::filesystem::create_directories(subPath.parent_path());
            }

            std::string output = pathToUtf8(subPath);

            if (!saveDdsTexture(file, loader, output, textureSourceExtension))
            {
                return NauMakeError("Failed to save texture {}", output);
            }

            return makeAssetMetaInfo(path, metaInfo.uid, std::format("{}/{}{}", folderIndex, toString(metaInfo.uid), ext.data()), textureSourceExtension, "Texture");
        }

        nau::Result<AssetMetaInfo> PngAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
            auto extraInfo = reinterpret_cast<ExtraInfoTexture*>(metaInfo.extraInfo.get());

            // copy all pngs to foloder, where UI can reach it
            if (utils::compilers::copyFileToExportDirectory(extraInfo->path, projectRootPath) != 0)
            {
                return NauMakeError("Failed to copy {} to export directory!", extraInfo->path);
            }

            return compileTextureToDds(stage, extraInfo->path, outputPath, metaInfo, "png", folderIndex, ext());
        }

        nau::Result<AssetMetaInfo> DdsAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
            auto extraInfo = reinterpret_cast<ExtraInfoTexture*>(metaInfo.extraInfo.get());

            if (nau::utils::compilers::copyAsset(extraInfo->path, outputPath, metaInfo, folderIndex, ".dds") != 0)
            {
                return NauMakeError("Failed to copy {} to {}!", extraInfo->path, outputPath);
            }

            return makeAssetMetaInfo(extraInfo->path, metaInfo.uid, std::format("{}/{}{}", folderIndex, toString(metaInfo.uid), ext().data()), "dds", "Texture");
        }

        nau::Result<AssetMetaInfo> JpgAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
			auto extraInfo = reinterpret_cast<ExtraInfoTexture*>(metaInfo.extraInfo.get());

            return compileTextureToDds(stage, extraInfo->path, outputPath, metaInfo, "jpg", folderIndex, ext());
        }
    }  // namespace compilers
}  // namespace nau
