// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "usd_physics_rigid_body_cylinder.h"

#include <nau/service/service_provider.h>

#include "nau/NauPhysicsSchema/rigidBodyCylinder.h"
#include "nau/physics/physics_collision_shapes_factory.h"

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "RigidBodyCylinder";
    }

    PhysicsRigidCylinderAdapter::PhysicsRigidCylinderAdapter(PXR_NS::UsdPrim prim) :
        PhysicsRigidBodyAdapter(prim)
    {
    }

    std::string_view PhysicsRigidCylinderAdapter::getType() const
    {
        return g_typeName;
    }

    void PhysicsRigidCylinderAdapter::updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const
    {
        PXR_NS::PhysicsRigidBodyCylinder cylinderBody{getPrim()};

        double radius{};
        cylinderBody.GetRadiusAttr().Get(&radius);

        double height{};
        cylinderBody.GetHeightAttr().Get(&height);

        // TODO: setup physics material
        nau::physics::CollisionDescription collisions;
        collisions.addCylinder(static_cast<float>(height), static_cast<float>(radius));
        component.setCollisions(std::move(collisions));
    };

    DEFINE_TRANSLATOR(PhysicsRigidCylinderAdapter, "RigidBodyCylinder"_tftoken);
}  // namespace UsdTranslator
