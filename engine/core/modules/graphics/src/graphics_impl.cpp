// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "graphics_impl.h"

#include <EASTL/unique_ptr.h>
#include <imgui.h>
#include "nau/input.h"
#include "nau/scene/scene_manager.h"

#include "graphics_assets/shader_asset.h"
#include "graphics_assets/texture_asset.h"
#include "nau/app/core_window_manager.h"
#include "nau/app/platform_window.h"
#include "nau/app/window_manager.h"
#include "nau/assets/asset_manager.h"
#include "nau/assets/asset_ref.h"
#include "nau/assets/shader_asset_accessor.h"
#include "nau/diag/logging.h"
#include "nau/dxil/compiled_shader_header.h"
#include "nau/gui/dag_imgui.h"
#include "nau/gui/imguiInput.h"
#include "nau/image/dag_texPixel.h"
#include "nau/input.h"
#include "nau/module/module_manager.h"
#include "nau/osApiWrappers/dag_cpuJobs.h"
#include "nau/shaders/shader_defines.h"
#include "nau/service/service_provider.h"
#include "nau/shaders/shader_globals.h"
#include "nau/ui.h"
#include "nau/utils/performance_profiling.h"
#include "render/daBfg/bfg.h"


#define USE_SHADER_CACHE 1
#define VIEWPORT_AUTO_RESIZE 0


class Driver3dInitCB : public Driver3dInitCallback
{
public:
    void verifyResolutionSettings(
        int& ref_scr_wdt,
        int& ref_scr_hgt,
        int base_scr_wdt,
        int base_scr_hgt,
        bool window_mode) const override
    {
        if ((ref_scr_wdt > base_scr_wdt || ref_scr_hgt > base_scr_hgt) && window_mode)
        {
            // Nothing to do.
        }
        else
        {
            allowResolutionOverlarge = -1;
        }

        if (allowResolutionOverlarge == 1)
        {
            ref_scr_wdt = base_scr_wdt;
            ref_scr_hgt = base_scr_hgt;
        }
    }

    int validateDesc(Driver3dDesc&) const override
    {
        return 1;
    }

    int compareDesc(Driver3dDesc&, Driver3dDesc&) const override
    {
        return 0;
    }

    bool desiredStereoRender() const override
    {
        return false;
    }

    int64_t desiredAdapter() const override
    {
        return 0;
    }

    RenderSize desiredRendererSize() const override
    {
        return {};
    }

    const char* desiredRendererDeviceExtensions() const override
    {
        return nullptr;
    }

    const char* desiredRendererInstanceExtensions() const override
    {
        return nullptr;
    }

    VersionRange desiredRendererVersionRange() const override
    {
        return {0, 0};
    }

    mutable int allowResolutionOverlarge = -1;
};

static Driver3dInitCB cb;
namespace nau
{
    GraphicsImpl::GraphicsImpl() = default;

    GraphicsImpl::~GraphicsImpl() = default;

