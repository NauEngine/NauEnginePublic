// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/usd_meta_tools/usd_meta_manager.h"

#include "usd_proxy/usd_proxy.h"

#include <pxr/usd/usd/attribute.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/plug/plugin.h>

#include <functional>

#ifdef WIN32
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif
#include <iostream>
#include "nau/shared/logger.h"
#include <filesystem>

namespace nau
{
    namespace
    {
        const std::set<std::string> g_requiredPlugins =
        {
            "usdGltf_plugin",
            "NauUsdFormat",
            "NauVFXLib",
            "NauComponentLib",
            "NauInputLib",
            "NauAudioLib",
            "NauAnimationAssetLib",
            "NauGuiLib",
            "NauPhysicsLib",
            "NauAssetMetadata"
        };
    }

    UsdMetaManager& UsdMetaManager::instance()
    {
        static UsdMetaManager inst;
        return inst;
    }

    UsdMetaInfoArray UsdMetaManager::getInfo(const std::string& filePath) const
    {
        auto stage = PXR_NS::UsdStage::Open(filePath, PXR_NS::UsdStage::LoadNone);
        return std::move(getInfo(stage));
    }

    UsdMetaInfoArray UsdMetaManager::getInfo(PXR_NS::UsdStageRefPtr stage) const
    {
        UsdMetaInfoArray dest;

        if (!stage)
            return std::move(dest);

        std::function<void(PXR_NS::UsdPrim, UsdMetaInfo*)> traverse = [&](PXR_NS::UsdPrim prim, UsdMetaInfo* parent)
        {
            auto info = getPrimInfo(prim);

            if (auto attr = prim.GetAttribute("path"_tftoken))
            {
                PXR_NS::SdfAssetPath path;
                if(attr.Get(&path))
                    info.assetSourcePath = path.GetAssetPath();
            }

            for (auto it : prim.GetChildren())
                traverse(it, &info);

            if (!parent)
                dest.push_back(std::move(info));
            else
                parent->children.push_back(std::move(info));
        };

        auto root = stage->GetPseudoRoot();

        for (auto it : root.GetChildren())
        {
            traverse(it, nullptr);
        }

        return std::move(dest);
    }

    nau::UsdMetaInfo UsdMetaManager::getPrimInfo(PXR_NS::UsdPrim& prim) const
    {
        if (!prim.IsValid())
            return {};

        auto typeName = prim.GetTypeName();
        auto it = m_regestry.find(typeName);

        UsdMetaInfo info;
        info.assetPath = prim.GetStage()->GetSessionLayer()->GetIdentifier();
        info.metaSourcePath = prim.GetPath().GetAsString();
        info.name = prim.GetName().GetString();
        if (it != m_regestry.end())
            info.isValid = it->second->process(prim, info);
        else
        {
            info.type = "undefined";
            if (!typeName.IsEmpty())
                LOG_WARN("Asset processor is not defined for type '{}'", typeName.GetString());
        }

        std::string uidStr;
        prim.GetAttribute("uid"_tftoken).Get(&uidStr);
        if (auto uid = Uid::parseString(uidStr))
            info.uid = *uid;

        return info;
    }

    bool UsdMetaManager::addProcessor(const PXR_NS::TfToken& primType, IMetaProcessor* processor)
    {
        m_regestry[primType] = processor;
        return true;
    }

    const IMetaProcessor* UsdMetaManager::getProcessor(const PXR_NS::TfToken& primType) const
    {
        auto it = m_regestry.find(primType);
        if (it != m_regestry.end())
        {
            return it->second;
        }
        return nullptr;
    }

    const bool UsdMetaManager::isValidMeta(const UsdMetaInfo& meta) const
    {
        return meta.isValid;
    }

    void loadPlugins()
    {
        using Entry = void (*)();
        static std::set<void*> regestried;

        auto required = g_requiredPlugins;
        auto plugins = PXR_NS::PlugRegistry::GetInstance().GetAllPlugins();
        for (auto& it : plugins)
        {
#ifdef WIN32

            auto handle = LoadLibraryW(std::filesystem::u8path(it->GetPath()).c_str());
            if (handle)
            {
                if (required.contains(it->GetName()))
                {
                    required.erase(it->GetName());
                }

                auto proc = GetProcAddress(handle, "nau_meta_plugin_entry");
                if (Entry entry = reinterpret_cast<Entry>(proc))
                {
                    if (regestried.find(entry) != regestried.end())
                    {
                        continue;
                    }

                    regestried.emplace(entry);
                    entry();
                }
            }
#else
            auto handle = dlopen(it->GetPath().c_str(), RTLD_NOW);
            if (handle)
            {
                if (required.contains(it->GetName()))
                {
                    required.erase(it->GetName());
                }

                if (Entry entry = reinterpret_cast<Entry>(dlsym(handle, "nau_meta_plugin_entry")))
                {
                    if (regestried.find(entry) != regestried.end())
                    {
                        continue;
                    }

                    regestried.emplace(entry);
                    entry();
                }
            }
#endif
        }

        static std::once_flag flag;
        std::call_once(flag, [&]()
        {
            if (!required.empty())
            {
                std::stringstream msg;
                msg << "Missing required plugins: ";
                for (auto& it : required)
                {
                    msg << it << " ";
                }
                auto msgStr = msg.str();
                LOG_WARN(msgStr, "");
#ifdef WIN32
                std::thread([msgStr]()
                {
                    MessageBox(NULL, msgStr.c_str(), "Plugins", MB_ICONWARNING | MB_OK);
                }).detach();
#else
#endif
            }
        });

        
    }
}  // namespace nau