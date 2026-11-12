// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "usd_physics_rigid_body_cube.h"
#include <nau/service/service_provider.h>
#include "nau/physics/physics_world.h"

#include <nau/service/service_provider.h>

#include "nau/NauPhysicsSchema/rigidBodyCube.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include <pxr/usd/usd/stage.h>

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "RigidBodyCube";
    }

    PhysicsRigidCubeAdapter::PhysicsRigidCubeAdapter(PXR_NS::UsdPrim prim) :
        PhysicsRigidBodyAdapter(prim)
    {
    }

    std::string_view PhysicsRigidCubeAdapter::getType() const
    {
        return g_typeName;
    }

    void PhysicsRigidCubeAdapter::updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const
    {
        PXR_NS::PhysicsRigidBodyCube cubeBody{getPrim()};

        PXR_NS::GfVec3d extent;
        cubeBody.GetExtentAttr().Get(&extent);

        const nau::math::vec3 colliderExtent {static_cast<float>(extent[0]), static_cast<float>(extent[1]), static_cast<float>(extent[2])};

        // TODO: setup physics material (createMaterial(getPrim()).get())
        nau::physics::CollisionDescription collisions;
        collisions.addBox(colliderExtent);
        component.setCollisions(std::move(collisions));
    };

    DEFINE_TRANSLATOR(PhysicsRigidCubeAdapter, "RigidBodyCube"_tftoken);
}  // namespace UsdTranslator
