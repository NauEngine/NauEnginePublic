// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/asset_tools/compilers/scene_compilers.h"

#include <nau/assets/asset_container_builder.h>
#include <nau/scene/scene_factory.h>
#include <nau/service/service_provider.h>

#include "nau/asset_tools/asset_utils.h"
#include "nau/io/file_system.h"
#include "nau/io/io_constants.h"
#include "nau/io/memory_stream.h"
#include "nau/io/stream.h"
#include "nau/io/stream_utils.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"
#include "nau/usd_meta_tools/usd_meta_info.h"

namespace nau
{
    namespace compilers
    {
        static IAssetContainerBuilder* findSceneBuilder(const SceneAsset::Ptr& asset)
        {
            for (auto builder : getServiceProvider().getAll<IAssetContainerBuilder>())
            {
                if (builder->isAcceptable(asset))
                {
                    return builder;
                }
            }

            return nullptr;
        }

        using translateScene = void (*)(PXR_NS::UsdStageRefPtr stage, nau::scene::IScene::WeakRef scene);

        translateScene getTranslatorFunction()
        {
            void* plugin = utils::getUsdPlugin("UsdTranslatorWrapper.dll");

            if (!plugin)
            {
                return nullptr;
            }
#ifdef WIN32
            auto proc = GetProcAddress(reinterpret_cast<HMODULE>(plugin), "translateScene");
#else
            auto proc = dlsym(plugin, "translateScene");
#endif
            if (translateScene func = reinterpret_cast<translateScene>(proc))
            {
                return func;
            }

            return nullptr;
        }

        nau::Result<AssetMetaInfo> SceneAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
            auto& sceneFactory = getServiceProvider().get<nau::scene::ISceneFactory>();
            auto scene = sceneFactory.createEmptyScene();
            auto extraData = reinterpret_cast<ExtraInfoScene*>(metaInfo.extraInfo.get());

            auto stageToCompile = pxr::UsdStage::Open(extraData->path);

            NAU_VERIFY(scene, "Failed to create scene");

            translateScene translatorFunction = getTranslatorFunction();

            if (!translatorFunction)
            {
                return NauMakeError("Failed to get translator function from plugin! Plugin does not exist or is not loaded!");
            }

            translatorFunction(stageToCompile, scene.getRef());

            const std::filesystem::path subPath = std::filesystem::u8path(outputPath) / std::to_string(folderIndex) / std::string(toString(metaInfo.uid) + ext().data());

            if (!std::filesystem::exists(subPath.parent_path()))
            {
                std::filesystem::create_directories(subPath.parent_path());
            }

            std::string output = pathToUtf8(subPath);

            io::IStreamWriter::Ptr stream = io::createNativeFileStream(output.c_str(), io::AccessMode::Write, io::OpenFileMode::CreateAlways);

            {
                SceneAsset::Ptr sceneAsset = scene::wrapSceneAsAsset(*scene);
                IAssetContainerBuilder* const assetBuilder = findSceneBuilder(sceneAsset);

                if (!assetBuilder)
                {
                    return NauMakeError("Could not find builder for scene!");
                }

                const Result<> writeResult = assetBuilder->writeAssetToStream(stream, sceneAsset);
            }

            NAU_ASSERT(stream->getPosition() > 0, "Failed to write to stream");

            if (std::filesystem::exists(subPath))
            {
                return makeAssetMetaInfo(extraData->path, metaInfo.uid, std::format("{}/{}{}", folderIndex, toString(metaInfo.uid), ext().data()), "nausd_scene", "scene");
            }

            return NauMakeError("Failed to write scene {} at path {}!", extraData->path.c_str(), output.c_str());
        }
    }  // namespace compilers
}  // namespace nau
