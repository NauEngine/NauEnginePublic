// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/physics/jolt/jolt_physics_world.h"

#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include "jolt_physics_layers.h"
#include "nau/diag/assertion.h"
#include "nau/physics/internal/core_physics_internal.h"
#include "nau/physics/jolt/jolt_physics_body.h"
#include "nau/physics/jolt/jolt_physics_material.h"
#include "nau/physics/jolt/jolt_physics_math.h"
#include "nau/scene/scene_manager.h"
#include "nau/service/service_provider.h"

namespace nau::physics::jolt
{
    namespace
    {
        constexpr int JOLT_TEMP_ALLOC_SIZE = 1 << 20;
        constexpr int JOLT_MAX_JOBS = 32;

        constexpr int JOLT_SETTING_MAX_BODIES = 16384;
        constexpr int JOLT_SETTING_NUM_BODY_MUTEXES = 32;
        constexpr int JOLT_SETTING_MAX_BODY_PAIRS = 1 << 16;
        constexpr int JOLT_SETTING_MAX_CONTACT_CONSTRAINTS = 1 << 10;

        /**
         * At this moment 2 broad phase layers(and moving objects) would be enough.
         */
        constexpr unsigned JOLT_SETTING_BROAD_PHASE_LAYERS_COUNT = 2;

        constexpr unsigned JOLT_SETTING_OBJECT_LAYERS_COUNTS = 1000;

        static const JPH::Vec3 gravityAcceleration{.0f, -9.81f, .0f};
    };  // namespace

    JoltPhysicsWorld::JoltPhysicsWorld()
    {
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory;

        auto joltDefaultMaterial = eastl::make_unique<JoltPhysicsMaterial>();
        m_engineDefaultMaterial = rtti::createInstance<NauJoltPhysicsMaterialImpl>("Default");
        joltDefaultMaterial->setEngineMaterial(m_engineDefaultMaterial);

        // Jolt will take care about clearing sDefault in JPH::UnregisterTypes.
        JPH::PhysicsMaterial::sDefault = joltDefaultMaterial.release();

        JPH::RegisterTypes();

        m_layerPairFilter = eastl::make_unique<JPH::ObjectLayerPairFilterTable>(JOLT_SETTING_OBJECT_LAYERS_COUNTS);
        m_broadPhaseLayerInterface = eastl::make_unique<JPH::BroadPhaseLayerInterfaceTable>(
            JOLT_SETTING_OBJECT_LAYERS_COUNTS, JOLT_SETTING_BROAD_PHASE_LAYERS_COUNT);

        m_objectOverBroadPhaseFilter = eastl::make_unique<DefaultObjectVsBroadPhaseLayerFilter>();

        m_bodyDrawFilter = eastl::make_unique<JoltBodyDrawFilterImpl>();
        m_joltDebugRender = eastl::make_unique<DebugRendererImp>();

        m_joltPhysicsSystem = eastl::make_unique<JPH::PhysicsSystem>();
        m_joltJobSystem = eastl::make_unique<JPH::JobSystemSingleThreaded>(JOLT_MAX_JOBS);
        m_joltTempAllocator = eastl::make_unique<JPH::TempAllocatorImpl>(JOLT_TEMP_ALLOC_SIZE);

        m_joltPhysicsSystem->Init(
            JOLT_SETTING_MAX_BODIES,
            JOLT_SETTING_NUM_BODY_MUTEXES,
            JOLT_SETTING_MAX_BODY_PAIRS,
            JOLT_SETTING_MAX_CONTACT_CONSTRAINTS,
            *m_broadPhaseLayerInterface,
            *m_objectOverBroadPhaseFilter,
            *m_layerPairFilter);

        m_joltPhysicsSystem->SetPhysicsSettings(JPH::PhysicsSettings{});
        m_joltPhysicsSystem->SetGravity(gravityAcceleration);
        m_joltPhysicsSystem->SetContactListener(this);
    }

    JoltPhysicsWorld::~JoltPhysicsWorld()
    {
        // Unregisters all types with the factory and cleans up the default material
        JPH::UnregisterTypes();
    }

    void JoltPhysicsWorld::tick(float dt)
    {
        m_joltPhysicsSystem->Update(dt, m_collisionStepsCount, m_joltTempAllocator.get(), m_joltJobSystem.get());
    }

    nau::Ptr<IPhysicsBody> JoltPhysicsWorld::createBody(Uid sceneObjectUid, const PhysicsBodyCreationData& creationData)
    {
        auto body = rtti::createInstance<JoltPhysicsBody>(this, sceneObjectUid, creationData);
        return body->isValid() ? body : nullptr;
    }

