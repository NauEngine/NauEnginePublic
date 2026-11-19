// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "usd_physics_adapter.h"

#include <EASTL/string.h>
#include <nau/math/math.h>
#include <nau/scene/scene_factory.h>
#include <nau/service/service_provider.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCache.h>

#include <filesystem>

#include "nau/NauPhysicsSchema/physicsMaterial.h"
#include "nau/NauPhysicsSchema/rigidBody.h"
#include "nau/service/service_provider.h"
#include "nau/diag/logging.h"
#include "nau/assets/asset_db.h"
#include "nau/physics/components/rigid_body_component.h"
#include "nau/physics/physics_body.h"
#include "nau/physics/physics_collision_shapes_factory.h"
#include "nau/physics/physics_contact_listener.h"
#include "nau/physics/physics_material.h"
#include "nau/physics/physics_world.h"
#include "usd_proxy/usd_proxy_decorators_regestry.h"

using namespace nau;
using namespace nau::scene;
using namespace PXR_NS;

namespace UsdTranslator
{
    PhysicsRigidBodyAdapter::PhysicsRigidBodyAdapter(PXR_NS::UsdPrim prim) :
        IPrimAdapter(prim)
    {
    }

    bool PhysicsRigidBodyAdapter::isValid() const
    {
        return static_cast<bool>(m_obj);
    }

    nau::async::Task<> PhysicsRigidBodyAdapter::update()
    {
        using namespace nau::physics;
        if (!m_obj)
        {
            co_return;
        }

        UsdGeomXformCache cache;
        bool resetsXformStack = false;
        const auto localTransform = cache.GetLocalTransformation(getPrim(), &resetsXformStack);
        const math::vec3 scale{float(localTransform.GetRow3(0).GetLength()),
            float(localTransform.GetRow3(1).GetLength()), float(localTransform.GetRow3(2).GetLength())};

        if (!m_component)
        {
            m_component = m_obj->addComponent<RigidBodyComponent>([this, scale](RigidBodyComponent& component)
            {
                preInitRigidBodyComponent(component);
                fillRigidBodyComponent(component);
                component.setScale(scale);
                updateRigidBodyComponent(component);
            });
        }
        else
        {
            m_component->setScale(scale);
            updateRigidBodyComponent(*m_component);
        }
    }

    nau::async::Task<nau::scene::ObjectWeakRef<nau::scene::SceneObject>> PhysicsRigidBodyAdapter::initializeSceneObject(
        nau::scene::ObjectWeakRef<nau::scene::SceneObject> dest)
    {
        m_obj = dest;

        co_await update();
        co_return m_obj;
    }

    nau::scene::ObjectWeakRef<nau::scene::SceneObject> PhysicsRigidBodyAdapter::getSceneObject() const
    {
        return m_obj;
    }

    void PhysicsRigidBodyAdapter::destroySceneObject()
    {
        if (m_obj && m_component)
        {
            m_obj->removeComponent(m_component);
        }
        // m_physBody.reset();
        m_component = nullptr;
        m_obj = nullptr;
    }

    void PhysicsRigidBodyAdapter::preInitRigidBodyComponent(nau::physics::RigidBodyComponent& component) const
    {
        using namespace nau::physics;

        const PhysicsRigidBody pxBody{getPrim()};


            bool isStatic = false;
            pxBody.GetIsStaticAttr().Get(&isStatic);
            component.setMotionType(isStatic ? MotionType::Static : MotionType::Dynamic);



            bool isTrigger = false;
            pxBody.GetIsTriggerAttr().Get(&isTrigger);
            component.setIsTrigger(isTrigger);


        double value = 0.0;

        pxBody.GetMassAttr().Get(&value);
        component.setMass(static_cast<TFloat>(value));

        pxBody.GetFrictionAttr().Get(&value);
        component.setFriction(static_cast<TFloat>(value));

        pxBody.GetRestitutionAttr().Get(&value);
        component.setRestitution(static_cast<TFloat>(value));

        bool debugDrawEnabled = false;
        pxBody.GetDebugDrawAttr().Get(&debugDrawEnabled);
        component.setDebugDrawEnabled(debugDrawEnabled);

        PXR_NS::GfVec3d offset;
        pxBody.GetCenterOfMassShiftAttr().Get(&offset);
        component.setCenterMassShift( {float(offset[0]), float(offset[1]), float(offset[2])});

        int channel = 0;
        pxBody.GetPhysicsCollisionChannelAttr().Get(&channel);
        component.setCollisionChannel(static_cast<CollisionChannel>(channel));

    }

    nau::AssetPath PhysicsRigidBodyAdapter::getMeshAsset(const PXR_NS::SdfAssetPath& sdfPath)
    {
        static const std::string_view scheme = "uid";

        std::string assetPath = sdfPath.GetAssetPath();
        if (assetPath.empty())
        {
            return {};
        }

        if (assetPath.starts_with(scheme))
        {
            const auto uuid = assetPath.substr(scheme.size() + 1);
            return nau::AssetPath(scheme.data(), uuid.c_str(), "mesh/0");
        }

        const auto uuid = nau::getServiceProvider().get<nau::IAssetDB>().getUidFromNausdPath(assetPath.c_str());
        return nau::AssetPath(scheme.data(), toString(uuid).c_str(), "mesh/0");
    }

    nau::physics::IPhysicsMaterial::Ptr PhysicsRigidBodyAdapter::createMaterial(const PXR_NS::UsdPrim& prim)
    {
        // ToDo 
        return {};
    }
}  // namespace UsdTranslator
