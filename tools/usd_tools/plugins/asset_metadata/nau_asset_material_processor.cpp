// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau_asset_material_processor.h"
#include "nau/shared/file_system.h"

#include <type_traits>

#include "nau/NauAssetMetadata/nauAssetMaterial.h"
#include "nau/NauAssetMetadata/nauAssetShader.h"
#include "nau/NauAssetMetadata/nauMaterialPipeline.h"
#include "nau/NauAssetMetadata/nauShaderConfig.h"
#include "nau/NauAssetMetadata/nauShaderInputItem.h"
#include "nau/NauAssetMetadata/nauShaderInputLayout.h"
#include "pxr/usd/usdGeom/gprim.h"
#include "usd_proxy/usd_proxy.h"

const nau::MetaArgs& NauAssetShaderGenerator::getDefaultArgs() const
{
    static const nau::MetaArgs args;
    return args;
}

bool NauAssetShaderGenerator::generate(const std::filesystem::path& path, PXR_NS::UsdStagePtr stage, const nau::MetaArgs& args)
{
    using namespace PXR_NS;

    auto meta = UsdNauAssetShader::Define(stage, SdfPath("/Root"));

    meta.CreatePathAttr().Set(SdfAssetPath(nau::pathToUtf8(path.filename())));

    auto shInputLayout = UsdNauShaderInputLayout::Define(stage, SdfPath("/Root/DefaultInputLayout"));
    shInputLayout.CreateStreamAttr().Set("VSD_STREAM_PER_VERTEX_DATA");

    auto shInputItem = UsdNauShaderInputItem::Define(stage, SdfPath("/Root/DefaultInputLayout/POSITION"));
    shInputItem.CreateTypeAttr().Set("VSDT_FLOAT3"_tftoken);
    shInputItem.CreateBufferIndexAttr().Set(0);

    auto shConfig = UsdNauShaderConfig::Define(stage, SdfPath("/Root/Main"));
    shConfig.CreateEntryPointAttr().Set("VSMain");
    shConfig.CreateTargetAttr().Set("vs");

    return true;
}

bool NauAssetShaderProcessor::process(PXR_NS::UsdPrim prim, nau::UsdMetaInfo& dest)
{
    using namespace PXR_NS;
    using namespace nau;

    UsdNauAssetShader asset(prim);
    if (!asset)
    {
        return false;
    }
    dest.type = "shader";
    auto info = std::make_shared<ExtraInfoShader>();
    dest.extraInfo = info;

    SdfAssetPath path;
    asset.GetPathAttr().Get(&path);
    info->path = path.GetResolvedPath();

    for (auto child : prim.GetAllChildren())
    {
        if (auto inputLayout = UsdNauShaderInputLayout(child))
        {
            auto& layout = info->layouts[child.GetName().GetString()];
            inputLayout.GetStreamAttr().Get(&layout.stream);

            for (auto item : child.GetAllChildren())
            {
                if (auto inputItem = UsdNauShaderInputItem(item))
                {
                    auto& itemDest = layout.items[item.GetName().GetString()];
                    inputItem.GetTypeAttr().Get(&itemDest.type);
                    inputItem.GetBufferIndexAttr().Get(&itemDest.bufferIndex);
                }
            }
        }
        else if (auto config = UsdNauShaderConfig(child))
        {
            auto& dest = info->configs[child.GetName()];

            config.GetEntryPointAttr().Get(&dest.entryPoint);
            config.GetTargetAttr().Get(&dest.target);
            VtArray<std::string> defines;
            config.GetDefinesAttr().Get(&defines);
            dest.defines = std::vector<std::string>(defines.begin(), defines.end());
            SdfPathVector targets;
            config.GetInputLayoutRel().GetTargets(&targets);
            if (!targets.empty())
            {
                dest.inputLayout = targets[0].GetName();
            }
        }
    }

    return true;
}

const nau::MetaArgs& NauAssetMaterialGenerator::getDefaultArgs() const
{
    using namespace PXR_NS;
    using namespace nau;
    static const MetaArgs args =
        {
            {"MaterialName"_tftoken, VtValue("Material")},
            {"Pipelines"_tftoken, VtValue(std::map<std::string, MetaArgs>())}
    };
    return args;
}

