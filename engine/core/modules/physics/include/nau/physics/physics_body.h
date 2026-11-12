// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/physics/physics_collider.h"
#include "nau/physics/physics_defines.h"
#include "nau/math/math.h"
#include "nau/math/transform.h"
#include "nau/rtti/rtti_impl.h"

namespace nau::physics
{
    class RigidBodyComponent;

    /**
     * @brief Encapsulates physical properties that are used to initialize a physical body.
     */
    struct PhysicsBodyCreationData
    {
        /**
         * @brief A pointer to the collision shape to be attached to the associated body.
         */
        ICollisionShape::Ptr collisionShape;

        /**
         * @brief Collision channel to be attached to the associated body.
         */
        CollisionChannel collisionChannel{};

        /**
         * @brief Mass of the associated body.
         */
        TFloat mass = .0f;

        /**
         * @brief Degree of how the body resists being dragged. It has to be between 0.0 (no friction) and 1.0 (the body will stick to the surface and stay immobile).
         */
        TFloat friction = .0f;

        /**
         * @brief Degree of body tougheness on collision. It has to be between 0.0 (completely inelastic collision response) and 1.0 (completely elastic collision response).
         */
        TFloat restitution = .0f;

        /**
         * @brief Body initial position.
         */
        math::vec3 position = math::vec3::zero();

        /**
         * @brief Body initial rotation.
         */
        math::quat rotation = math::quat::identity();

        /**
         * Center of mass offset.
         */
        math::vec3 comOffset = math::vec3::zero();

        /**
         * @brief Indicates body motion type (i.e. whether it is static, kinematic or dynamic).
         */
        MotionType motionType = MotionType::Static;

        bool debugDraw = false; /** < Indicates whether the body shape is rendered at debug drawing. */

        bool isTrigger = false; /** < Indicates whether the body is a trigger or not (i.e. it is a collidable object).*/
    };

    /*
     */
    class NAU_ABSTRACT_TYPE IPhysicsBody : public virtual IRefCounted
    {
        NAU_INTERFACE(nau::physics::IPhysicsBody, IRefCounted)

        /**
         * @brief Retrieves the transformation matrix of the body.
         * 
         * @param [out] Output transformation matrix.
         */
        virtual void getTransform(math::mat4& transform) const = 0;
        
        /**
         * @brief Sets the body transform to the specified value.
         * 
         * @param [in] transform Value to assign.
         */
        virtual void setTransform(const nau::math::Transform& transform) = 0;

        /**
         * @brief Replaces collision geometry while preserving body identity, velocity, collision channel and trigger state.
         * Mass and inertia are recalculated from the new shape.
         * @return False if the shape cannot be applied; the previous shape is retained.
         */
        virtual bool setCollisionShape(const ICollisionShape& shape) = 0;

        /**
         * @brief Attaches a collision channel to the body.
         * 
         * @param [in] channel Index of the collision channel to attach the body to.
         * 
         * Only a single collision channel is allowed for a body.
         */
        virtual void setCollisionChannel(CollisionChannel channel) = 0;

        /**
         * @brief Sets the body transform to the specified value.
         *
         * @param [in] transform Value to assign.
         */
        virtual void setDebugDrawEnabled(bool enabled) = 0;

        /**
         * @brief Sets the body shift of center of the mass.
         * 
         * @param [in] shift vector.
         */
        virtual void setCenterMassShift(const nau::math::vec3& shift) = 0;

        /** 
         * @brief Checks whether the body is used a trigger.
         * 
         * @return `true` if the body is trigger, `false` otherwise (i.e. the body is a collidable object).
         */
        virtual bool isTrigger() const = 0;

        virtual void addForce(const nau::math::vec3& force) = 0;
        virtual void addForce(const nau::math::vec3& force, const nau::math::vec3& applyPoint) = 0;

        virtual void addTorque(const nau::math::vec3& torque) = 0;

        virtual void addImpulse(const nau::math::vec3& impulse) = 0;
        virtual void addImpulse(const nau::math::vec3& impulse, const nau::math::vec3& applyPoint) = 0;
    };
}  // namespace nau::physics
