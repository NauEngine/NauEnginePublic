// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "graphics_scene.h"

#include <nau/service/service_provider.h>

#include "graphics_impl.h"
#include "nau/animation/components/skeleton_component.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/diag/logging.h"
#include "nau/scene/components/billboard_component.h"
#include "nau/scene/components/camera_component.h"
#include "nau/scene/components/directional_light_component.h"
#include "nau/scene/components/omnilight_component.h"
#include "nau/scene/components/skinned_mesh_component.h"
#include "nau/scene/components/spotlight_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/internal/scene_manager_internal.h"
#include "nau/scene/scene_object.h"
#include "nau/shaders/dag_renderStateId.h"
#include "nau/shaders/shader_globals.h"
#include "nau/utils/performance_profiling.h"
#include "nau/vfx_manager.h"

namespace nau
{
    GraphicsScene::GraphicsScene(Uid worldUid) :
        m_worldUid(worldUid)
    {
        m_renderScene = nau::rtti::createInstance<nau::RenderScene>();
        m_renderScene->addManager(nau::rtti::createInstance<nau::StaticMeshManager>());
        m_renderScene->addManager(nau::rtti::createInstance<nau::SkinnedMeshManager>());
    }

    static RenderViews createCameraViews(bool withShadows)
    {
        RenderViews views;
        auto mainView = eastl::make_shared<nau::RenderView>("Main View");
        mainView->addTag(nau::RenderScene::Tags::opaqueTag);
        views.push_back(mainView);

        const int cascadeCount = withShadows ? nau::csm::CascadeShadows::MAX_CASCADES : 0;
        for (int i = 0; i < cascadeCount; ++i)
        {
            auto csmView = eastl::make_shared<nau::RenderView>(nau::utils::format("{}_{}", "csmView", i).c_str());
            csmView->addTag(nau::RenderScene::Tags::shadowCascadeTag);
            csmView->setUserData((void*)i);
            nau::RenderView* csmViewPtr = csmView.get();
            auto csmFilter = eastl::function<bool(const InstanceInfo&)>([csmViewPtr](const InstanceInfo& info)
            {
                return info.isCastShadow && csmViewPtr->getFrustum().testSphere(info.worldSphere) != 0;
            });
            csmView->setInstanceFilter(csmFilter);

            views.push_back(csmView);
        }

        auto translucentView = eastl::make_shared<nau::RenderView>("Main View (Translucent)");
        translucentView->addTag(nau::RenderScene::Tags::translucentTag);

        auto translucentFilter = eastl::function<bool(const MaterialAssetView::Ptr)>([](const MaterialAssetView::Ptr material)
        {
            nau::BlendMode mode = material->getBlendMode("default");
            return mode != nau::BlendMode::Opaque && mode != nau::BlendMode::Masked;
        });
        translucentView->setMaterialFilter(translucentFilter);
        views.push_back(translucentView);
        return views;
    }

    bool GraphicsScene::prepareView(GraphicsSceneView& view, Uid cameraUid, int width, int height,
                                    eastl::span<const Uid> excluded, bool withShadows, const math::Transform* worldPose)
    {
        if (width <= 0 || height <= 0 || m_cameras.empty())
        {
            return false;
        }
        if (cameraUid == NullUid)
        {
            view.camera = getMainCamera();
        }
        else
        {
            const auto it = m_cameras.find(cameraUid);
            if (it == m_cameras.end())
            {
                return false;
            }
            view.camera = it->second;
        }

        if (worldPose)
        {
            view.camera.setWorldPose(*worldPose);
        }
        view.camera.aspectRatioRec = static_cast<float>(height) / static_cast<float>(width);
        const auto expectedViewCount = 2u + (withShadows ? csm::CascadeShadows::MAX_CASCADES : 0);
        if (view.views.size() != expectedViewCount)
        {
            view.views = createCameraViews(withShadows);
        }
        const math::Matrix4 vp = view.camera.getProjMatrix() * view.camera.getViewMatrix();
        for (const auto& renderView : view.views)
        {
            if (!renderView->containsTag(RenderScene::Tags::shadowCascadeTag))
            {
                m_renderScene->updateView(*renderView, vp, view.camera.worldPosition, excluded);
            }
        }
        return true;
    }

