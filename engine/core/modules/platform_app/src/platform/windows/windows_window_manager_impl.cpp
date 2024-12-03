// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
// windows_app.cpp


#include "./windows_window_manager_impl.h"

#include "nau/app/app_messages.h"
#include "nau/runtime/internal/runtime_object_registry.h"
#include "nau/service/service_provider.h"
#include "nau/utils/performance_profiling.h"
#include <nau/graphics/core_graphics.h>
// #include "nau/input.h"

namespace nau
{
    namespace
    {
        constexpr UINT WM_NAU_ASYNC_MESSAGE = WM_USER + 100;

        void registerNauWindowClass(WNDPROC wndProc)
        {
            const HINSTANCE hInst = ::GetModuleHandleA(nullptr);

            WNDCLASSEXW windowClass = {};

            windowClass.cbSize = sizeof(WNDCLASSEX);
            windowClass.style = CS_HREDRAW | CS_VREDRAW;
            windowClass.lpfnWndProc = wndProc;
            windowClass.cbClsExtra = 0;
            windowClass.cbWndExtra = 0;
            windowClass.hInstance = hInst;
            windowClass.hIcon = ::LoadIcon(hInst, NULL);
            windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
            windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            windowClass.lpszMenuName = NULL;
            windowClass.lpszClassName = WindowsWindowManager::WindowClassName;
            windowClass.hIconSm = ::LoadIcon(hInst, NULL);

            ATOM atom = ::RegisterClassExW(&windowClass);
            NAU_ASSERT(atom > 0);
        }

    }  // namespace

    WindowsWindowManager::WindowsWindowManager()
    {
        registerNauWindowClass(WindowsWindowManager::wndProc);
        RuntimeObjectRegistration{{this}}.setAutoRemove();
    }

    WindowsWindowManager::~WindowsWindowManager()
    {
        m_window.reset();
        ::UnregisterClassW(WindowsWindowManager::WindowClassName, ::GetModuleHandleA(nullptr));
    }

    bool WindowsWindowManager::checkAppThread()
    {
        const auto currentThreadId = ::GetCurrentThreadId();

        if (m_threadId == 0)
        {
            m_threadId = currentThreadId;
            return true;
        }

        NAU_ASSERT(m_threadId == currentThreadId, "Invalid thread");
        return m_threadId == currentThreadId;
    }

    IPlatformWindow& WindowsWindowManager::getActiveWindow()
    {
        NAU_ASSERT(m_window);
        return *m_window;
    }

    void WindowsWindowManager::bindToCurrentThread()
    {
        checkAppThread();

        m_window = rtti::createInstanceSingleton<WindowsWindow>(*this, ::GetModuleHandleA(nullptr), WindowsWindowManager::WindowClassName, true);

        {
            const auto windowMessageHandlers = getServiceProvider().getAll<IWindowMessageHandler>();
            m_windowMessageHandlers.resize(windowMessageHandlers.size());
            eastl::copy(windowMessageHandlers.begin(), windowMessageHandlers.end(), m_windowMessageHandlers.begin());
        }

        {
            const auto appMessageHandlers = getServiceProvider().getAll<IWindowsApplicationMessageHandler>();
            m_appMessageHandlers.resize(appMessageHandlers.size());
            eastl::copy(appMessageHandlers.begin(), appMessageHandlers.end(), m_appMessageHandlers.begin());
        }
    }

    async::Executor::Ptr WindowsWindowManager::getExecutor()
    {
        return static_cast<async::Executor*>(this);
    }

    Result<> WindowsWindowManager::pumpMessageQueue(bool waitForMessage, std::optional<std::chrono::milliseconds> maxProcessingTime)
    {
        // TODO Tracy        NAU_CPU_SCOPED_TAG(nau::PerfTag::Platform);
        if (!checkAppThread())
        {
            return NauMakeError("Invalid thread");
        }

        scope_on_leave
        {
            processAsyncInvocations();
        };

        if (m_isDisposed)
        {
            return NauMakeError("Disposed");
        }

        // TODO: use maxProcessingTime
        MSG msg{};
        memset(&msg, 0, sizeof(msg));

        const BOOL messageIsTaken = waitForMessage ? ::GetMessageA(&msg, nullptr, 0, 0) : ::PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE);
        if (messageIsTaken == TRUE)
        {
            if (msg.message == WM_QUIT)
            {
                dispose();
                return NauMakeError("Disposed");
            }

            {
                using PreDispatchMsgResult = IWindowsApplicationMessageHandler::PreDispatchMsgResult;

                auto preDispatchRes = PreDispatchMsgResult::Normal;
                for (IWindowsApplicationMessageHandler* const handler : m_appMessageHandlers)
                {
                    const auto res = handler->preDispatchMsg(msg);
                    if (res == PreDispatchMsgResult::QuitApp)
                    {
                        preDispatchRes = PreDispatchMsgResult::QuitApp;
                    }
                    else if (res == PreDispatchMsgResult::SkipMessage && preDispatchRes == PreDispatchMsgResult::Normal)
                    {
                        preDispatchRes = PreDispatchMsgResult::SkipMessage;
                    }
                }

                if (preDispatchRes == PreDispatchMsgResult::QuitApp)
                {
                    dispose();
                    return NauMakeError("Disposed");
                }

                if (preDispatchRes == PreDispatchMsgResult::SkipMessage)
                {
                    return {};
                }
            }

            ::TranslateMessage(&msg);
            ::DispatchMessageA(&msg);


            for (IWindowsApplicationMessageHandler* const handler : m_appMessageHandlers)
            {
                handler->postDispatchMsg(msg);
            }
        }

