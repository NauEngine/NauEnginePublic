// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "physics_world_state.h"

#include <cmath>

#include "nau/assets/asset_manager.h"
#include "nau/physics/physics_assets.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include "nau/service/service_provider.h"

namespace nau::physics
{
    namespace
    {
        bool isPositiveFinite(float value)
        {
            return std::isfinite(value) && value > 0.0f;
        }

        bool isValidScale(math::vec3 scale)
        {
            return isPositiveFinite(std::abs(float(scale.getX()))) &&
                isPositiveFinite(std::abs(float(scale.getY()))) &&
                isPositiveFinite(std::abs(float(scale.getZ())));
        }

        // Compound shapes are not supported yet. Keep the same primitive priority
        // for initial creation, live updates and unchanged-geometry detection.
        bool hasSamePrimitiveGeometry(const CollisionDescription& left, const CollisionDescription& right)
        {
            if (!left.spheres.empty() || !right.spheres.empty())
            {
                return !left.spheres.empty() && !right.spheres.empty() &&
                    left.spheres.front().radius == right.spheres.front().radius;
            }
            if (!left.boxes.empty() || !right.boxes.empty())
            {
                return !left.boxes.empty() && !right.boxes.empty() &&
                    left.boxes.front().extent == right.boxes.front().extent;
            }
            if (!left.capsules.empty() || !right.capsules.empty())
            {
                return !left.capsules.empty() && !right.capsules.empty() &&
                    left.capsules.front().height == right.capsules.front().height &&
                    left.capsules.front().radius == right.capsules.front().radius;
            }
            if (!left.cylinders.empty() || !right.cylinders.empty())
            {
                return !left.cylinders.empty() && !right.cylinders.empty() &&
                    left.cylinders.front().height == right.cylinders.front().height &&
                    left.cylinders.front().radius == right.cylinders.front().radius;
            }
            return true;
        }

        ICollisionShape::Ptr createPrimitiveCollision(const CollisionDescription& collisions)
        {
            auto& factory = getServiceProvider().get<ICollisionShapesFactory>();
            if (!collisions.spheres.empty())
            {
                const float radius = collisions.spheres.front().radius;
                return isPositiveFinite(radius) ? factory.createSphereCollision(radius) : nullptr;
            }
            if (!collisions.boxes.empty())
            {
                const auto& extent = collisions.boxes.front().extent;
                return isPositiveFinite(extent.getX()) && isPositiveFinite(extent.getY()) && isPositiveFinite(extent.getZ())
                    ? factory.createBoxCollision(extent) : nullptr;
            }
            if (!collisions.capsules.empty())
            {
                const auto& capsule = collisions.capsules.front();
                return isPositiveFinite(capsule.height * 0.5f) && isPositiveFinite(capsule.radius)
                    ? factory.createCapsuleCollision({.height = capsule.height, .radius = capsule.radius}) : nullptr;
            }
            if (!collisions.cylinders.empty())
            {
                const auto& cylinder = collisions.cylinders.front();
                return isPositiveFinite(cylinder.height * 0.5f) && isPositiveFinite(cylinder.radius)
                    ? factory.createCylinderCollision({.height = cylinder.height, .radius = cylinder.radius}) : nullptr;
            }
            return nullptr;
        }

        void applyComponentScale(ICollisionShape& shape, const RigidBodyComponent& component)
        {
            if (auto scale = component.getWorldTransform().getScale(); !scale.similar(math::vec3::one()))
            {
                math::Transform shapeTransform;
                shapeTransform.setScale(scale);
                shape.setShapeTransform(shapeTransform);
            }
        }

