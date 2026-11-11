// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/scene/camera/camera_manager.h"
#include "nau/scene/components/camera_component.h"
#include "scene_test_base.h"

namespace nau::test
{

    class TestCameraManager : public SceneTestBase
    {
    protected:
        static scene::ICameraManager& getCameraManager()
        {
            return getServiceProvider().get<scene::ICameraManager>();
        }

        static void checkCameras(eastl::span<scene::ICameraControl*> expectedCameras)
        {
            using namespace nau::scene;

            constexpr size_t RepeatCount = 2;

            // multiple repeats, to check that getCameras() returns same state
            for (size_t i = 0; i < RepeatCount; ++i)
            {
                auto snapshot = getCameraManager().getCameras();
                for (scene::ICameraControl* expectedCamera : expectedCameras)
                {
                    auto camera = snapshot.getCameraByUid(expectedCamera->getCameraUid());
                    ASSERT_TRUE(camera) << "Camera with specified uid not found";
                    ASSERT_EQ(camera->getCameraUid(), expectedCamera->getCameraUid());
                    ASSERT_EQ(camera->getWorldUid(), expectedCamera->getWorldUid());
                    ASSERT_TRUE(camera->getTranslation().similar(expectedCamera->getTranslation()));
                    ASSERT_EQ(camera->getFov(), expectedCamera->getFov());
                    ASSERT_EQ(camera->getClipNearPlane(), expectedCamera->getClipNearPlane());
                    ASSERT_EQ(camera->getClipFarPlane(), expectedCamera->getClipFarPlane());
                }
            }
        }

        static void checkCamera(scene::ICameraControl* expectedCamera)
        {
            std::array cameras = {expectedCamera};
            checkCameras(cameras);
        }
    };

    /**
        Test:
            Checks that there is no any camera by default
     */
    TEST_F(TestCameraManager, NoCamerasByDefault)
    {
        ASSERT_TRUE(getCameraManager().getCameras().isEmpty());
    }

    /**
        Test:
           - create detached camera.
           - checks that camera parameters obtained from getCamera() corresponds to the created camera
     */
    TEST_F(TestCameraManager, CreateDetachedCamera)
    {
        auto camera = getCameraManager().createDetachedCamera();
        ASSERT_TRUE(camera);
        ASSERT_EQ(getSceneManager().getDefaultWorld().getUid(), camera->getWorldUid());

        camera->setFov(40.f);
        camera->setTranslation({10.f, 11.f, 12.f});
        camera->setClipNearPlane(1.f);
        camera->setClipFarPlane(50.f);

        EXPECT_NO_FATAL_FAILURE(checkCamera(camera.get()));
    }

    /**
        Test:
            - create new Detached Camera
            - check that the new camera has been added
            - remove Detached Camera
            - check that the camera's properties associated with the detached camera has been removed
    */
    TEST_F(TestCameraManager, DeleteDetachedCamera)
    {
        auto camera = getCameraManager().createDetachedCamera();
        ASSERT_TRUE(camera);
        ASSERT_EQ(getSceneManager().getDefaultWorld().getUid(), camera->getWorldUid());

        auto snapshot = getCameraManager().getCameras();
        auto cameras = snapshot.getWorldAllCameras();
        ASSERT_EQ(cameras.size(), 1);

        camera.reset();
        ASSERT_TRUE(getCameraManager().getCameras().getWorldAllCameras().empty());
    }