        return {};
    }

    nau::Ptr<IPlatformWindow> WindowsWindowManager::createWindow(bool exitAppOnClose)
    {
        if (m_threadId != ::GetCurrentThreadId())
        {
            auto task = async::run([](WindowsWindowManager& self, bool exitAppOnClose) -> nau::Ptr<WindowsWindow>
            {
                return rtti::createInstance<WindowsWindow>(self, ::GetModuleHandleA(nullptr), WindowsWindowManager::WindowClassName, exitAppOnClose);

            }, nau::Ptr{this}, std::ref(*this), exitAppOnClose);

            async::wait(task);
            return *task;
        }

        return rtti::createInstance<WindowsWindow>(*this, ::GetModuleHandleA(nullptr), WindowsWindowManager::WindowClassName, exitAppOnClose);
    }

    void WindowsWindowManager::processAsyncInvocations()
    {
// TODO Tracy        NAU_CPU_SCOPED_TAG(nau::PerfTag::Platform);
        using namespace nau::async;

        eastl::vector<Executor::Invocation> invocations;
        {
            lock_(m_mutex);
            if (m_asyncInvocations.empty())
            {
                return;
            }

            invocations = std::move(m_asyncInvocations);
            m_asyncInvocations.resize(0);
            m_isProcessAsyncInvocations = true;
        }

        scope_on_leave
        {
            m_isProcessAsyncInvocations = false;
        };

        const Executor::InvokeGuard invokeGuard{*this};

        for (auto& invocation : invocations)
        {
            Executor::invoke(*this, std::move(invocation));
        }
    }

    void WindowsWindowManager::waitAnyActivity() noexcept
    {
    }

    void WindowsWindowManager::scheduleInvocation(Invocation invocation) noexcept
    {
        {
            lock_(m_mutex);
            m_asyncInvocations.emplace_back(std::move(invocation));
        }

        if (m_threadId != 0)
        {
            [[maybe_unused]] const auto postOk = ::PostThreadMessageW(m_threadId, WM_NAU_ASYNC_MESSAGE, 0, 0);
        }
    }

    bool WindowsWindowManager::hasWorks()
    {
        lock_(m_mutex);
        return !m_asyncInvocations.empty() || m_isProcessAsyncInvocations;
    }

    void WindowsWindowManager::dispose()
    {
        if (const bool alreadyDisposed = m_isDisposed.exchange(true); alreadyDisposed)
        {
            return;
        }

        if (m_threadId == 0)
        {
            return;
        }

        if (m_threadId == ::GetCurrentThreadId())
        {
            ::PostQuitMessage(0);
        }
        else
        {
            [[maybe_unused]] const bool postOk = ::PostThreadMessageW(m_threadId, WM_QUIT, 0, 0) == TRUE;
            NAU_ASSERT(postOk);
        }
    }

    bool WindowsWindowManager::handleWindowMessage(WindowsWindow& window, HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
// TODO Tracy NAU_CPU_SCOPED_TAG(nau::PerfTag::Platform);
        if (message == WM_CLOSE)
        {
            if (window.exitAppOnClose())
            {
                AppWindowClosed.post();
            }
            else
            {
                if (auto* const coreGraphics = getServiceProvider().find<ICoreGraphics>())
                {
                    auto task = coreGraphics->closeWindow(hWnd);
                    task.detach();
                }
                window.destroyWindow();
            }
        }
        else if (message == WM_SIZE)
        {
            int width  = LOWORD(lParam);
            int height = HIWORD(lParam);

            if (auto* const coreGraphics = getServiceProvider().find<ICoreGraphics>())
            {
                auto task = coreGraphics->requestViewportResize(width, height, hWnd);
                task.detach();
            }
        }
        else if (message == WM_DESTROY)
        {
            if (window.exitAppOnClose())
            {
                dispose();
            }
        }
        else
        {
            bool messageHandled = false;

            for (IWindowMessageHandler* handler : m_windowMessageHandlers)
            {
                const bool handled = handler->handleMessage(hWnd, message, wParam, lParam);
                messageHandled = messageHandled || handled;
            }

            return messageHandled;
        }

        return true;
    }

    LRESULT WindowsWindowManager::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_CREATE)
        {
            const auto* const createStruct = reinterpret_cast<CREATESTRUCT*>(lParam);
            NAU_ASSERT(createStruct->lpCreateParams);
            ::SetWindowLongPtrA(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
        }

        WindowsWindow* const window = EXPR_Block->WindowsWindow*
        {
            const LONG_PTR ptr = ::GetWindowLongPtrW(hWnd, GWLP_USERDATA);
            return ptr ? reinterpret_cast<WindowsWindow*>(ptr) : nullptr;
        };

        if (!window)
        {
            return ::DefWindowProcW(hWnd, message, wParam, lParam);
        }

        auto& self = window->getWindowManager().as<WindowsWindowManager&>();
        if (!self.handleWindowMessage(*window, hWnd, message, wParam, lParam))
        {
            return ::DefWindowProcW(hWnd, message, wParam, lParam);
        }

        return 0;
    }
}  // namespace nau
