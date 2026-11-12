// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "usd_physics_rigid_body_sphere.h"

#include <nau/service/service_provider.h>

#include "nau/NauPhysicsSchema/rigidBodySphere.h"
#include "nau/physics/physics_collision_shapes_factory.h"

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "RigidBodySphere";
    }

    PhysicsRigidSphereAdapter::PhysicsRigidSphereAdapter(PXR_NS::UsdPrim prim) :
        PhysicsRigidBodyAdapter(prim)
    {
    }

    std::string_view PhysicsRigidSphereAdapter::getType() const
    {
        return g_typeName;
    }

    void PhysicsRigidSphereAdapter::updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const
    {
        PXR_NS::PhysicsRigidBodySphere sphereBody{getPrim()};

        double radius{};
        sphereBody.GetRadiusAttr().Get(&radius);

        // TODO: setup physics material
        nau::physics::CollisionDescription collisions;
        collisions.addSphere(static_cast<float>(radius));
        component.setCollisions(std::move(collisions));
    };

    DEFINE_TRANSLATOR(PhysicsRigidSphereAdapter, "RigidBodySphere"_tftoken);
}  // namespace UsdTranslator
