// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/usd_meta_tools/usd_meta_generator.h"
#include "nau/shared/file_system.h"

#include "nau/shared/logger.h"
#include "usd_proxy/usd_proxy.h"

#include <pxr/usd/usd/attribute.h>

#include <filesystem>

namespace nau
{
    namespace
    {
        static const std::set<std::string> containerExt =
        {
            ".usd",
            ".usda",
            ".usdc",
            ".nausd",
            ".gltf",
            ".glb"
        };
    }

    UsdMetaGenerator& UsdMetaGenerator::instance()
    {
        static UsdMetaGenerator inst;
        return inst;
    }

    bool UsdMetaGenerator::addAssetTemplate(const std::string& templateName, IMetaGeneratorTemplate::Ptr generator)
    {
        if (!generator)
        {
            LOG_ERROR("No generator provided", "");
            return false;
        }

        m_templateRegistry[templateName] = generator;
        return true;
    }

    bool UsdMetaGenerator::generateAssetTemplate(const std::filesystem::path& dest, const std::string& templateName, const MetaArgs& args)
    {
        auto it = m_templateRegistry.find(templateName);
        if (it == m_templateRegistry.end())
        {
            LOG_ERROR("No generator provided for template {}", templateName);
            return false;
        }

        auto stage = PXR_NS::UsdStage::CreateInMemory(pathToUtf8(dest));
        if (!it->second->generate(stage, args))
        {
            return false;
        }
        return stage->GetRootLayer()->Export(pathToUtf8(dest));
    }

    bool UsdMetaGenerator::addFileGenerator(const std::set<std::string>& extentions, IMetaGenerator::Ptr generator)
    {
        if (!generator)
        {
            LOG_ERROR("No generator provided: {}", "unknown");
            return false;
        }
        for (const auto& ext : extentions)
        {
            m_fileRegistry[ext] = generator;
        }
        return true;
    }

    bool UsdMetaGenerator::addPrimGenerator(const PXR_NS::TfToken& primType, IPrimMetaGenerator::Ptr generator)
    {
        if (!generator)
        {
            LOG_ERROR("No generator provided", "");
            return false;
        }

        m_primRegistry[primType] = generator;
        return true;
    }

    PXR_NS::UsdStageRefPtr UsdMetaGenerator::generate(const std::filesystem::path& path, MetaArgs args)
    {
        auto ext = pathToUtf8(path.extension());
        if (containerExt.find(ext) != containerExt.end())
        {
            return processContainer(path, args);
        }

        return processAsset(ext, path, args);
    }

    PXR_NS::UsdPrim UsdMetaGenerator::generate(const std::filesystem::path& sourceAssetPath, PXR_NS::UsdPrim source, PXR_NS::UsdStagePtr stage, const PXR_NS::SdfPath& dest, const MetaArgs& args)
    {
        PXR_NS::UsdPrim target;
        auto it = m_primRegistry.find(source.GetTypeName());
        if (it == m_primRegistry.end())
        {
            target = stage->DefinePrim(dest, "NauAssetGroup"_tftoken);
        }
        else
        {
            target = it->second->generate(source, stage, dest, args);
        }

        if (!target)
            return target;

        if (PXR_NS::UsdAttribute attr = target.CreateAttribute("path"_tftoken, PXR_NS::SdfValueTypeNames->Asset))
        {
            attr.Set(PXR_NS::SdfAssetPath(pathToUtf8(sourceAssetPath.filename())));
        }

        return target;
    }

    bool UsdMetaGenerator::write(const std::filesystem::path& path, PXR_NS::UsdStageRefPtr stage) const
    {
        return stage->GetRootLayer()->Export(pathToUtf8(path) + ".nausd");
    }

