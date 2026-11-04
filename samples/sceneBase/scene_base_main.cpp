// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <imgui.h>


#include "./sample_components/asset_reloader.h"
#include "./sample_components/mgpu_toggle.h"
#include "./sample_components/scene_reloader.h"
#include "./sample_components/window_maker.h"
#include "./scene_loaders.h"
#include "nau/animation/animation_manager.h"
#include "nau/animation/components/animation_component.h"
#include "nau/animation/components/skeleton_component.h"
#include "nau/animation/components/skeleton_socket_component.h"
#include "nau/animation/playback/animation_instance.h"
#include "nau/animation/playback/animation_scalars.h"
#include "nau/animation/playback/animation_transforms.h"
#include "nau/app/core_window_manager.h"
#include "nau/app/global_properties.h"
#include "nau/app/run_application.h"
#include "nau/dataBlock/dag_dataBlock.h"
#include "nau/module/module_manager.h"
#include "nau/physics/components/rigid_body_component.h"
#include "nau/physics/physics_body.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include "nau/physics/physics_world.h"
#include "nau/samples/components/camera_control.h"
#include "nau/scene/components/directional_light_component.h"
#include "nau/scene/components/environment_component.h"
#include "nau/scene/components/omnilight_component.h"
#include "nau/scene/components/spotlight_component.h"
#include "nau/service/service_provider.h"

// #define USE_PHYSICS
#define USE_ANIMATION

namespace nau::sample
{
    async::Task<scene::IScene::Ptr> loadGltfScene(const eastl::string& sceneAssetPath, bool setupCamera);

    struct CustomHeightAnimTarget : animation::ScalarParameterAnimatable
    {
        NAU_CLASS(CustomHeightAnimTarget, rtti::RCPolicy::StrictSingleThread, animation::ScalarParameterAnimatable)

        CustomHeightAnimTarget(scene::SceneObject* obj) :
            objectToAnimate(*obj)
        {
            rootPos = obj->getTranslation();
        }

        virtual void animateFloat(float value) override
        {
            if (auto* object = objectToAnimate.get())
            {
                auto newPos = rootPos + math::vec3(.0f, value, .0f);
                object->setTranslation(newPos);
            }
        }

        scene::ObjectWeakRef<scene::SceneObject> objectToAnimate;
        math::vec3 rootPos;
    };

