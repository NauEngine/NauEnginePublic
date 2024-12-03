// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "vfx_component.h"
#include "nau/vfx_manager.h"
#include "nau/scene/scene_manager.h"
#include "nau/io/virtual_file_system.h"

namespace nau::vfx
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(VFXComponent)

    VFXComponent::~VFXComponent()
    {
        if (m_isVFXInScene)
        {
            getServiceProvider().get<VFXManager>().removeInstance(m_vfxInstance);
            m_isVFXInScene = false;
        }
    }

    nau::async::Task<> VFXComponent::activateComponentAsync()
    {
        //const bool paused = getServiceProvider().get<scene::ISceneManager>().getDefaultWorld().isSimulationPaused();
        //if (paused)
        //{
        //    co_return;
        //}

        if (!m_vfxInstance && getServiceProvider().has<VFXManager>())
        {
            MaterialAssetView::Ptr defaultMaterial = co_await defaultMaterialRef.getAssetViewTyped<MaterialAssetView>();
            m_vfxInstance = std::static_pointer_cast<modfx::VFXModFXInstance>(getServiceProvider().get<VFXManager>().addInstance(defaultMaterial));
            m_isVFXInScene = true;

            if (m_blk.load(resolvePath(m_assetPath).c_str()))
            {
                m_vfxInstance->deserialize(&m_blk);

                TextureAssetRef assetRef = AssetPath{m_blk.getStr("texture", "file:/content/textures/default.jpg")};
                auto texAsset = co_await assetRef.getReloadableAssetViewTyped<TextureAssetView>();
                if (texAsset)
                {
                    m_vfxInstance->setTexture(texAsset);
                }

                forcePositionUpdate();
            }
        }
    }

    void VFXComponent::deactivateComponent()
    {
        if (m_isVFXInScene)
        {
            getServiceProvider().get<vfx::VFXManager>().removeInstance(m_vfxInstance);
            m_isVFXInScene = false;
        }
    }

    void VFXComponent::updateComponent(float dt)
    {
        const bool paused = getServiceProvider().get<scene::ISceneManager>().getDefaultWorld().isSimulationPaused();
        if (paused)
        {
            return;
        }
    }

    void VFXComponent::setAssetPath(const eastl::string& assetPath)
    {
        m_assetPath = assetPath;
        m_cachedAssetPath = resolvePath(m_assetPath);

        if (!m_vfxInstance)
            return;

        if (m_blk.load(m_cachedAssetPath.c_str()))
            m_vfxInstance->deserialize(&m_blk);
    }

    void VFXComponent::forceUpdateTexture(ReloadableAssetView::Ptr texture)
    {
        m_vfxInstance->setTexture(texture);
    }

    void VFXComponent::forceBLKUpdate()
    {
        if (!m_vfxInstance || m_cachedAssetPath.empty())
            return;

        if (m_blk.load(m_cachedAssetPath.c_str()))
            m_vfxInstance->deserialize(&m_blk);
    }

    void VFXComponent::forcePositionUpdate()
    {
        auto pos = getWorldTransform().getMatrix().getTranslation();
        if (m_vfxInstance)
            m_vfxInstance->setTransform(getWorldTransform().getMatrix());
    }

    std::filesystem::path VFXComponent::dbPath()
    {
        auto& vfs = getServiceProvider().get<io::IVirtualFileSystem>();
        auto projectPath = std::filesystem::path(vfs.resolveToNativePath("/content")).parent_path();

        return std::filesystem::path(projectPath) / "assets_database";
    }

    eastl::string VFXComponent::resolvePath(const eastl::string& assetPath)
    {
        return eastl::string((dbPath().string() + "\\" + assetPath.c_str()).c_str());
    }

}  // namespace nau::physics
