// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once
#include "nau/scene/components/component.h"
#include "nau/scene/components/component_life_cycle.h"

namespace nau::sample
{
    // UI load for the UI layer measurements (ui_layer in graphics/src/render_window/render_graph.cpp).
    // The "UI stress" window sets how much UI is drawn every frame:
    //   windows  - ImGui windows with text and a plot (vertices, draw calls);
    //   layers   - semi-transparent full-screen rectangles under the windows (pixels: the GPU cost of the UI).
    // NAU_UI_STRESS=<windows>,<layers> sets the values at start, e.g. NAU_UI_STRESS=32,8, so runs are repeatable.
    class UiStress final : public scene::Component,
                           public scene::IComponentUpdate
    {
    public:
        NAU_OBJECT(UiStress, scene::Component, scene::IComponentUpdate)
        NAU_DECLARE_DYNAMIC_OBJECT

    private:
        void updateComponent(float dt) override;

        bool m_initialized = false;
        int m_windows = 0;
        int m_layers = 0;
        int m_linesPerWindow = 20;
        float m_time = 0.0f;
    };
}  // namespace nau::sample
