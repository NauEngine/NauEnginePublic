// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/compilers/usd_compilers.h"

#include <nau/shared/file_system.h>
#include <nau/shared/logger.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usd/sdf/copyUtils.h>

#include "nau/asset_tools/asset_compiler.h"
#include "nau/asset_tools/asset_utils.h"
#include "nau/asset_tools/db_manager.h"
#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "usd_translator/usd_mesh_adapter.h"
#include "usd_translator/usd_mesh_composer.h"
#include "usd_translator/usd_translator.h"
#include <nau/asset_tools/compilers/material_compilers.h>

namespace nau
{
    namespace compilers
    {
        namespace
        {
            bool isDirty(const std::string& path, const std::time_t& lastModified)
            {
                const std::time_t modTime = std::filesystem::last_write_time(std::filesystem::u8path(path)).time_since_epoch().count();
                return lastModified != modTime;
            }

        } // namespace

        nau::Result<AssetMetaInfo> UsdMeshAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
            FileSystem fs;
            AssetDatabaseManager& dbManager = AssetDatabaseManager::instance();

            auto extraInfo = reinterpret_cast<ExtraInfoMesh*>(metaInfo.extraInfo.get());

            NAU_ASSERT(extraInfo, "Invalid extra info!");

            PXR_NS::UsdPrim primToCompile = stage->GetPrimAtPath(PXR_NS::SdfPath(extraInfo->meshSource));

            if (!primToCompile.IsValid())
            {
                return NauMakeError("Prim {} is invalid!", extraInfo->meshSource);
            }

            const auto& typeName = primToCompile.GetTypeName().GetString();

            if (typeName != "Mesh")
            {
                return NauMakeError("Prim {} is not a mesh!", extraInfo->meshSource);
            }

            auto relativePath = pathToUtf8(FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(metaInfo.assetPath), true));
            const auto dbMeta = dbManager.get(metaInfo.uid);
            const std::string sourcePath = std::format("{}+[{}]", relativePath.c_str(), primToCompile.GetName().GetString());

            if (!dbMeta.isError() && dbManager.compiled(metaInfo.uid) && !isDirty(metaInfo.assetPath, dbMeta->lastModified))
            {
                return NauMakeError("Skipping compilation, asset's {} prim {} is not dirty!", metaInfo.assetPath, primToCompile.GetName().GetString());
            }

            const std::filesystem::path basePath = std::filesystem::u8path(outputPath) / std::to_string(folderIndex);
            std::string output = pathToUtf8((basePath / toString(metaInfo.uid))) + ".gltf";

            if (!std::filesystem::exists(basePath))
            {
                std::filesystem::create_directories(basePath);
            }

            const auto exportResult = stage->Export(output, true,
            {
                {"computeTangents", extraInfo->generateTangents ? "true" : "false"},
                {"flipU", extraInfo->flipU ? "true" : "false"},
                {"flipV", extraInfo->flipV ? "true" : "false"}
            });

            if (!exportResult)
            {
                return NauMakeError("Failed to export prim {} to gltf!", primToCompile.GetName().GetString());
            }

            const std::filesystem::path dbPath = std::filesystem::path(std::to_string(folderIndex)) / (toString(metaInfo.uid) + ".gltf");

            auto assetPath = std::filesystem::u8path(metaInfo.assetPath);
            AssetMetaInfo composedMeshMeta;
            composedMeshMeta.uid = metaInfo.uid;
            composedMeshMeta.dbPath = dbPath.string().c_str();
            composedMeshMeta.kind = "Model";
            composedMeshMeta.sourceType = assetPath.extension().string().c_str() + 1;
            composedMeshMeta.sourcePath = sourcePath.c_str();
            composedMeshMeta.nausdPath = ((relativePath + assetPath.extension().string()) + ".nausd").c_str();
            composedMeshMeta.dirty = false;
            composedMeshMeta.lastModified = std::filesystem::last_write_time(std::filesystem::u8path(metaInfo.assetPath)).time_since_epoch().count();

            LOG_INFO("Saved model {}", output);

            return composedMeshMeta;
        }
    }  // namespace compilers
}  // namespace nau