bool NauAssetMaterialGenerator::generate(PXR_NS::UsdStagePtr stage, const nau::MetaArgs& args)
{
    using namespace PXR_NS;
    using namespace nau;

    auto it = args.find("MaterialName"_tftoken);
    std::string materialName = it != args.end() ? it->second.Get<std::string>() : "Material";

    auto meta = UsdNauAssetMaterial::Define(stage, SdfPath("/" + materialName));
    if (!meta)
    {
        return false;
    }
    meta.CreateUidAttr(VtValue(nau::toString(Uid::generate())));
    auto pipls = args.find("Pipelines"_tftoken);
    if (pipls == args.end())
    {
        auto targetPipeline = UsdNauMaterialPipeline::Define(stage, SdfPath("/" + materialName + "/Default"));
        targetPipeline.CreateShadersAttr();
        return true;
    }

    std::map<std::string, MetaArgs> pipelines = pipls->second.Get<std::map<std::string, MetaArgs>>();
    for (auto& it : pipelines)
    {
        auto targetPipeline = UsdNauMaterialPipeline::Define(stage, SdfPath("/" + materialName + "/" + it.first));
        auto prim = targetPipeline.GetPrim();
        for (auto& attr : it.second)
        {
            prim.CreateAttribute(attr.first, SdfSchema::GetInstance().FindType(attr.second)).Set(attr.second);
        }
    }

    return true;
}

bool NauAssetMaterialProcessor::process(PXR_NS::UsdPrim prim, nau::UsdMetaInfo& dest)
{
    using namespace PXR_NS;
    using namespace nau;

    static const std::set<std::string> ignoreAttrs =
    {
        "cullMode",
        "blendMode",
        "depthMode",
        "shaders",
        "stencilCmpFunc",
        "isScissorsEnabled"
    };

    UsdNauAssetMaterial asset(prim);
    if (!asset)
    {
        return false;
    }

    dest.type = "material";
    auto info = std::make_shared<nau::ExtraInfoMaterial>();
    dest.extraInfo = info;

    for (auto ch : prim.GetAllChildren())
    {
        auto pipeline = UsdNauMaterialPipeline(ch);
        if (!pipeline)
        {
            continue;
        }

        auto& configDest = info->configs[ch.GetName().GetString()];

        pipeline.GetShadersAttr().Get(&configDest.shaders);

        PXR_NS::TfToken mode;
        auto modeAttr = pipeline.GetCullModeAttr();
        if (modeAttr && modeAttr.IsAuthored())
        {
            modeAttr.Get(&mode);
            configDest.cullMode = mode.GetString();
        }
        modeAttr = pipeline.CreateBlendModeAttr();
        if (modeAttr && modeAttr.IsAuthored())
        {
            modeAttr.Get(&mode);
            configDest.blendMode= mode.GetString();
        }
        modeAttr = pipeline.GetDepthModeAttr();
        if (modeAttr && modeAttr.IsAuthored())
        {
            modeAttr.Get(&mode);
            configDest.depthMode = mode.GetString();
        }    

        modeAttr = pipeline.GetStencilCmpFuncAttr();
        if (modeAttr && modeAttr.IsAuthored())
        {
            modeAttr.Get(&mode);
            configDest.stencilCmpFunc = mode.GetString();
        }

        modeAttr = pipeline.GetIsScissorsEnabledAttr();
        if (modeAttr && modeAttr.IsAuthored())
        {
            bool mode;
            modeAttr.Get(&mode);
            configDest.isScissorsEnabled = mode;
        }

        for (auto attr : ch.GetAttributes())
        {
            if (ignoreAttrs.contains(attr.GetName()))
            {
                continue;
            }


            attr.Get(&configDest.properties[attr.GetName()]);
        }
    }

    auto commonPrim = prim.GetChild("Common"_tftoken);

    if (commonPrim)
    {
        for (auto attr : commonPrim.GetAttributes())
        {
            for (auto& it : info->configs)
            {
                attr.Get(&it.second.properties[attr.GetName()]);
            }
        }
    }

    return true;
}