    void GraphicsScene::prepareShadowView(RenderView& cascadeView, const math::Matrix4& vp, eastl::span<const Uid> excluded)
    {
        m_renderScene->updateView(cascadeView, vp, math::Vector3(0.f), excluded);
    }

    async::Task<> GraphicsScene::initialize()
    {
        async::TaskCollection tasks;
        tasks.push(m_renderScene->initialize());
        tasks.push(m_lights.init(0, 0, 0));
        co_await tasks.awaitCompletion();
        co_return;
    }

    async::Task<> GraphicsScene::activateComponents(eastl::span<const scene::Component*> components, [[maybe_unused]] async::Task<> barrier)
    {
        using namespace nau::async;
        using namespace nau::scene;

        const bool hasAcceptableComponents = eastl::any_of(components.begin(), components.end(), [](const scene::Component* component)
        {
            return component->is<StaticMeshComponent>() ||
                   component->is<BillboardComponent>() ||
                   component->is<DirectionalLightComponent>() ||
                   component->is<OmnilightComponent>() ||
                   component->is<SpotlightComponent>() ||
                   component->is<SkinnedMeshComponent>() ||
                   component->is<EnvironmentComponent>();
        });

        if (!hasAcceptableComponents)
        {
            co_return;
        }

        ASYNC_SWITCH_EXECUTOR(Executor::getDefault())

        // Async objects creation step
        eastl::vector<Task<StaticMeshNode>> staticMeshes;
        eastl::vector<Task<SkinnedMeshNode>> skinnedMeshes;
        eastl::vector<Task<BillboardNode>> billboards;
        eastl::vector<DirectionalLightNode> directionalLights;
        eastl::vector<EnvironmentNode> envNodes;
        eastl::vector<LightNode> lights;

        for (const Component* component : components)
        {
            auto& parentObj = component->getParentObject();
            auto myName = parentObj.getName();
            MaterialAssetRef meshMaterial{};
            if (myName == "node_damagedHelmet_-6514")
            {
                meshMaterial = MaterialAssetRef{"file:/res/materials/embedded/standard_opaque.nmat_json"};
            }
            else if (myName == "node_damagedHelmet_-6514_Translucent" || myName == "node_damagedHelmet_-6514_Translucent_copy")
            {
                meshMaterial = MaterialAssetRef{"file:/res/materials/embedded/standard_translucent.nmat_json"};
            }
            else if (myName == "robot")
            {
                meshMaterial = MaterialAssetRef{"file:/res/materials/skinned_robot.nmat_inst_json"};
            }
            if (const auto* const meshComponent = component->as<const StaticMeshComponent*>())
            {
                staticMeshes.emplace_back(makeStaticMeshNode(m_renderScene, *meshComponent, meshMaterial));
            }
            else if (const auto* const skinnedMeshComponent = component->as<const SkinnedMeshComponent*>())
            {
                skinnedMeshes.emplace_back(makeSkinnedMeshNode(m_renderScene, *skinnedMeshComponent, meshMaterial));
            }
            else if (const auto* const billboardComponent = component->as<const BillboardComponent*>())
            {
                billboards.emplace_back(makeBillboardNode(m_renderScene, *billboardComponent));
            }
            else if (const auto* const directionalLightComponent = component->as<const DirectionalLightComponent*>())
            {
                directionalLights.emplace_back(makeDirectionalLightNode(*directionalLightComponent));
            }
            else if (const auto* const envComponent = component->as<const EnvironmentComponent*>())
            {
                envNodes.emplace_back(makeEnvironmentNode(*envComponent));
            }
            else if (const auto* const omnilightComponent = component->as<const OmnilightComponent*>())
            {
                auto lightId = m_lights.addOmniLight(render::ClusteredLights::OmniLight::create_empty());
                LightNode node;
                node.lightId = lightId;
                node.componentUid = omnilightComponent->getUid();
                lights.emplace_back(node);
            }
            else if (const auto* const spotlightComponent = component->as<const SpotlightComponent*>())
            {
                auto lightId = m_lights.addSpotLight(render::ClusteredLights::SpotLight::create_empty());
                LightNode node;
                node.lightId = lightId;
                node.componentUid = spotlightComponent->getUid();
                lights.emplace_back(node);
            }
        }

        TaskCollection taskToMakeComponents;

        taskToMakeComponents.push(async::whenAll(staticMeshes));
        taskToMakeComponents.push(async::whenAll(skinnedMeshes));
        taskToMakeComponents.push(async::whenAll(billboards));

        co_await taskToMakeComponents.awaitCompletion();

        // switching to before render step,
        // so can modify scene state without locking it.
        auto& graphics = getServiceProvider().get<GraphicsImpl>();
        co_await graphics.getPreRenderExecutor();

        if (!staticMeshes.empty())
        {
            m_staticMeshes.reserve(m_staticMeshes.size() + staticMeshes.size());
            for (Task<StaticMeshNode>& meshTask : staticMeshes)
            {
                if (meshTask.isReady())
                {
                    m_staticMeshes.emplace_back(*std::move(meshTask));

                    auto& mesh = m_staticMeshes.back();
                    mesh.handle->setUid(mesh.componentUid);
                }
            }
        }

        if (!skinnedMeshes.empty())
        {
            m_skinnedMeshes.reserve(m_skinnedMeshes.size() + skinnedMeshes.size());
            for (Task<SkinnedMeshNode>& meshTask : skinnedMeshes)
            {
                if (meshTask.isReady())
                {
                    m_skinnedMeshes.emplace_back(*std::move(meshTask));

                    auto& mesh = m_skinnedMeshes.back();
                    mesh.instance->setUid(mesh.componentUid);
                }
            }
        }

        if (!billboards.empty())
        {
            m_billboards.reserve(m_billboards.size() + billboards.size());
            for (Task<BillboardNode>& billTask : billboards)
            {
                if (billTask.isReady())
                {
                    m_billboards.emplace_back(*std::move(billTask));
                }
            }
        }

        if (!directionalLights.empty())
        {
            m_directionalLights.reserve(m_directionalLights.size() + directionalLights.size());
            for (auto& light : directionalLights)
            {
                m_directionalLights.emplace_back(light);
            }
            if (m_directionalLights.size() > 1)
            {
                NAU_LOG_WARNING("More than 1 directional light on scene ({} lights). Render supports only one.", m_directionalLights.size());
            }
        }

        if (!envNodes.empty())
        {
            m_envNodes.reserve(m_envNodes.size() + envNodes.size());
            for (auto& env : envNodes)
            {
                m_envNodes.emplace_back(env);
            }
            if (m_envNodes.size() > 1)
            {
                NAU_LOG_WARNING("More than 1 environment component on scene ({} env components). Render supports only one.", m_envNodes.size());
            }
        }
        if (!lights.empty())
        {
            m_lightNodes.reserve(m_lightNodes.size() + lights.size());
            for (LightNode& light : lights)
            {
                m_lightNodes.emplace_back(std::move(light));
            }
        }
    }

