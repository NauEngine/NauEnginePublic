// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "nau/asset_tools/asset_api.h"
#include "nau/assets/asset_meta_info.h"
#include "nau/serialization/runtime_value_builder.h"
#include "nau/string/string_utils.h"
#include "nau/utils/uid.h"
#include "nlohmann/json.hpp"
#include "nau/shared/file_system.h"

namespace nau
{
    constexpr static int MAX_FILES_COUNT = 10000;

    struct ASSET_TOOL_API AssetMetaInfo : public AssetMetaInfoBase
    {
        uint64_t lastModified;
        bool dirty;

        NAU_CLASS_BASE(AssetMetaInfoBase)
        NAU_CLASS_FIELDS(
            CLASS_FIELD(lastModified),
            CLASS_FIELD(dirty))
    };

    inline static AssetMetaInfo makeAssetMetaInfo(const std::string& path, const nau::Uid& uid, const std::string& dbPath, const std::string& type, const std::string& kind, bool sourceAsMeta = false)
    {
        const std::string sorceRelativePath = pathToUtf8(FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(path), true));

        AssetMetaInfo result;
        result.uid = uid;
        result.dbPath = dbPath.c_str();
        result.kind = kind.c_str();
        result.sourceType = type.c_str();
        result.sourcePath = sorceRelativePath.c_str();

        result.nausdPath = sourceAsMeta
            ? (sorceRelativePath + ".nausd").c_str()
            : (sorceRelativePath + "." + type + ".nausd").c_str();
        result.dirty = false;
        result.lastModified = std::filesystem::last_write_time(std::filesystem::u8path(path)).time_since_epoch().count();

        return result;
    }

    static const std::unordered_map<std::string, std::vector<std::string>> typeMap =
    {
            {   "folder",                                                       {"*."}},
            {  "texture",               {"*.png", "*.jpg", "*.jpeg", "*.tga", "*.hdr"}},
            {     "mesh",                        {"*.obj", "*.gltf", "*.glb", "*.fbx"}},
            {   "shader", {"*.vert", "*.frag", "*.comp", "*.geom", "*.tesc", "*.tese"}},
            {"animation",                                          {"*.anim", "*.fbx"}},
            {    "scene",                               {"*.scene", "*.gltf", "*.glb"}},
            { "material",                                           {"*.mat", "*.mtl"}},
            {    "sound",                                 {"*.wav", "*.mp3", "*.flac"}},
            {  "uiScene",                                                    {"*.nui"}},
    };

    enum class AssetType
    {
        AssetTypeUnknown = 0,
        AssetTypeMesh = 1,
        AssetTypeMaterial = 2,
        AssetTypeTexture = 3,
        AssetTypeShader = 4,
        AssetTypeAnimation = 5,
        AssetTypeScene = 6,
        AssetTypeUiScene = 7,
    };

    static const std::unordered_map<std::string, AssetType> assetTypeMap =
        {
            {     "mesh",      AssetType::AssetTypeMesh},
            { "material",  AssetType::AssetTypeMaterial},
            {  "texture",   AssetType::AssetTypeTexture},
            {   "shader",    AssetType::AssetTypeShader},
            {"animation", AssetType::AssetTypeAnimation},
            {    "scene",     AssetType::AssetTypeScene},
            {  "uiScene",   AssetType::AssetTypeUiScene}
    };

    enum class AssetStatus
    {
        AssetStatusUnknown = 0,
        AssetStatusLoaded = 1,
        AssetStatusUnloaded = 2
    };

    class IAsset
    {
    public:
        virtual ~IAsset() = default;
        virtual std::string getExtension() = 0;
        virtual std::string getName() = 0;
        virtual std::string getPath() = 0;
        virtual AssetType getType() = 0;
    };
}  // namespace nau