        nau::Ptr<IPhysicsBody> createBodyWithShape(IPhysicsWorld& world, const RigidBodyComponent& component, ICollisionShape::Ptr shape)
        {
            if (!component.getMeshCollision() && !isValidScale(component.getWorldTransform().getScale()))
            {
                return nullptr;
            }
            applyComponentScale(*shape, component);
            const scene::SceneObject& parentObject = component.getParentObject();

            PhysicsBodyCreationData creationData{
                .collisionShape = std::move(shape),
            };

            creationData.mass = component.getMass();
            creationData.collisionChannel = component.getCollisionChannel();
            creationData.motionType = component.getMotionType();
            creationData.position = parentObject.getWorldTransform().getTranslation();
            creationData.rotation = parentObject.getRotation();
            creationData.isTrigger = component.isTrigger();
            creationData.debugDraw = component.isDebugDrawEnabled();
            creationData.comOffset = component.centerMassShift();

            return world.createBody(parentObject.getUid(), creationData);
        }
    }

    PhysicsWorldState::PhysicsWorldState(Uid worldUid) :
        m_worldUid(worldUid)
    {
        auto classes = getServiceProvider().findClasses<IPhysicsWorld>();
        NAU_FATAL(classes.size() == 1);
        const IMethodInfo* const ctor = classes.front()->getConstructor();
        NAU_FATAL(ctor);
        m_physics = ctor->invokeToPtr(nullptr, {});
        NAU_FATAL(m_physics);
    }

    PhysicsWorldState::~PhysicsWorldState()
    {
        m_bodies.clear();
    }

    Uid PhysicsWorldState::getWorldUid() const
    {
        return m_worldUid;
    }

    nau::Ptr<IPhysicsWorld> PhysicsWorldState::getPhysicsWorld() const
    {
        NAU_FATAL(m_physics);
        return m_physics;
    }

    void PhysicsWorldState::tick(float secondsDt)
    {
        if (m_isPaused)
        {
            return;
        }

        m_physics->tick(secondsDt);
    }

    async::Task<> PhysicsWorldState::activateComponents(eastl::span<const scene::Component*> components)
    {
        using namespace nau::scene;

        for (const Component* const component : components)
        {
            auto* const rigidBodyComponent = component->as<const RigidBodyComponent*>();
            if (rigidBodyComponent)
            {
                nau::Ptr<IPhysicsBody> physBody = co_await createPhysicsBodyForComponent(*rigidBodyComponent);
                if (!physBody)
                {
                    NAU_LOG_ERROR("Fail to create PhysicsBody for corresponding RigidBodyComponent");
                    if (rigidBodyComponent->getMeshCollision())
                    {
                        continue;
                    }
                    // Retain invalid primitive components so a later valid edit can
                    // create their body without reactivating the scene.
                }

                m_bodies.emplace_back(*rigidBodyComponent, std::move(physBody), rigidBodyComponent->m_collisionRevision);
            }
        }
    }

    void PhysicsWorldState::deactivateComponents(eastl::span<const scene::DeactivatedComponentData> components)
    {
        eastl::erase_if(m_bodies, [&components](const PhysicsBodyEntry& entry)
        {
            return eastl::find_if(components.begin(), components.end(), [&entry](const scene::DeactivatedComponentData& c)
            {
                return c.componentUid == entry.componentUid;
            });
        });
    }

