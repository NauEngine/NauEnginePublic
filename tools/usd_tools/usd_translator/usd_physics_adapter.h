// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include <nau/scene/scene.h>
#include <pxr/usd/usd/stage.h>

#include "nau/physics/physics_body.h"
#include "nau/assets/asset_path.h"
#include "usd_prim_adapter.h"
#include "usd_prim_translator.h"
#include "usd_proxy/usd_proxy.h"
#include "nau/physics/physics_material.h"
#include "nau/physics/physics_body.h"
#include "nau/physics/components/rigid_body_component.h"

namespace UsdTranslator
{
    class PhysicsRigidBodyAdapter : public IPrimAdapter
    {
    public:
        PhysicsRigidBodyAdapter(PXR_NS::UsdPrim prim);

        bool isValid() const override;
        nau::async::Task<> update() override;

        nau::async::Task<nau::scene::ObjectWeakRef<nau::scene::SceneObject>> initializeSceneObject(nau::scene::ObjectWeakRef<nau::scene::SceneObject> dest) override;
        nau::scene::ObjectWeakRef<nau::scene::SceneObject> getSceneObject() const override;

    protected:
        void destroySceneObject() override;
        
        // Initial-only setup, including mesh collision assets.
        virtual void fillRigidBodyComponent(nau::physics::RigidBodyComponent& component) const {};

        // Called at initialization and on subsequent USD edits. Mesh assets are initialized separately.
        virtual void updateRigidBodyComponent(nau::physics::RigidBodyComponent& component) const {};

        void preInitRigidBodyComponent(nau::physics::RigidBodyComponent& component) const;

        static nau::AssetPath getMeshAsset(const PXR_NS::SdfAssetPath& sdfPath);
        static nau::physics::IPhysicsMaterial::Ptr createMaterial(const PXR_NS::UsdPrim& prim);

    protected:

        nau::scene::ObjectWeakRef<nau::scene::SceneObject> m_obj;
        nau::scene::ObjectWeakRef<nau::physics::RigidBodyComponent> m_component;
    };
}  // namespace UsdTranslator
