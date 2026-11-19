// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/physics/jolt/jolt_physics_collider.h"

#include "nau/physics//jolt/jolt_physics_shapes.h"
#include "nau/physics/jolt/jolt_physics_material.h"
#include "nau/physics/jolt/jolt_physics_math.h"

#include <cmath>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>

namespace nau::physics::jolt
{
    namespace
    {
        inline TFloat getBoxConvexRadius(math::vec3 extent)
        {
            return eastl::min(eastl::min(JPH::cDefaultConvexRadius, extent.getX() * 0.5f), eastl::min(extent.getY() * 0.5f, extent.getZ() * 0.5f));
        }

        inline TFloat getCylinderConvexRadius(TFloat halfHeight, TFloat radius)
        {
            return eastl::min(eastl::min(JPH::cDefaultConvexRadius, halfHeight), radius);
        }

    }  // namespace

    JoltCollisionShape::JoltCollisionShape() = default;

    JoltCollisionShape::JoltCollisionShape(JPH::RefConst<JPH::Shape> shape) :
        m_collisionShape(std::move(shape))
    {
    }

    JPH::RefConst<JPH::Shape> JoltCollisionShape::getCollisionShape() const
    {
        auto shape = m_collisionShape;
        if (!shape || (shape->GetSubType() != JPH::EShapeSubType::Sphere &&
            shape->GetSubType() != JPH::EShapeSubType::Box &&
            shape->GetSubType() != JPH::EShapeSubType::Capsule &&
            shape->GetSubType() != JPH::EShapeSubType::Cylinder))
        {
            return shape;
        }
        if (auto scale = m_transform.getScale(); !scale.similar(math::vec3::one()))
        {
            const JPH::Vec3 vec3Scale{scale.getX(), scale.getY(), scale.getZ()};
            if (!std::isfinite(vec3Scale.GetX()) || !std::isfinite(vec3Scale.GetY()) || !std::isfinite(vec3Scale.GetZ()) ||
                vec3Scale.GetX() == 0.0f || vec3Scale.GetY() == 0.0f || vec3Scale.GetZ() == 0.0f)
            {
                NAU_LOG_ERROR("Cannot scale collision shape: scale must be finite and nonzero");
                return nullptr;
            }

            JPH::ShapeSettings::ShapeResult scaledShape;
            if (shape->IsValidScale(vec3Scale))
            {
                scaledShape = shape->ScaleShape(vec3Scale);
            }
            else
            {
                // shortcut: nonuniform curved primitives use a convex approximation; refine it if contact accuracy requires it.
                const auto& convex = static_cast<const JPH::ConvexShape&>(*shape);
                JPH::ConvexHullShapeSettings hull;
                hull.mMaterial = convex.GetMaterial();
                hull.mDensity = convex.GetDensity();
                if (shape->GetSubType() == JPH::EShapeSubType::Cylinder)
                {
                    const auto& cylinder = static_cast<const JPH::CylinderShape&>(*shape);
                    constexpr int segments = 32;
                    hull.mPoints.reserve(segments * 2);
                    for (int i = 0; i < segments; ++i)
                    {
                        const float angle = 2.0f * JPH::JPH_PI * i / segments;
                        const float x = cylinder.GetRadius() * std::cos(angle);
                        const float z = cylinder.GetRadius() * std::sin(angle);
                        hull.mPoints.push_back(JPH::Vec3(x, cylinder.GetHalfHeight(), z) * vec3Scale);
                        hull.mPoints.push_back(JPH::Vec3(x, -cylinder.GetHalfHeight(), z) * vec3Scale);
                    }
                }
                else
                {
                    JPH::Shape::GetTrianglesContext context;
                    shape->GetTrianglesStart(context, JPH::AABox::sBiggest(), JPH::Vec3::sZero(),
                        JPH::Quat::sIdentity(), JPH::Vec3::sReplicate(1.0f));
                    constexpr int triangleCount = JPH::Shape::cGetTrianglesMinTrianglesRequested;
                    JPH::Float3 vertices[triangleCount * 3];
                    for (int count; (count = shape->GetTrianglesNext(context, triangleCount, vertices)) != 0;)
                    {
                        for (int i = 0; i < count * 3; ++i)
                        {
                            hull.mPoints.push_back(JPH::Vec3(vertices[i].x, vertices[i].y, vertices[i].z) * vec3Scale);
                        }
                    }
                }
                scaledShape = hull.Create();
            }
            if (scaledShape.HasError())
            {
                NAU_LOG_ERROR("Cannot scale collision shape: {}", scaledShape.GetError());
                return nullptr;
            }
            shape = scaledShape.Get();
        }
        return shape;
    }

