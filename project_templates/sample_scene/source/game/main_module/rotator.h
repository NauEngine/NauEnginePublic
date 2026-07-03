// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/scene_object.h"
#include "nau/utils/enum/enum_reflection.h"

NAU_DEFINE_ENUM_(Axis, X, Y, Z);

/**
    */
class MyRotator final : public nau::scene::Component,
                        public nau::scene::IComponentUpdate
{
    NAU_OBJECT(MyRotator, nau::scene::Component, nau::scene::IComponentUpdate)
    NAU_DECLARE_DYNAMIC_OBJECT

    NAU_CLASS_FIELDS(
        CLASS_NAMED_FIELD(m_axis, "axis"),
        CLASS_NAMED_FIELD(m_speedFactor, "speedFactor")
    )

    void updateComponent(float dt) override;

public:
    void setRotationAxis(Axis axis);

    void setSpeedFactor(float factor);

private:
    Axis m_axis = Axis::Y;
    float m_speedFactor = 0.5f;
};
