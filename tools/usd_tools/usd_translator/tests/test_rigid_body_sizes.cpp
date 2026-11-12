// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <pxr/base/plug/registry.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/tf/errorMark.h>
#include <pxr/usd/sdf/fileFormat.h>
#include <pxr/usd/usdGeom/xform.h>

#include "nau/app/application_services.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/diag/device_error.h"
#include "nau/module/module_manager.h"
#include "nau/physics/components/rigid_body_component.h"
#include "nau/physics/physics_body.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include "nau/scene/scene_factory.h"
#include "nau/scene/scene_manager.h"
#include "nau/service/service_provider.h"
#include "physics_world_state.h"
#include "usd_translator/usd_stage_translator.h"

namespace UsdTranslator::test
{
    using namespace nau;
    using namespace nau::physics;
    using namespace nau::scene;

    namespace
    {
        class TestDeviceError final : public diag::IDeviceError
        {
            NAU_RTTI_CLASS(TestDeviceError, diag::IDeviceError)

            diag::FailureActionFlag handleFailure(const diag::FailureData& failure) override
            {
                std::fprintf(stderr, "Engine assertion: %.*s:%u %.*s %.*s\n",
                    int(failure.source.filePath.size()), failure.source.filePath.data(), failure.source.line.value_or(0),
                    int(failure.condition.size()), failure.condition.data(), int(failure.message.size()), failure.message.data());
                std::exit(EXIT_FAILURE);
            }
        };

        // Ray queries also emit debug lines in Debug builds. No GPU is needed to
        // exercise the real Jolt shapes and the production scene synchronizer.
        class NullDebugRenderer final : public DebugRenderSystem
        {
            NAU_RTTI_CLASS(NullDebugRenderer, DebugRenderSystem)

            void draw(const math::Matrix4&, float) override {}
            void clear() override {}
            void drawLine(const math::Point3&, const math::Point3&, const math::Color4&, float) override {}
            void drawBoundingBox(const math::BBox3&, const math::Color4&, float) override {}
            void drawBoundingBox(const math::BBox3&, const math::Matrix4&, const math::Color4&, float) override {}
            void drawArrow(const math::Point3&, const math::Point3&, const math::Color4&, const math::Vector3&, float) override {}
            void drawPoint(const math::Point3&, const float&, float) override {}
            void drawCircle(const double&, const math::Color4&, const math::Matrix4&, int, float) override {}
            void drawSphere(const double&, const math::Color4&, const math::Matrix4&, int, float) override {}
            void drawPlane(const math::Vector4&, const math::Color4&, float, float, bool, float) override {}
            void drawFrustrum(const math::Matrix4&, const math::Matrix4&, float) override {}
            void drawStaticMesh(const StaticMesh&, const math::Matrix4&, const math::Color4&, float) override {}
        };

        class CountingShapeFactory final : public ICollisionShapesFactory
        {
            NAU_CLASS(CountingShapeFactory, rtti::RCPolicy::Concurrent, ICollisionShapesFactory)

        public:
            explicit CountingShapeFactory(Ptr<ICollisionShapesFactory> delegate) : m_delegate(std::move(delegate)) {}

            ICollisionShape::Ptr createGenericCollisionShape(const rtti::TypeInfo& type,
                const RuntimeValue::Ptr& data, IPhysicsMaterial* material) const override
            {
                ++shapeCreations;
                return m_delegate->createGenericCollisionShape(type, data, material);
            }

            IConvexHullCollision::Ptr createConvexHullCollision(const IConvexHullCollision::ConstructionData& data,
                IPhysicsMaterial* material) const override
            {
                return m_delegate->createConvexHullCollision(data, material);
            }

            IMeshCollision::Ptr createMeshCollision(const IMeshCollision::ConstructionData& data) const override
            {
                return m_delegate->createMeshCollision(data);
            }

            ICollisionShape::Ptr createMeshCollisionFromAsset(Ptr<TriMeshAssetView> asset) const override
            {
                return m_delegate->createMeshCollisionFromAsset(std::move(asset));
            }

