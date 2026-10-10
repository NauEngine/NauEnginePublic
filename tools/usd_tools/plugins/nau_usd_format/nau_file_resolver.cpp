// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_file_resolver.h"
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

NAUAR_DEFINE_RESOLVER(NauFileResolver, ArDefaultResolver);

std::string NauFileResolver::_CreateIdentifier(const std::string& assetPath, const PXR_NS::ArResolvedPath& anchorAssetPath) const
{
    NAU_TOUCH_REGISTRY(NauFileResolver);

    return assetPath;
}

PXR_NS::ArResolvedPath NauFileResolver::_Resolve(const std::string& assetPath) const
{
    if (TfStringStartsWith(assetPath, "file:"))
    {
        auto& vfs = nau::getServiceProvider().get<nau::io::IVirtualFileSystem>();

        auto str = assetPath.substr(5);
        auto fs_path = vfs.resolveToNativePath(str + ".nausd");

        if (!fs_path.empty())
        {
            namespace fs = std::filesystem;
            return ArResolvedPath(nau::pathToUtf8(fs::path(fs_path)));
        }
    }

    return ArDefaultResolver::_Resolve(assetPath);
}

