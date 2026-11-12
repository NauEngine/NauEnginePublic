// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/physics/components/rigid_body_component.h"

#include "nau/physics/physics_body.h"
#include "nau/scene/scene_object.h"

namespace nau::physics
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(RigidBodyComponent)

    void RigidBodyComponent::updateComponent(float)
    {
    }

    void RigidBodyComponent::setCollisions(CollisionDescription collisions)
    {
        m_collisions = std::move(collisions);
        ++m_collisionRevision;
    }

    const CollisionDescription& RigidBodyComponent::getCollisions() const
    {
        return m_collisions;
    }

    CollisionDescription& RigidBodyComponent::getCollisions()
    {
        return m_collisions;
    }

    void RigidBodyComponent::setMeshCollision(AssetRef<> meshAsset)
    {
        m_meshCollisionAsset = std::move(meshAsset);
    }

    AssetRef<> RigidBodyComponent::getMeshCollision() const
    {
        return m_meshCollisionAsset;
    }

    void RigidBodyComponent::setUseConvexHullForCollision(bool useConvexHull)
    {
        m_useConvexHullForCollision = useConvexHull;
    }

    bool RigidBodyComponent::useConvexHullForCollision() const
    {
        return m_useConvexHullForCollision;
    }

    void RigidBodyComponent::setCollisionChannel(CollisionChannel channel)
    {
        value_changes_scope;
        m_collisionChannel = channel;
    }

    CollisionChannel RigidBodyComponent::getCollisionChannel() const
    {
        return m_collisionChannel;
    }

    void RigidBodyComponent::setMotionType(MotionType motionType)
    {
        m_motionType = motionType;
    }

    MotionType RigidBodyComponent::getMotionType() const
    {
        return m_motionType;
    }

    void RigidBodyComponent::setMass(TFloat mass)
    {
        m_mass = mass;
    }

    TFloat RigidBodyComponent::getMass() const
    {
        return m_mass;
    }

    void RigidBodyComponent::setFriction(TFloat friction)
    {
        m_friction = friction;
    }

    TFloat RigidBodyComponent::getFriction() const
    {
        return m_friction;
    }

    void RigidBodyComponent::setRestitution(TFloat restitution)
    {
        m_restitution = restitution;
    }

    TFloat RigidBodyComponent::getRestitution() const
    {
        return m_restitution;
    }

    void RigidBodyComponent::setIsTrigger(bool isTrigger)
    {
        m_isTrigger = isTrigger;
    }

    bool RigidBodyComponent::isTrigger() const
    {
        return m_isTrigger;
    }

    void RigidBodyComponent::setDebugDrawEnabled(bool enabled)
    {
        m_isDebugDrawEnabled = enabled;
    }

    bool RigidBodyComponent::isDebugDrawEnabled() const
    {
        return m_isDebugDrawEnabled;
    }

    void RigidBodyComponent::setCenterMassShift(math::vec3 shift)
    {
        m_comShift = shift;
    }

    const math::vec3& RigidBodyComponent::centerMassShift() const
    {
        return m_comShift;
    }

    void RigidBodyComponent::addForce(math::vec3 force)
    {
        m_pendingActions.emplace_back([force](IPhysicsBody& body)
        {
            body.addForce(force);
        });
    }

    void RigidBodyComponent::addForce(math::vec3 force, math::vec3 applyPoint)
    {
        m_pendingActions.emplace_back([force, applyPoint](IPhysicsBody& body)
        {
            body.addForce(force, applyPoint);
        });
    }

    void RigidBodyComponent::addTorque(math::vec3 torque)
    {
        m_pendingActions.emplace_back([torque](IPhysicsBody& body)
        {
            body.addTorque(torque);
        });
    }

    void RigidBodyComponent::addImpulse(math::vec3 impulse)
    {
        m_pendingActions.emplace_back([impulse](IPhysicsBody& body)
        {
            body.addImpulse(impulse);
        });
    }

    void RigidBodyComponent::addImpulse(math::vec3 impulse, math::vec3 applyPoint)
    {
        m_pendingActions.emplace_back([impulse, applyPoint](IPhysicsBody& body)
        {
            body.addImpulse(impulse, applyPoint);
        });
    }

    void RigidBodyComponent::applyPhysicsBodyActions(IPhysicsBody* body)
    {
        if (body)
        {
            for (auto& action : m_pendingActions)
            {
                action(*body);
            }
        }

        m_pendingActions.clear();
    }
}  // namespace nau::physics