            ICollisionShape::Ptr createConvexHullCollisionFromAsset(Ptr<ConvexHullAssetView> asset) const override
            {
                return m_delegate->createConvexHullCollisionFromAsset(std::move(asset));
            }

            mutable unsigned shapeCreations = 0;

        private:
            Ptr<ICollisionShapesFactory> m_delegate;
        };

        std::string pathUtf8(const std::filesystem::path& path)
        {
            const auto utf8 = path.u8string();
            return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
        }

        void require(bool condition, const char* message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        }
    }

    class RigidBodySizes : public testing::Test
    {
    protected:
        void SetUp() override
        {
            pxr::PlugRegistry::GetInstance().RegisterPlugins(NAU_TEST_USD_PLUGINS);
            m_app = createApplication([this]() -> Result<>
            {
                diag::setDeviceError(eastl::make_unique<TestDeviceError>());
                // PhysicsService is intentionally not started. Its production
                // PhysicsWorldState is advanced synchronously by this fixture.
                NauCheckResult(loadModulesList(NAU_MODULES_LIST));
                getServiceProvider().addClass<RigidBodyComponent>();
                getServiceProvider().addService(eastl::make_unique<NullDebugRenderer>());
                const auto factories = getServiceProvider().findClasses<ICollisionShapesFactory>();
                require(factories.size() == 1, "Jolt collision factory missing");
                Ptr<ICollisionShapesFactory> factory = factories.front()->getConstructor()->invokeToPtr(nullptr, {});
                m_factory = rtti::createInstance<CountingShapeFactory>(std::move(factory));
                getServiceProvider().addService(m_factory);
                return ResultSuccess;
            });
            m_app->startupOnCurrentThread();
            m_stage = pxr::UsdStage::CreateInMemory();
        }

        void TearDown() override
        {
            closeScene();
            m_factory = nullptr;
            m_stage.Reset();
            if (m_app)
            {
                m_app->stop();
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
                while (m_app->step())
                {
                    require(std::chrono::steady_clock::now() < deadline, "application shutdown timed out");
                    std::this_thread::yield();
                }
                m_app.reset();
            }
            if (!m_savedPath.empty())
            {
                std::filesystem::remove(m_savedPath);
            }
        }

        template <typename T>
        void wait(async::Task<T>& task)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
            while (!task.isReady())
            {
                require(std::chrono::steady_clock::now() < deadline, "scene operation timed out");
                require(m_app->step(), "application stopped during scene operation");
                std::this_thread::yield();
            }
            require(!task.isRejected(), "scene operation rejected");
        }

        pxr::UsdPrim addBody(const char* owner, const char* type, pxr::GfVec3d position = pxr::GfVec3d(0))
        {
            const pxr::SdfPath ownerPath(std::string("/") + owner);
            auto xform = pxr::UsdGeomXform::Define(m_stage, ownerPath);
            xform.AddTranslateOp().Set(position);
            auto prim = m_stage->DefinePrim(ownerPath.AppendChild(pxr::TfToken("Collider")), pxr::TfToken(type));
            require(prim.GetAttribute(pxr::TfToken("isStatic")).Set(true), "physics schema must be registered");
            return prim;
        }

        void openScene()
        {
            auto& manager = getServiceProvider().get<ISceneManager>();
            manager.getDefaultWorld().setSimulationPause(true);
            auto source = getServiceProvider().get<ISceneFactory>().createEmptyScene();
            m_translator = std::make_unique<StageTranslator>();
            m_translator->setSource(m_stage).setTarget(*source);
            auto translated = m_translator->initScene();
            wait(translated);
            auto active = manager.activateScene(std::move(source));
            wait(active);
            m_scene = *active;
            m_physics = std::make_unique<PhysicsWorldState>(manager.getDefaultWorld().getUid());
            m_physics->getPhysicsWorld()->setGravity(math::vec3::zero());
            eastl::vector<const Component*> components;
            for (auto* component : m_scene->getRoot().getAllComponents<RigidBodyComponent>())
            {
                components.push_back(component);
            }
            auto activated = m_physics->activateComponents(components);
            wait(activated);
            synchronize();
            m_translator->follow();
        }

