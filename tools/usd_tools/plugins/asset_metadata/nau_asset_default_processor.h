// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "nau/usd_meta_tools/usd_meta_generator.h"
#include "nau/shared/file_system.h"

class NauAssetDefaultProcessor final : public nau::IMetaProcessor
{
	bool process(PXR_NS::UsdPrim prim, nau::UsdMetaInfo& dest) override;
};

template<class TMetaType>
class NauAssetDefaultGenerator final : public nau::IMetaGenerator
{
	bool generate(const std::filesystem::path& path, PXR_NS::UsdStagePtr stage, const nau::MetaArgs& args) override
	{
        auto meta = TMetaType::Define(stage, PXR_NS::SdfPath("/Root"));
        meta.CreatePathAttr().Set(PXR_NS::SdfAssetPath(nau::pathToUtf8(path.filename())));
		meta.CreateUidAttr().Set(""); // TODO
		return true;
	}

	const nau::MetaArgs& getDefaultArgs() const override
	{
		static const nau::MetaArgs defaultArgs;
		return defaultArgs;
	}

};

template<class TMetaType>
class NauAssetDefaultPrimGenerator final : public nau::IPrimMetaGenerator
{
	PXR_NS::UsdPrim generate(PXR_NS::UsdPrim source, PXR_NS::UsdStagePtr stage, const PXR_NS::SdfPath& dest, const nau::MetaArgs& args) override
	{
		auto meta = TMetaType::Define(stage, dest);
		return meta.GetPrim();
	}

	const nau::MetaArgs& getDefaultArgs() const override
	{
		static const nau::MetaArgs defaultArgs;
		return defaultArgs;
	}
};