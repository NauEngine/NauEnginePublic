// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "usd_physics_rigid_body_capsule.h"

#include <nau/service/service_provider.h>

#include "nau/NauPhysicsSchema/rigidBodyCapsule.h"
#include "nau/physics/physics_collision_shapes_factory.h"

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "RigidBodyCapsule";
    }

    PhysicsRigidCapsuleAdapter::PhysicsRigidCapsuleAdapter(PXR_NS::UsdPrim prim) :
        PhysicsRigidBodyAdapter(prim)
    {
    }

    std::string_view PhysicsRigidCapsuleAdapter::getType() const
    {
        return g_typeName;
    }

    void PhysicsRigidCapsuleAdapter::updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const
    {
        PXR_NS::PhysicsRigidBodyCapsule capsuleBody{getPrim()};

        double radius{};
        capsuleBody.GetRadiusAttr().Get(&radius);

        double height{};
        capsuleBody.GetHeightAttr().Get(&height);

        // TODO: setup physics material
        nau::physics::CollisionDescription collisions;
        collisions.addCapsule(static_cast<float>(height), static_cast<float>(radius));
        component.setCollisions(std::move(collisions));
    };

    DEFINE_TRANSLATOR(PhysicsRigidCapsuleAdapter, "RigidBodyCapsule"_tftoken);
}  // namespace UsdTranslator
