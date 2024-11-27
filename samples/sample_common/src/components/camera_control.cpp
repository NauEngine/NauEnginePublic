// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/samples/components/camera_control.h"

#include <imgui.h>

#include "nau/input.h"
#include "nau/scene/camera/camera_manager.h"

namespace nau::sample
{
    NAU_IMPLEMENT_DYNAMIC_OBJECT(CameraControl)

    CameraControl::~CameraControl()
    {
        NAU_LOG("~CameraControl");
    }

    void CameraControl::setCamControlKind(CamControlKind kind)
    {
        m_cameraKind = kind;
        if (m_cameraKind == CamControlKind::UseCameraManager)
        {
            getDetachedCamera();
        }
    }

    void CameraControl::setStepFactor(float stepFactor)
    {
        m_stepFactor = stepFactor;
    }

    scene::ICameraControl& CameraControl::getDetachedCamera()
    {
        if (!m_camera)
        {
            m_camera = getServiceProvider().get<scene::ICameraManager>().createDetachedCamera();
            m_camera->setCameraName(getParentObject().getName());
        }

        return *m_camera;
    }

    void CameraControl::updateComponent(float dt)
    {
        using namespace nau::input;

        ImGui::Begin("Component Window");
        ImGui::SetWindowSize(ImVec2(300, 100));
        ImGui::IsWindowHovered();
        ImGui::Text("Component name: %s", reinterpret_cast<const char*>(m_name.c_str()));

        ImGui::End();

        // Camera controller settngs 
        ImGui::Begin("Camera Controll");
        ImGui::SliderFloat("Camera Speed", &m_motionFactor, 2.0f, 10.0f);
        ImGui::SliderFloat("Move Speed", &m_stepFactor, 10.0f, 100.0f);
        ImGui::IsWindowHovered();
        ImGui::End();

        const float setpOffset = m_stepFactor * dt;

        if (!ImGui::GetIO().WantCaptureMouse)
        {
            if (isKeyboardButtonHold(0, input::Key::W))
            {
                doStep({0, 0, -setpOffset});
            }
            else if (isKeyboardButtonHold(0, input::Key::S))
            {
                doStep({0, 0, setpOffset});
            }
            else if (isKeyboardButtonHold(0, input::Key::A))
            {
                doStep({-setpOffset, 0, 0});
            }
            else if (isKeyboardButtonHold(0, input::Key::D))
            {
                doStep({setpOffset, 0, 0});
            }
            else if (isKeyboardButtonHold(0, input::Key::Q))
            {
                doStep({0, setpOffset, 0});
            }
            else if (isKeyboardButtonHold(0, input::Key::E))
            {
                doStep({0, -setpOffset, 0});
            }
            else if (isKeyboardButtonHold(0, input::Key::Escape))
            {
                NAU_LOG("Quit the application...");
                getApplication().stop();
            }

            if (isMouseButtonHold(0, MouseKey::ButtonLeft))
            {
                // add X axis turn
                if (const float xr = getMouseAxisDelta(0, MouseKey::AxisX); xr != 0.f)
                {
                    auto newRot = getControlledTransform().getRotation() * math::quat::rotationY(-xr * m_motionFactor);
                    getControlledTransform().setRotation(newRot);
                }

                // add y axis look up
                if (const float yr = getMouseAxisDelta(0, MouseKey::AxisY); yr != 0.0f)
                {
                    auto newRotationY = getControlledTransform().getRotation() * math::quat::rotationX(-yr * m_motionFactor);
                    getControlledTransform().setRotation(newRotationY);
                }
            }
        }

        if (!m_omnilightComponent)
        {
            scene::OmnilightComponent& omnilight = this->getParentObject().addComponent<scene::OmnilightComponent>();
            omnilight.setIntensity(7.5);
            omnilight.setAttenuation(0);
            omnilight.setColor({1, 1, 0});
            omnilight.setRadius(15);
            omnilight.setShift({0, 0, 0});
            m_omnilightComponent = &omnilight;
        }

        if (m_omnilightComponent)
        {
            ImGui::Begin("TestOmnilight");

            static float intensity = 4;
            ImGui::SliderFloat("Intencity", &intensity, 0, 10);
            m_omnilightComponent->setIntensity(intensity);

            static float radius = 7.5;
            ImGui::SliderFloat("Radius", &radius, 0, 10);
            m_omnilightComponent->setRadius(radius);

            static float attenuation = 50;
            ImGui::SliderFloat("Attenuation", &attenuation, 0, 180);
            m_omnilightComponent->setAttenuation(attenuation * std::numbers::pi_v<float> / 180.f);

            static float shift[3] = {.0f, -0.f, 0.f};
            ImGui::SliderFloat3("Shift", shift, -3, 3);
            m_omnilightComponent->setShift(math::Vector3(shift[0], shift[1], shift[2]));

            static float color[3] = {1, 1, 0};
            ImGui::ColorPicker3("LightColor", color);
            m_omnilightComponent->setColor({color[0], color[1], color[2]});

            ImGui::End();
        }

        if (!m_spotlightComponent)
        {
            scene::SpotlightComponent& spotlight = this->getParentObject().addComponent<scene::SpotlightComponent>();
            spotlight.setIntensity(7.5);
            spotlight.setAttenuation(1.5);
            spotlight.setColor({1, 1, 0});
            spotlight.setRadius(15);
            spotlight.setAngle(std::numbers::pi_v<float> / 6.f);
            spotlight.setShift({0, 0, 0});
            m_spotlightComponent = &spotlight;
        }

        if (m_spotlightComponent)
        {
            ImGui::Begin("TestSpotlight");

            static float intensity = 1;
            ImGui::SliderFloat("Intencity", &intensity, 0, 10);
            m_spotlightComponent->setIntensity(intensity);

            static float radius = 1;
            ImGui::SliderFloat("Radius", &radius, 0, 10);
            m_spotlightComponent->setRadius(radius);

            static float attenuation = 1;
            ImGui::SliderFloat("Attenuation", &attenuation, 0, 180);
            m_spotlightComponent->setAttenuation(attenuation * std::numbers::pi_v<float> / 180.f);

            static float angle = 30;
            ImGui::SliderFloat("Angle", &angle, 0, 180);
            m_spotlightComponent->setAngle(angle * std::numbers::pi_v<float> / 180.f);

            static float direction[3] = {0.f, 0.f, -1.0f};
            ImGui::SliderFloat3("Direction", direction, -1, 1);
            auto vDirection = math::Vector3(direction[0], direction[1], direction[2]);
            if (!vDirection.similar(math::Vector3(0, 0, 0)))
            {
                m_spotlightComponent->setDirection(Vectormath::SSE::normalize(vDirection));
            }

            static float shift[3] = {.0f, -0.f, 0.f};
            ImGui::SliderFloat3("Shift", shift, -3, 3);
            m_spotlightComponent->setShift(math::Vector3(shift[0], shift[1], shift[2]));

            static float color[3] = {0, 1, 0};
            ImGui::ColorPicker3("LightColor", color);
            m_spotlightComponent->setColor({color[0], color[1], color[2]});

            ImGui::End();
        }
    }

    void CameraControl::onComponentCreated()
    {
        NAU_LOG("Created ({})", m_name);
    }

    void CameraControl::onComponentActivated()
    {
        NAU_LOG("Component going to be activated");
        NAU_LOG("Component ready");

        getControlledTransform().setRotation(math::quat::identity());
    }

    void CameraControl::doStep(const math::vec3& offset)
    {
        auto& control = getControlledTransform();

        const math::vec3 vec = math::mat3::rotation(control.getRotation()) * offset;
        control.setTranslation(control.getTranslation() + vec);
        this->getParentObject().setTranslation(control.getTranslation());
    }

    scene::TransformControl& CameraControl::getControlledTransform()
    {
        if (m_cameraKind == CamControlKind::UseSceneObject)
        {
            return getParentObject();
        }

        return getDetachedCamera();
    }

}  // namespace nau::sample