        void closeScene()
        {
            m_translator.reset();
            m_physics.reset();
            if (m_scene)
            {
                getServiceProvider().get<ISceneManager>().deactivateScene(m_scene);
                for (unsigned i = 0; i < 8; ++i)
                {
                    require(m_app->step(), "application stopped during scene deactivation");
                }
                m_scene = nullptr;
            }
        }

        void synchronize()
        {
            // USD notices run the actual adapters through StageTranslator::follow().
            for (unsigned i = 0; i < 8; ++i)
            {
                require(m_app->step(), "application stopped during USD update");
            }
            require(m_physics->syncSceneState(getServiceProvider().get<ISceneManager>()), "physics world disappeared");
        }

        template <typename T>
        void edit(const pxr::UsdPrim& prim, const char* name, const T& value)
        {
            require(prim.GetAttribute(pxr::TfToken(name)).Set(value), "failed to author collider size");
            synchronize();
        }

        RigidBodyComponent& component(const char* owner)
        {
            auto adapter = m_translator->getRootAdapter()->getChild(pxr::TfToken(owner));
            require(bool(adapter), "translated owner missing");
            auto* result = adapter->getSceneObject()->findFirstComponent<RigidBodyComponent>();
            require(result != nullptr, "rigid body component missing");
            return *result;
        }

        void expectBounds(const char* owner, math::vec3 halfSize, CollisionChannel channel = 0)
        {
            auto& body = component(owner);
            const auto center = body.getParentObject().getWorldTransform().getTranslation();
            for (unsigned axis = 0; axis < 3; ++axis)
            {
                for (float sign : {-1.f, 1.f})
                {
                    math::vec3 direction = math::vec3::zero();
                    direction.setElem(axis, -sign);
                    const auto origin = center - direction * 10.f;
                    RayCastQuery query{.origin = origin, .direction = direction, .maxDistance = 12.f, .reactChannels = {channel}};
                    auto hit = m_physics->getPhysicsWorld()->castRay(query);
                    ASSERT_TRUE(hit) << owner << " axis=" << axis << " sign=" << sign;
                    EXPECT_EQ(hit->sceneObjectUid, body.getParentObject().getUid());
                    ASSERT_TRUE(hit->rigidBody);
                    EXPECT_EQ(hit->rigidBody->getUid(), body.getUid());
                    EXPECT_NEAR(hit->position.getElem(axis), center.getElem(axis) + sign * halfSize.getElem(axis), 0.001f)
                        << owner << " axis=" << axis << " sign=" << sign;
                }
            }
        }

