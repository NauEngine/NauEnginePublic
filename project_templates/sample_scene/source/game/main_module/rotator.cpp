// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "rotator.h"

NAU_IMPLEMENT_DYNAMIC_OBJECT(MyRotator)

void MyRotator::updateComponent(float dt)
{
    NAU_LOG_WARNING("MainGameModule updateComponent root");
    nau::math::Transform transform = getParentObject().getTransform();
    const float angle = dt * m_speedFactor;

    if (m_axis == Axis::X)
    {
        transform.addRotation(nau::math::quat::rotationX(angle));
    }
    else if (m_axis == Axis::Y)
    {
        transform.addRotation(nau::math::quat::rotationY(angle));
    }
    else
    {
        transform.addRotation(nau::math::quat::rotationZ(angle));
    }
    getParentObject().setTransform(transform);
}

void MyRotator::setRotationAxis(Axis axis)
{
    value_changes_scope;

    m_axis = axis;
}

void MyRotator::setSpeedFactor(float factor)
{
    value_changes_scope;

    m_speedFactor = factor;
}
