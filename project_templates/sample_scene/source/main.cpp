// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <nau/app/core_window_manager.h>
#include <nau/app/run_application.h>

#include "myapi.h"
#include "nau/app/application.h"
#include "nau/app/application_delegate.h"
#include "nau/app/application_services.h"
#include "nau/app/application_utils.h"
#include "nau/app/global_properties.h"
#include "nau/app/platform_window.h"
#include "nau/app/window_manager.h"
#include "nau/assets/asset_db.h"
#include "nau/input.h"
#include "nau/module/module_manager.h"
#include "nau/service/service_provider.h"
#include "nau/physics/physics_world.h"
#include "nau/physics/core_physics.h"
#include "json/json.h"

#include <fstream>

#define RUN_PLATFORM_APP

using namespace std::chrono_literals;
using namespace nau;

/**
 */
class MainAppDelegate final : public ApplicationDelegate
{
public:
    MainAppDelegate(Functor<async::Task<>()>&& startup) :
        m_startup(std::move(startup))
    {
        NAU_FATAL(m_startup);
    }

private:
    Result<> configureApplication() override
    {
        namespace fs = std::filesystem;

        const auto projectRootDir = EXPR_Block->fs::path
        {
            const fs::path projectRelativeDir{L"config"};

            fs::path currentPath = fs::current_path();

            do
            {
                fs::path targetPath = currentPath / projectRelativeDir;

                if (fs::exists(targetPath))
                {
                    return fs::canonical(targetPath.parent_path());
                }

                currentPath = currentPath.parent_path();

            } while (currentPath.has_relative_path());

            return {};
        };

        auto defaultConfigResult = nau::app_utils::parseAppConfigs(projectRootDir.string());
        NAU_FATAL(defaultConfigResult.isSuccess());

        return nau::applyDefaultAppConfiguration();
    }

    eastl::string getModulesListString() const override
    {
#if !defined(NAU_STATIC_RUNTIME)
        return NAU_MODULES_LIST;
#else
        return {};
#endif
    }

    /**
     */
    Result<> initializeServices() override
    {
        initializeDefaultApplication();

        return ResultSuccess;
    }

    void setupPhysicsWorld()
    {
        GlobalProperties& globalProperties = getServiceProvider().get<GlobalProperties>();
        auto result = globalProperties.getValue<eastl::u8string>("projectDir");
        NAU_ASSERT(result.has_value(), "Fail to get project dir value.");

        const std::filesystem::path& projectRootDir =
            std::string(reinterpret_cast<const char*>(result.value().data()), result.value().length());
         
        auto physWorld = getServiceProvider().get<physics::ICorePhysics>().getDefaultPhysicsWorld();
        physWorld->resetChannelsCollisionSettings();
        std::ifstream ifs;
        const auto channelsFileName = projectRootDir / std::filesystem::path("resources/physics/channels.data");
        ifs.open(channelsFileName);
        if (!ifs)
        {            
            NAU_LOG_ERROR("Can't load physics channel collisions from {}", channelsFileName.string());
            return;
        }

        NAU_LOG_DEBUG("Loading physics channels from {}", channelsFileName.string().c_str());
        Json::Value root;
        ifs >> root;

        const Json::Value channels = root["collisionChannels"];
        for ( int index = 0; index < channels.size(); ++index )
        {
            const auto& channel = channels[index];
            const auto channelA = channel["channel"].asInt();

            const Json::Value collisions = channel["collisions"];
            for (int collisionIdx = 0; collisionIdx < collisions.size(); ++collisionIdx )
            {
                const auto channelB = collisions[collisionIdx].asInt();
                physWorld->setChannelsCollidable(channelA, channelB);
            }
        }
    }
    
    /**
     */
    void onApplicationInitialized() override
    {
        auto& windowService = getServiceProvider().get<IWindowManager>();
        auto& window = windowService.getActiveWindow();

        window.setVisible(true);

        const auto [width, height] = window.getClientSize();

        nau::input::setScreenResolution(width, height);

        setupPhysicsWorld();
    }

    /**
     */
    async::Task<> startupApplication() override
    {
        return m_startup();
    }

    Functor<async::Task<>()> m_startup;
};

ApplicationDelegate::Ptr createSampleAppDelegate(Functor<async::Task<>()> startup)
{
    return eastl::make_unique<MainAppDelegate>(std::move(startup));
}

namespace
{
    async::Task<> startup()
    {
        co_await nau::app_utils::loadStartupScene();
    }
} 

int main()
{
    using namespace nau;

    ApplicationDelegate::Ptr delegate = createSampleAppDelegate([]
    {
        return startup();
    });

    return nau::runApplication(std::move(delegate));
}