    std::map<PXR_NS::TfToken, nau::MetaArgs> UsdMetaGenerator::getArgs(const std::filesystem::path& path) const
    {
        auto ext = pathToUtf8(path.extension());
        std::map<PXR_NS::TfToken, nau::MetaArgs> out;

        if (containerExt.find(ext) != containerExt.end())
        {
            auto stage = PXR_NS::UsdStage::CreateInMemory(pathToUtf8(path));
            auto assetPrim = stage->DefinePrim(PXR_NS::SdfPath("/Asset"));
            if (!assetPrim.SetPayload(pathToUtf8(path), PXR_NS::SdfPath("")))
            {
                return std::move(out);
            }
            assetPrim.Load();
            if (!assetPrim.IsValid())
            {
                return std::move(out);
            }

            std::function<void(PXR_NS::UsdPrim)> traverse = [&](PXR_NS::UsdPrim src)
            {
                auto it = m_primRegistry.find(src.GetTypeName());
                if (it != m_primRegistry.end())
                {
                    if (out.find(src.GetTypeName()) == out.end())
                    {
                        out[src.GetTypeName()] = it->second->getDefaultArgs();
                    }
                }

                for (auto child : src.GetChildren())
                {
                    traverse(child);
                }
            };

            for (auto child : assetPrim.GetChildren())
            {
                traverse(child);
            }
        }
        else
        {
            auto it = m_fileRegistry.find(ext);
            if (it == m_fileRegistry.end())
            {
                return std::move(out);
            }

            return 
            {
                {PXR_NS::TfToken(ext), it->second->getDefaultArgs()}
            };
        }

        return std::move(out);
    }

    PXR_NS::UsdStageRefPtr UsdMetaGenerator::processAsset(const std::string& ext, const std::filesystem::path& path, const MetaArgs& args)
    {
        auto it = m_fileRegistry.find(ext);
        if (it == m_fileRegistry.end())
        {
            return nullptr;
        }

        auto stage = PXR_NS::UsdStage::CreateInMemory(pathToUtf8(path));

        if (!it->second->generate(path, stage, args))
        {
            return nullptr;
        }

        return stage;
    }

    PXR_NS::UsdStageRefPtr UsdMetaGenerator::processContainer(const std::filesystem::path& path, const MetaArgs& args)
    {
        auto srcStage = PXR_NS::UsdStage::Open(pathToUtf8(path));
        if (!srcStage)
        {
            return nullptr;
        }

        auto stage = PXR_NS::UsdStage::CreateInMemory(pathToUtf8(path) + ".nausd");
        auto rootPath = PXR_NS::SdfPath("/Asset");
        auto assetPrim = stage->DefinePrim(rootPath);

        for (auto child : srcStage->GetPseudoRoot().GetAllChildren())
        {
            auto destPrim = stage->DefinePrim(rootPath.AppendChild(child.GetName()));
            if (!destPrim.SetPayload(pathToUtf8(path), child.GetPath()))
            {
                return nullptr;
            }
            destPrim.Load();
            if (!destPrim.IsValid())
            {
                return nullptr;
            }
        }

        auto metaRoot = stage->DefinePrim(PXR_NS::SdfPath("/Root"), "NauAssetGroup"_tftoken);
        if (PXR_NS::UsdAttribute attr = metaRoot.CreateAttribute("path"_tftoken, PXR_NS::SdfValueTypeNames->Asset))
        {
            attr.Set(PXR_NS::SdfAssetPath(pathToUtf8(path.filename())));
        }

        std::function<void(PXR_NS::UsdPrim, PXR_NS::UsdPrim)> traverse = [&](PXR_NS::UsdPrim src, PXR_NS::UsdPrim dest)
            {
                for (auto child : src.GetChildren())
                {
                    auto meta = generate(path, child, stage, dest.GetPath().AppendChild(child.GetName()), args);
                    traverse(child, meta);
                }
            };
        traverse(assetPrim, metaRoot.GetPrim());

        for (auto child : srcStage->GetPseudoRoot().GetAllChildren())
        {
            auto destPrim = stage->GetPrimAtPath(rootPath.AppendChild(child.GetName()));
            if (!destPrim.SetPayload(pathToUtf8(path.filename()), child.GetPath()))
            {
                return nullptr;
            }
        }
        return stage;
    }

    bool UsdMetaGenerator::canGenerate(const std::filesystem::path& path) const
    {
        return containerExt.contains(pathToUtf8(path.extension())) || m_fileRegistry.contains(pathToUtf8(path.extension()));
    }
}  // namespace nau