        void checkNativeSceneSave()
        {
            const auto plugin = pxr::PlugRegistry::GetInstance().GetPluginWithName("NauUsdFormat");
            ASSERT_TRUE(plugin);
            ASSERT_TRUE(plugin->Load());
            ASSERT_TRUE(pxr::SdfFileFormat::FindByExtension("nausd_scene"));

            const auto unique = std::chrono::steady_clock::now().time_since_epoch().count();
            m_savedPath = std::filesystem::temp_directory_path() /
                ("nau-native-collider-save-" + std::to_string(unique) + ".nausd_scene");
            ASSERT_TRUE(std::filesystem::copy_file(std::filesystem::u8path(NAU_TEST_NATIVE_SCENE), m_savedPath));
            RecordProperty("native_scene_path", pathUtf8(m_savedPath));
            m_stage = pxr::UsdStage::Open(pathUtf8(m_savedPath));
            ASSERT_TRUE(m_stage);
            EXPECT_EQ(m_stage->GetRootLayer()->GetFileFormat()->GetFormatId(), pxr::TfToken("nausd_scene"));
            auto sphere = m_stage->GetPrimAtPath(pxr::SdfPath("/Sphere/Collider"));
            auto cylinder = m_stage->GetPrimAtPath(pxr::SdfPath("/Cylinder/Collider"));
            auto cube = m_stage->GetPrimAtPath(pxr::SdfPath("/Cube/Collider"));
            openScene();
            expectBounds("Sphere", {1, 1, 1});
            expectBounds("Cylinder", {1, 1, 1});
            expectBounds("Cube", {1, 1, 1});
            edit(sphere, "radius", 2.25);
            edit(cylinder, "height", 5.5);
            edit(cylinder, "radius", 1.75);
            edit(cube, "extent", pxr::GfVec3d(1.25, 2.5, 3.75));
            ASSERT_TRUE(m_stage->GetRootLayer()->IsDirty());

            pxr::TfErrorMark saveErrors;
            m_stage->Save();
            const bool saved = saveErrors.IsClean();
            for (const auto& error : saveErrors)
            {
                ADD_FAILURE() << error.GetCommentary();
            }
            saveErrors.Clear();
            ASSERT_TRUE(saved) << "Save failed for " << pathUtf8(m_savedPath);
            ASSERT_FALSE(m_stage->GetRootLayer()->IsDirty());

            pxr::SdfLayerHandle oldLayer = m_stage->GetRootLayer();
            closeScene();
            sphere = {};
            cylinder = {};
            cube = {};
            m_stage.Reset();
            // A still-live layer would let Open reuse memory and hide a failed save.
            ASSERT_FALSE(oldLayer);
            m_stage = pxr::UsdStage::Open(pathUtf8(m_savedPath));
            ASSERT_TRUE(m_stage);
            openScene();
            expectBounds("Sphere", {2.25, 2.25, 2.25});
            expectBounds("Cylinder", {1.75, 2.75, 1.75});
            expectBounds("Cube", {1.25, 2.5, 3.75});
        }