    /**
        Test:
            - create new SceneObject with CameraComponent
            - check that the new camera (camera associated with the scene object) has been added
    */
    TEST_F(TestCameraManager, SceneCamera)
    {
        using namespace testing;
        using namespace nau::async;
        using namespace nau::scene;

        const AssertionResult testResult = runTestApp([]() -> Task<AssertionResult>
        {
            ObjectWeakRef sceneRef = co_await getSceneManager().activateScene(createEmptyScene());
            ObjectWeakRef objectRef = co_await sceneRef->getRoot().attachChildAsync(createObject<CameraComponent>());
            CameraComponent& camera = objectRef->getRootComponent<CameraComponent>();

            camera.setFov(40.f);
            camera.setTranslation({10.f, 11.f, 12.f});
            camera.setClipNearPlane(1.f);
            camera.setClipFarPlane(50.f);

            // loop: check that subsequent calls returns same state
            for (size_t i = 0; i < 2; ++i)
            {
                auto snapshot = getCameraManager().getCameras();
                auto cameras = snapshot.getWorldAllCameras();

                ASSERT_ASYNC(cameras.size() == 1);
                ASSERT_ASYNC(cameras.front()->getCameraUid() == camera.getCameraUid());
                ASSERT_ASYNC(cameras.front()->getWorldUid() == camera.getWorldUid());
                ASSERT_ASYNC(cameras.front()->getTranslation().similar(camera.getTranslation()));
                ASSERT_ASYNC(cameras.front()->getFov() == camera.getFov());
                ASSERT_ASYNC(cameras.front()->getClipNearPlane() == camera.getClipNearPlane());
                ASSERT_ASYNC(cameras.front()->getClipFarPlane() == camera.getClipFarPlane());
            }

            co_return AssertionSuccess();
        });

        ASSERT_TRUE(testResult);
    }

    TEST_F(TestCameraManager, SceneCameraSnapshotTracksParentTransform)
    {
        using namespace testing;
        using namespace nau::async;
        using namespace nau::scene;

        const AssertionResult result = runTestApp([]() -> Task<AssertionResult>
        {
            ObjectWeakRef scene = co_await getSceneManager().activateScene(createEmptyScene());
            ObjectWeakRef parent = co_await scene->getRoot().attachChildAsync(createObject());
            ObjectWeakRef child = co_await parent->attachChildAsync(createObject<CameraComponent>());
            CameraComponent& camera = child->getRootComponent<CameraComponent>();
            camera.setTranslation({1.f, 2.f, 3.f});
            ICameraManager::CamerasSnapshot snapshot;
            for (float offset : {10.f, 30.f})
            {
                parent->getRootComponent().setTranslation({offset, 0.f, 0.f});
                camera.setFov(40.f + offset);
                getCameraManager().syncCameras(snapshot);
                const auto properties = snapshot.getCameraByUid(camera.getCameraUid());
                ASSERT_ASYNC(properties);
                ASSERT_ASYNC(properties->getTransform().similar(camera.getTransform()));
                ASSERT_ASYNC(properties->getWorldTransform().getTranslation().similar({offset + 1.f, 2.f, 3.f}));
                ASSERT_ASYNC(properties->getFov() == camera.getFov());
            }
            co_return AssertionSuccess();
        });
        ASSERT_TRUE(result);
    }

    /**
        Test:
            - create the new World
            - create new SceneObject with CameraComponent (within custom world)
            - check that the new camera (camera associated with the scene object) has been added
            - check that the new camera has correct worldUid (same as newly created world object)
    */
    TEST_F(TestCameraManager, World_SceneCamera)
    {
        using namespace testing;
        using namespace nau::async;
        using namespace nau::scene;

        const AssertionResult testResult = runTestApp([]() -> Task<AssertionResult>
        {
            ObjectWeakRef newWorld = getSceneManager().createWorld();

            ObjectWeakRef sceneRef = co_await newWorld->addScene(createEmptyScene());
            ObjectWeakRef objectRef = co_await sceneRef->getRoot().attachChildAsync(createObject<CameraComponent>());
            CameraComponent& camera = objectRef->getRootComponent<CameraComponent>();

            camera.setFov(40.f);
            camera.setTranslation({10.f, 11.f, 12.f});
            camera.setClipNearPlane(1.f);
            camera.setClipFarPlane(50.f);

            // loop: check that subsequent calls returns same state
            for (size_t i = 0; i < 2; ++i)
            {
                auto snapshot = getCameraManager().getCameras();
                auto cameras = snapshot.getWorldAllCameras(newWorld->getUid());

                ASSERT_ASYNC(cameras.size() == 1);
                ASSERT_ASYNC(cameras.front()->getWorldUid() == newWorld->getUid());
                ASSERT_ASYNC(cameras.front()->getCameraUid() == camera.getCameraUid());
                ASSERT_ASYNC(cameras.front()->getWorldUid() == camera.getWorldUid());
                ASSERT_ASYNC(cameras.front()->getTranslation().similar(camera.getTranslation()));
                ASSERT_ASYNC(cameras.front()->getFov() == camera.getFov());
                ASSERT_ASYNC(cameras.front()->getClipNearPlane() == camera.getClipNearPlane());
                ASSERT_ASYNC(cameras.front()->getClipFarPlane() == camera.getClipFarPlane());
            }

            co_return AssertionSuccess();
        });

        ASSERT_TRUE(testResult);
    }