    async::Task<scene::IScene::Ptr> loadGltfScene(const eastl::string& sceneAssetPath, bool setupCamera)
    {
        using namespace nau::scene;

        AssetRef<> sceneAssetRef{sceneAssetPath};

        SceneAsset::Ptr sceneAsset = co_await sceneAssetRef.getAssetViewTyped<SceneAsset>();

        IScene::Ptr scene = getServiceProvider().get<scene::ISceneFactory>().createSceneFromAsset(*sceneAsset);

        bool isScalarAnimAttached = false;

#ifdef USE_PHYSICS
        auto& physWorld = getServiceProvider().get<physics::IPhysicsWorld>();
        auto& physShapesFactory = getServiceProvider().get<physics::ICollisionShapesFactory>();
#endif

#ifdef USE_ANIMATION
        // todo: Must get rid of this: move to scene service
        scene->getRoot().addComponent<animation::AnimationManager>();
#endif

        for (SceneObject* const obj : scene->getRoot().getChildObjects(true))
        {
            if (auto* const staticMesh = obj->findFirstComponent<StaticMeshComponent>(); staticMesh)
            {
#ifdef USE_PHYSICS
                physics::PhysicsBodyCreationData creationData;
                float boundingRadius = 1.f;
                creationData.collisionShape = physShapesFactory.createSphereCollision(boundingRadius);
                creationData.position = obj->getWorldTransform().getMatrix().getTranslation();
                creationData.rotation = obj->getRotation();
                creationData.isStatic = false;
                creationData.mass = 1.f;

                if (auto body = physWorld.createBody(creationData))
                {
                    auto& physComp = obj->addComponent<physics::RigidBodyComponent>();
                    physComp.initialize(body);
                }
#endif

#ifdef USE_ANIMATION
                if (obj->getName().find("Teapot", 0) == 0)
                {
                    using namespace animation;
                    using namespace math;

                    auto& animComp = obj->addComponent<animation::AnimationComponent>();

                    if (!isScalarAnimAttached)
                    {
                        isScalarAnimAttached = true;

                        auto animation = rtti::createInstance<FloatAnimation>();

                        if (auto animationEditor = animation->createEditor())
                        {
                            animationEditor.addKeyFrame(0, .0f);
                            animationEditor.addKeyFrame(50, 12.f);

                            animationEditor.addFrameEvent(3, FrameEvent("one-time"));
                            animationEditor.addFrameEvent(10, FrameEvent("long-lasting", FrameEventType::Start));
                            animationEditor.addFrameEvent(20, FrameEvent("long-lasting", FrameEventType::Stop));

                            DataBlock block;
                            animationEditor.serialize(block);
                            animationEditor.deserialize(block);
                        }

                        auto animInstance = rtti::createInstance<AnimationInstance>("jump", animation);
                        animInstance->setPlayMode(PlayMode::PingPong);
                        animInstance->setInterpolationMethod(animation::AnimationInterpolationMethod::Step);
                        animComp.addAnimation(animInstance);

                        IAnimatable::Ptr heightSetTarget = rtti::createInstance<CustomHeightAnimTarget>(obj);

                        animComp.addCustomAnimationTarget(std::move(heightSetTarget), animInstance->getPlayer());
                    }
                    else
                    {
                        auto animation = rtti::createInstance<TransformAnimation>();

                        if (auto animationEditor = animation->createEditor())
                        {
                            animationEditor.addKeyFrame(0, Transform::identity());
                            animationEditor.addKeyFrame(100, Transform(quat::identity(), vec3(10.f, .0f, 10.0f), vec3(1.f, 1.f, 1.f)));
                            animationEditor.addKeyFrame(200, Transform(quat::identity(), vec3(.0f, .0f, 20.0f), vec3(1.f, 1.f, 1.f)));
                            animationEditor.addKeyFrame(300, Transform(quat::identity(), vec3(-10.f, .0f, 10.0f), vec3(1.f, 1.f, 1.f)));
                            animationEditor.addKeyFrame(400, Transform::identity());
                        }

                        auto animInstance = rtti::createInstance<AnimationInstance>("move", animation);
                        animInstance->setPlayMode(PlayMode::Looping);
                        animComp.addAnimation(animInstance);
                    }
                }
                else if (obj->getName().find("Torus", 0) == 0)
                {
                    using namespace animation;
                    using namespace math;

                    auto& animComp = obj->addComponent<animation::AnimationComponent>();

                    auto animation = rtti::createInstance<TransformAnimation>();

                    if (auto animationEditor = animation->createEditor())
                    {
                        animationEditor.addKeyFrame(0, Transform::identity());
                        animationEditor.addKeyFrame(100, Transform(quat::identity(), vec3(.0f, 3.0f, .0f), vec3(1.f, 1.f, 1.f)));
                    }

                    auto animInstance = rtti::createInstance<AnimationInstance>("up-down", animation);
                    animInstance->setPlayMode(PlayMode::PingPong);
                    animComp.addAnimation(animInstance);
                }
#endif
            }

            if (auto* const skeletonComponent = obj->findFirstComponent<SkeletonComponent>(); skeletonComponent)
            {
                // example for adding skeleton socket from code
                auto& sceneFactory = getServiceProvider().get<ISceneFactory>();
                scene::ObjectUniquePtr<SceneObject> skeletonSocket = sceneFactory.createSceneObject<SkeletonSocketComponent>();
                skeletonSocket->setName("SkeletonSocket_HEAD");

                auto& skeletonSocketComponent = skeletonSocket->getRootComponent<SkeletonSocketComponent>();
                skeletonSocketComponent.setBoneName("head.R");
                skeletonSocketComponent.setRelativeToBoneOffset(math::Transform(math::quat::identity(), math::Vector3(0, 0.2, 0)));

                obj->attachChild(std::move(skeletonSocket));
            }
        }

        co_return scene;
    }