    async::Task<> GraphicsScene::deactivateComponents(eastl::span<const scene::DeactivatedComponentData> components)
    {
        using namespace nau::scene;

        auto& graphics = getServiceProvider().get<GraphicsImpl>();
        co_await graphics.getPreRenderExecutor();

        const auto componentRemoved = [&components](Uid uid)
        {
            return eastl::any_of(components.begin(), components.end(), [&uid](const DeactivatedComponentData& c)
            {
                return c.componentUid == uid;
            });
        };

        const auto removeObjects = [&]<typename Container>(Container& container)
        {
            auto iter = std::remove_if(container.begin(), container.end(), [&](const auto& node)
            {
                return componentRemoved(node.componentUid);
            });

            container.erase(iter, container.end());
        };

        const auto removeLights = [&]<typename Container>(Container& container)
        {
            auto iter = std::remove_if(container.begin(), container.end(), [&](const auto& node)
            {
                return componentRemoved(node.componentUid);
            });

            if (iter != container.end())
            {
                m_lights.destroyLight(iter->lightId);
                container.erase(iter, container.end());
            }
        };

        removeObjects(m_skinnedMeshes);
        removeObjects(m_staticMeshes);
        removeObjects(m_billboards);
        removeObjects(m_directionalLights);
        removeObjects(m_envNodes);
        removeLights(m_lightNodes);
    }