    bool PhysicsWorldState::syncSceneState(scene::ISceneManager& sceneManager)
    {
        using namespace nau::scene;

        IWorld::WeakRef world = sceneManager.findWorld(m_worldUid);
        if (!world)
        {
            return false;
        }

        if (m_isPaused != sceneManager.getDefaultWorld().isSimulationPaused())
        {
            m_isPaused = !m_isPaused;
            if (m_isPaused)
            {
                NAU_LOG_WARNING(
                    "Physics: the simulation has just been DISABLED. In this mode, "
                    "the physics world is not updated, but is synchronized with the scene.");
            }
            else
            {
                NAU_LOG_WARNING(
                    "Physics: the simulation has just been ENABLED. In this mode, "
                    "the physics world is not synchronized with the scene, but the scene is updated from it.");
            }
        }

        for (auto entry = m_bodies.begin(); entry != m_bodies.end();)
        {
            if (!entry->componentRef)
            {
                entry = m_bodies.erase(entry);
                continue;
            }

            SceneObject& parentObject = entry->componentRef->getParentObject();

            const RigidBodyComponent& component = *entry->componentRef;
            const auto worldScale = component.getWorldTransform().getScale();
            const bool validScale = isValidScale(worldScale);
            // Treat all invalid scales alike so NaN does not retry every frame.
            const auto collisionScale = validScale ? worldScale : math::vec3::zero();
            const bool scaleChanged = entry->collisionScale != collisionScale;
            if (entry->collisionRevision != component.m_collisionRevision || scaleChanged)
            {
                // A rejected edit is processed once. Keep the last valid shape until
                // the next edit, including while editor simulation is paused.
                entry->collisionRevision = component.m_collisionRevision;
                entry->collisionScale = collisionScale;
                if (!component.getMeshCollision() &&
                    (scaleChanged || !entry->physicsBody || !hasSamePrimitiveGeometry(entry->appliedCollisions, component.getCollisions())))
                {
                    auto shape = validScale ? createPrimitiveCollision(component.getCollisions()) : nullptr;
                    bool applied = false;
                    if (shape)
                    {
                        if (entry->physicsBody)
                        {
                            applyComponentScale(*shape, component);
                            applied = entry->physicsBody->setCollisionShape(*shape);
                        }
                        else
                        {
                            entry->physicsBody = createBodyWithShape(*m_physics, component, std::move(shape));
                            applied = static_cast<bool>(entry->physicsBody);
                        }
                    }

                    if (applied)
                    {
                        entry->appliedCollisions = component.getCollisions();
                    }
                    else
                    {
                        NAU_LOG_WARNING("Cannot apply collision geometry for rigid body ({})", parentObject.getName());
                    }
                }
            }

            if (!entry->physicsBody)
            {
                entry->componentRef->applyPhysicsBodyActions(nullptr);
                ++entry;
                continue;
            }

            if (!m_isPaused)
            {
                math::mat4 physTransform;
                entry->physicsBody->getTransform(physTransform);

                // TODO: maybe do not need to getting world transform ?
                // If no need to deal with scale
                auto transform = parentObject.getWorldTransform();
                transform.setTranslation(physTransform.getTranslation());
                transform.setRotation(math::quat(physTransform.getUpper3x3()));
                parentObject.setWorldTransform(transform);

                entry->componentRef->applyPhysicsBodyActions(entry->physicsBody.get());
            }
            else
            {
                entry->physicsBody->setTransform({parentObject.getRotation(), parentObject.getWorldTransform().getTranslation()});
                entry->componentRef->applyPhysicsBodyActions(nullptr);
            }

            ++entry;
        }

        if (!m_isPaused)
        {
            m_physics->syncSceneState();
        }

        return true;
    }

    async::Task<nau::Ptr<IPhysicsBody>> PhysicsWorldState::createPhysicsBodyForComponent(const RigidBodyComponent& component)
    {
        auto& shapeFactory = getServiceProvider().get<physics::ICollisionShapesFactory>();

        ICollisionShape::Ptr collisionShape;

        if (auto meshCollision = component.getMeshCollision())
        {
            if (component.useConvexHullForCollision())
            {
                auto convexHullAssetView = co_await meshCollision.getAssetViewTyped<physics::ConvexHullAssetView>();
                if (convexHullAssetView)
                {
                    collisionShape = shapeFactory.createConvexHullCollisionFromAsset(std::move(convexHullAssetView));
                }
            }
            else
            {
                auto meshAssetView = co_await meshCollision.getAssetViewTyped<physics::TriMeshAssetView>();
                if (meshAssetView)
                {
                    collisionShape = shapeFactory.createMeshCollisionFromAsset(std::move(meshAssetView));
                }
            }
        }
        else
        {
            collisionShape = createPrimitiveCollision(component.getCollisions());
        }

        if (!collisionShape)
        {
            NAU_LOG_ERROR("Can not create collisions for rigid body ({})", component.getParentObject().getName());
            co_return nullptr;
        }

        co_return createBodyWithShape(*m_physics, component, std::move(collisionShape));
    }

}  // namespace nau::physics