    async::Task<> GraphicsImpl::preInitService()
    {
        bool isDriverInited = d3d::init_driver();
        NAU_ASSERT(isDriverInited);
        unsigned memSizeKb = d3d::get_dedicated_gpu_memory_size_kb();

        const char* gameName = "render test";
        uint32_t gameVersion = 1;

        d3d::driver_command(DRV3D_COMMAND_SET_APP_INFO, (void*)gameName, (void*)&gameVersion, nullptr);

        d3d::update_window_mode();

        // d3d::driver_command(DRV3D_COMMAND_ENABLE_MT, NULL, NULL, NULL);
        // ::enable_tex_mgr_mt(true, 64 << 10);

        cpujobs::init();

        IFrameAllocator::setFrameAllocator(&m_frameAllocator);

        IWindowManager& wndManager = getServiceProvider().get<IWindowManager>();
        HWND hwnd = wndManager.getActiveWindow().as<IWindowsWindow*>()->getWindowHandle();

        void* mainHwnd = hwnd;
        HINSTANCE hinst = GetModuleHandle(nullptr);
        const char* title = "render test";
        const char* wcName = "wcName";
        int ncmd = SW_SHOW;

        main_wnd_f* wndProc = nullptr;

        d3d::init_video(hinst, wndProc, wcName, ncmd, mainHwnd, mainHwnd, nullptr, title, &cb);

        int posx, posy, width, height;
        float minz, maxz;
        d3d::getview(posx, posy, width, height, minz, maxz);
        d3d::setview(posx, posy, width, height, 0.0f, 1.0f);

        auto& sceneManager = getServiceProvider().get<nau::scene::ISceneManager>();
        m_defaultWorld = sceneManager.getDefaultWorld().getUid();
        m_worldToGraphicScene[m_defaultWorld] = eastl::make_unique<GraphicsScene>(m_defaultWorld);
        co_await m_worldToGraphicScene[m_defaultWorld]->initialize();

        Ptr<nau::render::RenderWindowImpl> rendWindow = rtti::createInstance<nau::render::RenderWindowImpl>();
        m_renderWindows[DEFAULT_SWAPID] = rendWindow;
        m_defaultRenderWindow = rendWindow;

        m_hwndToSwapChain[hwnd] = DEFAULT_SWAPID;

        rendWindow->initialize("MainRenderView", DEFAULT_SWAPID, hwnd);
        rendWindow->setWorkQueue(getPreRenderExecutor());
        rendWindow->setWorld(m_defaultWorld);

        createDefaultTexture();

        nau::hal::init_main_thread_id();

        dabfg::startup();

        co_await rendWindow->createRenderGraph();

        m_isInitialized = true;

        co_return;
    }

    async::Task<> GraphicsImpl::initService()
    {
        return async::makeResolvedTask();
    }

    math::mat4 GraphicsImpl::getProjMatrix()
    {
        int width, height;
        m_defaultRenderWindow->getViewportSize(width, height);
        auto camera = m_worldToGraphicScene[m_defaultWorld]->getMainCamera();
        if (width > 0 && height > 0)
        {
            camera.aspectRatioRec = static_cast<float>(height) / width;
        }
        return camera.getProjMatrix();
    }

    async::Task<> GraphicsImpl::requestViewportResize(int32_t newWidth, int32_t newHeight, void* hwnd)
    {
        ASYNC_SWITCH_EXECUTOR(getPreRenderExecutor());

        if (newWidth == 0 || newHeight == 0)
        {
            co_return;
        }

        if (!m_hwndToSwapChain.count(hwnd))
        {
            NAU_LOG_ERROR("No RenderWindow with {} handle was found.", hwnd);
            co_return;
        }

        SWAPID swapID = m_hwndToSwapChain.at(hwnd);
        co_await m_renderWindows.at(swapID)->requestViewportResize(newWidth, newHeight);

        co_return;
    }

    async::Task<> GraphicsImpl::registerWindow(void* hwnd)
    {
        ASYNC_SWITCH_EXECUTOR(getPreRenderExecutor());

        if (m_hwndToSwapChain.size() >= 7)
        {
            co_return;
        }

        m_hwndToSwapChain[hwnd] = d3d::create_swapchain(hwnd);
    }

    async::Task<> GraphicsImpl::closeWindow(void* hwnd)
    {
        ASYNC_SWITCH_EXECUTOR(getPreRenderExecutor());

        if (!m_hwndToSwapChain.count(hwnd))
        {
            co_return;
        }
        SWAPID id = m_hwndToSwapChain[hwnd];
        m_hwndToSwapChain.erase(hwnd);
        m_renderWindows.erase(id);

        d3d::remove_swapchain(id);

        co_return;
    }

    async::Task<WeakPtr<nau::render::IRenderWindow>> GraphicsImpl::createRenderWindow(void* hwnd)
    {
        NAU_ASSERT(hwnd);

        ASYNC_SWITCH_EXECUTOR(getPreRenderExecutor());

        if (m_renderWindows.size() >= 8)
        {
            co_return nullptr;
        }

        Ptr<nau::render::RenderWindowImpl> rendWindow = rtti::createInstance<nau::render::RenderWindowImpl>();
        WeakPtr<nau::render::IRenderWindow> weak = WeakPtr<nau::render::IRenderWindow>(rendWindow);

        SWAPID swapchain = d3d::create_swapchain(hwnd);
        m_renderWindows[swapchain] = rendWindow;
        m_hwndToSwapChain[hwnd] = swapchain;


        rendWindow->initialize("RenderWindow #" + eastl::to_string(m_renderWindowsIds++), swapchain, hwnd);
        rendWindow->setWorkQueue(getPreRenderExecutor());
        rendWindow->setWorld(m_defaultWorld);

        waitResult(rendWindow->createRenderGraph()).ignore();

        waitResult(rendWindow->disableRenderStages(
                       nau::render::NauRenderStage::DebugStage |
                       nau::render::NauRenderStage::NauGUIStage |
                       nau::render::NauRenderStage::OutlineStage |
                       nau::render::NauRenderStage::UIDStage))
            .ignore();

        co_return weak;
    }