        eastl::unique_ptr<Application> m_app;
        pxr::UsdStageRefPtr m_stage;
        std::unique_ptr<StageTranslator> m_translator;
        IScene::WeakRef m_scene;
        std::unique_ptr<PhysicsWorldState> m_physics;
        Ptr<CountingShapeFactory> m_factory;
        std::filesystem::path m_savedPath;
    };

    TEST_F(RigidBodySizes, SphereRadiusUpdatesExistingBody)
    {
        auto sphere = addBody("Sphere", "RigidBodySphere");
        addBody("Neighbour", "RigidBodyCube", {20, 0, 0});
        openScene();
        const Uid uid = component("Sphere").getUid();
        expectBounds("Sphere", {1, 1, 1});
        for (double radius : {2.5, 0.75, 1.5, 0.25})
        {
            edit(sphere, "radius", radius);
            expectBounds("Sphere", math::vec3(static_cast<float>(radius)));
            expectBounds("Neighbour", {1, 1, 1});
            EXPECT_EQ(component("Sphere").getUid(), uid);
            EXPECT_EQ(component("Sphere").getCollisions().spheres.size(), 1);
        }
    }

    TEST_F(RigidBodySizes, CylinderHeightAndRadiusUpdateIndependently)
    {
        auto cylinder = addBody("Cylinder", "RigidBodyCylinder");
        openScene();
        expectBounds("Cylinder", {1, 0.5, 1});
        edit(cylinder, "height", 6.0);
        expectBounds("Cylinder", {1, 3, 1});
        edit(cylinder, "radius", 2.0);
        expectBounds("Cylinder", {2, 3, 2});
        edit(cylinder, "height", 2.0);
        expectBounds("Cylinder", {2, 1, 2});
        EXPECT_EQ(component("Cylinder").getCollisions().cylinders.size(), 1);
        EXPECT_TRUE(component("Cylinder").getCollisions().capsules.empty());
        // A capsule would still be hit above the cylinder's flat cap.
        const RayCastQuery aboveCap{.origin = {0, 1.25f, 5}, .direction = {0, 0, -1}, .maxDistance = 10};
        EXPECT_FALSE(m_physics->getPhysicsWorld()->castRay(aboveCap));
    }

    TEST_F(RigidBodySizes, CubeExtentUpdatesEachAxisIndependently)
    {
        auto cube = addBody("Cube", "RigidBodyCube");
        openScene();
        edit(cube, "extent", pxr::GfVec3d(2, 1, 1));
        expectBounds("Cube", {2, 1, 1});
        edit(cube, "extent", pxr::GfVec3d(2, 3, 1));
        expectBounds("Cube", {2, 3, 1});
        edit(cube, "extent", pxr::GfVec3d(2, 3, 4));
        expectBounds("Cube", {2, 3, 4});
        edit(cube, "extent", pxr::GfVec3d(0.5, 1.25, 2.25));
        expectBounds("Cube", {0.5, 1.25, 2.25});
        EXPECT_EQ(component("Cube").getCollisions().boxes.size(), 1);
    }

    TEST_F(RigidBodySizes, NativeSceneSavePersistsSizesAfterCompleteReopen)
    {
        checkNativeSceneSave();
    }

    TEST_F(RigidBodySizes, ResizePreservesSettingsAndMotion)
    {
        auto sphere = addBody("Sphere", "RigidBodySphere");
        sphere.GetAttribute(pxr::TfToken("isStatic")).Set(false);
        sphere.GetAttribute(pxr::TfToken("isTrigger")).Set(true);
        sphere.GetAttribute(pxr::TfToken("physics:collisionChannel")).Set(7);
        sphere.GetAttribute(pxr::TfToken("centerOfMassShift")).Set(pxr::GfVec3d(0.3, -0.2, 0.5));
        openScene();
        const Uid uid = component("Sphere").getUid();
        edit(sphere, "radius", 1.5);
        expectBounds("Sphere", {1.5, 1.5, 1.5}, 7);
        EXPECT_TRUE(component("Sphere").isTrigger());
        EXPECT_EQ(component("Sphere").getCollisionChannel(), 7);
        EXPECT_EQ(component("Sphere").getUid(), uid);
        getServiceProvider().get<ISceneManager>().getDefaultWorld().setSimulationPause(false);
        component("Sphere").addImpulse({1000, 0, 0});
        synchronize();
        const float initialX = component("Sphere").getParentObject().getWorldTransform().getTranslation().getX();
        m_physics->tick(0.1f);
        synchronize();
        const float firstX = component("Sphere").getParentObject().getWorldTransform().getTranslation().getX();
        ASSERT_GT(firstX, initialX);
        edit(sphere, "radius", 0.75);
        expectBounds("Sphere", {0.75, 0.75, 0.75}, 7);
        m_physics->tick(0.1f);
        synchronize();
        const float nextX = component("Sphere").getParentObject().getWorldTransform().getTranslation().getX();
        EXPECT_NEAR(nextX - firstX, firstX - initialX, (firstX - initialX) * 0.02f);
        EXPECT_EQ(component("Sphere").getUid(), uid);
    }

    TEST_F(RigidBodySizes, ResizedSphereMatchesFreshMassInertiaAndCenterOfMass)
    {
        auto resized = addBody("Resized", "RigidBodySphere");
        auto fresh = addBody("Fresh", "RigidBodySphere", {20, 0, 0});
        for (const auto& prim : {resized, fresh})
        {
            ASSERT_TRUE(prim.GetAttribute(pxr::TfToken("isStatic")).Set(false));
            ASSERT_TRUE(prim.GetAttribute(pxr::TfToken("centerOfMassShift")).Set(pxr::GfVec3d(0.3, -0.2, 0.5)));
        }
        ASSERT_TRUE(fresh.GetAttribute(pxr::TfToken("radius")).Set(2.0));
        openScene();
        edit(resized, "radius", 2.0);
        expectBounds("Resized", {2, 2, 2});
        expectBounds("Fresh", {2, 2, 2});
        const auto resizedStart = component("Resized").getParentObject().getWorldTransform().getTranslation();
        const auto freshStart = component("Fresh").getParentObject().getWorldTransform().getTranslation();
        getServiceProvider().get<ISceneManager>().getDefaultWorld().setSimulationPause(false);
        component("Resized").addImpulse({40000, 0, 0}, resizedStart + math::vec3(0, 1, 0));
        component("Fresh").addImpulse({40000, 0, 0}, freshStart + math::vec3(0, 1, 0));
        synchronize();
        m_physics->tick(0.1f);
        synchronize();
        const auto resizedTransform = component("Resized").getParentObject().getWorldTransform();
        const auto freshTransform = component("Fresh").getParentObject().getWorldTransform();
        const auto resizedDisplacement = resizedTransform.getTranslation() - resizedStart;
        const auto freshDisplacement = freshTransform.getTranslation() - freshStart;
        ASSERT_GT(freshDisplacement.getX(), 0.01f);
        ASSERT_LT(freshTransform.getRotation().getW(), 0.9999f);
        for (unsigned axis = 0; axis < 3; ++axis)
        {
            EXPECT_NEAR(resizedDisplacement.getElem(axis), freshDisplacement.getElem(axis), 0.001f) << axis;
        }
        for (unsigned element = 0; element < 4; ++element)
        {
            EXPECT_NEAR(resizedTransform.getRotation().getElem(element), freshTransform.getRotation().getElem(element), 0.001f)
                << element;
        }
    }

    TEST_F(RigidBodySizes, ResizePreservesExistingAngularVelocity)
    {
        auto resized = addBody("Resized", "RigidBodySphere");
        auto control = addBody("Control", "RigidBodySphere", {20, 0, 0});
        ASSERT_TRUE(resized.GetAttribute(pxr::TfToken("isStatic")).Set(false));
        ASSERT_TRUE(control.GetAttribute(pxr::TfToken("isStatic")).Set(false));
        openScene();
        getServiceProvider().get<ISceneManager>().getDefaultWorld().setSimulationPause(false);
        component("Resized").addImpulse({4000, 0, 0}, {0, 1, 0});
        component("Control").addImpulse({4000, 0, 0}, {20, 1, 0});
        synchronize();
        m_physics->tick(0.1f);
        synchronize();
        const auto resizedFirstRotation = component("Resized").getParentObject().getWorldTransform().getRotation();
        const auto controlFirstRotation = component("Control").getParentObject().getWorldTransform().getRotation();
        ASSERT_LT(controlFirstRotation.getZ(), -0.01f);
        for (unsigned element = 0; element < 4; ++element)
        {
            ASSERT_NEAR(resizedFirstRotation.getElem(element), controlFirstRotation.getElem(element), 0.001f) << element;
        }
        edit(resized, "radius", 2.0);
        expectBounds("Resized", {2, 2, 2});
        m_physics->tick(0.1f);
        synchronize();
        const auto resizedRotation = component("Resized").getParentObject().getWorldTransform().getRotation();
        const auto controlRotation = component("Control").getParentObject().getWorldTransform().getRotation();
        ASSERT_LT(controlRotation.getZ(), static_cast<float>(controlFirstRotation.getZ()) - 0.01f);
        for (unsigned element = 0; element < 4; ++element)
        {
            EXPECT_NEAR(resizedRotation.getElem(element), controlRotation.getElem(element), 0.001f) << element;
        }
    }

    TEST_F(RigidBodySizes, InvalidSizesKeepLastValidShapeAndCanRecover)
    {
        auto sphere = addBody("Sphere", "RigidBodySphere");
        auto cylinder = addBody("Cylinder", "RigidBodyCylinder", {20, 0, 0});
        auto cube = addBody("Cube", "RigidBodyCube", {40, 0, 0});
        openScene();
        edit(sphere, "radius", 2.5);
        edit(cylinder, "height", 6.0);
        edit(cylinder, "radius", 1.5);
        edit(cube, "extent", pxr::GfVec3d(2, 3, 4));
        expectBounds("Sphere", {2.5, 2.5, 2.5});
        expectBounds("Cylinder", {1.5, 3, 1.5});
        expectBounds("Cube", {2, 3, 4});
        for (double radius : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        {
            edit(sphere, "radius", radius);
            expectBounds("Sphere", {2.5, 2.5, 2.5});
        }
        edit(cylinder, "height", 0.0);
        expectBounds("Cylinder", {1.5, 3, 1.5});
        edit(cylinder, "height", 6.0);
        edit(cylinder, "radius", -1.0);
        expectBounds("Cylinder", {1.5, 3, 1.5});
        edit(cube, "extent", pxr::GfVec3d(2, -1, 4));
        expectBounds("Cube", {2, 3, 4});
        edit(sphere, "radius", 2.0);
        edit(cylinder, "radius", 1.25);
        edit(cylinder, "height", 4.0);
        edit(cube, "extent", pxr::GfVec3d(0.5, 1.25, 2.25));
        expectBounds("Sphere", {2, 2, 2});
        expectBounds("Cylinder", {1.25, 2, 1.25});
        expectBounds("Cube", {0.5, 1.25, 2.25});
    }

    TEST_F(RigidBodySizes, CapsuleKeepsItsRoundedEnds)
    {
        auto capsule = addBody("Capsule", "RigidBodyCapsule");
        openScene();
        edit(capsule, "height", 4.0);
        edit(capsule, "radius", 1.5);
        expectBounds("Capsule", {1.5, 3.5, 1.5});
        EXPECT_EQ(component("Capsule").getCollisions().capsules.size(), 1);
        EXPECT_TRUE(component("Capsule").getCollisions().cylinders.empty());
        const RayCastQuery roundedEnd{.origin = {0, 3, 5}, .direction = {0, 0, -1}, .maxDistance = 10};
        const auto hit = m_physics->getPhysicsWorld()->castRay(roundedEnd);
        ASSERT_TRUE(hit);
        EXPECT_EQ(hit->sceneObjectUid, component("Capsule").getParentObject().getUid());
        // At y = 3, the cap is one unit above its center and z = sqrt(1.5^2 - 1^2).
        EXPECT_NEAR(hit->position.getZ(), 1.118034f, 0.001f);
    }

    TEST_F(RigidBodySizes, InitiallyInvalidSizeRecoversWithoutSceneReload)
    {
        auto sphere = addBody("Sphere", "RigidBodySphere");
        sphere.GetAttribute(pxr::TfToken("radius")).Set(0.0);
        addBody("Neighbour", "RigidBodyCube", {20, 0, 0});
        openScene();
        const RayCastQuery query{.origin = {0, 0, 5}, .direction = {0, 0, -1}, .maxDistance = 10};
        EXPECT_FALSE(m_physics->getPhysicsWorld()->castRay(query));
        expectBounds("Neighbour", {1, 1, 1});
        edit(sphere, "radius", 2.0);
        expectBounds("Sphere", {2, 2, 2});
        expectBounds("Neighbour", {1, 1, 1});
    }

    TEST_F(RigidBodySizes, UnchangedDimensionsAndTransformEditsDoNotRebuildShapes)
    {
        auto sphere = addBody("Sphere", "RigidBodySphere");
        auto cylinder = addBody("Cylinder", "RigidBodyCylinder", {20, 0, 0});
        auto cube = addBody("Cube", "RigidBodyCube", {40, 0, 0});
        openScene();
        const auto creations = m_factory->shapeCreations;
        edit(sphere, "radius", 1.0);
        edit(cylinder, "radius", 1.0);
        edit(cylinder, "height", 1.0);
        edit(cube, "extent", pxr::GfVec3d(1, 1, 1));
        edit(m_stage->GetPrimAtPath(pxr::SdfPath("/Sphere")), "xformOp:translate", pxr::GfVec3d(2, 3, 4));
        EXPECT_EQ(m_factory->shapeCreations, creations);
        expectBounds("Sphere", {1, 1, 1});
        edit(sphere, "radius", 2.0);
        EXPECT_EQ(m_factory->shapeCreations, creations + 1);
        expectBounds("Sphere", {2, 2, 2});
        edit(cylinder, "radius", 1.5);
        edit(cylinder, "height", 4.0);
        edit(cube, "extent", pxr::GfVec3d(2, 3, 4));
        EXPECT_EQ(m_factory->shapeCreations, creations + 4);
        edit(sphere, "radius", 2.0);
        edit(cylinder, "radius", 1.5);
        edit(cylinder, "height", 4.0);
        edit(cube, "extent", pxr::GfVec3d(2, 3, 4));
        synchronize();
        synchronize();
        edit(m_stage->GetPrimAtPath(pxr::SdfPath("/Sphere")), "xformOp:translate", pxr::GfVec3d(3, 4, 5));
        edit(m_stage->GetPrimAtPath(pxr::SdfPath("/Cylinder")), "xformOp:translate", pxr::GfVec3d(20, 3, 2));
        edit(m_stage->GetPrimAtPath(pxr::SdfPath("/Cube")), "xformOp:translate", pxr::GfVec3d(40, 3, 2));
        EXPECT_EQ(m_factory->shapeCreations, creations + 4);
        expectBounds("Sphere", {2, 2, 2});
        expectBounds("Cylinder", {1.5, 2, 1.5});
        expectBounds("Cube", {2, 3, 4});
    }
    TEST_F(RigidBodySizes, StaticOnlyMeshReplacementRejectsMovingBodiesAndKeepsSphere)
    {
        openScene();
        auto world = m_physics->getPhysicsWorld();
        const Uid ownerUid = m_scene->getRoot().getUid();

        IMeshCollision::ConstructionData meshData;
        meshData.triangles.push_back({
            .p1 = {-2.f, -2.f, 0.f},
            .p2 = {2.f, -2.f, 0.f},
            .p3 = {2.f, 2.f, 0.f},
        });
        meshData.triangles.push_back({
            .p1 = {-2.f, -2.f, 0.f},
            .p2 = {2.f, 2.f, 0.f},
            .p3 = {-2.f, 2.f, 0.f},
        });
        auto mesh = m_factory->createMeshCollision(meshData);
        ASSERT_TRUE(mesh);

        const RayCastQuery centerRay{
            .origin = {0.f, 0.f, 3.f},
            .direction = {0.f, 0.f, -1.f},
            .maxDistance = 6.f,
        };
        const RayCastQuery outsideSphereRay{
            .origin = {1.5f, 0.f, 3.f},
            .direction = {0.f, 0.f, -1.f},
            .maxDistance = 6.f,
        };

        // A zero mass selects the kinematic backend path, while positive mass selects dynamic.
        for (TFloat mass : {1.f, 0.f})
        {
            SCOPED_TRACE(mass == 0.f ? "kinematic" : "dynamic");
            PhysicsBodyCreationData creation;
            creation.collisionShape = m_factory->createSphereCollision(1.f);
            creation.mass = mass;
            creation.motionType = mass == 0.f ? MotionType::Kinematic : MotionType::Dynamic;
            auto body = world->createBody(ownerUid, creation);
            ASSERT_TRUE(body);

            auto before = world->castRay(centerRay);
            ASSERT_TRUE(before.has_value());
            EXPECT_EQ(before->sceneObjectUid, ownerUid);
            EXPECT_NEAR(before->position.getZ(), 1.f, 0.001f);
            EXPECT_FALSE(world->castRay(outsideSphereRay).has_value());

            EXPECT_FALSE(body->setCollisionShape(*mesh));

            auto after = world->castRay(centerRay);
            ASSERT_TRUE(after.has_value());
            EXPECT_EQ(after->sceneObjectUid, ownerUid);
            EXPECT_NEAR(after->position.getZ(), 1.f, 0.001f);
            EXPECT_FALSE(world->castRay(outsideSphereRay).has_value());
        }

        // Positive control proves this is a usable mesh, not a failed/null shape.
        PhysicsBodyCreationData creation;
        creation.collisionShape = m_factory->createSphereCollision(1.f);
        creation.motionType = MotionType::Static;
        auto staticBody = world->createBody(ownerUid, creation);
        ASSERT_TRUE(staticBody);
        ASSERT_TRUE(staticBody->setCollisionShape(*mesh));
        auto meshHit = world->castRay(outsideSphereRay);
        ASSERT_TRUE(meshHit.has_value());
        EXPECT_EQ(meshHit->sceneObjectUid, ownerUid);
        EXPECT_NEAR(meshHit->position.getZ(), 0.f, 0.001f);
    }
}  // namespace UsdTranslator::test
