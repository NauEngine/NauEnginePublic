// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "./ui_stress.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace nau::sample
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(UiStress)

    namespace
    {
        constexpr int MAX_WINDOWS = 128;
        constexpr int MAX_LAYERS = 64;
        constexpr int PLOT_POINTS = 64;
    }

    void UiStress::updateComponent(float dt)
    {
        if (!m_initialized)
        {
            m_initialized = true;
            if (const char* variable = std::getenv("NAU_UI_STRESS"))
            {
                int windows = 0;
                int layers = 0;
                if (std::sscanf(variable, "%d,%d", &windows, &layers) >= 1)
                {
                    m_windows = std::clamp(windows, 0, MAX_WINDOWS);
                    m_layers = std::clamp(layers, 0, MAX_LAYERS);
                }
            }
        }
        m_time += dt;

        const ImVec2 display = ImGui::GetIO().DisplaySize;

        // Control window, top right (MGPU is bottom left, the other sample windows top left).
        ImGui::SetNextWindowPos(ImVec2(std::max(10.0f, display.x - 330.0f), 10.0f), ImGuiCond_Once);
        ImGui::SetNextWindowSize(ImVec2(320.0f, 130.0f), ImGuiCond_Once);
        ImGui::Begin("UI stress");
        ImGui::SliderInt("Windows", &m_windows, 0, MAX_WINDOWS);
        ImGui::SliderInt("Full-screen layers", &m_layers, 0, MAX_LAYERS);
        ImGui::SliderInt("Lines per window", &m_linesPerWindow, 1, 100);
        ImGui::Text("F7 - UI layer on/off");
        ImGui::End();

        // Pixel load: every layer covers the whole screen with alpha 0.05, so each one is a full-screen blend.
        ImDrawList* background = ImGui::GetBackgroundDrawList();
        for (int layer = 0; layer < m_layers; ++layer)
        {
            const float hue = static_cast<float>(layer) / static_cast<float>(std::max(1, m_layers));
            const ImU32 color = ImColor::HSV(hue, 0.6f, 0.9f, 0.05f);
            background->AddRectFilled(ImVec2(0.0f, 0.0f), display, color);
        }

        // Geometry load: windows in a grid, each with text and an animated plot.
        float plot[PLOT_POINTS];
        for (int i = 0; i < PLOT_POINTS; ++i)
        {
            plot[i] = std::sin(m_time * 3.0f + static_cast<float>(i) * 0.2f);
        }
        const int columns = std::max(1, static_cast<int>(display.x / 230.0f));
        for (int w = 0; w < m_windows; ++w)
        {
            char title[32];
            std::snprintf(title, sizeof(title), "Stress %d", w);
            const float x = 10.0f + static_cast<float>(w % columns) * 230.0f;
            const float y = 150.0f + static_cast<float>(w / columns) * 40.0f;
            ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(220.0f, 260.0f), ImGuiCond_Once);
            ImGui::SetNextWindowBgAlpha(0.7f);
            ImGui::Begin(title);
            ImGui::PlotLines("##plot", plot, PLOT_POINTS, 0, nullptr, -1.0f, 1.0f, ImVec2(-1.0f, 60.0f));
            for (int line = 0; line < m_linesPerWindow; ++line)
            {
                ImGui::Text("line %d: value %.3f", line, plot[(line + w) % PLOT_POINTS]);
            }
            ImGui::End();
        }
    }
}  // namespace nau::sample
