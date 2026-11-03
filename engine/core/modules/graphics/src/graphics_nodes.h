// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include <shared_mutex>

#include "nau/math/math.h"
#include "nau/scene/camera/camera_manager.h"
#include "nau/scene/components/billboard_component.h"
#include "nau/scene/components/directional_light_component.h"
#include "nau/scene/components/environment_component.h"
#include "nau/scene/components/scene_component.h"
#include "nau/scene/components/skinned_mesh_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/animation/components/skeleton_component.h"
#include "render_pipeline/skinned_mesh_manager.h"
#include "render_pipeline/static_mesh_manager.h"

namespace nau::scene
{
    class CameraComponent;
}

namespace nau
{
    struct GraphicsSceneNode
    {
        Uid componentUid;
        nau::math::Matrix4 worldTransform;

        static void updateFromScene(GraphicsSceneNode& node, const scene::SceneComponent& sceneComponent);
    };

    struct StaticMeshNode : GraphicsSceneNode  // TODO: get rid of this nodes and left only handles
    {
        nau::Ptr<MaterialAssetView> material;
        eastl::unique_ptr<nau::MeshHandle> handle;
        eastl::optional<MaterialAssetRef> materialOverride;

        nau::math::Transform transform;

        static void updateFromScene(StaticMeshNode& node, const scene::SceneComponent& sceneComponent);

        explicit operator bool() const
        {
            return (componentUid != NullUid) && handle->isValid();
        }
    };

    struct CameraNode
    {
        nau::Ptr<scene::ICameraProperties> cameraProperties;
        nau::math::Matrix4 viewTransform;
        nau::math::Vector3 worldPosition;
        float aspectRatioRec = 1.f;

        void updateFromCamera();
        void setWorldPose(const math::Transform& pose);
        nau::math::Matrix4 getViewMatrix() const;
        nau::math::Matrix4 getProjMatrix() const;
        nau::math::Matrix4 getProjMatrixReverseZ() const;
        nau::math::Matrix4 getViewProjectionMatrix() const;
        const scene::ICameraProperties& getProperties() const;
    };

    struct DirectionalLightNode
    {
        Uid componentUid;

        constexpr static uint32_t csmMinWidth = 256;
        constexpr static uint32_t csmMaxWidth = 2048;

        math::Vector3 m_direction = {0.5f, -0.5f, 0.0f};
        math::Color3 m_color = {1, 1, 1};
        float m_intensity = 1;
        bool m_castShadows = false;
        uint32_t m_csmSize = 512;
        uint32_t m_csmCascadesCount = 4;
        float m_csmPowWeight = 0.985f;
    };

    struct SkinnedMeshNode : GraphicsSceneNode
    {
        Uid skeletonComponentUid;
        eastl::shared_ptr<nau::SkinnedMeshInstance> instance;

        eastl::optional<MaterialAssetRef> materialOverride;

        explicit operator bool() const
        {
            return componentUid != NullUid;
        }

        static void updateFromScene(SkinnedMeshNode& node, const scene::SceneComponent& sceneComponent, const SkeletonComponent& skeletonComponent);
    };

    struct BillboardNode : GraphicsSceneNode
    {
        nau::BillboardHandle::Ptr billboardHandle;
        eastl::optional<nau::TextureAssetRef> overrideTexture;
    };

    struct EnvironmentNode
    {
        Uid componentUid;
        float envIntensity = 1.0f;
        eastl::optional<nau::TextureAssetRef> newTextureRef;

        ReloadableAssetView::Ptr textureView;

        bool isDirty = false;
    };
    
    struct LightNode : public GraphicsSceneNode
    {
        uint32_t lightId;

        explicit operator bool() const
        {
            return componentUid != NullUid;
        }
    };

    async::Task<StaticMeshNode> makeStaticMeshNode(
        nau::Ptr<nau::RenderScene> renderScene,
        const nau::scene::StaticMeshComponent& meshComponent,
        MaterialAssetRef overrideMaterial);

    async::Task<SkinnedMeshNode> makeSkinnedMeshNode(
        nau::Ptr<nau::RenderScene> renderScene,
        const nau::SkinnedMeshComponent& skinnedMeshComponent,
        MaterialAssetRef overrideMaterial);

    async::Task<BillboardNode> makeBillboardNode(
        nau::Ptr<nau::RenderScene> renderScene,
        const scene::BillboardComponent& billboardComponent);

    EnvironmentNode makeEnvironmentNode(const scene::EnvironmentComponent& envComponent);

    inline DirectionalLightNode makeDirectionalLightNode(const scene::DirectionalLightComponent& directionalLightComponent)
    {
        DirectionalLightNode node;
        node.componentUid = directionalLightComponent.getUid();
        node.m_direction = directionalLightComponent.getDirection();
        node.m_color = directionalLightComponent.getColor();
        node.m_intensity = directionalLightComponent.getIntensity();
        node.m_castShadows = directionalLightComponent.hasShadows();
        node.m_csmCascadesCount = directionalLightComponent.getShadowCascadeCount();
        node.m_csmSize = Vectormath::clamp(directionalLightComponent.getShadowMapSize(), DirectionalLightNode::csmMinWidth, DirectionalLightNode::csmMaxWidth);
        node.m_csmPowWeight = directionalLightComponent.getCsmPowWeight();
        return node;
    }

}  // namespace nau
