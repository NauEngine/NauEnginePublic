// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/math/transform.h"
#include "render_pipeline/render_scene.h"
#include "render_pipeline/render_manager.h"
#include "render_pipeline/static_mesh_instance_group.h"
#include "nau/scene/components/static_mesh_component.h"


namespace nau
{
    class MeshHandle;

    class StaticMeshManager : public IRenderManager
    {
        NAU_CLASS_(nau::StaticMeshManager, IRenderManager);

    public:
        using Ptr = nau::Ptr<StaticMeshManager>;

        async::Task<eastl::unique_ptr<nau::MeshHandle>> addStaticMesh(StaticMeshAssetRef ref, const nau::math::Matrix4& matrix);

        void render(nau::math::Matrix4 viewProj); // temporal, for testing only

        // Inherited via IRenderManager
        RenderList::Ptr getRenderList(const nau::math::Vector3& viewerPosition,
            eastl::function<bool(const InstanceInfo&)>& filterFunc,
            eastl::function<bool(const nau::MaterialAssetView::Ptr)>& materialFilter, eastl::span<const Uid> excluded) override;

        void update() override;

    protected:
        async::Task<eastl::shared_ptr<StaticMeshInstanceGroup>> findOrCreateGroup(StaticMeshAssetRef ref);

        eastl::vector<eastl::weak_ptr<StaticMeshInstanceGroup>> m_meshGroups;
        eastl::vector<eastl::pair<StaticMeshAssetRef, eastl::weak_ptr<StaticMeshInstanceGroup>>> m_assetRefToGroup;

        RenderScene::Ptr m_sceneOwner;

        bool m_isGroupsDirty = false;
    };

    class MeshHandle
    {
    public:
        bool isValid() const;

        void setWorldTransform(const nau::math::Transform& transform);
        nau::math::Matrix4 getWorldPos();

        void setVisibility(bool isVisible);
        bool getVisibility() const;

        void setHighlighted(bool isHighlighted);
        bool isHighlighted() const;

        void setUid(const nau::Uid& uid);
        nau::Uid getUid() const;

        void addRenderTag(RenderTag tag);
        void removeRenderTag(RenderTag tag);

        void setCastShadow(bool castShadow);

        void syncState(nau::scene::StaticMeshComponent& component);

        void overrideMaterial(uint32_t lodIndex, uint32_t slotIndex, ReloadableAssetView::Ptr material);

        ~MeshHandle();

    private:
        friend class StaticMeshManager;

        InstanceInfo m_instInfo;
        eastl::shared_ptr<StaticMeshInstanceGroup> m_group;

        uint32_t m_generation;
        StaticMeshManager::Ptr m_manager;
        RenderScene::Ptr m_scene;
        bool isMaterialDirty = false;
    };
} // namespace nau

