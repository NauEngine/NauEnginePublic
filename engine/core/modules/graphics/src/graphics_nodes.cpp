// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "graphics_nodes.h"

#include "graphics_impl.h"
#include "nau/animation/assets/skeleton_asset.h"
#include "nau/animation/components/skeleton_component.h"
#include "nau/animation/components/skeleton_socket_component.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/diag/logging.h"
#include "nau/scene/components/camera_component.h"
#include "nau/scene/components/skinned_mesh_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/internal/scene_manager_internal.h"
#include "nau/scene/scene_object.h"
#include "nau/shaders/shader_globals.h"
#include "nau/utils/performance_profiling.h"

namespace nau
{
    namespace Debug
    {
        void drawBonesRecursive(const eastl::vector<SkeletonJoint>& joints, const nau::math::Matrix4* jointsTransforms, size_t rootJointIndex)
        {
            getDebugRenderer().drawSphere(0.05f, nau::math::Color4(0.0f, 0.0f, 1.0f, 1.0f), jointsTransforms[rootJointIndex], 8, 0.12f);
            for (size_t i = 0; i < joints.size(); ++i)
            {
                if (joints[i].parentIndex == rootJointIndex)
                {
                    const nau::math::Vector3& p1 = jointsTransforms[rootJointIndex].getTranslation();
                    const nau::math::Vector3& p2 = jointsTransforms[i].getTranslation();
                    getDebugRenderer().drawLine(nau::math::Point3(p1), nau::math::Point3(p2), nau::math::Color4(1.0f, 0.0f, 0.0f, 1.0f), 0.2f);

                    drawBonesRecursive(joints, jointsTransforms, i);
                }
            }
        }

        void debugDrawSkeleton(const SkeletonComponent& skeletonComponent)
        {
            const unsigned bonesCount = skeletonComponent.getBonesCount();

            if (bonesCount == 0)
            {
                return;
            }
            nau::math::Matrix4 debugBonesTransforms[NAU_MAX_SKINNING_BONES_COUNT];
            const auto& modelSpaceJointMatrices = skeletonComponent.getModelSpaceJointMatrices();

            std::memcpy(&debugBonesTransforms[0], &modelSpaceJointMatrices[0], bonesCount * 64);  // 64 == 16 elements * 4 (sizeof(float))

            auto modelTr = skeletonComponent.getWorldTransform().getMatrix();
            for (auto& tr : debugBonesTransforms)
            {
                tr = modelTr * tr;
            }
            const eastl::vector<SkeletonJoint>& joints = skeletonComponent.getJoints();
            auto rootNodeIt = eastl::find_if(joints.begin(), joints.end(), [](const SkeletonJoint& joint)
            {
                return joint.parentIndex == -1;
            });
            if (rootNodeIt != joints.end())
            {
                const size_t rootNodeIndex = eastl::distance(joints.begin(), rootNodeIt);
                Debug::drawBonesRecursive(joints, &debugBonesTransforms[0], rootNodeIndex);
            }
            else
            {
                NAU_ASSERT(false);
            }

            Vector<scene::SceneObject*> children = skeletonComponent.getParentObject().getDirectChildObjects();
            for (scene::SceneObject* child : children)
            {
                SkeletonSocketComponent* socketComponent = child->findFirstComponent<SkeletonSocketComponent>();
                if (socketComponent)
                {
                    const auto& tr = child->getWorldTransform();
                    getDebugRenderer().drawSphere(0.03f, nau::math::Color4(0.0f, 1.0f, 0.0f, 1.0f), tr.getMatrix(), 8, 0.12f);
                }
            }
        }
    }  // namespace Debug

    async::Task<StaticMeshNode> makeStaticMeshNode(nau::Ptr<nau::RenderScene> renderScene, const scene::StaticMeshComponent& meshComponent, MaterialAssetRef overrideMaterial)
    {
        StaticMeshAssetRef meshAsset = meshComponent.getMeshGeometry();
        if (!meshAsset)
        {
            NAU_LOG("Mesh missing");
            co_return StaticMeshNode{};
        }

        nau::MaterialAssetRef matRef = meshComponent.getMaterial();

        StaticMeshNode renderableMesh;
        renderableMesh.componentUid = meshComponent.getUid();

        renderableMesh.handle = co_await renderScene->getManagerTyped<StaticMeshManager>()->addStaticMesh(meshAsset, meshComponent.getWorldTransform().getMatrix());

        if (matRef)
        {
            renderableMesh.handle->overrideMaterial(0, 0, co_await matRef.getReloadableAssetViewTyped<MaterialAssetView>());
        }

        if (overrideMaterial)
        {
            ReloadableAssetView::Ptr dx12MaterialAsset;
            dx12MaterialAsset = co_await overrideMaterial.getReloadableAssetViewTyped<MaterialAssetView>();
            renderableMesh.handle->overrideMaterial(0, 0, dx12MaterialAsset);
        }

        co_return renderableMesh;
    }

