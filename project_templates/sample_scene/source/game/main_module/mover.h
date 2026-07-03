// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/scene_object.h"
#include "nau/utils/enum/enum_reflection.h"

NAU_DEFINE_ENUM_(AxisM, X, Y, Z);

/**
    */
class MyMover final : public nau::scene::Component,
                        public nau::scene::IComponentUpdate
{
    NAU_OBJECT(MyMover, nau::scene::Component, nau::scene::IComponentUpdate)
    NAU_DECLARE_DYNAMIC_OBJECT

    NAU_CLASS_FIELDS(
        CLASS_NAMED_FIELD(m_axis, "axis"),
        CLASS_NAMED_FIELD(m_frequency, "frequency"),
        CLASS_NAMED_FIELD(m_amplitude, "amplitude"),
        CLASS_NAMED_FIELD(m_phase, "phase")
    )

    void updateComponent(float dt) override;

public:
    void setRotationAxis(AxisM axis);

    void setFrequency(float frequency);

    void setPhase(float phase);

    void setAmplitude(float amplitude);

private:
    AxisM m_axis = AxisM::Y;
    float m_frequency = 2.f;
    float m_amplitude = 0.5f;
    float m_phase = 0.5f;
    float m_time = 0.0f;
};
