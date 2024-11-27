// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"
#include "nau/scene/scene_object.h"
#include "nau/utils/enum/enum_reflection.h"

namespace nau::sample
{
    /**
     */
    class SampleComponent final : public scene::Component,
                                  public scene::IComponentUpdate
    {
        NAU_OBJECT(nau::sample::SampleComponent, scene::Component, scene::IComponentUpdate)
        NAU_DECLARE_DYNAMIC_OBJECT

        void updateComponent(float dt) override;

    };
}  // namespace nau::sample
