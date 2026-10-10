// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/asset_tools/compilers/material_compilers.h"

#include <pxr/usd/usd/attribute.h>
#include <pxr/base/gf/matrix2d.h>
#include <pxr/base/gf/matrix2f.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/gf/vec4i.h>

#include <fstream>

#include "nau/asset_tools/asset_utils.h"
#include "nau/assets/material.h"
#include "nau/math/math.h"
#include "nau/serialization/json_utils.h"
#include "nau/service/service_provider.h"
#include "nau/io/virtual_file_system.h"

namespace nau
{
    namespace compilers
    {
        std::string_view UsdMaterialAssetCompiler::ext() const
        {
            return ".nmat_json";
        }

        bool UsdMaterialAssetCompiler::canCompile(const std::string& path) const
        {
            return true;
        }

        nau::Result<AssetMetaInfo> UsdMaterialAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& info, int folderIndex)
        {
            if (info.type != "material")
                return NauMakeError("Expected material, got something else {}", info.type.c_str());

            Material material;
            material.name = info.name.c_str();
            auto extra = info.getExtraInfoAs<ExtraInfoMaterial>();
            for (auto& config : extra->configs)
            {
                auto& dest = material.pipelines[config.first.c_str()];

                if (config.second.isScissorsEnabled)
                {
                    dest.isScissorsEnabled = *config.second.isScissorsEnabled;
                }


                if (config.second.blendMode)
                {
                    if(auto val = nau::EnumTraits<nau::BlendMode>::parse(config.second.blendMode->c_str()))
                        dest.blendMode = *val;
                }

                if (config.second.cullMode)
                {
                    if (auto val = nau::EnumTraits<nau::CullMode>::parse(config.second.cullMode->c_str()))
                        dest.cullMode = *val;
                }

                if (config.second.depthMode)
                {
                    if (auto val = nau::EnumTraits<nau::DepthMode>::parse(config.second.depthMode->c_str()))
                        dest.depthMode = *val;
                }

                if (config.second.stencilCmpFunc)
                {
                    if (auto val = nau::EnumTraits<nau::ComparisonFunc>::parse(config.second.stencilCmpFunc->c_str()))
                        dest.stencilCmpFunc = *val;
                }

                for (auto& shader : config.second.shaders)
                {
                    dest.shaders.push_back(resolveShaderPath(shader).c_str());
                }

                for (auto& prop : config.second.properties)
                {
                    std::string key = prop.first.GetText();
                    if (auto val = resolveValue(prop.second))
                    {
                        auto namespaceEnd = key.find_last_of(':', std::string::npos);
                        if (namespaceEnd != std::string::npos)
                        {
                            key = key.substr(namespaceEnd + 1);
                        }
                        if (auto it = dest.properties.find(key.c_str()); it != dest.properties.end())
                        {
                            if(!it->second->is<RuntimeStringValue>())
                            {
                                dest.properties[key.c_str()] = val;
                            }
                        }
                        else
                            dest.properties[key.c_str()] = val;
                    }
                }
            }

            auto data = serialization::JsonUtils::stringify(material);
            namespace fs = std::filesystem;

            const std::filesystem::path basePath = std::filesystem::u8path(outputPath) / std::to_string(folderIndex);
            const auto out = basePath / (toString(info.uid) + ".nmat_json");

            if (!fs::exists(basePath))
            {
                fs::create_directories(basePath);
            }

            std::ofstream output;
            output.open(out);
            output << std::string(data.begin(), data.end());
            output.close();

            if (fs::exists(out))
            {
                return makeAssetMetaInfo(info.assetPath, info.uid, std::format("{}/{}{}", folderIndex, toString(info.uid), ext().data()), "nausd", "Material", true);
            }

            return NauMakeError("Failed to save asset {} at path {}", info.name.c_str(), pathToUtf8(out));
        }

