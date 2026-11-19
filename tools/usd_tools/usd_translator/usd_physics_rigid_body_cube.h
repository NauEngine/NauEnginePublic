// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "usd_physics_adapter.h"

namespace UsdTranslator
{
    class PhysicsRigidCubeAdapter : public PhysicsRigidBodyAdapter
    {
    public:
        PhysicsRigidCubeAdapter(PXR_NS::UsdPrim prim);

        std::string_view getType() const override;

    protected:
        void updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const override;
    };
}  // namespace UsdTranslator