    async::Task<bool> GraphicsImpl::renderFrame()
    {
        NAU_FATAL(m_isInitialized, "Service GraphicsImpl is not initialized!");
        using namespace nau::async;
        if (m_isDisposed)
        {
            if (!m_renderStopedSignal.isReady())
            {
                stopGraphics();
                m_renderStopedSignal.resolve();
            }

            co_return false;
        }

        IFrameAllocator* frameAllocator = IFrameAllocator::getFrameAllocator();
        NAU_ASSERT(frameAllocator, "Frame allocator is not initialized");
        bool hasPrepared = frameAllocator->prepareFrame();
        NAU_ASSERT(hasPrepared);

        for (auto& [world, scene] : m_worldToGraphicScene)
        {
            auto task = scene->update();
            task.detach();
        }

        d3d::finish_render_commands();

        co_await executeRenderJobs();

        d3d::finish_render_commands();

        renderMainScene();

        co_return true;
    }

    async::Task<bool> GraphicsImpl::update([[maybe_unused]] std::chrono::milliseconds dt)
    {
        return renderFrame();
    }

    void GraphicsImpl::renderMainScene()
    {
// TODO Tracy        NAU_CPU_SCOPED_TAG(nau::PerfTag::Render);

#if VIEWPORT_AUTO_RESIZE
        IWindowManager& wndManager = getServiceProvider().get<IWindowManager>();
        auto& window = wndManager.getActiveWindow();
        const auto [clientWidth, clientHeight] = window.getClientSize();

        int w, h;
        d3d::get_screen_size(w, h);

        if (w != clientWidth || h != clientHeight)
        {
            resizeViewport(clientWidth, clientHeight, DEFAULT_SWAPID);
        }
#endif
        d3d::driver_command(DRV3D_COMMAND_ACQUIRE_OWNERSHIP, NULL, NULL, NULL);
        // Render for all RenderWindows
        for (auto& [swapId, renderWindow] : m_renderWindows)
        {
            if (renderWindow->getWorld() != nau::NullUid)
            {
                auto worldPtr = renderWindow->getWorld();
                renderWindow->setRenderScene(m_worldToGraphicScene[worldPtr]);
            }
            renderWindow->render();
        }

#if VIEWPORT_AUTO_RESIZE
        IWindowManager& wndManager = getServiceProvider().get<IWindowManager>();
        auto& window = wndManager.getActiveWindow();
        const auto [clientWidth, clientHeight] = window.getClientSize();

        int w, h;
        d3d::get_screen_size(w, h);

        if (w != clientWidth || h != clientHeight)
        {
            resizeViewport(clientWidth, clientHeight);
        }
#endif

        dabfg::update_external_state(dabfg::ExternalState{false, false});
        dabfg::run_nodes();

        d3d::update_screen();
        d3d::driver_command(DRV3D_COMMAND_RELEASE_OWNERSHIP, NULL, NULL, NULL);
    }

    void GraphicsImpl::createDefaultTexture()
    {
        if (m_defaultTex)
        {
            return;
        }

        eastl::allocator texAlloc;
        TexImage32* genImg = TexImage32::create(800, 800, &texAlloc);

        auto pixPtr = genImg->getPixels();

        for (int row = 0; row < 800; ++row)
        {
            for (int col = 0; col < 800; ++col)
            {
                TexPixel32 pix = {};
                bool t0 = col / 20 % 2 == 1;
                bool t1 = row / 20 % 2 == (int)t0;
                if (t1)
                {
                    pix.r = 255;
                }
                else
                {
                    pix.g = 255;
                }
                pixPtr[row * 800 + col] = pix;
            }
        }

        m_defaultTex = d3d::create_tex(genImg, 800, 800, 0, 1, u8"default_texture");

        memfree(genImg, &texAlloc);
    }

