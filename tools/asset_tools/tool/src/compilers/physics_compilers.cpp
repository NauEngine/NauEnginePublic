// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/compilers/physics_compilers.h"

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

namespace nau::compilers
{
    // TODO True Physics compiler
    nau::Result<AssetMetaInfo> UsdPhysicsCompiler::compile(
        PXR_NS::UsdStageRefPtr stage,
        const std::string& outputPath,
        const std::string& projectRootPath,
        const nau::UsdMetaInfo& metaInfo,
        int folderIndex)
    {
        AssetDatabaseManager& dbManager = AssetDatabaseManager::instance();
        NAU_ASSERT(dbManager.isLoaded(), "Asset database not loaded!");

        auto stageToCompile = pxr::UsdStage::Open(metaInfo.assetPath);

        if (!stageToCompile)
        {
            return NauMakeError("Can't load source stage from '{}'", metaInfo.assetPath);
        }

        auto rootPrim = stage->GetPrimAtPath(pxr::SdfPath("/Root"));
        if (!rootPrim)
        {
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

        AssetMetaInfo nphysMeta;
        const std::string relativeSourcePath = pathToUtf8(FileSystemExtensions::getRelativeAssetPath(std::filesystem::u8path(metaInfo.assetPath), false));
        const std::string sourcePath = std::format("{}", relativeSourcePath.c_str());

        auto id = dbManager.findIf(sourcePath);

        if (id.isError() && !stringUID.empty())
        {
            id = Uid::parseString(stringUID);
        }

        const std::filesystem::path out = std::filesystem::u8path(outputPath) / std::to_string(folderIndex);
        std::string fileName = toString(*id) + ".nphys";

        nphysMeta.uid = *id;
        nphysMeta.dbPath = (out.filename() / fileName).string().c_str();
        nphysMeta.sourcePath = sourcePath.c_str();
        nphysMeta.dirty = false;
        nphysMeta.kind = "PhysicsMaterial";

        auto outFilePath = utils::compilers::ensureOutputPath(outputPath, nphysMeta, "");

        DataBlock outBlk;
        outBlk.setStr("dummy", "dummyPhys");
        bool bRet = outBlk.saveToTextFile(pathToUtf8(outFilePath).c_str());

        if (bRet)
        {
            dbManager.addOrReplace(nphysMeta);
            return nphysMeta;
        }

        return NauMakeError("Phys asset loading failed");
    }

}  // namespace nau::compilers
