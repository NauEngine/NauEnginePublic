// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "./sample_component.h"
#include "graphics_assets/static_mesh_asset.h"
#include "nau/app/run_application.h"
#include "nau/assets/asset_container.h"
#include "nau/assets/asset_container_builder.h"
#include "nau/assets/asset_manager.h"
#include "nau/assets/asset_ref.h"
#include "nau/graphics/core_graphics.h"
#include "nau/input.h"
#include "nau/math/math.h"
#include "nau/physics/components/rigid_body_component.h"
#include "nau/physics/core_physics.h"
#include "nau/physics/physics_body.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include "nau/physics/physics_contact_listener.h"
#include "nau/physics/physics_material.h"
#include "nau/physics/physics_world.h"
#include "nau/rtti/rtti_impl.h"
#include "nau/samples/components/camera_control.h"
#include "nau/samples/sample_app_delegate.h"
#include "nau/scene/components/camera_component.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/components/skinned_mesh_component.h"
#include "nau/scene/components/static_mesh_component.h"
#include "nau/scene/scene.h"
#include "nau/scene/scene_factory.h"
#include "nau/scene/scene_manager.h"
#include "nau/service/service_provider.h"
#include "nau/utils/performance_profiling.h"

#define WINDOW_SERVICE


namespace nau::sample
{
    class MyContactListener : public nau::physics::IPhysicsContactListener
    {
        NAU_CLASS(nau::sample::MyContactListener, rtti::RCPolicy::Concurrent, nau::physics::IPhysicsContactListener)

        void onContactAdded(
            const nau::physics::IPhysicsContactListener::ContactManifold& data1,
            const nau::physics::IPhysicsContactListener::ContactManifold& data2,
            const eastl::vector<nau::math::vec3>& collisionWorldPoints) override
        {
            std::ostringstream ss;
            for (const auto& point : collisionWorldPoints)
            {
                ss << "(" << float(point.getX()) << "," << float(point.getY()) << "," << float(point.getZ()) << "),";
            }

            physics::RigidBodyComponent& rb1 = data1.rigidBody;
            physics::RigidBodyComponent& rb2 = data2.rigidBody;
            scene::SceneObject& obj1 = rb1.getParentObject();
            scene::SceneObject& obj2 = rb2.getParentObject();

            NAU_LOG_DEBUG("Objects {}(material:{}, trigger:{}) and {}(material:{}, trigger:{}) just collided on {}: ",
                          obj1.getName(), data1.material->getName(), rb1.isTrigger(),
                          obj2.getName(), data2.material->getName(), rb2.isTrigger(),
                          ss.str());
        }

        void onContactContinued(const ContactManifold& data1, const ContactManifold& data2, const eastl::vector<nau::math::vec3>& collisionWorldPoints) override
        {
        }

        void onContactRemovedCompletely(const ContactManifold& data1, const ContactManifold& data2) override
        {
            scene::SceneObject& obj1 = data1.rigidBody.getParentObject();
            scene::SceneObject& obj2 = data2.rigidBody.getParentObject();
            NAU_LOG_DEBUG("Objects {} and {} have ceased all contacts", obj1.getName(), obj2.getName());
        }
    };

    nau::Ptr<MyContactListener> sContactListener;
    Vectormath::SSE::Vector3 sRayPoint1{};
    Vectormath::SSE::Vector3 sRayPoint2{};

    static const nau::physics::CollisionChannel sStaticChannel = 0;
    static const nau::physics::CollisionChannel sFirstMovingChannel = 1;
    static const nau::physics::CollisionChannel sSecondMovingChannel = 2;

    nau::scene::SceneObject* cameraObject = nullptr;

