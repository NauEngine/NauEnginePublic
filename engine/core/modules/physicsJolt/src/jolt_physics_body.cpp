// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/physics/jolt/jolt_physics_body.h"
#include "nau/physics/jolt/jolt_physics_collider.h"
#include "nau/physics/jolt/jolt_physics_math.h"
#include "nau/physics/jolt/jolt_physics_world.h"
#include "jolt_physics_layers.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/EActivation.h>

#include <type_traits>

namespace nau::physics::jolt
{
    JoltPhysicsBody::JoltPhysicsBody(Ptr<JoltPhysicsWorld> physWorld,
        Uid originObjectUid, const PhysicsBodyCreationData& creationData)
        : m_physWorld(std::move(physWorld))
        , m_sceneObjectUid(originObjectUid)
    {
        NAU_ASSERT(m_physWorld);

        initializeJoltBody(creationData);
    }

    JoltPhysicsBody::~JoltPhysicsBody()
    {
        if (m_physWorld && m_physWorld->getBodyInterface().IsAdded(m_bodyId))
        {
            m_physWorld->getBodyInterface().RemoveBody(m_bodyId);
            m_physWorld->getBodyInterface().DestroyBody(m_bodyId);
        }
    }

    void JoltPhysicsBody::getTransform(math::mat4& transform) const
    {
        JPH::RMat44 bodyTransform = m_physWorld->getBodyInterface().GetWorldTransform(m_bodyId);
        transform = joltMatToNauMat(bodyTransform);
    }

    void JoltPhysicsBody::setTransform(const nau::math::Transform& transform)
    {
        if (m_physWorld && m_physWorld->getBodyInterface().IsAdded(m_bodyId))
        {
            m_physWorld->getBodyInterface().SetPositionAndRotation(m_bodyId,
                vec3ToJolt(transform.getTranslation()),
                quatToJolt(transform.getRotation()),
                JPH::EActivation::DontActivate);
        }
    }

    void JoltPhysicsBody::setCollisionChannel(CollisionChannel channel)
    {
        static_assert(std::is_convertible_v<CollisionChannel, JPH::ObjectLayer>,
            "Implementation type is not compatible with required type for physics collision channel");

        m_physWorld->getBodyInterface().SetObjectLayer(m_bodyId, channel);
    }

    bool JoltPhysicsBody::setCollisionShape(const ICollisionShape& shape)
    {
        const auto* collider = shape.as<const JoltCollisionShape*>();
        if (!collider || !m_physWorld || m_bodyId.IsInvalid())
        {
            return false;
        }

        auto joltShape = collider->getCollisionShape();
        if (!joltShape)
        {
            return false;
        }

        if (m_comOffset != math::vec3::zero())
        {
            auto shiftedShape = JPH::OffsetCenterOfMassShapeSettings(vec3ToJolt(m_comOffset), joltShape).Create();
            if (shiftedShape.HasError())
            {
                NAU_LOG_ERROR("Cannot replace collision shape: {}", shiftedShape.GetError());
                return false;
            }
            joltShape = shiftedShape.Get();
        }

        auto& bodyInterface = m_physWorld->getBodyInterface();
        if (joltShape->MustBeStatic() && bodyInterface.GetMotionType(m_bodyId) != JPH::EMotionType::Static)
        {
            return false;
        }

        bodyInterface.SetShape(m_bodyId, joltShape, true, JPH::EActivation::Activate);
        return true;
    }

    void JoltPhysicsBody::setDebugDrawEnabled(bool enabled)
    {
        m_debugDrawEnabled = enabled;
    }

    void JoltPhysicsBody::setCenterMassShift(const nau::math::vec3& shift)
    {
        NAU_LOG_ERROR("Physics: updating center of mass shift not implemented in Jolt wrapper");
    }

    bool JoltPhysicsBody::isTrigger() const
    {
        return m_isTrigger;
    }

    void JoltPhysicsBody::addForce(const nau::math::vec3& force)
    {
        if (m_physWorld)
        {
            m_physWorld->getBodyInterface().AddForce(m_bodyId, vec3ToJolt(force));
        }
    }

