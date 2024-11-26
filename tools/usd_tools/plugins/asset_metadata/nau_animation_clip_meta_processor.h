// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "nau/usd_meta_tools/usd_meta_generator.h"

class NauAnimationClipMetaProcessor final : public nau::IMetaProcessor
{
    bool process(PXR_NS::UsdPrim prim, nau::UsdMetaInfo& dest) override;
};

class NauSkeletalAnimationClipMetaProcessor final : public nau::IMetaProcessor
{
    bool process(PXR_NS::UsdPrim prim, nau::UsdMetaInfo& dest) override;
};

class NauAnimationClipMetaGenerator final : public nau::IPrimMetaGenerator
{
	PXR_NS::UsdPrim generate(PXR_NS::UsdPrim source, PXR_NS::UsdStagePtr stage, const PXR_NS::SdfPath& dest, const nau::MetaArgs& args) override;
	const nau::MetaArgs& getDefaultArgs() const override;
};

class NauSkeletalAnimationClipMetaGenerator final : public nau::IPrimMetaGenerator
{
	PXR_NS::UsdPrim generate(PXR_NS::UsdPrim source, PXR_NS::UsdStagePtr stage, const PXR_NS::SdfPath& dest, const nau::MetaArgs& args) override;
	const nau::MetaArgs& getDefaultArgs() const override;
};