    async::Task<scene::IScene::Ptr> loadDemoScene(eastl::string sceneAssetPath)
    {
        IAssetDescriptor::Ptr asset = getServiceProvider().get<IAssetManager>().openAsset(AssetPath{sceneAssetPath});
        SceneAsset::Ptr sceneAsset = co_await asset->getAssetViewTyped<SceneAsset>();

        co_return getServiceProvider().get<scene::ISceneFactory>().createSceneFromAsset(*sceneAsset);
    }

    async::Task<> sampleStoreAsset()
    {
        using namespace nau::io;

        IAssetContainerLoader* textureLoader = nullptr;

        for (auto* const loader : getServiceProvider().getAll<IAssetContainerLoader>())
        {
            eastl::vector<eastl::string_view> supportedAssetKind = loader->getSupportedAssetKind();
            if (std::find(supportedAssetKind.begin(), supportedAssetKind.end(), eastl::string_view{"png"}) != supportedAssetKind.end())
                textureLoader = loader;
            break;
        }

        NAU_ASSERT(textureLoader != nullptr);

        auto& fileSystem = getServiceProvider().get<IFileSystem>();
        auto file = fileSystem.openFile("/content/textures/white_8x8.png", AccessMode::Read, OpenFileMode::OpenExisting);

        IAssetContainer::Ptr originalAssetContainer = co_await textureLoader->loadFromStream(file->createStream(), {"png", "", textureLoader->getDefaultImportSettings()});
        NAU_ASSERT(originalAssetContainer);

        nau::Ptr<> asset = originalAssetContainer->getAsset();
        NAU_ASSERT(asset);

        IAssetContainerBuilder& builder = getServiceProvider().get<IAssetContainerBuilder>();

        IStreamWriter::Ptr stream = io::createNativeFileStream("white_8x8.dds", io::AccessMode::Write, io::OpenFileMode::CreateAlways);
        builder.writeAssetToStream(stream, asset).ignore();
    }