    async::Task<> GraphicsScene::update()
    {
        for (auto& m : m_staticMeshes)
        {
            if (m.materialOverride)
            {
                MaterialAssetRef& materialRef = *m.materialOverride;

                auto dx12MaterialAsset = co_await materialRef.getReloadableAssetViewTyped<MaterialAssetView>();

                m.handle->overrideMaterial(0, 0, dx12MaterialAsset);
                m.materialOverride.reset();
            }
        }
        for (auto& m : m_skinnedMeshes)
        {
            if (m.materialOverride)
            {
                MaterialAssetRef& materialRef = *m.materialOverride;

                auto dx12MaterialAsset = co_await materialRef.getReloadableAssetViewTyped<MaterialAssetView>();

                m.instance->overrideMaterial(dx12MaterialAsset);
                m.materialOverride.reset();
            }
        }

        for (auto& bill : m_billboards)
        {
            if (bill.overrideTexture)
            {
                TextureAssetRef& texRef = *bill.overrideTexture;
                auto texAsset = co_await texRef.getReloadableAssetViewTyped<TextureAssetView>();

                bill.billboardHandle->setTexture(texAsset);
                bill.overrideTexture.reset();
            }
        }

        if (!m_envNodes.empty())
        {
            auto& node = m_envNodes[0];
            if (node.newTextureRef)
            {
                TextureAssetRef& texRef = *node.newTextureRef;
                ReloadableAssetView::Ptr texAsset = co_await texRef.getReloadableAssetViewTyped<TextureAssetView>();
                node.newTextureRef.reset();
                if (texAsset)
                {
                    node.textureView = texAsset;
                    node.isDirty = true;
                }
            }
        }

        m_renderScene->updateManagers();

        co_return;
    }

    void GraphicsScene::renderFrame(const GraphicsSceneView& view)
    {
        m_renderScene->renderScene(view.views, view.camera.getViewProjectionMatrix());
    }

    void GraphicsScene::renderDepth(const GraphicsSceneView& view)
    {
        m_renderScene->renderDepth(view.views, view.camera.getViewProjectionMatrix());
    }

    void GraphicsScene::renderOutlineMask(const GraphicsSceneView& view)
    {
        m_renderScene->renderOutlineMask(view.views, view.camera.getViewProjectionMatrix());
    }

    void GraphicsScene::renderTranslucency(const GraphicsSceneView& view)
    {
        const auto& activeCamera = view.camera;
        if (getServiceProvider().has<vfx::VFXManager>())
        {
            getServiceProvider().get<vfx::VFXManager>().render(activeCamera.getViewMatrix(), activeCamera.getProjMatrixReverseZ());
        }

        m_renderScene->renderTranslucency(view.views, activeCamera.getViewProjectionMatrix());
    }

    void GraphicsScene::renderLights(const GraphicsSceneView& view)
    {
        const auto& activeCamera = view.camera;

        m_lights.cullFrustumLights(
            math::Point3(activeCamera.worldPosition),
            activeCamera.getViewProjectionMatrix(),
            activeCamera.getViewMatrix(),
            activeCamera.getProjMatrix(),
            activeCamera.getProperties().getClipNearPlane());

        if (!m_lights.hasDeferredLights())
        {
            return;
        }

        auto mvp = activeCamera.getViewProjectionMatrix();
        shader_globals::setVariable("mvp", &mvp);
        auto world_view_pos = math::Vector4(activeCamera.worldPosition);
        shader_globals::setVariable("world_view_pos", &world_view_pos);

        m_lights.renderOtherLights();
    }

    void GraphicsScene::renderDebugLights(const GraphicsSceneView& view)
    {
        if (!m_lights.hasDebugLights())
        {
            return;
        }

        d3d::setwire(true);

        const auto& activeCamera = view.camera;

        auto mvp = activeCamera.getViewProjectionMatrix();
        shader_globals::setVariable("mvp", &mvp);
        auto world_view_pos = math::Vector4(activeCamera.worldPosition);
        shader_globals::setVariable("world_view_pos", &world_view_pos);

        m_lights.renderDebugLights();
        d3d::setwire(false);
    }

    void GraphicsScene::renderSceneDebug(const GraphicsSceneView& view)
    {
        getDebugRenderer().draw(view.camera.getViewProjectionMatrix(), 1);
        renderDebugLights(view);
    }