    async::Task<scene::IScene::Ptr> loadGltfScene(eastl::string sceneAssetPath)
    {
        using namespace nau::scene;

        AssetRef<> sceneAssetRef{sceneAssetPath};
        nau::SceneAsset::Ptr sceneAsset = co_await sceneAssetRef.getAssetViewTyped<SceneAsset>();
        nau::scene::IScene::Ptr scene = getServiceProvider().get<scene::ISceneFactory>().createSceneFromAsset(*sceneAsset);

        auto& corePhysics = getServiceProvider().get<physics::ICorePhysics>();
        auto physWorld = corePhysics.getDefaultPhysicsWorld();
        physWorld->setContactListener(nau::rtti::createInstance<MyContactListener>());

        auto woodMaterial = physWorld->createMaterial("Wood", 0.541, 0.20);
        auto stoneMaterial = physWorld->createMaterial("Stone", 0.723, 0.999);
        auto ceramicMaterial = physWorld->createMaterial("Ceramic", 0.230, 0.382);

        physWorld->setChannelsCollidable(sStaticChannel, sFirstMovingChannel);
        physWorld->setChannelsCollidable(sStaticChannel, sSecondMovingChannel);

        physWorld->setChannelsCollidable(sSecondMovingChannel, sSecondMovingChannel);
        physWorld->setChannelsCollidable(sFirstMovingChannel, sFirstMovingChannel);

        // Cubes from first channel shouldn't collide with others from second one.
        physWorld->setChannelsCollidable(sFirstMovingChannel, sSecondMovingChannel, false);

        int objectIterator = 0;
        for (SceneObject* const obj : scene->getRoot().getDirectChildObjects())
        {
            if (obj->getName() == "Camera.001")
            {
                cameraObject = obj;
                CameraComponent* cam = cameraObject->findFirstComponent<CameraComponent>();
                if (cam)
                {
                    cam->setFov(45.f);
                }
            }
            else if (auto* const staticMesh = obj->findFirstComponent<StaticMeshComponent>(); staticMesh)
            {
                std::optional<physics::PhysicsBodyCreationData> physCreationData;
                if (obj->getName() == "Ray1")
                {
                    sRayPoint1 = obj->getTranslation();
                }
                if (obj->getName() == "Ray2")
                {
                    sRayPoint2 = obj->getTranslation();
                }
                if (obj->getName().substr(0, 5) == "Floor" || obj->getName().substr(0, 4) == "Wall")
                {
                    auto& rb = obj->addComponent<physics::RigidBodyComponent>();
                    rb.setIsTrigger(obj->getName() == "Wall");
                    rb.setMotionType(physics::MotionType::Static);
                    rb.setCollisionChannel(sStaticChannel);
                    // rb.getCollisions().addBox(math::vec3::one());
                    rb.getCollisions().addBox(obj->getScale());
                    //.material = stoneMaterial.get();

#if 0
                    physCreationData.emplace();

                    physCreationData->collisionShape =
                        physShapesFactory.createBoxCollision(obj->getScale(), stoneMaterial.get());

                    physCreationData->motionType = nau::physics::MotionType::Static;
                    physCreationData->collisionChannel = sStaticChannel;
                    physCreationData->debugDraw = false;
                    physCreationData->isTrigger = obj->getName() == "Wall";
#endif
                }
                if (obj->getName() == "Cylinder")
                {
                    auto& rb = obj->addComponent<physics::RigidBodyComponent>();
                    rb.setMotionType(physics::MotionType::Dynamic);
                    rb.setCollisionChannel(sFirstMovingChannel);
                    rb.setMass(10);
                    rb.setDebugDrawEnabled(true);
                    auto& collider = rb.getCollisions().addCylinder(8, 1);
                    collider.localTransform.setTranslation(0, 5, 0);
#if 0
                    physCreationData.emplace();
                    nau::physics::ICylinderCollision::ConstructionData convexData;
                    convexData.height = 8;
                    convexData.radius = 1;

                    physCreationData->collisionShape = physShapesFactory.createCylinderCollision(convexData, ceramicMaterial.get());
                    physCreationData->motionType = nau::physics::MotionType::Dynamic;
                    physCreationData->mass = 10;
                    physCreationData->collisionChannel = sFirstMovingChannel;
                    physCreationData->debugDraw = true;
                    physCreationData->comOffset = {0, 5, 0};
#endif
                }
                if (obj->getName() == "Teapot.002")
                {
                    auto& rb = obj->addComponent<physics::RigidBodyComponent>();
                    rb.setMotionType(physics::MotionType::Dynamic);
                    rb.setMass(100);
                    rb.setCollisionChannel(sFirstMovingChannel);
                    rb.setMeshCollision(AssetPath{"file:/content/scenes/teapot.gltf+[mesh/0]"});
                    rb.setUseConvexHullForCollision(true);
                    rb.setDebugDrawEnabled(true);
                    obj->setTranslation(math::vec3{0, 9, 0});
                }
                if (obj->getName() == "Teapot.001" || obj->getName() == "Teapot.003")
                {
                    auto& rb = obj->addComponent<physics::RigidBodyComponent>();
                    rb.setMotionType(physics::MotionType::Static);
                    rb.setCollisionChannel(sStaticChannel);
                    rb.setMeshCollision(AssetPath{"file:/content/scenes/teapot.gltf+[mesh/0]"});
                    rb.setUseConvexHullForCollision(false);
                    rb.setDebugDrawEnabled(true);

#if 0
                    physCreationData.emplace();

                    nau::physics::IMeshCollision::ConstructionData meshData;
                    meshData.materials = {woodMaterial.get(), ceramicMaterial.get(), stoneMaterial.get()};

                    uint32_t materialIndex = 0;
                    for (const auto& triangle : geometry)
                    {
                        const nau::math::vec3& p0 = eastl::get<0>(triangle);
                        const nau::math::vec3& p1 = eastl::get<1>(triangle);
                        const nau::math::vec3& p2 = eastl::get<2>(triangle);

                        auto point0 = Vectormath::SSE::scale({p0.getX(), p0.getY(), p0.getZ()}, obj->getScale());
                        auto point1 = Vectormath::SSE::scale({p1.getX(), p1.getY(), p1.getZ()}, obj->getScale());
                        auto point2 = Vectormath::SSE::scale({p2.getX(), p2.getY(), p2.getZ()}, obj->getScale());

                        meshData.triangles.push_back({
                            {point0.getX(), point0.getY(), point0.getZ()},
                            {point1.getX(), point1.getY(), point1.getZ()},
                            {point2.getX(), point2.getY(), point2.getZ()},
                            materialIndex++
                        });

                        if (materialIndex >= meshData.materials.size())
                        {
                            materialIndex = 0;
                        }
                    }

                    physCreationData->collisionShape = physShapesFactory.createMeshCollision(meshData);
                    physCreationData->motionType = nau::physics::MotionType::Static;
                    physCreationData->collisionChannel = sStaticChannel;
#endif
                }
                else if (obj->getName().substr(0, 4) == "Cube")
                {
                    auto& rb = obj->addComponent<physics::RigidBodyComponent>();
                    rb.setMotionType(physics::MotionType::Dynamic);
                    const auto collChannel = ++objectIterator % 2 ? sFirstMovingChannel : sSecondMovingChannel;
                    rb.setCollisionChannel(collChannel);
                    rb.setMass(10);
                    rb.setDebugDrawEnabled(true);
                    //[[maybe_unused]] auto& collider = rb.getCollisions().addBox(math::vec3::one());
                    [[maybe_unused]] auto& collider = rb.getCollisions().addBox(obj->getScale());
                }
            }
        }

        co_return scene;
    }

