// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/samples/sample_app_delegate.h"
#include "./sample_components/asset_reloader.h"
#include "./sample_components/mgpu_toggle.h"
#include "./sample_components/ui_stress.h"
#include "./sample_components/scene_reloader.h"
#include "./sample_components/window_maker.h"
#include "./texture_import_settings.h"
#include "nau/samples/components/camera_control.h"
#include "nau/samples/components/rotator.h"
#include "nau/utils/functor.h"
#include "nau/app/application_delegate.h"
#include "nau/app/global_properties.h"
#include "nau/input.h"
#include "nau/scene/internal/scene_listener.h"
#include "nau/scene/internal/scene_manager_internal.h"

namespace nau::sample
{
    class DemoSceneListener final : public scene::ISceneListener
    {
    public:
        DemoSceneListener() = default;

    private:
        void onSceneBegin() override
        {
        }

        void onSceneEnd() override
        {
        }

        void onAfterActivatingObjects(eastl::span<const scene::SceneObject*> objects) override
        {
        }

        void onBeforeDeletingObjects(eastl::span<const scene::SceneObject*> objects) override
        {
        }

        void onAfterActivatingComponents(eastl::span<const scene::Component*> components) override
        {
        }

        void onBeforeDeletingComponents(eastl::span<const scene::Component*> components) override
        {
        }

        void onComponentsChange(eastl::span<const scene::Component*> components) override
        {
        }
    };

    class MySampleDelegate final : public SampleAppDelegate
    {
    public:
        MySampleDelegate(Functor<async::Task<>()>&& startup) :
            SampleAppDelegate("sceneBase"),
            m_startup(std::move(startup))
        {
        }

    private:
        Result<> initializeServices() override
        {
            getServiceProvider().addService<TextureImportSettingsProvider>();
            getServiceProvider().addClass<nau::sample::CameraControl>();
            getServiceProvider().addClass<nau::sample::MyRotator>();
            getServiceProvider().addClass<nau::sample::AssetReloader>();
            getServiceProvider().addClass<nau::sample::SceneReloader>();
            getServiceProvider().addClass<nau::sample::WindowMaker>();
            getServiceProvider().addClass<nau::sample::MgpuToggle>();
            getServiceProvider().addClass<nau::sample::UiStress>();

            return ResultSuccess;
        }

        async::Task<> startupApplication() override
        {
            return m_startup();
        }

        Functor<async::Task<>()> m_startup;

        scene::SceneListenerRegistration m_sceneListenerReg;
        DemoSceneListener m_sceneListener;
    };

    ApplicationDelegate::Ptr createSampleAppDelegate(Functor<async::Task<>()>&& startup)
    {
        return eastl::make_unique<MySampleDelegate>(std::move(startup));
    }

}  // namespace nau::sample