    void GraphicsScene::renderBillboards(const GraphicsSceneView& view, eastl::span<const Uid> excluded)
    {
        m_renderScene->renderBillboards(view.camera.getViewProjectionMatrix(), 1.f / view.camera.aspectRatioRec, excluded);
    }

    void GraphicsScene::syncSceneState()
    {
        using namespace nau::scene;

        // TODO Tracy        NAU_CPU_SCOPED_TAG(nau::PerfTag::Core);
        if (!getServiceProvider().has<ISceneManagerInternal>())
        {
            return;
        }
        auto& sceneManager = getServiceProvider().get<ISceneManagerInternal>();

        for (auto& m : m_staticMeshes)
        {
            if (Component* const component = sceneManager.findComponent(m.componentUid))
            {
                // StaticMeshNode::updateFromScene(m, component->as<const SceneComponent&>());

                StaticMeshComponent& staticMeshComponent = component->as<StaticMeshComponent&>();
                if ((staticMeshComponent.getDirtyFlags() & static_cast<uint32_t>(StaticMeshComponent::DirtyFlags::Material)) && staticMeshComponent.getMaterial())
                {
                    m.materialOverride = staticMeshComponent.getMaterial();
                }
                m.handle->syncState(staticMeshComponent);

                staticMeshComponent.resetDirtyFlags();
            }
        }
        for (auto& m : m_skinnedMeshes)
        {
            Component* const skMeshComponent = sceneManager.findComponent(m.componentUid);

            if (!skMeshComponent)
            {
                continue;
            }

            Component* skeletonComponent = nullptr;
            if (m.skeletonComponentUid != NullUid)
            {
                skeletonComponent = sceneManager.findComponent(m.skeletonComponentUid);
            }
            if (!skeletonComponent)
            {
                SceneObject& parentObj = skMeshComponent->getParentObject();
                if (skeletonComponent = parentObj.findFirstComponent<SkeletonComponent>())
                {
                    m.skeletonComponentUid = skeletonComponent->getUid();
                }
            }

            SkinnedMeshComponent& skinnedMeshComponent = skMeshComponent->as<SkinnedMeshComponent&>();

            if (skinnedMeshComponent.isMaterialDirty() && skinnedMeshComponent.getMaterial())
            {
                m.materialOverride = skinnedMeshComponent.getMaterial();
                skinnedMeshComponent.resetIsMaterialDirty();
            }

            if (skeletonComponent)
            {
                SkinnedMeshNode::updateFromScene(m, skMeshComponent->as<const SceneComponent&>(), skeletonComponent->as<const SkeletonComponent&>());
            }
        }

        for (auto& bill : m_billboards)
        {
            if (Component* const component = sceneManager.findComponent(bill.componentUid))
            {
                GraphicsSceneNode::updateFromScene(bill, component->as<const SceneComponent&>());
                BillboardComponent& billComponent = component->as<BillboardComponent&>();
                bill.billboardHandle->setScreenPercentageSize(billComponent.getScreenPercentageSize());
                bill.billboardHandle->setWorldPos(billComponent.getWorldTransform().getTranslation());
                bill.billboardHandle->setVisibility(billComponent.getVisibility());
                if (billComponent.isTextureDirty())
                {
                    bill.overrideTexture = billComponent.getTextureRef();
                    billComponent.resetIsTextureDirty();
                }
            }
        }

        for (auto& directionalLight : m_directionalLights)
        {
            if (Component* const component = sceneManager.findComponent(directionalLight.componentUid))
            {
                DirectionalLightComponent& directionalLightComponent = component->as<DirectionalLightComponent&>();
                directionalLight = makeDirectionalLightNode(directionalLightComponent);
            }
        }
        for (auto& light : m_lightNodes)
        {
            if (Component* const component = sceneManager.findComponent(light.componentUid))
            {
                if (component->is<OmnilightComponent>())
                {
                    GraphicsSceneNode::updateFromScene(light, component->as<const SceneComponent&>());
                    OmnilightComponent& omnilightComponent = component->as<OmnilightComponent&>();
                    m_lights.setLight(light.lightId, render::ClusteredLights::OmniLight{
                                                         math::float3((omnilightComponent.getWorldTransform().getTranslation()) + omnilightComponent.getShift()),
                                                         omnilightComponent.getColor(),
                                                         omnilightComponent.getRadius(),
                                                         omnilightComponent.getAttenuation(),
                                                         omnilightComponent.getIntensity(),
                                                         omnilightComponent.getDebugDraw()});
                }
                if (component->is<SpotlightComponent>())
                {
                    GraphicsSceneNode::updateFromScene(light, component->as<const SceneComponent&>());
                    SpotlightComponent& spotlightComponent = component->as<SpotlightComponent&>();
                    m_lights.setLight(light.lightId, render::ClusteredLights::SpotLight{
                                                         math::float3((spotlightComponent.getWorldTransform().getTranslation()) + spotlightComponent.getShift()),
                                                         spotlightComponent.getColor(),
                                                         spotlightComponent.getRadius(),
                                                         spotlightComponent.getIntensity(),
                                                         spotlightComponent.getAttenuation(),
                                                         math::float3(spotlightComponent.getWorldTransform().transformVector(spotlightComponent.getDirection())),
                                                         spotlightComponent.getAngle(),
                                                         false,
                                                         spotlightComponent.getDebugDraw()});
                }
                // TODO: SpotlightComponent
            }
        }

        if (!m_envNodes.empty())
        {
            if (Component* const component = sceneManager.findComponent(m_envNodes[0].componentUid))
            {
                EnvironmentComponent& envComponent = component->as<EnvironmentComponent&>();
                m_envNodes[0].envIntensity = envComponent.getIntensity();
                if (envComponent.isTextureDirty())
                {
                    envComponent.resetIsTextureDirty();
                    m_envNodes[0].newTextureRef = envComponent.getTextureAsset();
                }
            }
        }

        syncSceneCameras();
    }