    void addCameraControl(scene::IScene& scene)
    {
        if (cameraObject)
        {
            auto& ctrl = cameraObject->addComponent<CameraControl>();
            ctrl.setCamControlKind(CamControlKind::UseSceneObject);
            ctrl.setStepFactor(10.0);
        }
    }

    void screenCenterToWorld(nau::math::vec3& world, nau::math::vec3& worldDirection)
    {
        if (!cameraObject)
        {
            return;
        }

        const nau::math::mat4 projectionMatrix = nau::getServiceProvider().get<nau::ICoreGraphics>().getProjMatrix();
        const nau::math::vec2 normalizedScreen{0.0f, 0.0f};
        const nau::math::mat4 camera = cameraObject->getWorldTransform().getMatrix();
        world = camera.getCol3().getXYZ();

        const nau::math::vec4 worldDirection4D =
            camera.getCol0() * normalizedScreen.getX() / projectionMatrix[0][0] +
            camera.getCol1() * normalizedScreen.getY() / projectionMatrix[1][1] + camera.getCol2();

        worldDirection = -Vectormath::SSE::normalize(worldDirection4D.getXYZ());
    }

    /**
     */
    class PhysicsSampleDelegate final : public SampleAppDelegate
    {
    public:
        PhysicsSampleDelegate() :
            SampleAppDelegate("physics")
        {
        }

    private:
        Result<> initializeServices() override
        {
            getServiceProvider().addClass<sample::SampleComponent>();
            getServiceProvider().addClass<sample::CameraControl>();
            return ResultSuccess;
        }

        async::Task<> startupApplication() override
        {
            using namespace nau::scene;

            scene::IScene::Ptr scene = co_await loadGltfScene("file:/content/scenes/physics_scene.gltf");
            addCameraControl(*scene);

            auto& sceneManager = getServiceProvider().get<ISceneManager>();

            co_await sceneManager.activateScene(std::move(scene));

            {
                auto& sceneFactory = getServiceProvider().get<ISceneFactory>();
                auto scene = sceneFactory.createEmptyScene();
                scene->getRoot().addComponent<SampleComponent>();

                ObjectWeakRef world1 = sceneManager.createWorld();
                co_await world1->addScene(std::move(scene));
            }
        }