    void JoltPhysicsBody::addForce(const nau::math::vec3& force, const nau::math::vec3& applyPoint)
    {
        if (m_physWorld)
        {
            m_physWorld->getBodyInterface().AddForce(m_bodyId, vec3ToJolt(force), vec3ToJolt(applyPoint));
        }
    }

    void JoltPhysicsBody::addTorque(const nau::math::vec3& torque)
    {
        if (m_physWorld)
        {
            m_physWorld->getBodyInterface().AddTorque(m_bodyId, vec3ToJolt(torque));
        }
    }

    void JoltPhysicsBody::addImpulse(const nau::math::vec3& impulse)
    {
        if (m_physWorld)
        {
            m_physWorld->getBodyInterface().AddImpulse(m_bodyId, vec3ToJolt(impulse));
        }
    }

    void JoltPhysicsBody::addImpulse(const nau::math::vec3& impulse, const nau::math::vec3& applyPoint)
    {
        if (m_physWorld)
        {
            m_physWorld->getBodyInterface().AddImpulse(m_bodyId, vec3ToJolt(impulse), vec3ToJolt(applyPoint));
        }
    }

    bool JoltPhysicsBody::debugDrawEnabled() const
    {
        return m_debugDrawEnabled;
    }

    Uid JoltPhysicsBody::getSceneObjectUid() const
    {
        return m_sceneObjectUid;
    }

    void JoltPhysicsBody::initializeJoltBody(const PhysicsBodyCreationData& creationData)
    {
        NAU_ASSERT(creationData.collisionShape);
        if (!creationData.collisionShape)
        {
            NAU_LOG_ERROR("Invalid PhysicsBodyCreationData: shape is missing");
            return;
        }

        JPH::BodyCreationSettings joltBodySettings;
        m_comOffset = creationData.comOffset;

        const auto& joltCollider = creationData.collisionShape->as<const JoltCollisionShape&>();
        if (creationData.comOffset != nau::math::vec3::zero())
        {
            auto shapeWithShiftedCOM = JPH::OffsetCenterOfMassShapeSettings(vec3ToJolt(creationData.comOffset),
                joltCollider.getCollisionShape()).Create().Get();

            joltBodySettings.SetShape(shapeWithShiftedCOM);
        }
        else
        {
            joltBodySettings.SetShape(joltCollider.getCollisionShape());
        }

        if (creationData.motionType == nau::physics::MotionType::Static)
        {
            joltBodySettings.mMotionType = JPH::EMotionType::Static;
        }
        else
        {
            if (creationData.collisionShape->is<JoltMeshCollision>())
            {
                // https://jrouwe.github.io/JoltPhysics/
                // Dynamic or kinematic mesh shapes cannot calculate their mass and inertia, so we have to provide them.
                joltBodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::MassAndInertiaProvided;
                joltBodySettings.mMassPropertiesOverride.mMass = creationData.mass;
                joltBodySettings.mMassPropertiesOverride.mInertia = JPH::Mat44::sZero();
            }

            joltBodySettings.mMotionType = creationData.mass == .0f ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic;
        }

        nau::math::Vector3 wpos;
        
        joltBodySettings.mPosition = vec3ToJolt(creationData.position);
        joltBodySettings.mRotation = quatToJolt(creationData.rotation);
        joltBodySettings.mObjectLayer = creationData.collisionChannel;
        joltBodySettings.mIsSensor = creationData.isTrigger;
        m_isTrigger = creationData.isTrigger;

        if (creationData.friction >= 0)
        {
            joltBodySettings.mFriction = creationData.friction;
        }
        if (creationData.restitution >= 0)
        {
            joltBodySettings.mRestitution = creationData.restitution;
        }

        // If object have fall a sleep, jolt calls ContactListener::onContactRemoved.
        joltBodySettings.mAllowSleeping = false;

        if (auto* joltBody = m_physWorld->getBodyInterface().CreateBody(joltBodySettings))
        {
            m_bodyId = joltBody->GetID();

            // We need quick reference from the jolt body to abstracted engine body.
            // Mainly for reporting about collisions to the user layer.
            joltBody->SetUserData(reinterpret_cast<uint64_t>(this));

            m_debugDrawEnabled = creationData.debugDraw;
            m_physWorld->getBodyInterface().AddBody(m_bodyId, JPH::EActivation::Activate);
        }
    }

} // namespace nau::physics::jolt