    /**
        Test:
            - create new SceneObject with CameraComponent
            - check that the new camera has been added
            - remove SceneObject (with camera)
            - check that the camera associated with the scene object has been removed
     */
    TEST_F(TestCameraManager, DeleteSceneCamera)
    {
        using namespace testing;
        using namespace nau::async;
        using namespace nau::scene;

        const AssertionResult testResult = runTestApp([]() -> Task<AssertionResult>
        {
            ObjectWeakRef sceneRef = co_await getSceneManager().activateScene(createEmptyScene());
            ObjectWeakRef objectRef = co_await sceneRef->getRoot().attachChildAsync(createObject<CameraComponent>());
            CameraComponent& camera = objectRef->getRootComponent<CameraComponent>();

            {
                auto snapshot = getCameraManager().getCameras();

                auto cameras = snapshot.getWorldAllCameras();
                ASSERT_ASYNC(cameras.size() == 1);
                ASSERT_ASYNC(cameras.front()->getCameraUid() == camera.getCameraUid());
            }

            sceneRef->getRoot().removeChild(objectRef);

            ASSERT_FALSE_ASYNC(objectRef);

            auto cameras = getCameraManager().getCameras();
            ASSERT_ASYNC(cameras.isEmpty());

            co_return AssertionSuccess();
        });

        ASSERT_TRUE(testResult);
    }

    /**
        Test:
            Checks ICameraManager::syncCameras()
     */
    TEST_F(TestCameraManager, SyncCameras_1)
    {
        using namespace nau::scene;

        ICameraManager::CamerasSnapshot snapshot;
        ICameraManager& manager = getCameraManager();

        const auto syncCameras = [&snapshot, &manager]() -> eastl::pair<size_t, size_t>
        {
            size_t addedCount = 0;
            size_t removedCount = 0;
            manager.syncCameras(snapshot, [&addedCount]([[maybe_unused]] const ICameraProperties& cam)
            {
                ++addedCount;
            }, [&removedCount]([[maybe_unused]] const ICameraProperties& cam)
            {
                ++removedCount;
            });

            return {addedCount, removedCount};
        };

        auto camera_0 = manager.createDetachedCamera();
        auto camera_1 = manager.createDetachedCamera();
        auto camera_2 = manager.createDetachedCamera();

        {
            // expect that syncCameras will append new cameras
            const auto [addedCount, removedCount] = syncCameras();
            ASSERT_EQ(addedCount, 3);
            ASSERT_EQ(removedCount, 0);
            ASSERT_EQ(snapshot.getWorldAllCameras().size(), 3);

            eastl::array cameras{camera_0.get(), camera_1.get(), camera_2.get()};
            EXPECT_NO_FATAL_FAILURE(checkCameras(cameras));
        }

        // replace camera_1 with the new one
        camera_1 = manager.createDetachedCamera();

        {
            // expect that syncCameras will append new camera and remove the deleted one
            const auto [addedCount, removedCount] = syncCameras();
            ASSERT_EQ(addedCount, 1);
            ASSERT_EQ(removedCount, 1);
            ASSERT_EQ(snapshot.getWorldAllCameras().size(), 3);

            eastl::array cameras{camera_0.get(), camera_1.get(), camera_2.get()};
            EXPECT_NO_FATAL_FAILURE(checkCameras(cameras));
        }

        camera_0.reset();
        camera_2.reset();
        {
            const auto [addedCount, removedCount] = syncCameras();
            ASSERT_EQ(addedCount, 0);
            ASSERT_EQ(removedCount, 2);
            ASSERT_EQ(snapshot.getWorldAllCameras().size(), 1);

            eastl::array cameras{camera_1.get()};
            EXPECT_NO_FATAL_FAILURE(checkCameras(cameras));
        }
    }