    async::Task<SkinnedMeshNode> makeSkinnedMeshNode(nau::Ptr<nau::RenderScene> renderScene, const SkinnedMeshComponent& skinnedMeshComponent, MaterialAssetRef overrideMaterial)
    {
        using namespace nau::scene;

        SkinnedMeshAssetRef& meshAsset = skinnedMeshComponent.getMeshGeometry();
        if (!meshAsset)
        {
            NAU_LOG("Mesh missing");
            co_return SkinnedMeshNode{};
        }

        SkinnedMeshNode renderableSkinnedMesh;
        renderableSkinnedMesh.componentUid = skinnedMeshComponent.getUid();

        renderableSkinnedMesh.instance = renderScene->getManagerTyped<SkinnedMeshManager>()->addSkinnedMesh(meshAsset);
        
        nau::MaterialAssetRef matRef = skinnedMeshComponent.getMaterial();
        if (matRef)
        {
            renderableSkinnedMesh.instance->overrideMaterial(co_await matRef.getReloadableAssetViewTyped<MaterialAssetView>());
        }

        if (overrideMaterial)
        {
            ReloadableAssetView::Ptr dx12MaterialAsset;
            dx12MaterialAsset = co_await overrideMaterial.getReloadableAssetViewTyped<MaterialAssetView>();
            renderableSkinnedMesh.instance->overrideMaterial(dx12MaterialAsset);
        }

        SceneObject& parentObj = skinnedMeshComponent.getParentObject();
        if (const Component* skeletonComponent = parentObj.findFirstComponent<SkeletonComponent>())
        {
            renderableSkinnedMesh.skeletonComponentUid = skeletonComponent->getUid();
            SkinnedMeshNode::updateFromScene(renderableSkinnedMesh, skinnedMeshComponent.as<const SceneComponent&>(), skeletonComponent->as<const SkeletonComponent&>());
        }

        co_return renderableSkinnedMesh;
    }

    async::Task<BillboardNode> makeBillboardNode(nau::Ptr<nau::RenderScene> renderScene, const scene::BillboardComponent& billboardComponent)
    {
        TextureAssetRef textureAsset = billboardComponent.getTextureRef();
        if (!textureAsset)
        {
            NAU_LOG("Texture missing");
            co_return BillboardNode{};
        }

        ReloadableAssetView::Ptr texView = co_await textureAsset.getReloadableAssetViewTyped<TextureAssetView>();

        BillboardNode node;
        node.componentUid = billboardComponent.getUid();

        GraphicsSceneNode::updateFromScene(node, billboardComponent);

        node.billboardHandle = co_await renderScene->getBillboardsManager()->addBillboard(texView, node.worldTransform.getTranslation(), node.componentUid);

        co_return node;
    }

    EnvironmentNode makeEnvironmentNode(const scene::EnvironmentComponent& envComponent)
    {
        EnvironmentNode node = EnvironmentNode();
        node.componentUid = envComponent.getUid();
        node.envIntensity = envComponent.getIntensity();
        node.newTextureRef = envComponent.getTextureAsset();
        return node;
    }

    void GraphicsSceneNode::updateFromScene(GraphicsSceneNode& node, const scene::SceneComponent& sceneComponent)
    {
        node.worldTransform = sceneComponent.getWorldTransform().getMatrix();
    }

    void SkinnedMeshNode::updateFromScene(SkinnedMeshNode& mesh, const scene::SceneComponent& sceneComponent, const SkeletonComponent& skeletonComponent)
    {
        GraphicsSceneNode::updateFromScene(mesh, sceneComponent);

        const unsigned bonesCount = skeletonComponent.getBonesCount();

        if (bonesCount == 0)
        {
            return;
        }
        NAU_ASSERT(bonesCount <= NAU_MAX_SKINNING_BONES_COUNT);

        const auto& modelSpaceJointMatrices = skeletonComponent.getModelSpaceJointMatrices();
        const auto& inverseBindTransforms = skeletonComponent.getInverseBindTransforms();

        std::memcpy(&mesh.instance->bonesTransforms[0], &modelSpaceJointMatrices[0], bonesCount * 64);  // 64 == 16 elements * 4 (sizeof(float))

        for (size_t i = 0; i < bonesCount; ++i)
        {
            mesh.instance->bonesTransforms[i] = mesh.worldTransform * mesh.instance->bonesTransforms[i] * inverseBindTransforms.at(i);

            mesh.instance->bonesNormalTransforms[i] = math::transpose(math::inverse(mesh.instance->bonesTransforms[i]));
        }

        mesh.instance->setWorldPos(mesh.worldTransform);

        if (SkeletonComponent::drawDebugSkeletons)
        {
            Debug::debugDrawSkeleton(skeletonComponent);
        }
    }

    const scene::ICameraProperties& CameraNode::getProperties() const
    {
        NAU_FATAL(cameraProperties);
        return *cameraProperties;
    }

    void CameraNode::updateFromCamera()
    {
        using namespace ::DirectX;

        NAU_FATAL(cameraProperties);

        worldPosition = cameraProperties->getWorldTransform().getTranslation();
        viewTransform = nau::math::inverse(cameraProperties->getWorldTransform().getMatrix());
    }

    nau::math::Matrix4 CameraNode::getViewMatrix() const
    {
        using namespace ::DirectX;

        return viewTransform;
    }

    nau::math::Matrix4 CameraNode::getProjMatrix() const
    {
        return nau::math::Matrix4::perspectiveRH(nau::math::degToRad(cameraProperties->getFov()),
            aspectRatioRec,
            cameraProperties->getClipNearPlane(), cameraProperties->getClipFarPlane());
    }

    nau::math::Matrix4 CameraNode::getProjMatrixReverseZ() const
    {
        return nau::math::Matrix4::perspectiveRH_ReverseZ(nau::math::degToRad(cameraProperties->getFov()),
            aspectRatioRec,
            cameraProperties->getClipNearPlane(), cameraProperties->getClipFarPlane());
    }

    nau::math::Matrix4 CameraNode::getViewProjectionMatrix() const
    {
        return getProjMatrixReverseZ() * getViewMatrix();
    }

    void StaticMeshNode::updateFromScene(StaticMeshNode& node, const scene::SceneComponent& sceneComponent)
    {
        GraphicsSceneNode::updateFromScene(node, sceneComponent);

        node.transform = sceneComponent.getWorldTransform();
    }

}  // namespace nau
