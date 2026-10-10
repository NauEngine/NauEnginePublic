// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_uid_resolver.h"
#include "nau_api.h"
#include "nau/utils/uid.h"
#include "nau/assets/asset_db.h"
#include "nau/service/service_provider.h"
#include "nau/io/virtual_file_system.h"

#include <pxr/usd/ar/defineResolver.h>
#include <pxr/base/tf/stringUtils.h>
#include <filesystem>
#include "nau/shared/file_system.h"



PXR_NAMESPACE_USING_DIRECTIVE

NAUAR_DEFINE_RESOLVER(NauUIDResolver, ArDefaultResolver);

std::string NauUIDResolver::_CreateIdentifier(const std::string& assetPath, const PXR_NS::ArResolvedPath& anchorAssetPath) const
{
    NAU_TOUCH_REGISTRY(NauUIDResolver);
    return assetPath;
}

PXR_NS::ArResolvedPath NauUIDResolver::_Resolve(const std::string& assetPath) const
{
    // TfStringStartsWith(assetPath, "uid:")
    if (auto pos = assetPath.find("uid:"); pos != std::string::npos)
    {
        auto uid_str = assetPath.substr(pos+4);
        auto uid = nau::Uid::parseString(uid_str);
        if (uid)
        {
            auto path = nau::getServiceProvider().get<nau::IAssetDB>().getNausdPathFromUid(*uid);
            auto& vfs = nau::getServiceProvider().get<nau::io::IVirtualFileSystem>();
            auto fs_path = vfs.resolveToNativePath("/content/" + path);

            if (!fs_path.empty())
            {
                namespace fs = std::filesystem;
                return ArResolvedPath(nau::pathToUtf8(fs::path(fs_path)));
            }
            else if(!path.empty())
                return ArResolvedPath(path.c_str());
        }
    }

    return ArDefaultResolver::_Resolve(assetPath);
}

