// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/compilers/vfx_compilers.h"

#include <EASTL/vector.h>
#include <nau/shared/logger.h>
#include <pxr/usd/sdf/fileFormat.h>

#include "nau/asset_tools/asset_utils.h"
#include "nau/asset_tools/db_manager.h"

#include "nau/assets/animation_asset_accessor.h"
#include "nau/assets/ui_asset_accessor.h"

#include "nau/dataBlock/dag_dataBlock.h"

#include "nau/io/stream_utils.h"

#include "nau/usd_meta_tools/usd_meta_info.h"

#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "usd_proxy/usd_prim_proxy.h"

#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/types.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/sdf/assetPath.h"

#include "usd_proxy/usd_prim_proxy.h"

namespace nau::compilers
{
    // TODO True VFX compiler
    nau::Result<AssetMetaInfo> UsdVFXCompiler::compile(
        PXR_NS::UsdStageRefPtr stage,
        const std::string& outputPath,
        const std::string& projectRootPath,
        const nau::UsdMetaInfo& metaInfo,
        int folderIndex)
    {
        AssetDatabaseManager& dbManager = AssetDatabaseManager::instance();
        NAU_ASSERT(dbManager.isLoaded(), "Asset database not loaded!");

        auto rootPrim = stage->GetPrimAtPath(pxr::SdfPath("/Root/VFX"));
        if (!rootPrim) {
            return NauMakeError("Can't load source stage from '{}'", metaInfo.assetPath);
        }

        auto proxyPrim = UsdProxy::UsdProxyPrim(rootPrim);

        std::string stringUID;
        auto uidProperty = proxyPrim.getProperty(pxr::TfToken("uid"));
        if (uidProperty)
        {
            pxr::VtValue val;
            uidProperty->getValue(&val);

            if (val.IsHolding<std::string>())
            {
                stringUID = val.Get<std::string>();
            }
        }

        AssetMetaInfo nvfxMeta;
        const std::string relativeSourcePath = pathToUtf8(FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(metaInfo.assetPath), false));
        const std::string sourcePath = std::format("{}", relativeSourcePath.c_str());

        auto id = dbManager.findIf(sourcePath);

        if (id.isError() && !stringUID.empty())
        {
            id = Uid::parseString(stringUID);
        }

        const std::filesystem::path out = std::filesystem::u8path(outputPath) / std::to_string(folderIndex);
        std::string fileName = toString(*id) + ".nvfx";

        nvfxMeta.uid = *id;
        nvfxMeta.dbPath = (out.filename() / fileName).string().c_str();
        nvfxMeta.sourcePath = sourcePath.c_str();
        nvfxMeta.nausdPath = (sourcePath + ".nausd").c_str();
        nvfxMeta.dirty = false;
        nvfxMeta.kind = "VFX";

        auto last_write_time = std::filesystem::last_write_time(std::filesystem::u8path(metaInfo.assetPath));
        
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            last_write_time - decltype(last_write_time)::clock::now() + std::chrono::system_clock::now());
        std::time_t cftime = std::chrono::system_clock::to_time_t(sctp);

        nvfxMeta.lastModified = cftime;

        auto outFilePath = utils::compilers::ensureOutputPath(outputPath, nvfxMeta, "");

        auto sourceStage = pxr::UsdStage::Open(metaInfo.assetPath);
        pxr::UsdPrim vfxPrim = sourceStage->GetPrimAtPath(pxr::SdfPath("/VFX"));
        UsdProxy::UsdProxyPrim vfxProxyPrim = UsdProxy::UsdProxyPrim(vfxPrim);

        auto mapPrimToBLK = [&dbManager](const UsdProxy::UsdProxyPrim& prim, nau::DataBlock* blk)
        {
            blk->addStr("primName", prim.getName().GetText());
            blk->addStr("primType", prim.getType().GetText());
            blk->addPoint3("position", nau::math::Vector3::zero());

            for (const auto& prop : prim.getProperties())
            {
                std::string propertyName = prop.second->getName();
                const auto& typeName = prop.second->getTypeName();

                if (typeName == pxr::SdfValueTypeNames->Double)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                        blk->addReal(propertyName.c_str(), value.Get<double>());
                }
                else if (typeName == pxr::SdfValueTypeNames->Bool)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                        blk->addBool(propertyName.c_str(), value.Get<bool>());
                }
                else if (typeName == pxr::SdfValueTypeNames->Int)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                        blk->addInt(propertyName.c_str(), value.Get<int>());
                }
                else if (typeName == pxr::SdfValueTypeNames->Double3)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                    {
                        pxr::GfVec3d vec3 = value.Get<pxr::GfVec3d>();
                        blk->addPoint3(propertyName.c_str(), nau::math::Vector3(vec3[0], vec3[1], vec3[2]));
                    }
                }
                else if (typeName == pxr::SdfValueTypeNames->Color4d)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                    {
                        pxr::GfVec4d col4 = value.Get<pxr::GfVec4d>();
                        blk->addE3dcolor(propertyName.c_str(), nau::math::E3DCOLOR(col4[0], col4[1], col4[2], col4[3]));
                    }
                }
                else if (typeName == pxr::SdfValueTypeNames->Double2)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                    {
                        pxr::GfVec2d vec2 = value.Get<pxr::GfVec2d>();
                        blk->addPoint2(propertyName.c_str(), nau::math::Vector2(vec2[0], vec2[1]));
                    }
                }
                else if (typeName == pxr::SdfValueTypeNames->Int2)
                {
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                    {
                        pxr::GfVec2i vec2i = value.Get<pxr::GfVec2i>();
                        blk->addIPoint2(propertyName.c_str(), nau::math::IVector2(vec2i[0], vec2i[1]));
                    }
                }
                else if (typeName == pxr::SdfValueTypeNames->Asset)
                {
                    std::string pathToTexture = "file:/content/textures/default.jpg";
                    pxr::VtValue value;
                    if (prop.second->getValue(&value))
                    {
                        pxr::SdfAssetPath texturePath = value.Get<pxr::SdfAssetPath>();
                        auto textureUID = texturePath.GetAssetPath();
                        if (textureUID.starts_with("uid:") && textureUID.size() >= 4)
                        {
                            textureUID.erase(0, 4);
                            auto textureInfo = dbManager.get(*Uid::parseString(textureUID));
                            pathToTexture = ("file:/content/" + textureInfo->sourcePath + "." + textureInfo->sourceType).c_str();
                        }
                    }
                    blk->addStr("texture", pathToTexture.c_str());
                }
                else
                {
                    NAU_LOG_ERROR("Unsupported attribute type. VFX asset compiler");
                }
            }
        };

        DataBlock outBlk;
        mapPrimToBLK(vfxProxyPrim, &outBlk);
        bool bRet = outBlk.saveToTextFile(pathToUtf8(outFilePath).c_str());

        if (bRet)
        {
            dbManager.addOrReplace(nvfxMeta);
            return nvfxMeta;
        }

        return NauMakeError("VFX asset loading failed");
    }

}  // namespace nau::compilers
