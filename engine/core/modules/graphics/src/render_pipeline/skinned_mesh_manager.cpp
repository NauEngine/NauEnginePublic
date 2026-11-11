// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "render_pipeline/skinned_mesh_manager.h"

#include <EASTL/algorithm.h>

#include "graphics_assets/skinned_mesh_asset.h"
#include "nau/math/dag_lsbVisitor.h"

namespace nau
{
    eastl::shared_ptr<nau::SkinnedMeshInstance> SkinnedMeshManager::addSkinnedMesh(SkinnedMeshAssetRef ref)
    {
        ReloadableAssetView::Ptr meshAsset = *async::waitResult(ref.getReloadableAssetViewTyped<SkinnedMeshAssetView>());
        NAU_ASSERT(meshAsset);
        m_skinnedMeshes.push_back(meshAsset);

        eastl::shared_ptr<nau::SkinnedMeshInstance> ret = eastl::make_shared<nau::SkinnedMeshInstance>();

        ret->skinnedMesh = m_skinnedMeshes.back();

        m_skinnedMeshInstances.emplace_back(ret);

        return eastl::move(ret);
    }

    RenderList::Ptr nau::SkinnedMeshManager::getRenderList(const nau::math::Vector3& viewerPosition,
        eastl::function<bool(const InstanceInfo&)>&,
        eastl::function<bool(const nau::MaterialAssetView::Ptr)>& materialFilter, eastl::span<const Uid> excluded)
    {
        eastl::vector<RenderList::Ptr> lists;

        if (!m_skinnedMeshInstances.empty())
        {
            lists.emplace_back();
            lists.front() = eastl::make_shared<RenderList>();
        }

        for (auto& skinnedMeshInstanceWeak : m_skinnedMeshInstances)
        {
            const auto skinnedMeshInstance = skinnedMeshInstanceWeak.lock();
            // NAU-1797: frustum culling needs animated bounds.
            if (!skinnedMeshInstance || eastl::find(excluded.begin(), excluded.end(), skinnedMeshInstance->getUid()) != excluded.end())
            {
                continue;
            }
            if (!materialFilter(skinnedMeshInstance->getActiveMaterial(0, 0)))
            {
                continue;
            }

            auto& skinnedMesh = skinnedMeshInstance->skinnedMesh;

            RenderEntity& ent = lists.front()->emplaceBack();

            nau::Ptr<SkinnedMeshAssetView> skinnedMeshView;
            skinnedMesh->getTyped<SkinnedMeshAssetView>(skinnedMeshView);

            const SkinnedMeshLod& lod = skinnedMeshView->getMesh()->getLod(0);
            ent.positionBuffer = lod.m_positionsBuffer;
            ent.normalsBuffer = lod.m_normalsBuffer;
            ent.texcoordsBuffer = lod.m_texcoordsBuffer;
            ent.tangentsBuffer = lod.m_tangentsBuffer;
            ent.boneWeightsBuffer = lod.m_boneWeightsBuffer;
            ent.boneIndicesBuffer = lod.m_boneIndicesBuffer;

            ent.indexBuffer = lod.m_indexBuffer;

            ent.startInstance = 0;
            ent.instancesCount = 1;
            ent.instanceData = {};
            ent.tags = {};

            ent.startIndex = 0;
            ent.endIndex = lod.m_indexCount;
            ent.material = skinnedMeshInstance->getActiveMaterial(0, 0);

            ent.instancingSupported = false;
            ent.worldTransform = skinnedMeshInstance->worldMatrix;
            ent.cbStructsData["BonesTransforms"] = RenderEntity::ConstBufferStructData{sizeof(skinnedMeshInstance->bonesTransforms), skinnedMeshInstance->bonesTransforms};
            ent.cbStructsData["BonesNormalTransforms"] = RenderEntity::ConstBufferStructData{sizeof(skinnedMeshInstance->bonesNormalTransforms), skinnedMeshInstance->bonesNormalTransforms};

            ent.instanceData.emplace_back(skinnedMeshInstance->worldMatrix, skinnedMeshInstance->worldMatrix, skinnedMeshInstance->getUid(), skinnedMeshInstance->isHighlighted());
        }

        return eastl::make_shared<RenderList>(std::move(lists));
    }

    void SkinnedMeshManager::update()
    {
        eastl::erase_if(m_skinnedMeshInstances, [](eastl::weak_ptr<nau::SkinnedMeshInstance>& val)
        {
            return val.expired();
        });
    }

    void SkinnedMeshInstance::setWorldPos(const nau::math::Matrix4& matrix)
    {
        worldMatrix = matrix;

        // update aabb, bounding sphere, etc.
        worldSphere.c = worldMatrix.getTranslation();  // TODO: need to take into account the scale
    }

    nau::math::Matrix4 SkinnedMeshInstance::getWorldPos()
    {
        return worldMatrix;
    }

    void SkinnedMeshInstance::overrideMaterial(ReloadableAssetView::Ptr material)
    {
        m_materialOverride = material;
    }

    nau::Ptr<MaterialAssetView> SkinnedMeshInstance::getActiveMaterial(uint32_t lodIndex, uint32_t slotIndex)
    {
        if (m_materialOverride)
        {
            nau::Ptr<MaterialAssetView> materialMeshView;
            m_materialOverride->getTyped<MaterialAssetView>(materialMeshView);
            return materialMeshView;
        }
        nau::Ptr<SkinnedMeshAssetView> skinnedMeshView;
        skinnedMesh->getTyped<SkinnedMeshAssetView>(skinnedMeshView);
        nau::Ptr<MaterialAssetView> materialMeshView;
        skinnedMeshView->getMesh()->getLod(0).m_material->getTyped<MaterialAssetView>(materialMeshView);
        return materialMeshView;
    }

    void SkinnedMeshInstance::setUid(const nau::Uid& uid)
    {
        m_uid = uid;
    }

    nau::Uid SkinnedMeshInstance::getUid() const
    {
        return m_uid;
    }

    bool SkinnedMeshInstance::isHighlighted() const
    {
        return m_isHighlighted;
    }

}  // namespace nau