    async::Task<> startupDemo()
    {
        using namespace nau::async;
        using namespace nau::scene;
#if 1
        IScene::Ptr mainScene = co_await loadGltfScene("file:/content/scenes/scene_demo.gltf", true);
        IScene::Ptr helmetScene = co_await loadGltfScene("file:/content/scenes/damaged_helmet/DamagedHelmet.gltf", false);
        IScene::Ptr shadowScene = co_await loadGltfScene("file:/content/scenes/shadow_test.gltf", true);
        IScene::Ptr helmetTranslucentScene = co_await loadGltfScene("file:/content/scenes/damaged_helmet_translucent/DamagedHelmet_Translucent.gltf", false);

        IScene::Ptr skeletalAnimScene = co_await loadGltfScene("file:/content/scenes/robot/robot_skeletal_pbr.gltf", false);
#else
        auto scene = co_await loadDemoScene("file:/content/scenes/scene_demo.nscene_json");
#endif

        mainScene->setName("Main");

        auto& sceneManager = getServiceProvider().get<scene::ISceneManager>();
        auto& sceneFactory = getServiceProvider().get<scene::ISceneFactory>();

        {  // choose camera control kind: from scene object or standalone detached camera (from cam. manager):
            constexpr CamControlKind CameraControlKind = CamControlKind::UseCameraManager;

            auto scene_00 = sceneFactory.createEmptyScene();
            scene_00->setName("scene_00");

            ObjectWeakRef<SceneObject> cameraObject;
            if (CameraControlKind == CamControlKind::UseSceneObject)
            {
                cameraObject = scene_00->getRoot().attachChild(sceneFactory.createSceneObject<scene::CameraComponent>());
            }
            else
            {
                cameraObject = scene_00->getRoot().attachChild(sceneFactory.createSceneObject());
            }
            cameraObject->setName("Camera.Main");
            cameraObject->addComponent<CameraControl>().setCamControlKind(CameraControlKind);

            scene_00->getRoot().addComponent<SceneReloader>();
            scene_00->getRoot().addComponent<AssetReloader>();
            scene_00->getRoot().addComponent<MgpuToggle>();

            // Shadows on, so that the cascades (drawn by the second GPU in an Mgpu build, see MgpuToggle) are visible.
            // Direction: the default of the DirectionalLightControl window of WindowMaker, (0.5, -0.5, 0): the light
            // travels down. DirectionalLightComponent::getDirection() is the rotation applied to (1, 0, 0), so the
            // rotation turns (1, 0, 0) into that direction.
            // Do not build it with Matrix4::lookAtRH(Point3(), ...): Point3() does not initialize its value, the
            // random eye turned the light direction into garbage (even NaN) at every start and the cascades broke.
            auto& light = scene_00->getRoot().addComponent<DirectionalLightComponent>();
            light.setCastShadows(true);
            {
                using namespace nau::math;
                const Vector3 direction = normalize(Vector3(0.5f, -0.5f, 0.0f));
                light.setRotation(Quat::rotation(Vector3(1.0f, 0.0f, 0.0f), direction));
            }

            auto& env = scene_00->getRoot().addComponent<EnvironmentComponent>();
            env.setIntensity(0.5f);
            TextureAssetRef panoramaTex = TextureAssetRef("file:/content/textures/environment/SunnyHills_2k.hdr");
            env.setTextureAsset(panoramaTex);


            // Uncomment to add ability to create new system windows (from the UI)
            // scene_00->getRoot().addComponent<WindowMaker>();
            

            co_await sceneManager.activateScene(std::move(scene_00));
        }
        nau::async::TaskCollection sceneLoaders;

        sceneLoaders.push(sceneManager.activateScene(std::move(mainScene)));
        sceneLoaders.push(sceneManager.activateScene(std::move(shadowScene)));
        sceneLoaders.push(sceneManager.activateScene(std::move(helmetScene)));
        //sceneLoaders.push(sceneManager.activateScene(std::move(helmetTranslucentScene)));
        // todo: NAU-1295 add scene to scene when API is present
        sceneLoaders.push(getServiceProvider().get<scene::ISceneManager>().activateScene(std::move(skeletalAnimScene)));

        sceneLoaders.push(sceneManager.activateScene(sample::makeTransformDemoScene({30.f, 0, 0}, false)));
        sceneLoaders.push(sceneManager.activateScene(sample::makeTransformDemoScene({42.f, 0, 0}, false)));
        sceneLoaders.push(sceneManager.activateScene(sample::makeInstancingDemoScene({40.0f, 0, 40.0f}, false)));
        sceneLoaders.push(sceneManager.activateScene(sample::makeBillboardsScene({-10.0f, 0, 40.0f}, false)));

        co_await sceneLoaders.awaitCompletion();
    }

    // ./sample_app_delegate.cpp
    ApplicationDelegate::Ptr createSampleAppDelegate(Functor<async::Task<>()>&& startup);

}  // namespace nau::sample

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
    // TODO Tracy
    /*
    if (argc > 1 && argv[1] == std::string("tracy"))
    {
        while (!TracyIsConnected)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    */

    using namespace nau;

    ApplicationDelegate::Ptr sampleAppDelegate = sample::createSampleAppDelegate(sample::startupDemo);

    return nau::runApplication(std::move(sampleAppDelegate));
}
