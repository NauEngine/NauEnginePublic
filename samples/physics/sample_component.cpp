// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "./sample_component.h"

#include "nau/scene/scene_manager.h"
#include "nau/scene/world.h"


namespace nau::sample
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(SampleComponent)

    void SampleComponent::updateComponent(float dt)
    {
        auto& sceneManager = getServiceProvider().get<scene::ISceneManager>();
        auto& world = sceneManager.getDefaultWorld();

        ImGui::Begin("Demo", nullptr, ImGuiWindowFlags_NoResize);
        ImGui::Text("World Management");

        bool simulationPaused = world.isSimulationPaused();

        if (ImGui::Checkbox("Paused", &simulationPaused); simulationPaused != world.isSimulationPaused())
        {
            world.setSimulationPause(simulationPaused);
        }

        ImGui::Text("[W][A][S][D][Q][E] Move Control");
        ImGui::Text("[R] Cast rays between control points on the scene");
        ImGui::Text("[F] Apply force to object at the center of screen");
        ImGui::Text("[T] Apply torque to object at the center of screen");
        ImGui::Text("[ESC] Quit");
        ImGui::End();
    }

}  // namespace nau::sample