        RuntimeValue::Ptr UsdMaterialAssetCompiler::resolveValue(PXR_NS::VtValue value)
        {
            using namespace Vectormath;
            using namespace Vectormath::SSE;
            using namespace PXR_NS;

            static const std::map<std::type_index, std::function<RuntimeValue::Ptr(VtValue)>> vtToRv =
            {
                { typeid(std::string),     [&value](VtValue value) {return makeValueCopy(value.Get<std::string>());} },
                { typeid(SdfAssetPath),     [&value](VtValue value) -> RuntimeValue::Ptr
                    {
                        auto assetPath = value.Get<SdfAssetPath>();
                        auto path = assetPath.GetAssetPath();
                        auto stage = UsdStage::Open(assetPath.GetResolvedPath());
                        if (!stage)
                        {
                            return nullptr;
                        }

                        auto prim = stage->GetPrimAtPath(SdfPath("/Root"));
                        if (!prim || prim.GetTypeName() != "NauAssetTexture")
                        {
                            return nullptr;
                        }
                        
                        std::string textureUid;
                        if (!prim.GetAttribute(TfToken("uid")).Get(&textureUid))
                        {
                            return nullptr;
                        }

                        path = "uid:" + textureUid;
                        return makeValueCopy(path);
                    }
                },

                { typeid(float),    [&value](VtValue value) {return makeValueCopy(value.Get<float>());   } },
                { typeid(double),   [&value](VtValue value) {return makeValueCopy(value.Get<double>());  } },
                { typeid(int),      [&value](VtValue value) {return makeValueCopy(value.Get<int>());     } },
                { typeid(bool),     [&value](VtValue value) {return makeValueCopy(value.Get<bool>());    } },
                
                { typeid(GfVec2f),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec2f>();
                    return makeValueCopy(Vector2(val[0], val[1]));
                } },
                { typeid(GfVec2d),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec2d>();
                    return makeValueCopy(Vector2(val[0], val[1])); // todo Vector2d 
                } },
                { typeid(GfVec2i),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec2i>();
                    return makeValueCopy(Vector2(val[0], val[1])); // todo makeValueCopy for IVector2
                } },

                { typeid(GfVec3f),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec3f>();
                    return makeValueCopy(Vector3(val[0], val[1], val[2]));
                } },
                { typeid(GfVec3d),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec3d>();
                    return makeValueCopy(Vector3d(val[0], val[1], val[2]));
                } },
                { typeid(GfVec3i),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec3i>();
                    return makeValueCopy(IVector3(val[0], val[1], val[2]));
                } },
                
                { typeid(GfVec4f),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec4f>();
                    return makeValueCopy(Vector4(val[0], val[1], val[2], val[3]));
                } },
                { typeid(GfVec4d),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec4d>();
                    return makeValueCopy(Vector4d(val[0], val[1], val[2], val[3]));
                } },
                { typeid(GfVec4i),  [&value](VtValue value) {
                    auto& val = value.Get<GfVec4i>();
                    return makeValueCopy(IVector4(val[0], val[1], val[2], val[3]));
                } },

                // todo makeValueCopy for Matrix2
                { typeid(GfMatrix3f), [&value](VtValue value) {
                    auto& val = value.Get<GfMatrix3f>();
                    return makeValueCopy(Matrix3(    // todo makeValueCopy for Matrix3d
                        { val[0][0], val[1][0], val[2][0] },
                        { val[0][1], val[1][1], val[2][1] },
                        { val[0][2], val[1][2], val[2][2] }
                        ));
                } },
                { typeid(GfMatrix3d), [&value](VtValue value) {
                    auto& val = value.Get<GfMatrix3d>();
                    return makeValueCopy(Matrix3(    // todo makeValueCopy for Matrix3d
                        { (float)val[0][0], (float)val[1][0], (float)val[2][0] },
                        { (float)val[0][1], (float)val[1][1], (float)val[2][1] },
                        { (float)val[0][2], (float)val[1][2], (float)val[2][2] }
                        ));
                } },

                { typeid(GfMatrix4d), [&value](VtValue value) {
                    auto& val = value.Get<GfMatrix4d>();
                    return makeValueCopy(Matrix4(    // todo makeValueCopy for Matrix4d
                        { (float)val[0][0], (float)val[1][0], (float)val[2][0], (float)val[3][0] },
                        { (float)val[0][1], (float)val[1][1], (float)val[2][1], (float)val[3][1] },
                        { (float)val[0][2], (float)val[1][2], (float)val[2][2], (float)val[3][2] },
                        { (float)val[0][3], (float)val[1][3], (float)val[2][3], (float)val[3][3] }
                        ));
                } },
                { typeid(GfMatrix4f), [&value](VtValue value) {
                    auto& val = value.Get<GfMatrix4f>();
                    return makeValueCopy(Matrix4(    // todo makeValueCopy for Matrix4d
                        { val[0][0], val[1][0], val[2][0], val[3][0] },
                        { val[0][1], val[1][1], val[2][1], val[3][1] },
                        { val[0][2], val[1][2], val[2][2], val[3][2] },
                        { val[0][3], val[1][3], val[2][3], val[3][3] }
                        ));
                } },
            };

            auto converter = vtToRv.find(value.GetTypeid());
            if (converter == vtToRv.end())
                return nullptr;
            return converter->second(value);
        }

        std::string UsdMaterialAssetCompiler::resolveShaderPath(const std::string& path)
        {
            return path;
        }

    }  // namespace compilers
}  // namespace nau
