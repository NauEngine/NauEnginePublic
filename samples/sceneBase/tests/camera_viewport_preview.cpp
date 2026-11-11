// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "camera_preview_pixels.h"
#include "camera_preview_scene.h"

#include "graphics_assets/material_asset.h"
#include "graphics_assets/skinned_mesh_asset.h"
#include "graphics_assets/static_mesh_asset.h"
#include "nau/app/application.h"
#include "nau/app/run_application.h"
#include "nau/diag/logging.h"
#include "nau/graphics/core_graphics.h"
#include "nau/render/render_window.h"
#include "nau/samples/sample_app_delegate.h"
#include "nau/scene/camera/camera_manager.h"
#include "nau/scene/components/camera_component.h"
#include "nau/scene/components/directional_light_component.h"
#include "nau/scene/scene_manager.h"
#include "nau/service/service_provider.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdio>

namespace
{
    using namespace nau;
    using namespace camera_preview_test;
    using async::Task;
    using render::IRenderWindow;

    constexpr Rect firstInset{16, 204, 256, 144};  // Leave the billboard in the main picture unobstructed.
    constexpr Rect secondInset{180, 108, 288, 162};  // Covers the main pick target at (320, 180).
    constexpr int mainWidth = 640, mainHeight = 360;

    render::CameraPreview makePreview(Uid id, Rect dest, int width, int height)
    {
        render::CameraPreview preview{};
        preview.cameraUid = id;
        preview.x = dest.x;
        preview.y = dest.y;
        preview.width = dest.width;
        preview.height = dest.height;
        preview.renderWidth = width;
        preview.renderHeight = height;
        return preview;
    }

    // An answered pick is a fence for a real UID pass. Capture on the following pre-render
    // queue, after that frame has been presented. Game-loop ticks alone cannot guarantee this.
    void checkViewport(IRenderWindow& window, int width, int height);