    void GraphicsImpl::stopGraphics()
    {
        imgui_shutdown();

        for (auto& [world, scene] : m_worldToGraphicScene)
        {
            scene.reset();
        }
       
        m_worldToGraphicScene.clear();
        m_renderWindows.clear();
        m_defaultRenderWindow.reset();

        dabfg::shutdown();
        d3d::release_driver();
    }

    async::Task<> GraphicsImpl::shutdownService()
    {
        if (const bool alreadyDisposed = m_isDisposed.exchange(true); !alreadyDisposed)
        {
            NAU_LOG_DEBUG("Graphics shutdown started");
            co_await m_renderStopedSignal.getTask();
            NAU_LOG_DEBUG("Graphics shutdown completed");
        }
    }

    async::Task<> GraphicsImpl::activateComponentsAsync(Uid worldUid, eastl::span<const scene::Component*> components, [[maybe_unused]] async::Task<> barrier)
    {
        NAU_ASSERT(worldUid != NullUid);
        ASYNC_SWITCH_EXECUTOR(getPreRenderExecutor());

        auto worldEntry = m_worldToGraphicScene.find(worldUid);
        if (worldEntry == m_worldToGraphicScene.end())
        {
            [[maybe_unused]] bool emplaceOk;
            eastl::tie(worldEntry, emplaceOk) = m_worldToGraphicScene.emplace(worldUid, eastl::make_unique<GraphicsScene>(worldUid));
            co_await worldEntry->second->initialize();
        }

        co_await worldEntry->second->activateComponents(components, async::Task<>::makeResolved());
    }

    async::Task<> GraphicsImpl::deactivateComponentsAsync(Uid worldUid, eastl::span<const scene::DeactivatedComponentData> components)
    {
        if (auto worldEntry = m_worldToGraphicScene.find(worldUid); worldEntry != m_worldToGraphicScene.end())
        {
            co_await worldEntry->second->deactivateComponents(components);
        }
    }

    void GraphicsImpl::syncSceneState()
    {
        for (auto& [world, scene] : m_worldToGraphicScene)
        {
            scene->syncSceneState();
        }

        imgui_copy_render_data();
    }

    async::Executor::Ptr GraphicsImpl::getPreRenderExecutor()
    {
        return m_preRenderWorkQueue;
    }

    void GraphicsImpl::addPreRenderJob(AsyncAction action)
    {
        NAU_FATAL(action);
        lock_(m_preRenderJobsMutex);
        m_preRenderJobs.emplace_back(std::move(action));
    }

    async::Task<> GraphicsImpl::executeRenderJobs()
    {
        using namespace nau::async;

        m_preRenderWorkQueue->poll();

        decltype(m_preRenderJobs) jobs;
        {
            lock_(m_preRenderJobsMutex);
            if (m_preRenderJobs.empty())
            {
                co_return;
            }

            jobs = std::move(m_preRenderJobs);
            m_preRenderJobs.clear();
        }

        eastl::vector<Task<>> tasks;
        tasks.reserve(jobs.size());

        for (auto& work : jobs)
        {
            if (Task<> task = work(); !task.isReady())
            {
                tasks.emplace_back(std::move(task));
            }
        }

        co_await whenAll(tasks, Expiration::never());
    }

    WeakPtr<render::IRenderWindow> GraphicsImpl::getDefaultRenderWindow()
    {
        return WeakPtr<render::IRenderWindow>(m_defaultRenderWindow);
    }

    void GraphicsImpl::getRenderWindows(eastl::vector<WeakPtr<nau::render::IRenderWindow>>& windows)
    {
        windows.clear();
        windows.reserve(m_renderWindows.size());
        for (auto& window : m_renderWindows)
        {
            auto& ptr = window.second;
            windows.push_back(WeakPtr<render::IRenderWindow>(ptr));
        }
    }

    void GraphicsImpl::setObjectHighlight(nau::Uid uid, bool flag)
    {
        m_worldToGraphicScene[m_defaultWorld]->setObjectHighlight(uid, flag);
    }
}  // namespace nau