    void JoltPhysicsWorld::setChannelsCollidable(CollisionChannel channelA, CollisionChannel channelB, bool collidable)
    {
        NAU_ASSERT(m_layerPairFilter);

        if (collidable)
        {
            m_layerPairFilter->EnableCollision(channelA, channelB);
        }
        else
        {
            m_layerPairFilter->DisableCollision(channelA, channelB);
        }
    }

    void JoltPhysicsWorld::resetChannelsCollisionSettings()
    {
        NAU_ASSERT(m_layerPairFilter);
        m_layerPairFilter->Reset();
    }

    void JoltPhysicsWorld::setContactListener(nau::Ptr<IPhysicsContactListener> listener)
    {
        m_engineContactListener = eastl::move(listener);
    }

    JPH::BodyInterface& JoltPhysicsWorld::getBodyInterface() const
    {
        return m_joltPhysicsSystem->GetBodyInterface();
    }

    IPhysicsMaterial::Ptr JoltPhysicsWorld::createMaterial(eastl::string_view name,
                                                           eastl::optional<TFloat> friction,
                                                           eastl::optional<TFloat> restitution)
    {
        return nau::rtti::createInstance<NauJoltPhysicsMaterialImpl>(name, friction, restitution);
    }

    eastl::optional<RayCastResult> JoltPhysicsWorld::castRay(const nau::physics::RayCastQuery& query) const
    {
        using namespace nau::scene;

        static const auto failureDebugColor = nau::math::Color4(1.0, 0.0, 0.0);
        static const auto successDebugColor = nau::math::Color4(0.0, 1.0, 0.0);

        const JPH::RRayCast ray{vec3ToJolt(query.origin), vec3ToJolt(query.maxDistance * query.direction)};

        const math::Point3 debugRayStart{query.origin.get128()};
        const math::Point3 debugRayEnd = debugRayStart + query.direction * query.maxDistance;

        JPH::RayCastResult hit;
        const DefaultRayCastChannelFilter channelFilter(query.reactChannels);

        if (!m_joltPhysicsSystem->GetNarrowPhaseQuery().CastRay(ray, hit, {}, channelFilter))
        {
            debugDrawLine(debugRayStart, debugRayEnd, failureDebugColor, query.debugDrawDuration);
            return eastl::nullopt;
        }

        JPH::BodyLockRead lock(m_joltPhysicsSystem->GetBodyLockInterface(), hit.mBodyID);
        if (!lock.Succeeded())
        {
            debugDrawLine(debugRayStart, debugRayEnd, failureDebugColor, query.debugDrawDuration);
            return eastl::nullopt;
        }

        const JPH::Body& hitBody = lock.GetBody();
        const JPH::PhysicsMaterial* hitMaterial = hitBody.GetShape()->GetMaterial(hit.mSubShapeID2);
        const JPH::Vec3 hitPosition = ray.GetPointOnRay(hit.mFraction);
        const JPH::Vec3 normal = hitBody.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPosition);

        auto& joltBody = *reinterpret_cast<JoltPhysicsBody*>(hitBody.GetUserData());

        Uid sceneObjectUid = joltBody.getSceneObjectUid();

        ObjectWeakRef<> object = getServiceProvider().get<ISceneManager>().querySingleObject(SceneQuery{QueryObjectCategory::Object, sceneObjectUid});
        RigidBodyComponent* const component = object->as<SceneObject&>().findFirstComponent<RigidBodyComponent>();

        const RayCastResult result =
            {
                .queryId = query.id,
                .sceneObjectUid = sceneObjectUid,
                .rigidBody = component ? ObjectWeakRef<RigidBodyComponent>(*component) : nullptr,
                .material = reinterpret_cast<const JoltPhysicsMaterial*>(hitMaterial)->engineMaterial(),
                .position = nau::physics::jolt::joltVec3ToNauVec3(hitPosition),
                .normal = nau::physics::jolt::joltVec3ToNauVec3(normal)};