    Task<Frame> presentedFrame(IRenderWindow& window, int frames = 4, bool verifyViewport = false)
    {
        for (int i = 0; i < frames; ++i)
        {
            co_await window.requestUidByCoords(0, 0);
        }
        ASYNC_SWITCH_EXECUTOR(getServiceProvider().get<ICoreGraphics>().getPreRenderExecutor());
        try
        {
            auto frame = captureFrame();
            if (verifyViewport)
            {
                checkViewport(window, frame.width, frame.height);
            }
            co_return frame;
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "Capture failed: %s\n", error.what());
        }
        co_yield NauMakeError("DX12 screen capture failed");
    }

    void require(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    void checkMatchingCamera(const Frame& main, const Frame& preview)
    {
        // Allow a small color conversion difference between the swapchain and a tonemapped
        // intermediate target. A silhouette displacement still fails even at a single pixel.
        constexpr int tolerance = 4;
        const auto mismatches = changedPixels(main, preview, main.bounds(), tolerance);
        std::printf("same camera: %zu / %zu pixels differ by >%d RGB\n", mismatches, main.pixels.size(), tolerance);
        for (size_t i = 0, shown = 0; i < main.pixels.size() && shown < 4 && mismatches; ++i)
        {
            if (differs(main.pixels[i], preview.pixels[i], tolerance))
            {
                std::fprintf(stderr, "1:1 sample (%zu,%zu): main %06x, preview %06x\n",
                             i % main.width, i / main.width, main.pixels[i], preview.pixels[i]);
                ++shown;
            }
        }
        require(mismatches == 0, "Full-resolution main and preview pixels differ");
    }

    void checkViewport(IRenderWindow& window, int width, int height)
    {
        int actualWidth, actualHeight;
        window.getViewportSize(actualWidth, actualHeight);
        const auto projection = getServiceProvider().get<ICoreGraphics>().getProjMatrix();
        const float ratio = projection.getCol1().getY() / projection.getCol0().getX();
        std::printf("viewport: expected %dx%d, layout %dx%d, projection aspect %.5f\n", width, height, actualWidth, actualHeight, ratio);
        require(actualWidth == width && actualHeight == height, "Layout API returned a size different from the applied viewport");
        require(std::abs(ratio - float(width) / float(height)) < 1.e-4f,
                "getProjMatrix() aspect differs from the applied viewport");
    }

    void checkBillboardColor(const Frame& visible, const Frame& hidden, Rect inset)
    {
        size_t spritePixels = 0, overlap = 0, mainSpritePixels = 0;
        int xMin = inset.width, xMax = -1, yMin = inset.height, yMax = -1;
        for (int y = 0; y < inset.height; ++y)
        {
            for (int x = 0; x < inset.width; ++x)
            {
                const auto i = (inset.y + y) * visible.width + inset.x + x;
                const int mx = (2 * x + 1) * visible.width / (2 * inset.width);
                const int my = (2 * y + 1) * visible.height / (2 * inset.height);
                const int mi = my * visible.width + mx;
                const bool inOverlay = mx >= inset.x && mx < inset.x + inset.width && my >= inset.y && my < inset.y + inset.height;
                const bool onMainBillboard = !inOverlay && differs(visible.pixels[mi], hidden.pixels[mi], 8);
                mainSpritePixels += onMainBillboard;
                if (!differs(visible.pixels[i], hidden.pixels[i], 8))
                {
                    continue;
                }
                ++spritePixels;
                xMin = std::min(xMin, x);
                xMax = std::max(xMax, x);
                yMin = std::min(yMin, y);
                yMax = std::max(yMax, y);
                overlap += onMainBillboard;
            }
        }
        std::printf("billboard: overlap %zu, main mask %zu, inset mask %zu, inset bbox %d,%d..%d,%d\n",
                    overlap, mainSpritePixels, spritePixels, xMin, yMin, xMax, yMax);
        require(spritePixels > 20 && overlap * 10 >= spritePixels * 9 && overlap * 10 >= mainSpritePixels * 9,
                "Inset billboard coverage differs from the main billboard");
        const float spriteSize = 0.07f * inset.width;
        require(std::abs(xMax - xMin + 1 - spriteSize) <= 2 && std::abs(yMax - yMin + 1 - spriteSize) <= 2,
                "Inset billboard did not retain its width and height");
        for (int qy : {1, 3})
        {
            for (int qx : {1, 3})
            {
                const int x = xMin + (xMax - xMin + 1) * qx / 4;
                const int y = yMin + (yMax - yMin + 1) * qy / 4;
                const int mx = (2 * x + 1) * visible.width / (2 * inset.width);
                const int my = (2 * y + 1) * visible.height / (2 * inset.height);
                require(!differs(visible.pixels[(inset.y + y) * visible.width + inset.x + x],
                                 visible.pixels[my * visible.width + mx], 3),
                        "Billboard quadrant changed color in the inset");
            }
        }
    }

    class PreviewRegression final : public sample::SampleAppDelegate
    {
    public:
        explicit PreviewRegression(bool& passed) : SampleAppDelegate("sceneBase"), m_passed(passed) {}

        Result<> initializeServices() override { return ResultSuccess; }
        void onApplicationInitialized() override {}

        Task<> startupApplication() override
        {
            auto& factory = getServiceProvider().get<scene::ISceneFactory>();
            auto scene = factory.createEmptyScene();
            auto& root = scene->getRoot();
            auto& parent = root.attachChild(factory.createSceneObject());
            parent.getRootComponent().setScale({2.f, 0.65f, 3.f});
            auto& scaled = parent.attachChild(factory.createSceneObject<scene::CameraComponent>())
                               .getRootComponent<scene::CameraComponent>();
            scaled.setTranslation({0.f, 0.f, 4.f / 3.f});
            scaled.setFov(60.f);
            m_scaledCamera = scaled;

            auto& other = root.attachChild(factory.createSceneObject<scene::CameraComponent>())
                              .getRootComponent<scene::CameraComponent>();
            other.setTranslation({0.f, 0.f, 4.f});
            other.setFov(60.f);
            m_otherCamera = other;

            m_markers = test::addCameraPreviewMarkers(factory, root);
            root.addComponent<scene::DirectionalLightComponent>().setCastShadows(false);
            // Load material pipelines and textures before the two render paths can request
            // them concurrently. Keep the real asset views alive through scene deactivation.
            auto cubeAsset = root.findFirstComponent<scene::StaticMeshComponent>(true)->getMeshGeometry();
            auto& robot = *root.findFirstComponent<SkinnedMeshComponent>(true);
            auto spriteAsset = m_markers.billboard->getTextureRef();
            m_assets[0] = co_await cubeAsset.getReloadableAssetViewTyped<StaticMeshAssetView>();
            m_assets[1] = co_await robot.getMeshGeometry().getReloadableAssetViewTyped<SkinnedMeshAssetView>();
            m_assets[2] = co_await robot.getMaterial().getReloadableAssetViewTyped<MaterialAssetView>();
            m_assets[3] = co_await spriteAsset.getReloadableAssetViewTyped<TextureAssetView>();
            m_scene = co_await getServiceProvider().get<scene::ISceneManager>().activateScene(std::move(scene));
            getServiceProvider().get<scene::ICameraManager>().setMainCamera(scaled);
        }

        void onApplicationStep(std::chrono::milliseconds) override
        {
            if (!m_checks)
            {
                m_checks = runAndCleanUp();
            }
            else if (m_checks.isReady())
            {
                if (m_checks.isRejected())
                {
                    std::fprintf(stderr, "%s\n", m_checks.getError()->getMessage().c_str());
                }
                else
                {
                    m_passed = m_checks.result();
                }
                m_checks = nullptr;
                getApplication().stop();
            }
        }

    private:
        Task<bool> runAndCleanUp()
        {
            const bool passed = co_await runChecks();
            ASYNC_SWITCH_EXECUTOR(getApplication().getExecutor());
            auto window = getServiceProvider().get<ICoreGraphics>().getDefaultRenderWindow().acquire();
            if (window)
            {
                window->setCameraPreview(std::nullopt);
                co_await presentedFrame(*window);
                getServiceProvider().get<scene::ISceneManager>().deactivateScene(m_scene);
                co_await presentedFrame(*window);
            }
            m_assets = {};
            co_return passed;
        }

        Task<bool> runChecks()
        {
            ASYNC_SWITCH_EXECUTOR(getApplication().getExecutor());
            auto window = getServiceProvider().get<ICoreGraphics>().getDefaultRenderWindow().acquire();
            try
            {
                require(window && m_scaledCamera && m_otherCamera && m_markers.billboard, "Scene startup incomplete");
                require(std::all_of(m_assets.begin(), m_assets.end(), [](const auto& asset) { return bool(asset); }),
                        "Failed to load marker assets");
                window->setDrawViewportGrid(false);
                co_await window->disableRenderStages(render::NauRenderStage::DebugStage | render::NauRenderStage::NauGUIStage);
                co_await presentedFrame(*window, 12);
                co_await window->requestViewportResizeImmediate(mainWidth, mainHeight);
                const Frame baseline = co_await presentedFrame(*window, 20);
                require(baseline.width == mainWidth && baseline.height == mainHeight, "Initial viewport size incorrect");
                require((co_await window->requestUidByCoords(320, 180)) == m_markers.staticUid, "Center pick missed the cube");
                std::atomic<unsigned> errors = 0;
                auto subscription = diag::getLogger().subscribe([&](const diag::LoggerMessage& msg) {
                    if (msg.level == diag::LogLevel::Error || msg.level == diag::LogLevel::Critical)
                    {
                        ++errors;
                        std::fprintf(stderr, "Error during preview: %s\n", msg.data.c_str());
                    }
                });

                // The editor supplies the selected camera's world pose. Both paths must
                // agree even though that pose includes scale inherited from the parent.
                auto preview = makePreview(m_scaledCamera->getCameraUid(), baseline.bounds(), mainWidth, mainHeight);
                preview.worldPose = m_scaledCamera->getWorldTransform();
                preview.excludedComponents = {m_markers.staticUid};
                window->setCameraPreview(preview);
                const Rect cubeCenter{mainWidth / 2 - 8, mainHeight / 2 - 8, 16, 16};
                require(changedPixels(baseline, co_await presentedFrame(*window), cubeCenter, 8) > 128,
                        "Full-size preview did not remove its excluded cube");
                preview.excludedComponents.clear();
                window->setCameraPreview(preview);
                // Full-size comparison avoids spatial filtering at material and sprite edges.
                const Frame full = co_await presentedFrame(*window);
                checkMatchingCamera(baseline, full);
                preview.excludedComponents = {m_markers.billboardUid};
                window->setCameraPreview(preview);
                const Frame withoutSprite = co_await presentedFrame(*window);
                size_t billboardPixels = 0;
                for (size_t i = 0; i < full.pixels.size(); ++i)
                {
                    if (differs(full.pixels[i], withoutSprite.pixels[i], 4))
                    {
                        ++billboardPixels;
                        require(!differs(full.pixels[i], baseline.pixels[i], 2), "Full-size billboard colors differ");
                    }
                }
                require(billboardPixels > 100, "Full-size preview is missing its colored billboard");
                preview = makePreview(m_scaledCamera->getCameraUid(), firstInset, firstInset.width, firstInset.height);
                preview.worldPose = m_scaledCamera->getWorldTransform();
                window->setCameraPreview(preview);
                const Frame scaled = co_await presentedFrame(*window);
                require(changedPixels(baseline, scaled, firstInset, 4) > 200, "No inset at the requested rectangle");
                require(changedOutside(baseline, scaled, firstInset) == 0, "Showing the inset altered the main picture");
                require((co_await window->requestUidByCoords(320, 180)) == m_markers.staticUid, "Picking outside the inset changed");

                getServiceProvider().get<scene::ICameraManager>().setMainCamera(*m_otherCamera);
                preview.worldPose.reset();
                window->setCameraPreview(preview);
                const Frame both = co_await presentedFrame(*window);
                m_scaledCamera->setTranslation({0.7f, 0.f, 4.f / 3.f});
                const Frame previewMoved = co_await presentedFrame(*window);
                require(changedPixels(both, previewMoved, firstInset, 8) > 100, "Selected camera motion did not reach inset");
                require(changedOutside(both, previewMoved, firstInset) == 0, "Selected camera motion changed main");
                m_otherCamera->setTranslation({-1.5f, 0.f, 4.f});
                const Frame mainMoved = co_await presentedFrame(*window);
                require(changedOutside(previewMoved, mainMoved, firstInset, 8) > 100, "Main camera did not move");
                require(changedPixels(previewMoved, mainMoved, firstInset) == 0, "Main camera motion changed inset");

                m_scaledCamera->setTranslation({0.f, 0.f, 4.f / 3.f});
                m_otherCamera->setTranslation({0.f, 0.f, 4.f});
                const Frame visible = co_await presentedFrame(*window);
                const auto filter = [&](Uid id) {
                    preview.excludedComponents = {id};
                    window->setCameraPreview(preview);
                };
                // Exclude each marker separately to catch filters that remove an entire draw list.
                const Rect middle{firstInset.x + firstInset.width * 7 / 16, firstInset.y + firstInset.height / 3,
                                  firstInset.width / 8, firstInset.height / 3};
                const Rect right{firstInset.x + firstInset.width * 5 / 8, firstInset.y,
                                 firstInset.width * 3 / 8, firstInset.height};
                filter(m_markers.staticUid);
                const Frame noCube = co_await presentedFrame(*window);
                require(changedPixels(visible, noCube, middle, 8) > 40 && changedPixels(visible, noCube, right) == 0,
                        "Excluding the static cube did not remove only its inset geometry");
                require(changedOutside(visible, noCube, firstInset) == 0, "Static exclusion leaked into main");
                filter(m_markers.skinnedUid);
                const Frame noRobot = co_await presentedFrame(*window);
                require(changedPixels(visible, noRobot, right, 8) > 20 && changedPixels(visible, noRobot, middle) == 0,
                        "Excluding the skinned robot did not remove only its inset geometry");
                require(changedOutside(visible, noRobot, firstInset) == 0, "Skinned exclusion leaked into main");

                filter(m_markers.billboardUid);
                const Frame noBillboard = co_await presentedFrame(*window);
                require(changedPixels(visible, noBillboard, firstInset, 8) > 20, "No visible, excludable inset billboard");
                require(changedOutside(visible, noBillboard, firstInset) == 0, "Billboard exclusion leaked into main");
                preview.excludedComponents.clear();
                window->setCameraPreview(preview);
                m_markers.billboard->setVisibility(false);
                const Frame hidden = co_await presentedFrame(*window);
                require(changedPixels(noBillboard, hidden, firstInset) == 0 && changedOutside(visible, hidden, firstInset, 8) > 20,
                        "Billboard exclusion did not match hiding the actual billboard in the inset");
                checkBillboardColor(visible, hidden, firstInset);
                m_markers.billboard->setVisibility(true);

                window->setCameraPreview(std::nullopt);
                const Frame offBeforeMove = co_await presentedFrame(*window);
                window->setCameraPreview(preview);
                const Frame restored = co_await presentedFrame(*window);
                require(changedPixels(visible, restored, visible.bounds()) == 0, "Billboard or inset failed to reappear");
                preview = makePreview(m_scaledCamera->getCameraUid(), secondInset, 384, 216);
                preview.excludedComponents = {m_markers.staticUid};
                window->setCameraPreview(preview);
                const Frame relocated = co_await presentedFrame(*window);
                window->setCameraPreview(std::nullopt);
                const Frame off = co_await presentedFrame(*window);
                require(changedOutside(relocated, off, secondInset) == 0, "Old inset remained after moving/resizing it");
                require(changedPixels(relocated, off, secondInset, 8) > 200, "Relocated inset missing");
                require(changedPixels(off, offBeforeMove, off.bounds()) == 0, "Deselect failed to restore the original picture");
                window->setCameraPreview(preview);
                co_await presentedFrame(*window);
                require((co_await window->requestUidByCoords(320, 180)) == m_markers.staticUid,
                        "Picking through the inset used preview UID/depth instead of main");

                // Compare every observed OLD-size frame after the request to a stable active
                // frame. This exposes using the pending aspect before the G-buffer resizes.
                ASYNC_SWITCH_EXECUTOR(getServiceProvider().get<ICoreGraphics>().getPreRenderExecutor());
                checkViewport(*window, mainWidth, mainHeight);
                co_await window->requestViewportResize(480, 480);
                checkViewport(*window, mainWidth, mainHeight);
                int pendingFrames = 0;
                Frame resized;
                for (int i = 0; i < 12; ++i)
                {
                    resized = co_await presentedFrame(*window, 1, true);
                    if (resized.width == 480 && resized.height == 480)
                    {
                        break;
                    }
                    require(changedPixels(relocated, resized, relocated.bounds()) == 0,
                            "Projection changed before the underlying viewport resized");
                    ++pendingFrames;
                }
                require(pendingFrames > 0 && resized.width == 480 && resized.height == 480,
                        "Did not observe both deferred and applied resize");
                window->setCameraPreview(std::nullopt);
                const Frame square = co_await presentedFrame(*window);
                require((co_await window->requestUidByCoords(240, 240)) == m_markers.staticUid, "Pick after resize missed cube");
                // At a 60-degree FOV the front face (z=.48) of the cube spans about 114 px
                // on this square target. These probes distinguish square from stale 16:9 projection.
                require((co_await window->requestUidByCoords(240, 290)) == m_markers.staticUid &&
                        (co_await window->requestUidByCoords(240, 305)) != m_markers.staticUid, "Incorrect vertical aspect after resize");
                const Rect squareInset = square.bounds();
                auto squareRequest = makePreview(m_otherCamera->getCameraUid(), squareInset, square.width, square.height);
                squareRequest.excludedComponents = {m_markers.staticUid};
                window->setCameraPreview(squareRequest);
                require(changedPixels(square, co_await presentedFrame(*window), {232, 232, 16, 16}, 8) > 128,
                        "Square preview did not remove its excluded cube");
                squareRequest.excludedComponents.clear();
                window->setCameraPreview(squareRequest);
                const Frame squarePreview = co_await presentedFrame(*window);
                checkMatchingCamera(square, squarePreview);
                window->setCameraPreview(std::nullopt);
                require(changedPixels(square, co_await presentedFrame(*window), square.bounds()) == 0,
                        "Deselect after resize left pixels behind");
                require(errors == 0, "Renderer logged errors after enabling the preview");
                std::printf("DX12 camera viewport checks passed (%d old-size frames observed)\n", pendingFrames);
                co_return true;
            }
            catch (const std::exception& error)
            {
                std::fprintf(stderr, "DX12 camera viewport check failed: %s\n", error.what());
            }
            co_return false;
        }

        bool& m_passed;
        Task<bool> m_checks;
        scene::ObjectWeakRef<scene::CameraComponent> m_scaledCamera, m_otherCamera;
        scene::IScene::WeakRef m_scene;
        std::array<ReloadableAssetView::Ptr, 4> m_assets;
        test::CameraPreviewMarkers m_markers;
    };
}

int main()
{
    bool passed = false;
    const int status = nau::runApplication(eastl::make_unique<PreviewRegression>(passed));
    return status == 0 && passed ? 0 : 1;
}
