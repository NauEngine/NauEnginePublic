// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/camera/camera.h"
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/scene_object.h"
#include "nau/utils/enum/enum_reflection.h"
#include "nau/scene/components/omnilight_component.h"
#include "nau/scene/components/spotlight_component.h"

namespace nau::sample
{

    NAU_DEFINE_ENUM_(CamControlKind,
                     UseSceneObject,
                     UseCameraManager)

    class CameraControl final : public scene::Component,
                                public scene::IComponentUpdate,
                                public scene::IComponentEvents
    {
        NAU_OBJECT(CameraControl, scene::Component, scene::IComponentUpdate, scene::IComponentEvents)
        NAU_DECLARE_DYNAMIC_OBJECT

        NAU_CLASS_FIELDS(
            CLASS_NAMED_FIELD(m_name, "MyName"),
            CLASS_NAMED_FIELD(m_motionFactor, "MotionFactor"),
            CLASS_NAMED_FIELD(m_cameraKind, "CameraKind"))

    public:
        CameraControl() = default;

        ~CameraControl();

        void setCamControlKind(CamControlKind kind);
        void setStepFactor(float stepFactor);

    private:
        void updateComponent(float dt) override;

        void onComponentCreated() override;

        void onComponentActivated() override;

        void doStep(const math::vec3& offset);

        scene::ICameraControl& getDetachedCamera();

        scene::TransformControl& getControlledTransform();

        eastl::u8string m_name = u8"Default Name";
        float m_motionFactor = 10.0f;
        float m_stepFactor = 10.0f;

        CamControlKind m_cameraKind = CamControlKind::UseCameraManager;
        nau::Ptr<scene::ICameraControl> m_camera;

        scene::OmnilightComponent* m_omnilightComponent = nullptr;
        scene::SpotlightComponent* m_spotlightComponent = nullptr;
    };
}  // namespace nau::sample