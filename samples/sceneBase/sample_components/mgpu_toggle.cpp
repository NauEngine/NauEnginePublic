// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "./mgpu_toggle.h"

#include <algorithm>

#include "nau/input.h"
#include "nau/mgpu/bridge/mgpu_bridge.h"

namespace nau::sample
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(MgpuToggle)

    void MgpuToggle::updateComponent([[maybe_unused]] float dt)
    {
        using namespace nau::input;
        namespace bridge = nau::mgpu::bridge;

        if (isKeyboardButtonPressed(0, input::Key::M))
        {
            bridge::setEnabled(!bridge::isEnabled());
        }

        const bridge::Status status = bridge::getStatus();

        // Bottom-left corner: the other sample windows open in the top-left one.
        const ImVec2 windowSize(500, 210);
        ImGui::SetNextWindowPos(ImVec2(10, std::max(10.0f, ImGui::GetIO().DisplaySize.y - windowSize.y - 10)), ImGuiCond_Once);
        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Once);
        ImGui::Begin("MGPU");

        if (!status.linked)
        {
            ImGui::Text("Mode: 1 GPU (built without NAU_ENABLE_MGPU)");
        }
        else if (!status.enabled)
        {
            ImGui::Text("Mode: 1 GPU (MGPU switched off)");
        }
        else if (!status.ready)
        {
            ImGui::Text("Mode: 1 GPU (second GPU not available)");
            ImGui::TextWrapped("%s", status.lastError);
        }
        else
        {
            ImGui::Text("Mode: 2 GPU, shadow cascades: %s",
                        status.shadowsFromSecondGpu ? "second GPU" : "Nau (no atlas from the second GPU)");
            ImGui::Text("Scene GPU:  %s", status.primaryAdapter);
            ImGui::Text("Shadow GPU: %s", status.secondaryAdapter);
            ImGui::Text("Atlases/s: %.1f, latency %.1f ms, atlas age %u frames", status.atlasesPerSecond, status.latencyMs,
                        status.atlasAgeFrames);
            ImGui::Text("Draws: %u, completed %llu, skipped (busy) %llu, failed %llu", status.drawCount,
                        static_cast<unsigned long long>(status.atlasesCompleted),
                        static_cast<unsigned long long>(status.skippedBusy), static_cast<unsigned long long>(status.failed));
        }

        // DOF has its own switches and may run on the second GPU while the shadow path is off.
        if (status.linked)
        {
            if (!status.dofEnabled)
            {
                ImGui::Text("DOF: off");
            }
            else if (status.dofFromSecondGpu)
            {
                ImGui::Text("DOF: gpu1 (second GPU)");
            }
            else if (status.dofSecondGpuRequested)
            {
                ImGui::Text("DOF: gpu0 (second GPU requested, not used)");
            }
            else
            {
                ImGui::Text("DOF: gpu0 (Nau)");
            }
        }

        // ImGui's frame rate is the one of the update loop; Nau renders with vsync.
        if (status.linked)
        {
            ImGui::Text("Scene FPS: %.1f (update loop %.1f)", status.sceneFramesPerSecond, ImGui::GetIO().Framerate);
        }
        else
        {
            ImGui::Text("Update loop FPS: %.1f", ImGui::GetIO().Framerate);
        }
        if (status.linked)
        {
            ImGui::Text("M - switch MGPU on/off, F9 - DOF on/off, F8 - DOF on the second GPU");
        }

        ImGui::End();
    }
}  // namespace nau::sample
