// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"

namespace nau::sample
{
    // Key M switches the Mgpu path (nau::mgpu::bridge) on and off at run time; the "MGPU" window shows what it is
    // doing. With Mgpu on and two GPUs the second GPU draws the shadow cascades; with one GPU or Mgpu off Nau draws
    // them itself.
    class MgpuToggle final : public scene::Component,
                             public scene::IComponentUpdate
    {
    public:
        NAU_OBJECT(MgpuToggle, scene::Component, scene::IComponentUpdate)
        NAU_DECLARE_DYNAMIC_OBJECT

    private:
        void updateComponent(float dt) override;
    };
}  // namespace nau::sample
