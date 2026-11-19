// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/physics/physics_body.h"
#include "nau/scene/scene_object.h"
#include "nau/physics/physics_defines.h"
#include "nau/rtti/rtti_impl.h"

#include <EASTL/shared_ptr.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>

namespace nau::physics::jolt
{
    class JoltCollisionShape;
    class JoltPhysicsWorld;

    /**
     * @brief Implements nau::physics::IPhysicsBody interface utilizing Jolt physics engine.
     */
    class JoltPhysicsBody : public IPhysicsBody
    {
        NAU_CLASS(nau::physics::jolt::JoltPhysicsBody, rtti::RCPolicy::Concurrent, IPhysicsBody)

    public:
        JoltPhysicsBody(Ptr<JoltPhysicsWorld> physWorld, Uid originObjectUid, const PhysicsBodyCreationData& creationData);
        ~JoltPhysicsBody();

        bool isValid() const { return !m_bodyId.IsInvalid(); }

        /**
         * @brief Retrieves the transformation matrix of the body.
         *
         * @param [out] Output transformation matrix.
         */
        void getTransform(math::mat4& transform) const override;

        /**
         * @brief Sets the body transform to the specified value.
         * 
         * @param [in] transform Value to assign.
         */
        void setTransform(const nau::math::Transform& transform) override;

        bool setCollisionShape(const ICollisionShape& shape) override;

        /**
         * @brief Attaches a collision channel to the body.
         *
         * @param [in] channel Index of the collision channel to attach the body to.
         */
        void setCollisionChannel(CollisionChannel channel) override;

        /**
         * @brief Switches physics debug drawing for the body on or off.
         * 
         * @param [in] enabled  Indicates whether physics debug drawing should be turned on or off.
         */
        void setDebugDrawEnabled(bool enabled) override;

        /**
         * @brief Sets the body shift of center of the mass.
         * 
         * @param [in] shift vector.
         */
        void setCenterMassShift(const nau::math::vec3& shift) override;

        /** 
         * @brief Checks whether the body is used a trigger.
         * 
         * @return `true` if the body is trigger, `false` otherwise (i.e. the body is a collidable object).
         */
        bool isTrigger() const override;

        /**
         * @brief Applies the force to the center of mass of the physics body.
         * 
         * @param [in] force Force to apply.
         */
        void addForce(const nau::math::vec3& force) override;

        /**
         * @brief Applies the force to the physics body.
         *
         * @param [in] force        Force to apply.
         * @param [in] applyPoint   Point to apply the force at.
         */
        void addForce(const nau::math::vec3& force, const nau::math::vec3& applyPoint) override;

        /**
         * @brief Applies the torque to the physics body.
         * 
         * @param [in] torque Torque to apply.
         */
        void addTorque(const nau::math::vec3& torque) override;

        /**
         * @brief Applies the force to the center of mass of the physics body.
         *
         * @param [in] impulse Impulse to apply.
         */
        void addImpulse(const nau::math::vec3& impulse) override;

        /**
         * @brief Applies the impulse to the physics body.
         *
         * @param [in] force        Impulse to apply.
         * @param [in] applyPoint   Point to apply the impulse at.
         */
        void addImpulse(const nau::math::vec3& impulse, const nau::math::vec3& applyPoint) override;

        /**
         * @brief Checks whether debug drawing is enabled for the body.
         * 
         * @return `true` if debug drawing is enabled for the body, `false` otherwise.
         */
        bool debugDrawEnabled() const;

        //nau::scene::ObjectWeakRef<nau::scene::SceneObject> getSceneObject() const;
        //nau::physics::RigidBodyComponent* component() const;
        Uid getSceneObjectUid() const;

    private:

        /**
         * @brief Creates the body and adds it to the physical world.
         * 
         * @param [in] creationData Physical properties of the body.
         */
        void initializeJoltBody(const PhysicsBodyCreationData& creationData);

    private:
        eastl::shared_ptr<JoltCollisionShape> m_collisionShape;
        //nau::physics::RigidBodyComponent* m_component = nullptr;

        Ptr<JoltPhysicsWorld> m_physWorld;
        //nau::scene::ObjectWeakRef<nau::scene::SceneObject> m_sceneObject;
        Uid m_sceneObjectUid;
        math::vec3 m_comOffset = math::vec3::zero();

        JPH::BodyID m_bodyId; 												/** < A handle to the body within the physical world. */

        bool m_debugDrawEnabled = false;                                    /** < Indicates whether debug drawing is enabled for the body. */
        bool m_isTrigger = false;                                           /** < Indicates whether the body is a trigger or not (i.e. it is a collidable object).*/
    };
} // namespace nau::physics::jolt

