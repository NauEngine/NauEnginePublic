// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "camera/readonly_camera.h"

namespace nau::scene
{
    // do not replace camera parameter with rvalue reference (or value):
    // must guarantee that first syncCameraProperties (in constructor) call always success.
    ReadonlyCamera::ReadonlyCamera(const nau::Ptr<DetachedCamera>& camera) :
        m_cameraReference(eastl::in_place<DetachedCameraWeakRef>, camera)
    {
        NAU_VERIFY(syncCameraProperties());
    }

    ReadonlyCamera::ReadonlyCamera(ObjectWeakRef<ICameraControl> camera) :
        m_cameraReference(eastl::in_place<SceneCameraWeakRef>, camera)
    {
        NAU_VERIFY(syncCameraProperties());
    }

    bool ReadonlyCamera::syncCameraProperties()
    {
        NAU_FATAL(!m_cameraReference.valueless_by_exception());

        const auto applyCustomProps = [this](const ICameraProperties& props)
        {
            m_worldUid = props.getWorldUid();
            m_name = props.getCameraName();
            m_worldTransform = props.getWorldTransform();
        };

        if (m_cameraReference.index() == 0)
        {
            SceneCameraWeakRef& cameraRef = eastl::get<SceneCameraWeakRef>(m_cameraReference);
            if (!cameraRef)
            {
                return false;
            }

            RuntimeValue::assign(makeValueRef(*this), cameraRef->as<RuntimeValue*>()).ignore();
            applyCustomProps(*cameraRef);
        }
        else
        {
            NAU_FATAL(m_cameraReference.index() == 1, "Invalid camera reference state");
            nau::Ptr<DetachedCamera> camera = eastl::get<DetachedCameraWeakRef>(m_cameraReference).lock();
            if (!camera)
            {
                return false;
            }

            lock_(camera->m_mutex);

            RuntimeValue::assign(makeValueRef(*this), makeValueRef(*camera)).ignore();
            applyCustomProps(*camera);
        }

        return true;
    }
}  // namespace nau::scene
