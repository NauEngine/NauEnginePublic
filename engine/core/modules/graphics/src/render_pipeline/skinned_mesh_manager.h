// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "render_pipeline/render_scene.h"
#include "render_pipeline/render_manager.h"
#include "graphics_assets/skinned_mesh_asset.h"
#include "nau/assets/asset_ref.h"
#include "nau/shaders/shader_defines.h"

namespace nau
{
    class SkinnedMeshInstance
    {
    public:
        nau::math::Matrix4 bonesTransforms[NAU_MAX_SKINNING_BONES_COUNT];
        nau::math::Matrix4 bonesNormalTransforms[NAU_MAX_SKINNING_BONES_COUNT];

        void setWorldPos(const nau::math::Matrix4& matrix);
        nau::math::Matrix4 getWorldPos();

        void overrideMaterial(ReloadableAssetView::Ptr material);
        nau::Ptr<MaterialAssetView> getActiveMaterial(uint32_t lodIndex, uint32_t slotIndex);

        void setUid(const nau::Uid& uid);
        nau::Uid getUid() const;
        bool isHighlighted() const;


    private:
        friend class SkinnedMeshManager;

        ReloadableAssetView::Ptr skinnedMesh;
        ReloadableAssetView::Ptr m_materialOverride;

        nau::math::Matrix4 worldMatrix;
        nau::math::BSphere3 worldSphere;
        nau::Uid m_uid;
        bool m_isHighlighted;
    };

    class SkinnedMeshManager : public IRenderManager
    {
        NAU_CLASS_(nau::SkinnedMeshManager, IRenderManager);

    public:
        using Ptr = nau::Ptr<SkinnedMeshManager>;

        eastl::shared_ptr<nau::SkinnedMeshInstance> addSkinnedMesh(SkinnedMeshAssetRef ref);

        // IRenderManager
        RenderList::Ptr getRenderList(const nau::math::Vector3& viewerPosition,
            eastl::function<bool(const InstanceInfo&)>& filterFunc,
            eastl::function<bool(const nau::MaterialAssetView::Ptr)>& materialFilter, eastl::span<const Uid> excluded) override;

        void update() override;

    protected:

        RenderScene::Ptr m_sceneOwner;

        eastl::vector<ReloadableAssetView::Ptr> m_skinnedMeshes;

        eastl::vector<eastl::weak_ptr<nau::SkinnedMeshInstance>> m_skinnedMeshInstances;
    };
} // namespace nau