        void onApplicationStep([[maybe_unused]] std::chrono::milliseconds dt) override
        {
            using namespace std::chrono_literals;
            using namespace nau::async;

            auto& sceneManager = getServiceProvider().get<nau::scene::ISceneManager>();
            auto& corePhysics = getServiceProvider().get<physics::ICorePhysics>();
            auto defaultPhysWorld = corePhysics.getDefaultPhysicsWorld();

            auto& physWorld = *defaultPhysWorld;

// TODO Tracy            NAU_CPU_SCOPED;
            if (nau::input::isKeyboardButtonPressed(0, nau::input::Key::R))
            {
                const auto rayDir = Vectormath::SSE::normalize(nau::sample::sRayPoint2 - nau::sample::sRayPoint1);
                const auto rayLength = Vectormath::SSE::length(nau::sample::sRayPoint2 - nau::sample::sRayPoint1);

                [](physics::IPhysicsWorld& physWorld, const math::vec3 rayDir, const physics::TFloat rayLength) -> Task<>
                {
                    constexpr size_t NumRays = 5;
                    eastl::vector<physics::RayCastQuery> queries(NumRays);
                    for (int rayIdx = 0; rayIdx < NumRays; ++rayIdx)
                    {
                        physics::RayCastQuery& query = queries[rayIdx];

                        query.id = rayIdx;
                        query.origin = nau::sample::sRayPoint1 + Vectormath::SSE::Vector3(0, rayIdx * 1.5f, 0);
                        query.direction = rayDir;
                        query.maxDistance = rayLength;
                        query.debugDrawDuration = 10;
                        query.reactChannels = {nau::sample::sFirstMovingChannel, nau::sample::sSecondMovingChannel};
                    }

                    NAU_LOG_DEBUG("----  Initiate ray cast ------");
                    auto castResult = co_await physWorld.castRaysAsync(std::move(queries));

                    for (const physics::RayCastResult& castResult : castResult)
                    {
                        if (castResult)
                        {
                            scene::SceneObject& object = castResult.rigidBody->getParentObject();

                            if (castResult.material)
                            {
                                NAU_LOG_DEBUG("Ray [{}] hits object: obj:({}), mat:({}),", castResult.queryId, object.getName(), castResult.material->getName());
                            }
                            else
                            {
                                NAU_LOG_DEBUG("Ray [{}] hits object: obj:({}), mat:(undefined),", castResult.queryId, object.getName());
                            }
                        }
                        else
                        {
                            NAU_LOG_DEBUG("Ray [{}] hits NO objects", castResult.queryId);
                        }
                    }
                    NAU_LOG_DEBUG("----  Completed ray cast ------");
                }(physWorld, rayDir, rayLength).detach();
            }
            else if (input::isKeyboardButtonPressed(0, nau::input::Key::F) ||
                     input::isKeyboardButtonPressed(0, nau::input::Key::T))
            {
                physics::RayCastQuery query;
                sample::screenCenterToWorld(query.origin, query.direction);

                [](physics::IPhysicsWorld& physWorld, physics::RayCastQuery query, bool addTorque) -> Task<>
                {
                    query.maxDistance = 1000;
                    query.debugDrawDuration = 1;
                    query.reactChannels = {nau::sample::sFirstMovingChannel, nau::sample::sSecondMovingChannel};

                    if (auto castResult = co_await physWorld.castRayAsync(query))
                    {
                        const auto acceleration = 500000 * query.direction - nau::math::vec3(0.f, -9.8f, 0.0);
                        scene::SceneObject& object = castResult.rigidBody->getParentObject();
                        if (addTorque)
                        {
                            NAU_LOG_DEBUG("Apply torque on obj({}),", object.getName());
                            castResult.rigidBody->addTorque(acceleration);
                        }
                        else
                        {
                            NAU_LOG_DEBUG("Apply force on obj({}),", object.getName());
                            castResult.rigidBody->addForce(acceleration);
                        }
                    }
                    else
                    {
                        NAU_LOG("Cast from cam returns nothing");
                    }
                }(physWorld, std::move(query), input::isKeyboardButtonHold(0, input::Key::T)).detach();
            }
            else
            {
                // NAU_LOG_DEBUG("Force ray from main camera hits NO objects");
            }

            physWorld.drawDebug(getDebugRenderer());

            
// TODO Tracy            NAU_PROFILING_FRAME_END;
        }


        bool b_isDebug = false;
    };

}  // namespace nau::sample

int main(int argc, char** argv)
{
    using namespace nau;
    return runApplication(eastl::make_unique<sample::PhysicsSampleDelegate>());
}
