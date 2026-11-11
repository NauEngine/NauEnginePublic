// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/math/transform.h"
#include "nau/scene/camera/camera.h"
#include "nau/scene/nau_object.h"

namespace nau::scene
{
    /**
     */
    class NAU_ABSTRACT_TYPE InternalCameraProperties : public CameraProperties,
                                                       public virtual IRefCounted
    {
        NAU_INTERFACE(nau::scene::InternalCameraProperties, CameraProperties, IRefCounted)

        NAU_CLASS_FIELDS(
            CLASS_NAMED_FIELD(m_uid, "uid"),
            CLASS_NAMED_FIELD(m_transform, "transform"))

    public:
        InternalCameraProperties() = default;
        InternalCameraProperties(const InternalCameraProperties&) = delete;

        Uid getCameraUid() const final;
        Uid getWorldUid() const final;
        eastl::string_view getCameraName() const final;

        float getFov() const final;
        float getClipNearPlane() const final;
        float getClipFarPlane() const final;

        const math::Transform& getWorldTransform() const override;
        const math::Transform& getTransform() const final;

        math::quat getRotation() const final;
        math::vec3 getTranslation() const final;
        math::vec3 getScale() const final;

    protected:
        Uid m_uid;
        Uid m_worldUid;
        eastl::string m_name;
        math::Transform m_transform = math::Transform::identity();
    };

}  // namespace nau::scene
