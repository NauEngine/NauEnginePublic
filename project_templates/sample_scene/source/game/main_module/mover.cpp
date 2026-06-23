// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "mover.h"

NAU_IMPLEMENT_DYNAMIC_OBJECT(MyMover)

void MyMover::updateComponent(float dt)
{
    nau::math::Transform transform = getParentObject().getTransform();
    m_time += dt;
    const float period = std::remainderf(m_time * m_frequency + m_phase, PI);
    NAU_LOG_WARNING("MainGameModule updateComponent period{}", period);

    if (m_axis == AxisM::X)
    {
        transform.addTranslation(nau::math::vec3(sinf(period) * m_amplitude,0,0));
    }
    else if (m_axis == AxisM::Y)
    {
        transform.addTranslation(nau::math::vec3(0, sinf(period) * m_amplitude, 0));
    }
    else
    {
        transform.addTranslation(nau::math::vec3(0, 0, sinf(period) * m_amplitude));
    }
    getParentObject().setTransform(transform);
}

void MyMover::setRotationAxis(AxisM axis)
{
    value_changes_scope;

    m_axis = axis;
}

void MyMover::setFrequency(float frequency)
{
    value_changes_scope;

    m_amplitude = frequency;
}

void MyMover::setPhase(float phase)
{
    value_changes_scope;

    m_phase = phase;
}

void MyMover::setAmplitude(float amplitude)
{
    value_changes_scope;

    m_amplitude = amplitude;
}
