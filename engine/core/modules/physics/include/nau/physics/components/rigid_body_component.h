// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/meta/class_info.h"
#include "nau/physics/components/colliders.h"
#include "nau/physics/physics_body.h"
#include "nau/rtti/ptr.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/components/scene_component.h"
#include "nau/utils/functor.h"

namespace nau::physics
{
    /**
     * @brief Implements rigid body component logic.
     *
     * Such components can be attached to scene objects to make them exhibit 'physically correct' behavior.
     * Concrete behavior relies on the utilized physics engine and IPhysicsBody implementation.
     */
    class NAU_PHYSICS_EXPORT RigidBodyComponent : public scene::SceneComponent,
                                                  public scene::IComponentUpdate
    {
        NAU_OBJECT(nau::physics::RigidBodyComponent, scene::SceneComponent, scene::IComponentUpdate)
        NAU_DECLARE_DYNAMIC_OBJECT

        NAU_CLASS_FIELDS(
            CLASS_NAMED_FIELD(m_collisions, "collisions"),

            CLASS_NAMED_FIELD(m_meshCollisionAsset, "meshCollision"),
            CLASS_NAMED_FIELD(m_useConvexHullForCollision, "useConvexHullForCollision"),
            CLASS_NAMED_FIELD(m_collisionChannel, "collisionChannel"),
            CLASS_NAMED_FIELD(m_motionType, "motionType"),
            CLASS_NAMED_FIELD(m_mass, "mass"),
            CLASS_NAMED_FIELD(m_friction, "friction"),
            CLASS_NAMED_FIELD(m_restitution, "restitution"),
            CLASS_NAMED_FIELD(m_isTrigger, "isTrigger"),
            CLASS_NAMED_FIELD(m_comShift, "centerMassShift"))

    public:
        /**
         * @brief Keeps the parent scene object transformation up to date with the transformation of the associated body in the physical world.
         */
        virtual void updateComponent(float dt) override;

        /** Replaces primitive geometry, including the shape of an already active body. */
        void setCollisions(CollisionDescription collisions);
        const CollisionDescription& getCollisions() const;
        /** Builds initial geometry. Call setCollisions after editing an active component. */
        CollisionDescription& getCollisions();

        void setMeshCollision(AssetRef<>);
        AssetRef<> getMeshCollision() const;

        void setUseConvexHullForCollision(bool useConvexHull);
        bool useConvexHullForCollision() const;

        void setCollisionChannel(CollisionChannel channel);
        CollisionChannel getCollisionChannel() const;

        void setMotionType(MotionType motionType);
        MotionType getMotionType() const;

        void setMass(TFloat mass);
        TFloat getMass() const;

        void setFriction(TFloat friction);
        TFloat getFriction() const;

        void setRestitution(TFloat restitution);
        TFloat getRestitution() const;

        void setIsTrigger(bool isTrigger);
        bool isTrigger() const;

        void setDebugDrawEnabled(bool enabled);
        bool isDebugDrawEnabled() const;

        void setCenterMassShift(math::vec3 shift);
        const math::vec3& centerMassShift() const;

        void addForce(math::vec3 force);
        void addForce(math::vec3 force, math::vec3 applyPoint);

        void addTorque(math::vec3 torque);

        void addImpulse(math::vec3 impulse);
        void addImpulse(math::vec3 impulse, math::vec3 applyPoint);

    private:
        // TODO: Collision info must not present directly within RigidBodyComponent,
        // but instead it should be within
        using PhysicsBodyAction = Functor<void (IPhysicsBody& body)>;
        using PhysicsBodyActions = eastl::vector<PhysicsBodyAction>;

        void applyPhysicsBodyActions(IPhysicsBody* body);

        CollisionDescription m_collisions;
        uint64_t m_collisionRevision = 0;

        AssetRef<> m_meshCollisionAsset;
        bool m_useConvexHullForCollision = false;

        // TODO: seems collision channel should be a string
        CollisionChannel m_collisionChannel = 0;
        MotionType m_motionType = MotionType::Static;
        TFloat m_mass = 0.0f;
        TFloat m_friction = 0.0f;
        TFloat m_restitution = 0.0f;
        bool m_isTrigger = false;
        bool m_isDebugDrawEnabled = false;
        math::vec3 m_comShift = math::vec3::zero();

        PhysicsBodyActions m_pendingActions;

        friend class PhysicsWorldState;

    };
}  // namespace nau::physics
