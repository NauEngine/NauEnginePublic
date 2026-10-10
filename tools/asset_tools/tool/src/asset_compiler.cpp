// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/asset_tools/asset_compiler.h"

#include <nau/shared/logger.h>

#include <fstream>
#include <sstream>

#include "nau/asset_tools/asset_info.h"
#include "nau/asset_tools/asset_utils.h"
#include "nau/asset_tools/compilers/animation_compilers.h"
#include "nau/asset_tools/compilers/font_compilers.h"
#include "nau/asset_tools/compilers/material_compilers.h"
#include "nau/asset_tools/compilers/scene_compilers.h"
#include "nau/asset_tools/compilers/shader_compilers.h"
#include "nau/asset_tools/compilers/texture_compilers.h"
#include "nau/asset_tools/compilers/sound_compilers.h"
#include "nau/asset_tools/compilers/usd_compilers.h"
#include "nau/asset_tools/compilers/ui_compilers.h"
#include "nau/asset_tools/interface/asset_compiler.h"
#include "nau/shared/file_system.h"

namespace nau
{
    NAU_DEFINE_COMPILERS
    NAU_COMPILER("*", compilers::CopyAssetCompiler),
    NAU_COMPILER("png", compilers::PngAssetCompiler),
    NAU_COMPILER("dds", compilers::DdsAssetCompiler),
    NAU_COMPILER("jpg", compilers::JpgAssetCompiler),
    NAU_COMPILER("nausd_scene", compilers::SceneAssetCompiler),
    NAU_COMPILER("hlsl", compilers::HlslAssetCompiler),
    NAU_COMPILER("nausd_mesh", compilers::UsdMeshAssetCompiler),
    NAU_COMPILER("fnt", compilers::FontAssetCompiler),
    NAU_COMPILER("mp3", compilers::Mp3AssetCompiler),
    NAU_COMPILER("wav", compilers::WavAssetCompiler),
    NAU_COMPILER("flac", compilers::FlacAssetCompiler),
    NAU_COMPILER("ui", compilers::UiCompiler),
    NAU_CLOSE_COMPILERS

    namespace compilers
    {
        nau::Result<AssetMetaInfo> CopyAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const UsdMetaInfo& metaInfo, int folderIndex)
        {
            return NauMakeError("Not implemented");
        }
    }  // namespace compilers

    bool isExtensionSupported(const std::string& ext)
    {
        return g_compilers.find(ext) != g_compilers.end();
    }

    nau::Result<AssetMetaInfo> callCompiler(const std::string& sourceFilePath, PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
    {
        FileSystem fs;

        if (!fs.exist(std::filesystem::u8path(sourceFilePath)))
        {
            return NauMakeError("File {} not found!", sourceFilePath);
        }

        auto ext = pathToUtf8(std::filesystem::u8path(sourceFilePath).extension());

        std::shared_ptr<compilers::IAssetCompiler> compiler = getAssetCompiler(ext.substr(1));

        if (compiler)
        {
            if (!compiler->canCompile(sourceFilePath))
            {
                return NauMakeError("Cannot compile asset {}", sourceFilePath);
            }

            return compiler->compile(stage, outputPath, projectRootPath, metaInfo, folderIndex);
        }
        else
        {
            return NauMakeError("No compiler found for extension {}", ext);
        }
    }

    nau::Result<AssetMetaInfo> callCompilerWithoutSource(const std::string& compilerName, PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
    {
        auto compiler = getAssetCompiler(compilerName);
        if (!compiler)
        {
            LOG_ERROR("There is no compiler named {}", compilerName);
            return NauMakeError("Cannot compile asset");
        }

        return compiler->compile(stage, outputPath, projectRootPath, metaInfo, folderIndex);
    }

    std::string getTargetExtension(const std::string& ext)
    {
        NAU_ASSERT(isExtensionSupported(ext), "Extension {} is not supported!", ext);
        return isExtensionSupported(ext) ? std::string(g_compilers[ext]->ext()) : "";
    }

    std::shared_ptr<compilers::IAssetCompiler> getAssetCompiler(const std::string& ext)
    {
        auto it = g_compilers.find(ext);

        if (it != g_compilers.end())
        {
            return it->second;
        }

        return nullptr;
    }
};  // namespace nau