    void JoltCollisionShape::setCollisionShape(JPH::RefConst<JPH::Shape> shape)
    {
        m_collisionShape = std::move(shape);
    }

    JoltSphereCollision::JoltSphereCollision(TFloat radius, JPH::PhysicsMaterial* material) :
        Base(new JoltSphereShape(radius, material))
    {
    }

    TFloat JoltSphereCollision::getRadius() const
    {
        NAU_FAILURE("Not implemented");
        return 0.f;
    }

    void JoltSphereCollision::setRadius(TFloat)
    {
        NAU_FAILURE("Not implemented");
    }

    JoltBoxCollision::JoltBoxCollision(math::vec3 extent, JPH::PhysicsMaterial* material) :
        Base(new JPH::BoxShape(vec3ToJolt(extent), getBoxConvexRadius(extent), material))
    {
    }

    JoltCapsuleCollision::JoltCapsuleCollision(const ConstructionData& data, JPH::PhysicsMaterial* material) :
        Base(new JPH::CapsuleShape(.5f * data.height, data.radius, material))
    {
    }

    JoltCylinderCollision::JoltCylinderCollision(const ConstructionData& data, JPH::PhysicsMaterial* material)
    {
        const TFloat halfHeight = .5f * data.height;
        const TFloat convexRadius = getCylinderConvexRadius(halfHeight, data.radius);

        setCollisionShape(new JPH::CylinderShape(halfHeight, data.radius, convexRadius, material));
    }

    JoltConvexHullCollision::JoltConvexHullCollision(const ConstructionData& constructionData, JPH::PhysicsMaterial* material)
    {
        JPH::Array<JPH::Vec3> list;
        for (auto const& point : constructionData.points)
        {
            list.push_back({point.getX(), point.getY(), point.getZ()});
        }

        JPH::ConvexHullShapeSettings convexSettings{list, JPH::cDefaultConvexRadius, material};
        if (auto result = convexSettings.Create(); !result.HasError())
        {
            setCollisionShape(result.Get());
        }
    }

    JoltConvexHullCollision::JoltConvexHullCollision(nau::Ptr<JoltConvexHullAssetView> convexHullAssetView) :
        m_convexHullAsset(convexHullAssetView)
    {
        NAU_ASSERT(m_convexHullAsset);
        if (m_convexHullAsset)
        {
            JPH::ShapeSettings::ShapeResult shapeResult = m_convexHullAsset->getShapeSettings().Create();
            if (!shapeResult.HasError())
            {
                setCollisionShape(shapeResult.Get());
            }
            else
            {
                NAU_LOG_ERROR(shapeResult.GetError());
            }
        }
    }

    JoltMeshCollision::JoltMeshCollision(const ConstructionData& constructionData)
    {
        JPH::TriangleList list;
        for (const auto& triangle : constructionData.triangles)
        {
            list.push_back(JPH::Triangle{
                JPH::Float3{triangle.p1.getX(), triangle.p1.getY(), triangle.p1.getZ()},
                JPH::Float3{triangle.p2.getX(), triangle.p2.getY(), triangle.p2.getZ()},
                JPH::Float3{triangle.p3.getX(), triangle.p3.getY(), triangle.p3.getZ()},
                triangle.materialIndex
            });
        }

        JPH::PhysicsMaterialList materialList;
        for (const auto& engineMaterial : constructionData.materials)
        {
            materialList.push_back(engineMaterial->as<const NauJoltPhysicsMaterialImpl*>()->joltMaterial());
        }

        const JPH::MeshShapeSettings meshSettings{list, materialList};
        auto result = meshSettings.Create();

        if (!result.HasError())
        {
            setCollisionShape(result.Get());
        }
    }

    JoltMeshCollision::JoltMeshCollision(nau::Ptr<JoltTriMeshAssetView> meshAssetView) :
        m_meshAsset(meshAssetView)
    {
        NAU_ASSERT(m_meshAsset);
        if (m_meshAsset)
        {
            JPH::ShapeSettings::ShapeResult shapeResult = m_meshAsset->getShapeSettings().Create();
            if (!shapeResult.HasError())
            {
                setCollisionShape(shapeResult.Get());
            }
            else
            {
                NAU_LOG_ERROR(shapeResult.GetError());
            }
        }
    }
}  // namespace nau::physics::jolt