    void GraphicsScene::syncSceneCameras()
    {
        using namespace nau::scene;

        auto onCameraAdded = [&](ICameraProperties& cam)
        {
            if (cam.getWorldUid() != m_worldUid)
            {
                return;
            }
            NAU_LOG_VERBOSE("Found new camera:({}), uid:({}) from world:({})", cam.getCameraName(), toString(cam.getCameraUid()), toString(cam.getWorldUid()));

            [[maybe_unused]] auto [iter, emplaceCameraOk] = m_cameras.emplace(cam.getCameraUid(), CameraNode{.cameraProperties = Ptr{&cam}});
            NAU_ASSERT(emplaceCameraOk);
        };

        auto onCameraRemoved = [&](const ICameraProperties& cam)
        {
            m_cameras.erase(cam.getCameraUid());
        };

        getServiceProvider().get<ICameraManager>().syncCameras(m_allInGameCameras, onCameraAdded, onCameraRemoved);
        for (auto& [uid, camera]: m_cameras)
        {
            camera.updateFromCamera();
        }
    }

    CameraNode& GraphicsScene::getMainCamera()
    {
        NAU_ASSERT(!m_cameras.empty());

        Ptr<scene::ICameraProperties> cameraProps = m_allInGameCameras.getWorldMainCamera(m_worldUid);
        if (cameraProps)
        {
            auto camIter = m_cameras.find(cameraProps->getCameraUid());
            if (camIter != m_cameras.end())
            {
                return (camIter->second);
            }
        }
    
        return m_cameras.begin()->second;
    }

    bool GraphicsScene::hasMainCamera() const
    {
        return !m_cameras.empty();
    }

    void GraphicsScene::setObjectHighlight(nau::Uid uid, bool flag)
    {
        for (auto& m : m_staticMeshes)
        {
            if (uid == m.componentUid)
            {
                m.handle->setHighlighted(flag);
                break;
            }
        }
    }

    nau::RenderScene* GraphicsScene::getRenderScene()
    {
        return m_renderScene.get();
    }

    bool GraphicsScene::hasCamera()
    {
        return (!m_cameras.empty());
    }

    bool GraphicsScene::hasDirectionalLight()
    {
        return !m_directionalLights.empty();
    }

    const eastl::vector<DirectionalLightNode>& GraphicsScene::getDirectionalLights() const
    {
        return m_directionalLights;
    }

    bool GraphicsScene::hasEnvironmentNode()
    {
        return !m_envNodes.empty();
    }

    EnvironmentNode& GraphicsScene::getEnvironmentNode()
    {
        NAU_ASSERT(hasEnvironmentNode());

        return m_envNodes[0];
    }

}  // namespace nau