        debugDrawLine(debugRayStart, nau::math::Point3(result.position.get128()), successDebugColor, query.debugDrawDuration);
        return result;
    }

    async::Task<eastl::vector<RayCastResult>> JoltPhysicsWorld::castRaysAsync(eastl::vector<physics::RayCastQuery> queries) const
    {
        using namespace nau::async;
        using namespace nau::scene;

        auto performCastsOnPhysicsExecutor = [](const JoltPhysicsWorld& self, const decltype(queries)& queries) -> Task<eastl::vector<RayCastResult>>
        {
            const auto FailureDebugColor = math::Color4(1.0, 0.0, 0.0);
            const auto SuccessDebugColor = math::Color4(0.0, 1.0, 0.0);

            auto& corePhysics = getServiceProvider().get<ICorePhysicsInternal>();
            co_await corePhysics.getExecutor();

            eastl::vector<RayCastResult> castResults;
            castResults.reserve(queries.size());

            for (const physics::RayCastQuery& query : queries)
            {
                RayCastResult& result = castResults.emplace_back();
                result.queryId = query.id;

                const JPH::RRayCast ray{vec3ToJolt(query.origin), vec3ToJolt(query.maxDistance * query.direction)};

                const math::Point3 debugRayStart{query.origin.get128()};
                const math::Point3 debugRayEnd = debugRayStart + query.direction * query.maxDistance;

                JPH::RayCastResult hit;
                const DefaultRayCastChannelFilter channelFilter(query.reactChannels);

                // using NoLock API, because we known that tick is not performed
                if (!self.m_joltPhysicsSystem->GetNarrowPhaseQueryNoLock().CastRay(ray, hit, {}, channelFilter))
                {
                    debugDrawLine(debugRayStart, debugRayEnd, FailureDebugColor, query.debugDrawDuration);
                    continue;
                }

                // using NoLock API, because we known that tick is not performed
                JPH::BodyLockRead lock(self.m_joltPhysicsSystem->GetBodyLockInterfaceNoLock(), hit.mBodyID);
                if (!lock.Succeeded())
                {
                    debugDrawLine(debugRayStart, debugRayEnd, FailureDebugColor, query.debugDrawDuration);
                    continue;
                }

                const JPH::Body& hitBody = lock.GetBody();

                const JPH::PhysicsMaterial* hitMaterial = hitBody.GetShape()->GetMaterial(hit.mSubShapeID2);
                const JPH::Vec3 hitPosition = ray.GetPointOnRay(hit.mFraction);
                const JPH::Vec3 normal = hitBody.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, hitPosition);

                auto& joltBody = *reinterpret_cast<JoltPhysicsBody*>(hitBody.GetUserData());

                result.position = jolt::joltVec3ToNauVec3(hitPosition),
                result.normal = jolt::joltVec3ToNauVec3(normal);
                result.sceneObjectUid = joltBody.getSceneObjectUid();
                result.material = static_cast<const JoltPhysicsMaterial*>(hitMaterial)->engineMaterial();

                debugDrawLine(debugRayStart, math::Point3(result.position.get128()), SuccessDebugColor, query.debugDrawDuration);
            }

            co_return castResults;
        };

        auto castResults = co_await performCastsOnPhysicsExecutor(*this, queries);
        NAU_ASSERT(castResults.size() == queries.size());

        ISceneManager& sceneManger = getServiceProvider().get<ISceneManager>();

        for (RayCastResult& result : castResults)
        {
            if (result.sceneObjectUid == NullUid)
            {
                continue;
            }

            ObjectWeakRef<> object = sceneManger.querySingleObject({QueryObjectCategory::Object, result.sceneObjectUid});
            if (object)
            {
                if (RigidBodyComponent* const component = object->as<SceneObject&>().findFirstComponent<RigidBodyComponent>(); component)
                {
                    result.rigidBody = *component;
                }
                else
                {
                    NAU_LOG_ERROR("Object ({}) does not contains RigidBodyComponent", toString(result.sceneObjectUid));
                }
            }
            else
            {
                NAU_LOG_WARNING("Cast find intersection, but target object does not exists anymore:({})", toString(result.sceneObjectUid));
                result.sceneObjectUid = NullUid;
            }
        }

        co_return castResults;
    }

    void JoltPhysicsWorld::drawDebug(nau::DebugRenderSystem& dr)
    {
        m_joltDebugRender->setDebugRenderer(&dr);

        m_joltPhysicsSystem->DrawBodies(
            {.mDrawCenterOfMassTransform = true},
            m_joltDebugRender.get(), m_bodyDrawFilter.get());
    }

    void JoltPhysicsWorld::setGravity(const nau::math::vec3& gravity)
    {
        m_joltPhysicsSystem->SetGravity(vec3ToJolt(gravity));
    }

    void JoltPhysicsWorld::OnContactAdded(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
    {
        handleBodiesContact(body1, body2, manifold, settings);

        // TODO: thread unsafe access (for m_engineContactListener)
        if (m_engineContactListener)
        {
            const auto& jBody1 = *reinterpret_cast<JoltPhysicsBody*>(body1.GetUserData());
            const auto& jBody2 = *reinterpret_cast<JoltPhysicsBody*>(body2.GetUserData());

            // Get friction and restitution from custom material or from bodies.
            auto [friction1, restitution1, mat1] = getFrictionAndRestitution(body1, manifold.mSubShapeID1);
            auto [friction2, restitution2, mat2] = getFrictionAndRestitution(body2, manifold.mSubShapeID2);

            JPH::lock_guard lock(m_bodiesInContactGuard);
            auto itContacts = m_bodiesInContact.find({body1.GetID(), body2.GetID()});
            if (itContacts == m_bodiesInContact.end())
            {
                ContactData data;
                data.body1 = &jBody1;
                data.body2 = &jBody2;

                itContacts = m_bodiesInContact.insert({
                                                          {body1.GetID(), body2.GetID()},
                                                          data
                })
                                 .first;
            }
            ContactData& data = itContacts->second;
            data.contacts.insert({body1.GetID(), manifold.mSubShapeID1, body2.GetID(), manifold.mSubShapeID2});

            {
                auto& contactData = m_contactsData.emplace_back(ContactNotificationKind::Added);
                contactData.object1 = {
                    .sceneObjectUid = data.body1->getSceneObjectUid(),
                    .material = mat1->engineMaterial()};

                contactData.object2 = {
                    .sceneObjectUid = data.body2->getSceneObjectUid(),
                    .material = mat2->engineMaterial()};

                contactData.collisionWorldPoints = calculateContactPoints(manifold);
            }
        }
    }

    void JoltPhysicsWorld::OnContactPersisted(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
    {
        handleBodiesContact(body1, body2, manifold, settings);

        // TODO: thread unsafe access (for m_engineContactListener)
        if (m_engineContactListener)
        {
            const auto& jBody1 = *reinterpret_cast<JoltPhysicsBody*>(body1.GetUserData());
            const auto& jBody2 = *reinterpret_cast<JoltPhysicsBody*>(body2.GetUserData());

            // Get friction and restitution from custom material or from bodies.
            auto [friction1, restitution1, mat1] = getFrictionAndRestitution(body1, manifold.mSubShapeID1);
            auto [friction2, restitution2, mat2] = getFrictionAndRestitution(body2, manifold.mSubShapeID2);

            {
                auto& contactData = m_contactsData.emplace_back(ContactNotificationKind::Continued);
                contactData.object1 = {
                    .sceneObjectUid = jBody1.getSceneObjectUid(),
                    .material = mat1->engineMaterial()};

                contactData.object2 = {
                    .sceneObjectUid = jBody2.getSceneObjectUid(),
                    .material = mat2->engineMaterial()};

                contactData.collisionWorldPoints = calculateContactPoints(manifold);
            }
        }
    }

    void JoltPhysicsWorld::OnContactRemoved(const JPH::SubShapeIDPair& subShapePair)
    {
        // TODO: thread unsafe access (for m_engineContactListener)
        if (!m_engineContactListener)
        {
            return;
        }

        JPH::lock_guard lock(m_bodiesInContactGuard);
        auto itContacts = m_bodiesInContact.find({subShapePair.GetBody1ID(), subShapePair.GetBody2ID()});
        if (itContacts == m_bodiesInContact.end())
        {
            NAU_LOG_DEBUG("Unknown contact is reported as ended from physics system. That's not supposed to happened");
            return;
        }

        ContactData& data = itContacts->second;
        data.contacts.erase(subShapePair);

        const bool contactsBetweenBodiesCompleted = data.contacts.empty();
        if (contactsBetweenBodiesCompleted)
        {
            auto& contactData = m_contactsData.emplace_back(ContactNotificationKind::Removed);
            contactData.object1 = {
                .sceneObjectUid = data.body1->getSceneObjectUid(),
                .material = nullptr};

            contactData.object2 = {
                .sceneObjectUid = data.body2->getSceneObjectUid(),
                .material = nullptr};
        }
    }

    void JoltPhysicsWorld::handleBodiesContact(const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings)
    {
        // Get friction and restitution from custom material or from bodies.
        auto [friction1, restitution1, mat1] = getFrictionAndRestitution(body1, manifold.mSubShapeID1);
        auto [friction2, restitution2, mat2] = getFrictionAndRestitution(body2, manifold.mSubShapeID2);

        // Use the default formulas for combining friction and restitution.
        settings.mCombinedFriction = sqrt(friction1 * friction2);
        settings.mCombinedRestitution = eastl::max(restitution1, restitution2);
    }

    eastl::vector<nau::math::vec3> JoltPhysicsWorld::calculateContactPoints(const JPH::ContactManifold& manifold)
    {
        // At this moment we have only rigid bodies. For simplicity assume that two bodies don't penetrate into each other.
        // In this case manifold.mRelativeContactPointsOn1 and manifold.mRelativeContactPointsOn2 is the same.
        eastl::vector<nau::math::vec3> result;
        for (JPH::uint idx = 0; idx < manifold.mRelativeContactPointsOn1.size(); ++idx)
        {
            const auto& point = manifold.GetWorldSpaceContactPointOn1(idx);
            result.push_back({point.GetX(), point.GetY(), point.GetZ()});
        }

        return result;
    }

    void JoltPhysicsWorld::debugDrawLine(const nau::math::Point3& pos0, const nau::math::Point3& pos1, const nau::math::Color4& color, float time)
    {
#ifdef NAU_DEBUG
        getDebugRenderer().drawLine(pos0, pos1, color, time);
#endif
    }

    eastl::tuple<float, float, const JoltPhysicsMaterial*> JoltPhysicsWorld::getFrictionAndRestitution(
        const JPH::Body& joltBody,
        const JPH::SubShapeID& subShapeID)
    {
        // Get the material that corresponds to the sub shape ID
        const auto material = reinterpret_cast<const JoltPhysicsMaterial*>(joltBody.GetShape()->GetMaterial(subShapeID));

        if (material != JPH::PhysicsMaterial::sDefault && material->engineMaterial())
        {
            if (const auto nauPhysicsMaterial = material->engineMaterial())
            {
                return eastl::make_tuple(
                    nauPhysicsMaterial->getFriction().value_or(joltBody.GetFriction()),
                    nauPhysicsMaterial->getRestitution().value_or(joltBody.GetRestitution()),
                    material);
            }
        }

        // This is the default material, use the settings from the body.
        return eastl::make_tuple(joltBody.GetFriction(), joltBody.GetRestitution(),
                                 reinterpret_cast<const JoltPhysicsMaterial*>(JPH::PhysicsMaterial::sDefault.GetPtr()));
    }

    void JoltPhysicsWorld::syncSceneState()
    {
        using namespace nau::scene;

        if (m_engineContactListener && !m_contactsData.empty())
        {
            scope_on_leave
            {
                m_contactsData.clear();
            };

            ISceneManager& sceneManager = getServiceProvider().get<ISceneManager>();

            for (InternalContactManifoldEntry& contact : m_contactsData)
            {
                ObjectWeakRef<> object1 = sceneManager.querySingleObject({QueryObjectCategory::Object, contact.object1.sceneObjectUid});
                ObjectWeakRef<> object2 = sceneManager.querySingleObject({QueryObjectCategory::Object, contact.object2.sceneObjectUid});

                if (!object1 || !object2)
                {
                    continue;
                }

                RigidBodyComponent* const rb1 = object1->as<SceneObject&>().findFirstComponent<RigidBodyComponent>();
                if (!rb1)
                {
                    NAU_LOG_WARNING("Contact notification, but rigid body does not exists:({})", object1->as<SceneObject&>().getName());
                    continue;
                }

                RigidBodyComponent* const rb2 = object1->as<SceneObject&>().findFirstComponent<RigidBodyComponent>();
                if (!rb2)
                {
                    NAU_LOG_WARNING("Contact notification, but rigid body does not exists:({})", object2->as<SceneObject&>().getName());
                    continue;
                }

                const IPhysicsContactListener::ContactManifold data1{
                    .rigidBody = *rb1,
                    .material = std::move(contact.object1.material)};

                const IPhysicsContactListener::ContactManifold data2{
                    .rigidBody = *rb2,
                    .material = std::move(contact.object2.material)};

                switch (contact.kind)
                {
                    case ContactNotificationKind::Added:
                    {
                        m_engineContactListener->onContactAdded(data1, data2, contact.collisionWorldPoints);
                        break;
                    }

                    case ContactNotificationKind::Continued:
                    {
                        m_engineContactListener->onContactContinued(data1, data2, contact.collisionWorldPoints);
                        break;
                    }

                    case ContactNotificationKind::Removed:
                    {
                        m_engineContactListener->onContactRemovedCompletely(data1, data2);
                        break;
                    }
                }
            }
        }
    }
}  // namespace nau::physics::jolt