    TEST_F(TestCameraManager, HasNoMainCameraByDefault)
    {
        scene::ICameraManager& camManager = getCameraManager();
        auto snapshot = camManager.getCameras();

        ASSERT_FALSE(snapshot.getWorldMainCamera());
        ASSERT_FALSE(snapshot.getWorldExplicitMainCamera());
    }

    TEST_F(TestCameraManager, MainCameraByDefault)
    {
        scene::ICameraManager& camManager = getCameraManager();
        auto detachedCamera1 = camManager.createDetachedCamera();
        auto detachedCamera2 = camManager.createDetachedCamera();

        auto snapshot = camManager.getCameras();
        ASSERT_TRUE(snapshot.getWorldMainCamera());
        ASSERT_FALSE(snapshot.getWorldExplicitMainCamera());
    }

    TEST_F(TestCameraManager, SetDetachedMainCamera)
    {
        scene::ICameraManager& camManager = getCameraManager();
        auto detachedCamera1 = camManager.createDetachedCamera();
        auto detachedCamera2 = camManager.createDetachedCamera();

        camManager.setMainCamera(*detachedCamera1);

        auto snapshot = camManager.getCameras();

        {
            auto mainCamera = snapshot.getWorldMainCamera();
            ASSERT_TRUE(mainCamera);
            ASSERT_EQ(mainCamera->getCameraUid(), detachedCamera1->getCameraUid());
            ASSERT_EQ(snapshot.getWorldExplicitMainCamera()->getCameraUid(), detachedCamera1->getCameraUid());
        }

        camManager.setMainCamera(*detachedCamera2);
        camManager.syncCameras(snapshot);

        {
            auto mainCamera = snapshot.getWorldMainCamera();
            ASSERT_TRUE(mainCamera);
            ASSERT_EQ(mainCamera->getCameraUid(), detachedCamera2->getCameraUid());
            ASSERT_EQ(snapshot.getWorldExplicitMainCamera()->getCameraUid(), detachedCamera2->getCameraUid());
        }
    }

    /**
    */
    TEST_F(TestCameraManager, DeleteMainDetachedCamera)
    {
        scene::ICameraManager& camManager = getCameraManager();
        auto detachedCamera1 = camManager.createDetachedCamera();
        auto detachedCamera2 = camManager.createDetachedCamera();

        camManager.setMainCamera(*detachedCamera2);
        auto snapshot = camManager.getCameras();

        {
            auto mainCamera = snapshot.getWorldMainCamera();
            ASSERT_TRUE(mainCamera);
            ASSERT_EQ(mainCamera->getCameraUid(), detachedCamera2->getCameraUid());
            ASSERT_EQ(snapshot.getWorldExplicitMainCamera()->getCameraUid(), detachedCamera2->getCameraUid());
        }

        detachedCamera2.reset();
        camManager.syncCameras(snapshot);

        {
            auto mainCamera = snapshot.getWorldMainCamera();
            ASSERT_TRUE(mainCamera);
            ASSERT_EQ(mainCamera->getCameraUid(), detachedCamera1->getCameraUid());
            ASSERT_FALSE(snapshot.getWorldExplicitMainCamera());
        }
    }

    /**
    */
    TEST_F(TestCameraManager, DeleteMainSceneCamera)
    {
        using namespace nau::async;
        using namespace nau::scene;
        using namespace ::testing;

        const AssertionResult testResult = runTestApp([]() -> Task<AssertionResult>
        {
            ICameraManager& camManager = getCameraManager();

            ICameraManager::CamerasSnapshot snapshot;

            auto detachedCamera1 = camManager.createDetachedCamera();
            auto detachedCamera2 = camManager.createDetachedCamera();

            ObjectWeakRef sceneRef = co_await getSceneManager().activateScene(createEmptyScene());

            CameraComponent& camera1 = sceneRef->getRoot().addComponent<CameraComponent>();
            [[maybe_unused]] CameraComponent& camera2 = sceneRef->getRoot().addComponent<CameraComponent>();

            camManager.setMainCamera(camera1);
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldExplicitMainCamera()->getCameraUid() == camera1.getCameraUid());

            camera1.getParentObject().removeComponent(camera1);

            camManager.syncCameras(snapshot);
            ASSERT_FALSE_ASYNC(snapshot.getWorldExplicitMainCamera());
            ASSERT_ASYNC(snapshot.getWorldMainCamera());
        
            co_return AssertionSuccess();
        });

