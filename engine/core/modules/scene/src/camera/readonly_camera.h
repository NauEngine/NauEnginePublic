// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include <EASTL/variant.h>

#include <shared_mutex>

#include "camera/internal_camera_properties.h"

#include "camera/detached_camera.h"
#include "nau/rtti/rtti_impl.h"
#include "nau/rtti/weak_ptr.h"
#include "nau/scene/camera/camera.h"
#include "nau/scene/nau_object.h"


namespace nau::scene
{
    /**
     */
    class ReadonlyCamera final : public InternalCameraProperties
    {
        NAU_CLASS_(nau::scene::ReadonlyCamera, InternalCameraProperties)


    public:
        ReadonlyCamera(const ReadonlyCamera&) = delete;
        ReadonlyCamera(const nau::Ptr<DetachedCamera>& camera);
        ReadonlyCamera(ObjectWeakRef<ICameraControl> camera);

        bool syncCameraProperties();
        const math::Transform& getWorldTransform() const override { return m_worldTransform; }

    private:
        using SceneCameraWeakRef = ObjectWeakRef<ICameraControl>;
        using DetachedCameraWeakRef = nau::WeakPtr<DetachedCamera>;
        using CameraReference = eastl::variant<SceneCameraWeakRef, DetachedCameraWeakRef>;

        std::shared_mutex m_mutex;
        CameraReference m_cameraReference;
        math::Transform m_worldTransform = math::Transform::identity();
    };
}  // namespace nau::scene