        ASSERT_TRUE(testResult);
    }

    /**
    */
    TEST_F(TestCameraManager, ResetMainCamera)
    {
        scene::ICameraManager& camManager = getCameraManager();
        auto detachedCamera1 = camManager.createDetachedCamera();
        auto detachedCamera2 = camManager.createDetachedCamera();

        camManager.setMainCamera(*detachedCamera1);

        auto snapshot = camManager.getCameras();

        {
            auto mainCamera = snapshot.getWorldMainCamera();
            ASSERT_TRUE(mainCamera);
            ASSERT_EQ(mainCamera->getCameraUid(), detachedCamera1->getCameraUid());
            ASSERT_EQ(snapshot.getWorldExplicitMainCamera()->getCameraUid(), detachedCamera1->getCameraUid());
        }

        camManager.resetWorldMainCamera();
        camManager.syncCameras(snapshot);

        ASSERT_TRUE(snapshot.getWorldMainCamera());
        ASSERT_FALSE(snapshot.getWorldExplicitMainCamera());
    }

    TEST_F(TestCameraManager, SetMainCamera)
    {
        using namespace nau::async;
        using namespace nau::scene;
        using namespace ::testing;

        const AssertionResult testResult = runTestApp([]() -> Task<AssertionResult>
        {
            ICameraManager& camManager = getCameraManager();

            ICameraManager::CamerasSnapshot snapshot;

            auto detachedCamera1 = camManager.createDetachedCamera();
            auto detachedCamera2 = camManager.createDetachedCamera();

            ObjectWeakRef sceneRef = co_await getSceneManager().activateScene(createEmptyScene());

            CameraComponent& camera1 = sceneRef->getRoot().attachChild(createObject<CameraComponent>()).getRootComponent<CameraComponent>();
            CameraComponent& camera2 = sceneRef->getRoot().attachChild(createObject<CameraComponent>()).getRootComponent<CameraComponent>();

            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldAllCameras().size() == 4);
            ASSERT_ASYNC(snapshot.getWorldMainCamera());
            ASSERT_FALSE_ASYNC(snapshot.getWorldExplicitMainCamera());

            camManager.setMainCamera(camera1);
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldMainCamera()->getCameraUid() == camera1.getCameraUid());
            ASSERT_ASYNC(snapshot.getWorldExplicitMainCamera()->getCameraUid() == camera1.getCameraUid());

            camManager.setMainCamera(camera2);
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldMainCamera()->getCameraUid() == camera2.getCameraUid());
            ASSERT_ASYNC(snapshot.getWorldExplicitMainCamera()->getCameraUid() == camera2.getCameraUid());

            camManager.setMainCamera(*detachedCamera1);
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldMainCamera()->getCameraUid() == detachedCamera1->getCameraUid());
            ASSERT_ASYNC(snapshot.getWorldExplicitMainCamera()->getCameraUid() == detachedCamera1->getCameraUid());

            camManager.setMainCamera(*detachedCamera2);
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldMainCamera()->getCameraUid() == detachedCamera2->getCameraUid());
            ASSERT_ASYNC(snapshot.getWorldExplicitMainCamera()->getCameraUid() == detachedCamera2->getCameraUid());

            camManager.resetWorldMainCamera();
            camManager.syncCameras(snapshot);
            ASSERT_ASYNC(snapshot.getWorldMainCamera());
            ASSERT_ASYNC(!snapshot.getWorldExplicitMainCamera());

            co_return AssertionSuccess();
        });

        ASSERT_TRUE(testResult);
    }

}  // namespace nau::test
