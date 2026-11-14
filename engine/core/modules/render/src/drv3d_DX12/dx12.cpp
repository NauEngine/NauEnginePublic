// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "device.h"

#include "drv3d_commonCode/stereoHelper.h"
#include "drv3d_commonCode/dxgi_utils.h"
#include "drv3d_commonCode/gpuConfig.h"
#include "drv3d_commonCode/validate_sbuf_flags.h"

//#include <../hid_mouse/api_wrappers.h>

#include "nau/3d/dag_drv3d_res.h"
#include "nau/3d/dag_nvLowLatency.h"
#include "nau/3d/dag_lowLatency.h"
#include "nau/3d/dag_drv3dCmd.h"
#include "nau/3d/dag_drv3d_platform.h"
#include "nau/dag_ioSys/dag_genIo.h"
#include "nau/dag_ioSys/dag_memIo.h"
#include "nau/image/dag_texPixel.h"
//#include <math/integer/dag_IPoint2.h>

#include <EASTL/sort.h>

#include <drv3d_commonCode/drv_utils.h>
#include <drv3d_commonCode/drv_returnAddrStore.h>

#include "frontend_state.h"

#include "drv3d_commonCode/frameStateTM.inc.h"

#include "nau/dataBlock/dag_dataBlock.h"

//#include <util/dag_watchdog.h>
#include <drv3d_commonCode/renderPassGeneric.h>
//#include <perfMon/dag_pix.h>

#if _TARGET_PC_WIN
//#include <osApiWrappers/dag_direct.h>
//#include <osApiWrappers/dag_unicode.h>
#endif

#include "driver_mutex.h"
#include "nau/threading/spin_lock.h"
#include "nau/threading/lock_guard.h"


#if _TARGET_PC_WIN
extern "C"
{
  _declspec(dllexport) extern UINT D3D12SDKVersion = 0;
  _declspec(dllexport) extern const char *D3D12SDKPath = ".\\D3D12\\";
}

namespace drv3d_dx12
{
eastl::add_pointer_t<decltype(::WaitOnAddress)> WaitOnAddress;
eastl::add_pointer_t<decltype(::WakeByAddressAll)> WakeByAddressAll;
eastl::add_pointer_t<decltype(::WakeByAddressSingle)> WakeByAddressSingle;
} // namespace drv3d_dx12

namespace
{
constexpr int min_major_feature_level = 11;
constexpr int min_minor_feature_level = 0;

bool is_software_device(const DXGI_ADAPTER_DESC1 &desc)
{
  constexpr UINT software_driver_vendor = 0x1414;
  constexpr UINT software_driver_id = 0x8c;
  // checking software flag is insuficient, on some systems (even with exact same patch level and
  // drivers) this flag might not be set by the dx runtime and we have to manually check for
  // software device and vendor id.
  return (0 != (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) ||
         (desc.VendorId == software_driver_vendor && desc.DeviceId == software_driver_id);
}
D3D_FEATURE_LEVEL make_feature_level(int major, int minor)
{
  struct FeatureLevelTableEntry
  {
    int major;
    int minor;
    D3D_FEATURE_LEVEL level;
  };
  FeatureLevelTableEntry featureLevelTable[] = //
    {{11, 0, D3D_FEATURE_LEVEL_11_0}, {11, 1, D3D_FEATURE_LEVEL_11_1}, {12, 0, D3D_FEATURE_LEVEL_12_0},
      {12, 1, D3D_FEATURE_LEVEL_12_1}};
  auto ref = eastl::find_if(eastl::begin(featureLevelTable), eastl::end(featureLevelTable),
    [=](const FeatureLevelTableEntry &flte) { return major == flte.major && minor == flte.minor; });
  if (ref != eastl::end(featureLevelTable))
    return ref->level;
  return D3D_FEATURE_LEVEL_12_0;
}
} // namespace
#endif

namespace gpu
{
enum
{
  VENDOR_ID_AMD = 0x03EA,
  VENDOR_ID_INTEL = 0x8086,
  VENDOR_ID_NVIDIA = 0x10DE
};
}

bool check_is_main_thread();

using namespace drv3d_dx12;
namespace drv3d_dx12
{
FrameStateTM g_frameState;
}
#define CHECK_MAIN_THREAD()                                                                        \
  NAU_ASSERT(check_is_main_thread(), "DX12: Not thread safe D3D context function called without "   \
                                    "holding its context lock. If you are taking locks manually "  \
                                    "and mix it with RAII helpers, like render target scope, the " \
                                    "lock will be released before the destructor is called with "  \
                                    "the offending D3D function call, this is because "            \
                                    "destructors are called on scope exit at the closing }.")
//#include "drv3d_commonCode/frameStateTM.inc.cpp"

/////////////////////////// From frameStateTM.inc.cpp
bool d3d::setpersp(const Driver3dPerspective &p, nau::math::Matrix4 *proj_tm)
{
  CHECK_MAIN_THREAD();
  g_frameState.setpersp(p, proj_tm);
  return true;
}

bool d3d::calcproj(const Driver3dPerspective &p, nau::math::Matrix4 &proj_tm)
{
  g_frameState.calcproj(p, proj_tm);
  return true;
}

void d3d::calcglobtm(const nau::math::Matrix4 &view_tm, const nau::math::Matrix4 &proj_tm, nau::math::Matrix4 &result)
{
  g_frameState.calcglobtm(view_tm, proj_tm, result);
}

void d3d::calcglobtm(const nau::math::Matrix4 &view_tm, const Driver3dPerspective &persp, nau::math::Matrix4 &result)
{
  g_frameState.calcglobtm(view_tm, persp, result);
}

bool d3d::getpersp(Driver3dPerspective &p) { return g_frameState.getpersp(p); }

bool d3d::validatepersp(const Driver3dPerspective &p) { return g_frameState.validatepersp(p); }

void d3d::setglobtm(nau::math::Matrix4 &tm) { g_frameState.setglobtm(tm); }

bool d3d::settm(int which, const nau::math::Matrix4 *m)
{
  CHECK_MAIN_THREAD();
  g_frameState.settm(which, *m);
  return true;
}

bool d3d::settm(int which, const nau::math::Matrix4 &m) { g_frameState.settm(which, m);  return true; }


bool d3d::gettm(int which, nau::math::Matrix4 *out_m)
{
  g_frameState.gettm(which, out_m);
  return true;
}

const nau::math::Matrix4 &d3d::gettm_cref(int which) { return g_frameState.gettm_cref(which); }


bool d3d::gettm(int which, nau::math::Matrix4 &t)
{
  g_frameState.gettm(which, t);
  return true;
}

void d3d::getm2vtm(nau::math::Matrix4 &tm) { g_frameState.getm2vtm(tm); }

void d3d::getglobtm(nau::math::Matrix4 &tm) { g_frameState.getglobtm(tm); }

void d3d::setglobtm(const nau::math::Matrix4 &tm) { g_frameState.setglobtm(tm); }

//////////////// End from frameStateTM.inc.cpp



#include "nau/diag/logging.h"

const bool d3d::HALF_TEXEL_OFS = false;
const float d3d::HALF_TEXEL_OFSFU = 0.0f;

namespace drv3d_dx12
{
#if _TARGET_PC_WIN

static void report_agility_sdk_error(HRESULT hr)
{
  //TODO: repair
  //if (hr != D3D12_ERROR_INVALID_HOST_EXE_SDK_VERSION)
  //  return;
  //
  //wchar_t exePathW[MAX_PATH] = {};
  //if (::GetModuleFileNameW(NULL, exePathW, countof(exePathW)) == 0)
  //  return;
  //
  //char utf8buf[MAX_PATH * 3] = {};
  //::wcs_to_utf8(exePathW, utf8buf, countof(utf8buf));
  //
  //const char *exeDir = ::dd_get_fname_location(utf8buf, utf8buf);
  //
  //String dllPath(0, "{}{}D3D12Core.dll", exeDir, D3D12SDKPath);
  //if (!dd_file_exists(dllPath))
  //{
  //  NAU_LOG_DEBUG("DX12: {} is missing", dllPath.c_str());
  //  return;
  //}
  //dllPath.printf(0, "{}{}d3d12SDKLayers.dll", exeDir, D3D12SDKPath);
  //if (!dd_file_exists(dllPath))
  //{
  //  NAU_LOG_DEBUG("DX12: {} is missing", dllPath.c_str());
  //  return;
  //}
  //NAU_LOG_DEBUG("DX12: D3D12SDKVersion {} isn't available", D3D12SDKVersion);
}

static PresentationMode get_presentation_mode_from_settings()
{
  //TODO: repair
  //const nau::DataBlock *videoBlk = ::dgs_get_settings()->getBlockByNameEx("video");
  PresentationMode mode;
  // Only Nvidia modes are disabled
  const lowlatency::LatencyMode latencyMode = lowlatency::get_from_blk();
  const bool vsyncDisabled = (latencyMode == lowlatency::LATENCY_MODE_NV_ON || latencyMode == lowlatency::LATENCY_MODE_NV_BOOST);
  //if (videoBlk->getBool("vsync", false) && !vsyncDisabled)
    mode = PresentationMode::VSYNCED;
  //else if (videoBlk->getBool("adaptive_vsync", false))
    //mode = PresentationMode::CONDITIONAL_VSYNCED;
  //else
    //mode = PresentationMode::UNSYNCED;

  // Until the video settings are repaired, vsync can be switched off with the environment variable NAU_VSYNC=0 (for
  // frame time measurements, see doc/gpu_profiler.md). Without vsync the swapchain presents with
  // DXGI_PRESENT_ALLOW_TEARING when the adapter supports tearing, also in windowed mode (Swapchain::swapchainPresentOnPC).
  const char *vsyncVariable = getenv("NAU_VSYNC");
  if (vsyncVariable != nullptr && vsyncVariable[0] == '0' && vsyncVariable[1] == '\0')
    mode = PresentationMode::UNSYNCED;

  static bool reported = false;
  if (!reported)
  {
    reported = true;
    NAU_LOG_INFO("DX12: vsync {}{}", mode == PresentationMode::UNSYNCED ? "off" : "on",
      vsyncVariable != nullptr ? " (NAU_VSYNC)" : "");
  }
  return mode;
}

struct WindowState
{
  RenderWindowSettings settings = {};
  RenderWindowParams params = {};
  bool ownsWindow = false;
  bool vsync = false;
  bool occludedWindow = false;
  inline static main_wnd_f *mainCallback = nullptr;
  inline static WNDPROC originWndProc = nullptr;

  static LRESULT CALLBACK windowProcProxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  WindowState() = default;
  ~WindowState() { closeWindow(); }
  WindowState(const WindowState &) = delete;
  WindowState &operator=(const WindowState &) = delete;

  void set(void *hinst, const char *name, int show, void *mainw, void *renderw, void *icon, const char *title, void *wnd_proc)
  {
    ownsWindow = renderw == nullptr;
    params.hinst = hinst;
    params.wcname = name;
    params.ncmdshow = show;
    params.hwnd = mainw;
    params.rwnd = renderw;
    params.icon = icon;
    params.title = title;
    params.mainProc = &windowProcProxy;
    mainCallback = (main_wnd_f *)wnd_proc;
    if (!ownsWindow && !originWndProc)
      originWndProc = (WNDPROC)SetWindowLongPtrW((HWND)params.rwnd, GWLP_WNDPROC, (LONG_PTR)params.mainProc);
  }

  void getRenderWindowSettings(Driver3dInitCallback *cb)
  {
      if(dgs_get_window_mode() == WindowMode::WINDOWED_IN_EDITOR)
      {
          NAU_ASSERT(params.rwnd);

          RECT wndRect, clnRect;

          GetWindowRect((HWND)params.rwnd, &wndRect);
          GetClientRect((HWND)params.rwnd, &clnRect);

          settings.winRectLeft = wndRect.left;
          settings.winRectRight = wndRect.right;
          settings.winRectTop = wndRect.top;
          settings.winRectBottom = wndRect.bottom;

          int base_scr_wdt, base_scr_hgt;
          int base_scr_left, base_scr_top;
          get_current_display_screen_mode(base_scr_left, base_scr_top, base_scr_wdt, base_scr_hgt);

          settings.resolutionX = base_scr_wdt;
          settings.resolutionY = base_scr_hgt;
          settings.clientWidth = clnRect.right - clnRect.left;
          settings.clientHeight = clnRect.bottom - clnRect.top;
          settings.aspect = settings.clientHeight ? float(settings.clientWidth) / float(settings.clientHeight) : 1.0f;

          settings.winStyle = WS_POPUP; // WS_CHILD
          settings.winStyle |= WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_BORDER;

          return;
      }

      get_render_window_settings(settings, cb);
  }

  bool setRenderWindowParams() { return set_render_window_params(params, settings); }

  void *getMainWindow()
  {
      return params.hwnd;
  }

  void closeWindow()
  {
    if (ownsWindow)
    {
      DestroyWindow((HWND)params.hwnd);
      ownsWindow = false;
    }
    else
    {
      SetWindowLongPtrW((HWND)params.rwnd, GWLP_WNDPROC, (LONG_PTR)originWndProc);
      originWndProc = nullptr;
    }
  }

  bool updateWindowOcclusionState()
  {
    occludedWindow = is_window_occluded((HWND)params.hwnd);
    return occludedWindow;
  }

  bool isWindowOccluded() const { return occludedWindow; }
};

#elif _TARGET_XBOX

static PresentationMode get_presentation_mode_from_settings()
{
  const nau::DataBlock *videoBlk = ::dgs_get_settings()->getBlockByNameEx("video");
  PresentationMode mode;
  if (videoBlk->getBool("vsync", true))
    mode = PresentationMode::VSYNCED;
  else
    mode = PresentationMode::UNSYNCED;
  return mode;
}

struct WindowState
{
  struct Settings
  {
    int resolutionX;
    int resolutionY;
    float aspect;
  } settings = {};

  struct Params
  {
    void *hinst;
    const char *wcname;
    int ncmdshow;
    void *hwnd;
    void *rwnd;
    void *icon;
    const char *title;
    void *winRect;
    void *mainProc;
  } params = {};
  bool ownsWindow = false;
  bool vsync = false;
  inline static main_wnd_f *mainCallback = nullptr;

  static LRESULT CALLBACK windowProcProxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

  WindowState() = default;
  ~WindowState() { closeWindow(); }
  WindowState(const WindowState &) = delete;
  WindowState &operator=(const WindowState &) = delete;

  void set(void *hinst, const char *name, int show, void *mainw, void *renderw, void *icon, const char *title, void *wnd_proc)
  {
    ownsWindow = mainw == nullptr;
    params.hinst = hinst;
    params.wcname = name;
    params.ncmdshow = show;
    params.hwnd = mainw;
    params.rwnd = renderw;
    params.icon = icon;
    params.title = title;
    params.mainProc = &windowProcProxy;
    mainCallback = (main_wnd_f *)wnd_proc;
  }

  void getRenderWindowSettings(Driver3dInitCallback *) { getRenderWindowSettings(); }

  void getRenderWindowSettings() { xbox_get_render_window_settings(settings); }

  bool setRenderWindowParams() { return true; }

  void *getMainWindow() { return params.hwnd; }

  void closeWindow()
  {
    if (ownsWindow)
    {
      DestroyWindow((HWND)params.hwnd);
      ownsWindow = false;
    }
  }
};

#endif

struct ApiState
{
  bool isInitialized;
  WindowState windowState;
#if _TARGET_PC_WIN
  Direct3D12Enviroment d3d12Env;
  ComPtr<DXGIFactory> dxgi14;
#endif
  Device device;
  HRESULT lastErrorCode;
  eastl::string deviceName;
  bool deviceWasLost;
  Driver3dDesc driverDesc = {};
  DriverMutex globalLock;
  eastl::vector<uint8_t> screenCaptureBuffer;
  FrontendState state;
  ShaderProgramDatabase shaderProgramDatabase;
#if _TARGET_PC_WIN
  debug::GlobalState debugState;
  bool windowOcclusionCheckEnabled;
#endif
  bool initVideoDone = false;
  bool isHDREnabled = false;

  float minLum = 0.f;
  float maxLum = 0.f;
  float maxFullFrameLum = 0.f;

  void adjustCaps()
  {
    driverDesc.zcmpfunc = 0;
    driverDesc.acmpfunc = 0;
    driverDesc.sblend = 0;
    driverDesc.dblend = 0;
    driverDesc.mintexw = 1;
    driverDesc.mintexh = 1;
    driverDesc.maxtexw = 0x7FFFFFFF;
    driverDesc.maxtexh = 0x7FFFFFFF;
    driverDesc.mincubesize = 1;
    driverDesc.maxcubesize = 0x7FFFFFFF;
    driverDesc.minvolsize = 1;
    driverDesc.maxvolsize = 0x7FFFFFFF;
    driverDesc.maxtexaspect = 0;
    driverDesc.maxtexcoord = 0x7FFFFFFF;
    driverDesc.maxsimtex = 0x7FFFFFFF;
    driverDesc.maxvertexsamplers = 0x7FFFFFFF;
    driverDesc.maxclipplanes = 0x7FFFFFFF;
    driverDesc.maxstreams = 0x7FFFFFFF;
    driverDesc.maxstreamstr = 0x7FFFFFFF;
    driverDesc.maxvpconsts = 0x7FFFFFFF;
    driverDesc.maxprims = 0x7FFFFFFF;
    driverDesc.maxvertind = 0x7FFFFFFF;
    driverDesc.upixofs = 0.f;
    driverDesc.vpixofs = 0.f;
#if _TARGET_PC_WIN
    driverDesc.shaderModel = 6.6_sm;
#endif
    driverDesc.maxSimRT = 0x7FFFFFFF;
    driverDesc.is20ArbitrarySwizzleAvailable = true;

    device.adjustCaps(driverDesc);
  }

  ApiState()
  {
    isInitialized = false;
    lastErrorCode = S_OK;
    deviceWasLost = false;
#if _TARGET_PC_WIN
    windowOcclusionCheckEnabled = true;
#endif
  }

  void releaseAll()
  {
    auto &ctx = device.getContext();
    shaderProgramDatabase.shutdown(ctx);
    ctx.finish();

    device.shutdown(DeviceCapsAndShaderModel::fromDriverDesc(driverDesc));

    deviceName.clear();

    windowState.closeWindow();
#if _TARGET_PC_WIN
    debugState.teardown();
    dxgi14.Reset();
    d3d12Env.teardown();
#endif
    isHDREnabled = false;
    isInitialized = false;
  }
};

extern ApiState api_state;
} // namespace drv3d_dx12

drv3d_dx12::ApiState drv3d_dx12::api_state;

bool check_is_main_thread() { return drv3d_dx12::api_state.globalLock.validateOwnership(); }

nau::threading::SpinLock& drv3d_dx12::get_resource_binding_guard() { return drv3d_dx12::api_state.state.resourceBindingGuard; }

#if _TARGET_PC_WIN
DAGOR_NOINLINE static void toggle_fullscreen(HWND hWnd, UINT message, WPARAM wParam)
{
  if (!drv3d_dx12::api_state.device.isInitialized() || dgs_get_window_mode() != WindowMode::FULLSCREEN_EXCLUSIVE)
    return;

  STORE_RETURN_ADDRESS();

  bool hasFocus = has_focus(hWnd, message, wParam);
  drv3d_dx12::api_state.device.getContext().changeFullscreenExclusiveMode(hasFocus);
  if (!hasFocus)
    ShowWindow(hWnd, SW_MINIMIZE);
}

LRESULT CALLBACK drv3d_dx12::WindowState::windowProcProxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paint_window(hWnd, message, wParam, lParam, mainCallback); return 1;
    case WM_ACTIVATE:
    case WM_ACTIVATEAPP:
    {
      toggle_fullscreen(hWnd, message, wParam);
      break;
    }
  }

  if (originWndProc)
    return CallWindowProcW(originWndProc, hWnd, message, wParam, lParam);
  if (mainCallback)
    return mainCallback(hWnd, message, wParam, lParam);
  return DefWindowProcW(hWnd, message, wParam, lParam);
}
#elif _TARGET_XBOX

LRESULT CALLBACK drv3d_dx12::WindowState::windowProcProxy(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (mainCallback)
    return mainCallback(hWnd, message, wParam, lParam);
  return DefWindowProcW(hWnd, message, wParam, lParam);
}

#endif

void d3d::get_texture_statistics(uint32_t *num_textures, uint64_t *total_mem, nau::string *out_text)
{
  drv3d_dx12::api_state.device.generateResourceAndMemoryReport(num_textures, total_mem, out_text);
}

Device &drv3d_dx12::get_device() { return drv3d_dx12::api_state.device; }

void drv3d_dx12::report_oom_info() { drv3d_dx12::api_state.device.reportOOMInformation(); }
void drv3d_dx12::set_last_error(HRESULT error) { drv3d_dx12::api_state.lastErrorCode = error; }

HRESULT drv3d_dx12::get_last_error_code() { return drv3d_dx12::api_state.lastErrorCode; }

bool d3d::is_inited() { return drv3d_dx12::api_state.isInitialized && drv3d_dx12::api_state.initVideoDone; }

bool d3d::init_driver()
{
  if (d3d::is_inited())
  {
    NAU_LOG_ERROR("Driver is already created");
    return false;
  }
  drv3d_dx12::api_state.isInitialized = true;
  return true;
}

void d3d::release_driver()
{
  STORE_RETURN_ADDRESS();
  TEXQL_SHUTDOWN_TEX();
  tql::termTexStubs();
  drv3d_dx12::api_state.releaseAll();
  drv3d_dx12::api_state.isInitialized = false;
}

static bool create_output_window(void *hinst, main_wnd_f *wnd_proc, const char *wcname, int ncmdshow, void *&mainwnd, void *renderwnd,
  void *hicon, const char *title, Driver3dInitCallback *cb)
{
  drv3d_dx12::api_state.windowState.set(hinst, wcname, ncmdshow, mainwnd, renderwnd, hicon, title, *(void **)&wnd_proc);
  drv3d_dx12::api_state.windowState.getRenderWindowSettings(cb);

  if (!drv3d_dx12::api_state.windowState.setRenderWindowParams())
    return false;
  mainwnd = drv3d_dx12::api_state.windowState.getMainWindow();
  return true;
}

void drv3d_dx12::hdr_changed(bool is_hdr_enabled, float min_lum, float max_lum, float max_fullframe_lum)
{
  if (is_hdr_enabled)
  {
    NAU_LOG_DEBUG("DX12: HDR is {}: min lum: {}, max lum: {}, max FullFrame Lum {}", drv3d_dx12::api_state.isHDREnabled ? "changed" : "enabled", min_lum,
      max_lum, max_fullframe_lum);

    drv3d_dx12::api_state.minLum = min_lum;
    drv3d_dx12::api_state.maxLum = max_lum;
    drv3d_dx12::api_state.maxFullFrameLum = max_fullframe_lum;
  }
  else
  {
    NAU_LOG_DEBUG("DX12: HDR is disabled");
  }

  drv3d_dx12::api_state.isHDREnabled = is_hdr_enabled;
}

static void set_sci_hdr_config(SwapchainCreateInfo &sci)
{
#if _TARGET_PC_WIN
  sci.enableHdr = get_enable_hdr_from_settings();
  NAU_LOG_DEBUG("DX12: HDR is {} from config", sci.enableHdr ? "enabled" : "disabled");

  const nau::DataBlock *dxCfg = ::dgs_get_settings()->getBlockByNameEx("dx12");
  sci.forceHdr = dxCfg->getBool("forceHdr", false);
  if (sci.forceHdr)
    NAU_LOG_DEBUG("DX12: HDR will be forced due to config");
#elif _TARGET_XBOX
  if (is_hdr_available())
  {
    sci.enableHdr = get_enable_hdr_from_settings();
    NAU_LOG_DEBUG("DX12: HDR is {} from config", sci.enableHdr ? "enabled" : "disabled");
    const nau::DataBlock *dxCfg = ::dgs_get_settings()->getBlockByNameEx("dx12");
    sci.autoGameDvr = dxCfg->getBool("autoGameDvr", true);
    NAU_LOG_DEBUG("DX12: GameDvr output will be create by {}", sci.autoGameDvr ? "system" : "engine");
  }
  else
    NAU_LOG_DEBUG("DX12: HDR is disabled due to inappropriate hardware");
#else
  NAU_LOG_DEBUG("DX12: HDR is disabled due to inappropriate hardware");
#endif
}

#if _TARGET_XBOX
static bool is_auto_gamedvr() { return ::dgs_get_settings()->getBlockByNameEx("dx12")->getBool("autoGameDvr", true); }
#endif

#if _TARGET_PC_WIN
namespace
{
void setup_futex()
{
  auto futexLib = LoadLibraryA("API-MS-Win-Core-Synch-l1-2-0.dll");
  if (futexLib)
  {
    NAU_LOG_DEBUG("DX12 Memory wait uses WaitOnAddress");
    reinterpret_cast<FARPROC &>(drv3d_dx12::WaitOnAddress) = GetProcAddress(futexLib, "WaitOnAddress");
    reinterpret_cast<FARPROC &>(drv3d_dx12::WakeByAddressAll) = GetProcAddress(futexLib, "WakeByAddressAll");
    reinterpret_cast<FARPROC &>(drv3d_dx12::WakeByAddressSingle) = GetProcAddress(futexLib, "WakeByAddressSingle");
  }
  else
  {
    NAU_LOG_DEBUG("DX12 Memory wait uses polling");
  }
}

APISupport check_driver_version(const DXGI_ADAPTER_DESC1 &adapter_info, const DriverVersion &version, const nau::DataBlock &gpu_cfg,
  DriverVersion *out_min_version = nullptr)
{
  for (int i = 0; i < gpu_cfg.blockCount(); i++)
  {
    const nau::DataBlock &vendor = *gpu_cfg.getBlock(i);
    if (vendor.getInt("vendorId", 0) != adapter_info.VendorId)
      continue;

    DriverVersion minVersion{};
    sscanf(vendor.getStr("minDriver", "0.0.0.0"), " %hu . %hu . %hu . %hu", &minVersion.productVersion, &minVersion.majorVersion,
      &minVersion.minorVersion, &minVersion.buildNumber);

    if (version < minVersion)
    {
      if (out_min_version)
        *out_min_version = minVersion;
      return APISupport::OUTDATED_DRIVER;
    }

    bool result = false;
    nau::dblk::iterate_params_by_name(vendor, "blacklistedDrivers", [&](int param_idx, auto, auto) {
      DriverVersion blacklist{};
      sscanf(vendor.getStr(param_idx), " %hu . %hu . %hu . %hu", &blacklist.productVersion, &blacklist.majorVersion,
        &blacklist.minorVersion, &blacklist.buildNumber);

      result |= version == blacklist;
    });

    if (result)
      return APISupport::BLACKLISTED_DRIVER;
  }
  return APISupport::FULL_SUPPORT;
}

bool is_prefered_device(const nau::DataBlock &gpu_cfg, UINT vendor_id, UINT device_id)
{
  for (int i = 0; i < gpu_cfg.blockCount(); i++)
  {
    const nau::DataBlock &vendor = *gpu_cfg.getBlock(i);
    if (vendor.getInt("vendorId", 0) != vendor_id)
      continue;

    bool result = false;
    nau::dblk::iterate_params_by_name(vendor, "preferedDeviceIds",
      [&](int param_idx, auto, auto) { result |= vendor.getInt(param_idx) == device_id; });
    return result;
  }
  return false;
}

APISupport check_adapter(Direct3D12Enviroment &d3d12_env, D3D_FEATURE_LEVEL feature_level, const nau::DataBlock *gpu_cfg,
  bool use_any_device, ComPtr<IDXGIAdapter1> &adapter)
{
  DXGI_ADAPTER_DESC1 info;
  adapter->GetDesc1(&info);

  char strBuffer[sizeof(DXGI_ADAPTER_DESC1::Description) * 2 + 1];
  const size_t size = wcstombs(strBuffer, info.Description, sizeof(strBuffer));
  strBuffer[size] = '\0';
  NAU_LOG_DEBUG("DX12: Found device {} - {:#08x} - {:#08x} with flags {:#08x}", strBuffer, info.VendorId, info.DeviceId, info.Flags);

  // only accept non software devices we find
  if (is_software_device(info))
  {
    NAU_LOG_DEBUG("DX12: Rejected, because software device");
    return APISupport::NO_DEVICE_FOUND;
  }

  auto version = get_driver_version_from_registry(info.AdapterLuid);
  NAU_LOG_DEBUG("DX12: Driver version {}.{}.{}.{}", version.productVersion, version.majorVersion, version.minorVersion, version.buildNumber);
  if (gpu::VENDOR_ID_NVIDIA == info.VendorId)
  {
    // on NV we can deduce GeForce version and report more details.
    auto nvVersion = DriverVersionNVIDIA::fromDriverVersion(version);
    NAU_LOG_DEBUG("DX12: NVIDIA GeForce version {}.{:02u}", nvVersion.majorVersion, nvVersion.minorVersion);
  }

  if (gpu_cfg)
  {
    if (!use_any_device && !is_prefered_device(*gpu_cfg, info.VendorId, info.DeviceId))
    {
      NAU_LOG_DEBUG("DX12: Rejected, because the driver mode is \"auto\" and the device isn't a prefered one");
      return APISupport::NO_DEVICE_FOUND;
    }

    DriverVersion minVersion{};
    auto result = check_driver_version(info, version, *gpu_cfg, &minVersion);
    switch (result)
    {
      case APISupport::OUTDATED_DRIVER:
        NAU_LOG_DEBUG("DX12: Rejected, driver version is older than minVersion "
               "{}.{}.{}.{}",
          minVersion.productVersion, minVersion.majorVersion, minVersion.minorVersion, minVersion.buildNumber);
        return result;
      case APISupport::BLACKLISTED_DRIVER: NAU_LOG_DEBUG("DX12: Rejected, driver version is blacklisted"); return result;
      default: break;
    }
  }

  ComPtr<ID3D12Device> device;
  if (auto hr = d3d12_env.D3D12CreateDevice(adapter.Get(), feature_level, COM_ARGS(&device)); FAILED(hr))
  {
    NAU_LOG_DEBUG("DX12: Rejected, unable to create DX12 device, {}", dxgi_error_code_to_string(hr));
    report_agility_sdk_error(hr);
    return APISupport::NO_DEVICE_FOUND;
  }

  // devices below feature level 12.0 are not required to support 6.0+/DXIL shaders, but we only
  // ship those so we can only use devices with support for that.
  D3D12_FEATURE_DATA_SHADER_MODEL sm = {D3D_SHADER_MODEL_6_0};
  device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm));
  if (sm.HighestShaderModel < D3D_SHADER_MODEL_6_0)
  {
    NAU_LOG_DEBUG("DX12: Rejected, no HLSL shader model 6.0+ support (DXIL)");
    return APISupport::NO_DEVICE_FOUND;
  }

  NAU_LOG_DEBUG("DX12: Device fulfills requirements, DX12 is available!");
  return APISupport::FULL_SUPPORT;
}

void check_and_add_adapter(Direct3D12Enviroment &d3d12_env, D3D_FEATURE_LEVEL feature_level, const nau::DataBlock *gpu_driver_cfg,
  ComPtr<IDXGIAdapter1> &adapter, eastl::vector<Device::AdapterInfo> &adapter_list)
{
  Device::AdapterInfo info{};
  adapter->GetDesc1(&info.info);

  char strBuffer[sizeof(DXGI_ADAPTER_DESC1::Description) * 2 + 1];
  const size_t size = wcstombs(strBuffer, info.info.Description, sizeof(strBuffer));
  strBuffer[size] = '\0';
  NAU_LOG_DEBUG("DX12: Found device {} - {:#08x} - {:#08x} with flags {:#08x}", strBuffer, info.info.VendorId, info.info.DeviceId,
    info.info.Flags);

  // only accept non software devices we find
  if (is_software_device(info.info))
  {
    NAU_LOG_DEBUG("DX12: Rejected, because software device");
    return;
  }

  if (gpu_driver_cfg)
  {
    auto version = get_driver_version_from_registry(info.info.AdapterLuid);
    auto result = check_driver_version(info.info, version, *gpu_driver_cfg);
    switch (result)
    {
      case APISupport::OUTDATED_DRIVER:
        NAU_LOG_DEBUG("DX12: Rejected, because inadequate gpu driver, the {}.{}.{}.{} is outdated", version.productVersion,
          version.majorVersion, version.minorVersion, version.buildNumber);
        return;
      case APISupport::BLACKLISTED_DRIVER:
        NAU_LOG_DEBUG("DX12: Rejected, because inadequate gpu driver, the {}.{}.{}.{} is blacklisted", version.productVersion,
          version.majorVersion, version.minorVersion, version.buildNumber);
        return;
      default: break;
    }
  }

  // checks but does not create a device yet
  if (auto hr = d3d12_env.D3D12CreateDevice(adapter.Get(), feature_level, __uuidof(ID3D12Device), nullptr); FAILED(hr))
  {
    NAU_LOG_DEBUG("DX12: Rejected, because it failed DX12 support test, {}", dxgi_error_code_to_string(hr));
    report_agility_sdk_error(hr);
    return;
  }

  info.adapter = eastl::move(adapter);
  adapter_list.push_back(eastl::move(info));
}

// sort from dedicated to integrated by vram memory size
void sort_adapters_by_perf(eastl::vector<Device::AdapterInfo> &adapter_list)
{
  eastl::sort(begin(adapter_list), end(adapter_list), [](const Device::AdapterInfo &l, const Device::AdapterInfo &r) {
    return l.info.DedicatedVideoMemory > r.info.DedicatedVideoMemory;
  });
}

// sort from integrated to dedicated by testing for UMA
void sort_adapters_by_integrated(eastl::vector<Device::AdapterInfo> &adapter_list, D3D_FEATURE_LEVEL feature_level)
{
  for (Device::AdapterInfo &adapter : adapter_list)
  {
    VersionedComPtr<D3DDevice> device;
    if (device.autoQuery([&adapter, feature_level](auto uuid, auto ptr) {
          return SUCCEEDED(drv3d_dx12::api_state.d3d12Env.D3D12CreateDevice(adapter.adapter.Get(), feature_level, uuid, ptr));
        }))
    {
      D3D12_FEATURE_DATA_ARCHITECTURE data = {};
      if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &data, sizeof(data))))
        adapter.integrated = data.UMA;
    }
  }

  eastl::sort(begin(adapter_list), end(adapter_list), [](const Device::AdapterInfo &l, const Device::AdapterInfo &r) {
    if (l.integrated != r.integrated)
      return l.integrated;
    return l.info.DedicatedVideoMemory > r.info.DedicatedVideoMemory;
  });
}
} // namespace

using UpdateGpuDriverConfigFunc = void (*)(GpuDriverConfig &);
extern UpdateGpuDriverConfigFunc update_gpu_driver_config;

void update_dx12_gpu_driver_config(GpuDriverConfig &gpu_driver_config)
{
  auto info = drv3d_dx12::api_state.device.getAdapterInfo();

  gpu_driver_config.primaryVendor = d3d_get_vendor(info.info.VendorId);

  gpu_driver_config.deviceId = info.info.DeviceId;
  gpu_driver_config.integrated = info.integrated;

  auto version = get_driver_version_from_registry(info.info.AdapterLuid);

  gpu_driver_config.driverVersion[0] = version.productVersion;
  gpu_driver_config.driverVersion[1] = version.majorVersion;
  gpu_driver_config.driverVersion[2] = version.minorVersion;
  gpu_driver_config.driverVersion[3] = version.buildNumber;
}

bool d3d::init_video(void *hinst, main_wnd_f *wnd_proc, const char *wcname, int ncmdshow, void *&mainwnd, void *renderwnd, void *hicon,
  const char *title, Driver3dInitCallback *cb)
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.initVideoDone = false;

  const nau::DataBlock *videoCfg = ::dgs_get_settings()->getBlockByNameEx("video");
  const nau::DataBlock *directxCfg = ::dgs_get_settings()->getBlockByNameEx("directx");
  const nau::DataBlock *dxCfg = ::dgs_get_settings()->getBlockByNameEx("dx12");

  setup_futex();

  drv3d_dx12::api_state.windowOcclusionCheckEnabled = directxCfg->getBool("winOcclusionCheckEnabled", true);

  if (!drv3d_dx12::api_state.d3d12Env.setup())
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    return false;
  }

  stereo_config_callback = cb;

  drv3d_dx12::api_state.debugState.setup(dxCfg, drv3d_dx12::api_state.d3d12Env);

  auto deviceCfg = get_device_config(dxCfg);

  if (drv3d_dx12::api_state.debugState.captureTool().isAnyPIXActive())
  {
    NAU_LOG_DEBUG("DX12: ...PIX frame capturing is active, disabling pipeline library cache to avoid replay errors...");
    // pipeline library cache causes trouble with pix, it tries to replay but it
    // fails with driver version mismatch and then results in abort of replay
    // and pix reports TDR as an error.
    deviceCfg.features.set(DeviceFeaturesConfig::DISABLE_PIPELINE_LIBRARY_CACHE);
  }

#if DX12_DOES_SET_DEBUG_NAMES
  if (drv3d_dx12::api_state.debugState.captureTool().isAnyActive())
  {
    NAU_LOG_DEBUG("DX12: ...frame capturing tool active, enabling naming of API objects...");
    deviceCfg.features.set(DeviceFeaturesConfig::NAME_OBJECTS);
  }
  else if (dxCfg->getBool("nameObjects", false))
  {
    NAU_LOG_DEBUG("DX12: ...naming of API objects enabled by config value...");
    deviceCfg.features.set(DeviceFeaturesConfig::NAME_OBJECTS);
  }
#endif

  NAU_LOG_DEBUG("DX12: CreateDXGIFactory2 for DXGIFactory4...");
  // on cpu validation we also turn on DXGI validation
  UINT flags = drv3d_dx12::api_state.debugState.configuration().enableCPUValidation ? DXGI_CREATE_FACTORY_DEBUG : 0;
  auto hr = drv3d_dx12::api_state.d3d12Env.CreateDXGIFactory2(flags, COM_ARGS(&drv3d_dx12::api_state.dxgi14));
  if (FAILED(hr))
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    NAU_LOG_DEBUG("DX12: Failed, {}", dxgi_error_code_to_string(hr));
    drv3d_dx12::api_state.releaseAll();
    return false;
  }

  if (!create_output_window(hinst, wnd_proc, wcname, ncmdshow, mainwnd, renderwnd, hicon, title, cb))
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    NAU_LOG_DEBUG("DX12: Failed to create output window");
    drv3d_dx12::api_state.releaseAll();
    return false;
  }

  auto windowHandle = reinterpret_cast<HWND>(drv3d_dx12::api_state.windowState.getMainWindow());

  SwapchainCreateInfo sci;
  sci.window = windowHandle;
  sci.presentMode = get_presentation_mode_from_settings();
  sci.windowed = dgs_get_window_mode() != WindowMode::FULLSCREEN_EXCLUSIVE;
  sci.resolutionX = drv3d_dx12::api_state.windowState.settings.clientWidth;
  sci.resolutionY = drv3d_dx12::api_state.windowState.settings.clientHeight;
  set_sci_hdr_config(sci);

  auto featureLevel = make_feature_level(dxCfg->getInt("FeatureLevelMajor", min_major_feature_level),
    dxCfg->getInt("FeatureLevelMinor", min_minor_feature_level));

  auto init_device = [&](ComPtr<IDXGIAdapter1> adapter1, ComPtr<IDXGIOutput> output) {
    DXGI_ADAPTER_DESC1 info = {};
    adapter1->GetDesc1(&info);
    sci.output = eastl::move(output);
    if (drv3d_dx12::api_state.device.init(drv3d_dx12::api_state.dxgi14.Get(), {eastl::move(adapter1), info}, featureLevel, drv3d_dx12::api_state.d3d12Env,
          eastl::move(sci), drv3d_dx12::api_state.debugState, deviceCfg, dxCfg, cb ? cb->desiredStereoRender() : false))
    {
      char strBuffer[sizeof(DXGI_ADAPTER_DESC1::Description) * 2 + 1];
      const size_t size = wcstombs(strBuffer, info.Description, sizeof(strBuffer));
      strBuffer[size] = '\0';
      drv3d_dx12::api_state.deviceName = strBuffer;
      return true;
    }
    return false;
  };

  ComPtr<IDXGIAdapter1> adapter1;
  if (dxCfg->getBool("UseWARP", false))
  {
    NAU_LOG_DEBUG("DX12: WARP requested, DXGIFactory4::EnumWarpAdapter...");
    if (SUCCEEDED(drv3d_dx12::api_state.dxgi14->EnumWarpAdapter(COM_ARGS(&adapter1))))
      init_device(eastl::move(adapter1), nullptr);
  }

  if (!drv3d_dx12::api_state.device.isInitialized())
  {
    // use the adapter selected by its luid
    auto luidValue = cb->desiredAdapter() ? cb->desiredAdapter() : dxCfg->getInt64("AdapterLUID", 0);
    if (luidValue != 0)
    {
      LUID luid;
      luid.LowPart = static_cast<ULONG>(luidValue);
      luid.HighPart = static_cast<LONG>(luidValue >> 32);

      NAU_LOG_DEBUG("DX12: DXGIFactory4::EnumAdapterByLuid({})...", luidValue);
      if (SUCCEEDED(drv3d_dx12::api_state.dxgi14->EnumAdapterByLuid(luid, COM_ARGS(&adapter1))))
        init_device(eastl::move(adapter1), nullptr);
    }
  }

  if (!drv3d_dx12::api_state.device.isInitialized())
  {
    const char *displayName = get_monitor_name_from_settings();
    if (displayName)
    {
      NAU_LOG_DEBUG("DX12: DXGIFactory4::EnumAdapters : 'displayName'={}...", displayName);

      for (uint32_t adapterIndex = 0; SUCCEEDED(drv3d_dx12::api_state.dxgi14->EnumAdapters1(adapterIndex, &adapter1)); adapterIndex++)
      {
        ComPtr<IDXGIOutput> output = get_output_monitor_by_name(adapter1.Get(), displayName);
        if (output.Get() && init_device(eastl::move(adapter1), eastl::move(output)))
          break;
      }
    }
  }

  if (!drv3d_dx12::api_state.device.isInitialized())
  {
    const nau::DataBlock *gpuCfg = dxCfg->getBlockByName("gpuPreferences");
    eastl::vector<Device::AdapterInfo> adapterList;
    ComPtr<IDXGIFactory6> dxgi6;
    //if (SUCCEEDED(drv3d_dx12::api_state.dxgi14.As(&dxgi6)))
    //{
    //  DXGI_GPU_PREFERENCE gpuPreference;
    //  if (videoCfg->getBool("preferiGPU", false))
    //  {
    //    NAU_LOG_DEBUG("DX12: Enumerating available devices to prefer iGPU...");
    //    gpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER;
    //  }
    //  else
    //  {
    //    NAU_LOG_DEBUG("DX12: Enumerating available devices in performance order...");
    //    gpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
    //  }
    //
    //  for (UINT index = 0; dxgi6->EnumAdapterByGpuPreference(index, gpuPreference, COM_ARGS(&adapter1)) != DXGI_ERROR_NOT_FOUND;
    //       ++index)
    //  {
    //      if(adapter1)
    //        check_and_add_adapter(drv3d_dx12::api_state.d3d12Env, featureLevel, gpuCfg, adapter1, adapterList);
    //  }
    //}
    //else
    {
      NAU_LOG_DEBUG("DX12: Enumerating available devices...");
      for (UINT index = 0; drv3d_dx12::api_state.dxgi14->EnumAdapters1(index, &adapter1) != DXGI_ERROR_NOT_FOUND; ++index)
      {
        check_and_add_adapter(drv3d_dx12::api_state.d3d12Env, featureLevel, gpuCfg, adapter1, adapterList);
      }

      // sort from best to worse
      if (videoCfg->getBool("preferiGPU", false))
        sort_adapters_by_integrated(adapterList, featureLevel);
      else
        sort_adapters_by_perf(adapterList);
    }
    NAU_LOG_DEBUG("DX12: Found {} candidates", adapterList.size());

    for (auto &&adapter : adapterList)
    {
      char strBuffer[sizeof(DXGI_ADAPTER_DESC1::Description) * 2 + 1];
      const size_t size = wcstombs(strBuffer, adapter.info.Description, sizeof(strBuffer));
      strBuffer[size] = '\0';
      sci.output = get_default_monitor(adapter.adapter.Get());
      NAU_LOG_DEBUG("DX12: Trying with device {}", strBuffer);
      if (drv3d_dx12::api_state.device.init(drv3d_dx12::api_state.dxgi14.Get(), eastl::move(adapter), featureLevel, drv3d_dx12::api_state.d3d12Env, eastl::move(sci),
            drv3d_dx12::api_state.debugState, deviceCfg, dxCfg, cb ? cb->desiredStereoRender() : false))
      {
        drv3d_dx12::api_state.deviceName = strBuffer;
        break;
      }
    }
  }

  // TODO at this point try a software device again (needed to handle forced WARP mode correctly)
  if (!drv3d_dx12::api_state.device.isInitialized())
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    NAU_LOG_DEBUG("DX12: Failed to initialize, no suitable device found...");
    drv3d_dx12::api_state.releaseAll();
    return false;
  }

  if (videoCfg->getBool("preferiGPU", false))
  {
    D3D12_FEATURE_DATA_ARCHITECTURE data = {};
    if (SUCCEEDED(drv3d_dx12::api_state.device.getDevice()->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &data, sizeof(data))) && !data.UMA)
      NAU_LOG_WARNING("DX12: Despite the preferiGPU flag being enabled, the dedicated GPU is used!");
  }

  // TODO
  // update luid to use the same adapter next time
  // const_cast<DataBlock *>(dxCfg)->setInt64("AdpaterLUID", drv3d_dx12::api_state.device.getAdapterLuid());

  drv3d_dx12::api_state.adjustCaps();

  drv3d_dx12::api_state.shaderProgramDatabase.setup(drv3d_dx12::api_state.device.getContext(), dxCfg->getBool("disablePreCache", false)); //

  update_gpu_driver_config = update_dx12_gpu_driver_config;

  drv3d_dx12::api_state.initVideoDone = true;

  tql::initTexStubs();

  NAU_LOG_DEBUG("DX12: init_video done");
  return true;
}
#else
bool d3d::init_video(void *hinst, main_wnd_f *wnd_proc, const char *wcname, int ncmdshow, void *&mainwnd, void *renderwnd, void *hicon,
  const char *title, Driver3dInitCallback *)
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.initVideoDone = false;
  const nau::DataBlock *videoCfg = ::dgs_get_settings()->getBlockByNameEx("video");
  const nau::DataBlock *dxCfg = ::dgs_get_settings()->getBlockByNameEx("dx12");

  if (!create_output_window(hinst, wnd_proc, wcname, ncmdshow, mainwnd, renderwnd, hicon, title, nullptr))
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    NAU_LOG_DEBUG("DX12: Failed to create output window");
    drv3d_dx12::api_state.releaseAll();
    return false;
  }

  auto windowHandle = reinterpret_cast<HWND>(drv3d_dx12::api_state.windowState.getMainWindow());

  SwapchainCreateInfo sci;
  sci.window = windowHandle;
  sci.presentMode = get_presentation_mode_from_settings();
  sci.windowed = dgs_get_window_mode() != WindowMode::FULLSCREEN_EXCLUSIVE;
  sci.resolutionX = drv3d_dx12::api_state.windowState.settings.resolutionX;
  sci.resolutionY = drv3d_dx12::api_state.windowState.settings.resolutionY;

  set_sci_hdr_config(sci);

#if DAGOR_DBGLEVEL > 0
  constexpr float default_immediate_threshold_percent = 100.f;
#else
  constexpr float default_immediate_threshold_percent = 30.f;
#endif
  sci.frameImmediateThresholdPercent = dxCfg->getReal("frameImmediateThresholdPercent", default_immediate_threshold_percent);

  int freqLevel = videoCfg->getInt("freqLevel", 1);
  sci.freqLevel = freqLevel > -1 ? freqLevel : 1;

  auto deviceCfg = get_device_config(dxCfg);
#if DX12_DOES_SET_DEBUG_NAMES
  deviceCfg.features.set(DeviceFeaturesConfig::NAME_OBJECTS);
#endif

  drv3d_dx12::api_state.device.init(sci, deviceCfg);

  if (!drv3d_dx12::api_state.device.isInitialized())
  {
    drv3d_dx12::api_state.lastErrorCode = E_FAIL;
    NAU_LOG_DEBUG("DX12: Failed to initialize, no suitable device found...");
    drv3d_dx12::api_state.releaseAll();
    return false;
  }

  drv3d_dx12::api_state.adjustCaps();

  tql::initTexStubs();

  drv3d_dx12::api_state.shaderProgramDatabase.setup(drv3d_dx12::api_state.device.getContext(), dxCfg->getBool("disablePreCache", false));

  NAU_LOG_DEBUG("DX12: init_video done");
  drv3d_dx12::api_state.initVideoDone = true;
  return true;
}
#endif

void d3d::prepare_for_destroy() {}

void d3d::window_destroyed(void *handle)
{
  STORE_RETURN_ADDRESS();
  G_UNUSED(handle);
#if _TARGET_PC_WIN
  if (drv3d_dx12::api_state.windowState.params.hwnd == handle)
  {
    // may be called after shutdown of device, so make sure we don't crash
    if (drv3d_dx12::api_state.device.isInitialized())
    {
      // need to flush and wait or we might crash
      drv3d_dx12::api_state.device.getContext().shutdownSwapchain();
    }
    drv3d_dx12::api_state.windowState.params.hwnd = nullptr;
  }
#endif
}

void d3d::reserve_res_entries(bool strict_max, int max_tex, int max_vs, int max_ps, int max_vdecl, int max_vb, int max_ib,
  int max_stblk)
{
  drv3d_dx12::api_state.device.reserveTextureObjects(max_tex);
  drv3d_dx12::api_state.device.reserveBufferObjects(max_vb + max_ib);
  G_UNUSED(strict_max);
  G_UNUSED(max_tex);
  G_UNUSED(max_vs);
  G_UNUSED(max_ps);
  G_UNUSED(max_vdecl);
  G_UNUSED(max_stblk);
}

void d3d::get_max_used_res_entries(int &max_tex, int &max_vs, int &max_ps, int &max_vdecl, int &max_vb, int &max_ib, int &max_stblk)
{
  max_tex = drv3d_dx12::api_state.device.getTextureObjectCapacity();

  max_vb = max_ib = 0;
  drv3d_dx12::api_state.device.visitBufferObjects([&](auto buffer) {
    int flags = buffer->getFlags();
    if ((flags & SBCF_BIND_MASK) == SBCF_BIND_INDEX)
      ++max_ib;
    else
      ++max_vb;
  });

  auto total = max_vb + max_ib;
  auto cap = drv3d_dx12::api_state.device.getBufferObjectCapacity();
  max_vb *= cap;
  max_ib *= cap;
  max_vb /= total;
  max_ib /= total;

  G_UNUSED(max_vs);
  G_UNUSED(max_ps);
  G_UNUSED(max_vdecl);
  G_UNUSED(max_stblk);
}

void d3d::get_cur_used_res_entries(int &max_tex, int &max_vs, int &max_ps, int &max_vdecl, int &max_vb, int &max_ib, int &max_stblk)
{
  max_tex = drv3d_dx12::api_state.device.getActiveTextureObjectCount();

  max_vb = max_ib = 0;
  drv3d_dx12::api_state.device.visitBufferObjects([&](auto buffer) {
    int flags = buffer->getFlags();
    if ((flags & SBCF_BIND_MASK) == SBCF_BIND_INDEX)
      ++max_ib;
    else
      ++max_vb;
  });

  G_UNUSED(max_vs);
  G_UNUSED(max_ps);
  G_UNUSED(max_vdecl);
  G_UNUSED(max_stblk);
}

const char *d3d::get_driver_name() { return "DirectX 12"; }

DriverCode d3d::get_driver_code() { return DriverCode::make(d3d::dx12); }

const char *d3d::get_device_name() { return drv3d_dx12::api_state.deviceName.c_str(); }
const char *d3d::get_last_error() { return drv3d_dx12::dxgi_error_code_to_string(drv3d_dx12::api_state.lastErrorCode); }

uint32_t d3d::get_last_error_code() { return drv3d_dx12::api_state.lastErrorCode; }

const char *d3d::get_device_driver_version() { return "1.0"; }

void *d3d::get_device() { return drv3d_dx12::api_state.device.getDevice(); }

const Driver3dDesc &d3d::get_driver_desc() { return drv3d_dx12::api_state.driverDesc; }

namespace
{
void enable_tracking_on_resource(D3dResource *resource)
{
  switch (resource->restype())
  {
    case RES3D_TEX:
    case RES3D_CUBETEX:
    case RES3D_VOLTEX:
    case RES3D_ARRTEX:
    case RES3D_CUBEARRTEX: static_cast<BaseTex *>(resource)->getDeviceImage()->setReportStateTransitions(); break;
    case RES3D_SBUF:
      static_cast<GenericBufferInterface *>(resource)->updateDeviceBuffer(
        [](auto &buf) { buf.resourceId.setReportStateTransitions(); });
      break;
    default:
      NAU_FAILURE("DX12: Invalid type to enable resource state transition tracking");// {:p} - {}", resource, resource->restype());
      break;
  }
}
} // namespace

#if _TARGET_PC_WIN
static Vectormath::IVector2 handleAutoResolution(const Vectormath::IVector2 &target)
{
  Vectormath::IVector2 result = target;
  if (result.getX() <= 0 || result.getY() <= 0)
  {
    // We are expected to get resolution for "Auto" in this case
    // FIXME: This is getting the "Auto" resolution for the current video mode (fullscreen or windowed) saved in the settings and not
    //        for the one currently selected on the UI. "Auto" resolutions should be really close in these cases, so let's hope it
    //        doesn't cause problems. It's unlikely that availability of a DLSS quality mode will be dependent on resolution anyway.
    int base_scr_left, base_scr_top;
    int outWidth, outHeight;
    get_current_display_screen_mode(base_scr_left, base_scr_top, outWidth, outHeight);
    result.setX(outWidth);
    result.setY(outHeight);
  }
  return result;
}
#endif

namespace
{
int on_driver_command_compile_pipeline_set(void *par1)
{
  if (!par1)
  {
    return 1;
  }
  auto sets = static_cast<const CompilePipelineSet *>(par1);
  const char *defaultFormat = sets->defaultFormat ? sets->defaultFormat : "dx12";
  DynamicArray<InputLayoutID> inputLayouts;
  if (sets->inputLayoutSet)
  {
    inputLayouts =
      drv3d_dx12::api_state.shaderProgramDatabase.loadInputLayoutFromBlk(drv3d_dx12::api_state.device.getContext(), sets->inputLayoutSet, defaultFormat);
  }
  DynamicArray<StaticRenderStateID> renderStates;
  if (sets->renderStateSet)
  {
    renderStates = drv3d_dx12::api_state.device.getRenderStateSystem().loadStaticStatesFromBlk(drv3d_dx12::api_state.device.getContext(), drv3d_dx12::api_state.driverDesc,
      sets->renderStateSet, defaultFormat);
  }
  drv3d_dx12::api_state.device.getContext().compilePipelineSet(sets->featureSet, eastl::move(inputLayouts), eastl::move(renderStates),
    sets->outputFormatSet, sets->graphicsPipelineSet, sets->meshPipelineSet, sets->computePipelineSet, defaultFormat);
  return 1;
}

int on_get_buffer_gpu_address(void *buffer, void *address)
{
  if (!buffer || !address)
  {
    return 0;
  }

  auto bufferRef = get_any_buffer_ref(static_cast<GenericBufferInterface *>(buffer));
  *static_cast<uint64_t *>(address) = bufferRef.gpuPointer;

  return 1;
}

int on_copy_from_external_resource(void *par1, void *par2)
{
  auto src = static_cast<ID3D12Resource *>(par1);
  auto dstTex = cast_to_texture_base(static_cast<BaseTexture *>(par2));
  if (!src || !dstTex)
  {
    return 0;
  }
  Image *dst = dstTex->getDeviceImage();
  if (!dst || !dst->hasTrackedState())
  {
    NAU_LOG_ERROR("DX12: DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE: <{}> has no device image", dstTex->getTexName());
    return 0;
  }

  ComPtr<ID3D12Device> srcDevice;
  if (FAILED(src->GetDevice(COM_ARGS(&srcDevice))) ||
      srcDevice.Get() != static_cast<ID3D12Device *>(drv3d_dx12::api_state.device.getDevice()))
  {
    NAU_LOG_ERROR("DX12: DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE: source for <{}> is not created on the driver device",
      dstTex->getTexName());
    return 0;
  }

  // CopyResource requirements: same size, mips, layers and format, no MSAA
  const D3D12_RESOURCE_DESC srcDesc = src->GetDesc();
  const D3D12_RESOURCE_DESC dstDesc = dst->getHandle()->GetDesc();
  if (D3D12_RESOURCE_DIMENSION_TEXTURE2D != srcDesc.Dimension || D3D12_RESOURCE_DIMENSION_TEXTURE2D != dstDesc.Dimension ||
      srcDesc.Width != dstDesc.Width || srcDesc.Height != dstDesc.Height || 1 != srcDesc.MipLevels || 1 != dstDesc.MipLevels ||
      1 != srcDesc.DepthOrArraySize || 1 != dstDesc.DepthOrArraySize || 1 != srcDesc.SampleDesc.Count ||
      1 != dstDesc.SampleDesc.Count || srcDesc.Format != dstDesc.Format)
  {
    NAU_LOG_ERROR("DX12: DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE: source {}x{} mips {} layers {} samples {} format {} does "
                  "not fit <{}> {}x{} mips {} layers {} samples {} format {}",
      srcDesc.Width, srcDesc.Height, srcDesc.MipLevels, srcDesc.DepthOrArraySize, srcDesc.SampleDesc.Count, (int)srcDesc.Format,
      dstTex->getTexName(), dstDesc.Width, dstDesc.Height, dstDesc.MipLevels, dstDesc.DepthOrArraySize, dstDesc.SampleDesc.Count,
      (int)dstDesc.Format);
    return 0;
  }

#if DAGOR_DBGLEVEL > 0
  dstTex->setWasUsed();
#endif
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  drv3d_dx12::api_state.device.getContext().copyFromExternalResource(src, dst);
  return 1;
}
} // namespace

int d3d::driver_command(int command, void *par1, void *par2, void *par3)
{
  STORE_RETURN_ADDRESS();
  switch (command)
  {
    case DRV3D_COMMAND_GET_BUFFER_GPU_ADDRESS: return on_get_buffer_gpu_address(par1, par2);
    case DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE: return on_copy_from_external_resource(par1, par2);
    case DRV3D_COMMAND_COMPILE_PIPELINE_SET: return on_driver_command_compile_pipeline_set(par1);
    case DRV3D_COMMAND_REMOVE_DEBUG_BREAK_STRING_SEARCH:
      drv3d_dx12::api_state.device.getContext().removeDebugBreakString({static_cast<const char *>(par1)});
      return 1;
    case DRV3D_COMMAND_ADD_DEBUG_BREAK_STRING_SEARCH:
      drv3d_dx12::api_state.device.getContext().addDebugBreakString({static_cast<const char *>(par1)});
      return 1;
    case DRV3D_COMMAND_DEBUG_BREAK: drv3d_dx12::api_state.device.getContext().debugBreak(); return 1;
    case DRV3D_COMMAND_PROCESS_APP_INACTIVE_UPDATE:
      drv3d_dx12::api_state.state.onFrameEnd(drv3d_dx12::api_state.device.getContext());
      drv3d_dx12::api_state.device.getContext().present(OutputMode::MINIMIZED);
#if _TARGET_PC_WIN
      if (drv3d_dx12::api_state.windowState.isWindowOccluded())
        drv3d_dx12::api_state.windowState.updateWindowOcclusionState();
      if (par1)
      {
        bool *occ_window = reinterpret_cast<bool *>(par1);
        *occ_window = drv3d_dx12::api_state.windowState.isWindowOccluded();
      }
#endif
      return 1;
    case DRV3D_COMMAND_PROCESS_PENDING_RESOURCE_UPDATED:
      // This will flush when no query is active and return true, otherwise it will do nothing and return false
      if (drv3d_dx12::api_state.device.getContext().flushDrawWhenNoQueries())
      {
        return 1;
      }
      break;
#if _TARGET_PC_WIN
    case DRV3D_COMMAND_SEND_GPU_CRASH_DUMP:
      drv3d_dx12::api_state.device.sendGPUCrashDump(static_cast<const char *>(par1), par2, reinterpret_cast<uintptr_t>(par3));
      return 1;
#endif
    case DRV3D_COMMAND_REPORT_RESOURCE_STATE_TRANSITIONS: enable_tracking_on_resource(static_cast<D3dResource *>(par1)); return 1;
    case DRV3D_COMMAND_DEBUG_MESSAGE:
      drv3d_dx12::api_state.device.getContext().writeDebugMessage(static_cast<const char *>(par1), reinterpret_cast<intptr_t>(par2),
        reinterpret_cast<intptr_t>(par3));
      return 1;
    case DRV3D_COMMAND_GET_TIMINGS:
#if DX12_RECORD_TIMING_DATA
      *reinterpret_cast<Drv3dTimings *>(par1) = drv3d_dx12::api_state.device.getContext().getTiming(reinterpret_cast<uintptr_t>(par2));
      return timing_history_length;
#else
      return 0;
#endif
    case DRV3D_GET_SHADER_CACHE_UUID:
      // if (par1)
      //{
      //  auto &properties = drv3d_dx12::api_state.device.getDeviceProperties().properties;
      //  static_assert(sizeof(uint64_t) * 2 == sizeof(properties.pipelineCacheUUID));
      //  memcpy(par1, properties.pipelineCacheUUID, sizeof(properties.pipelineCacheUUID));
      //  return 1;
      //}
      break;
    case DRV3D_COMMAND_AFTERMATH_MARKER: drv3d_dx12::api_state.device.getContext().placeAftermathMarker((const char *)par1); break;
    case DRV3D_COMMAND_SET_VS_DEBUG_INFO:
      drv3d_dx12::api_state.shaderProgramDatabase.updateVertexShaderName(drv3d_dx12::api_state.device.getContext(), ShaderID::importValue(*(int *)par1),
        (const char *)par2);
      break;
    case DRV3D_COMMAND_SET_PS_DEBUG_INFO:
      drv3d_dx12::api_state.shaderProgramDatabase.updatePixelShaderName(drv3d_dx12::api_state.device.getContext(), ShaderID::importValue(*(int *)par1),
        (const char *)par2);
      break;
    case DRV3D_COMMAND_D3D_FLUSH: break;
    case DRV3D_COMMAND_FLUSH_STATES:
    {
      ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
      drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::ALL);
      drv3d_dx12::api_state.state.flushCompute(drv3d_dx12::api_state.device.getContext());
#if D3D_HAS_RAY_TRACING
      drv3d_dx12::api_state.state.flushRaytrace(drv3d_dx12::api_state.device.getContext());
#endif
    }
    break;
      //  case DRV3D_COMMAND_GETALLTEXS:
      //  case DRV3D_COMMAND_SET_STAT3D_HANDLER:
      //  case DRV3D_COMMAND_GETTEXTUREMEM:
      //    break;
    case DRV3D_COMMAND_GETVISIBILITYBEGIN:
    {
      Query **q = static_cast<Query **>(par1);
      if (!*q)
      {
        *q = drv3d_dx12::api_state.device.getQueryManager().newQuery();
      }
      drv3d_dx12::api_state.device.getContext().beginVisibilityQuery(*q);
      return 0;
    }
    case DRV3D_COMMAND_GETVISIBILITYEND:
    {
      Query *q = static_cast<Query *>(par1);
      if (q)
      {
        drv3d_dx12::api_state.device.getContext().endVisibilityQuery(q);
      }
      return 0;
    }
    case DRV3D_COMMAND_GETVISIBILITYCOUNT:
    {
      Query *q = static_cast<Query *>(par1);
      if (q)
      {
        if (q->isFinalized())
        {
          return static_cast<int>(q->getValue());
        }
        else
        {
          return -1;
        }
      }
      return 0;
    }
      //  case DRV3D_COMMAND_ENABLEDEBUGTEXTURES:
      //  case DRV3D_COMMAND_DISABLEDEBUGTEXTURES:
      //    break;
    case DRV3D_COMMAND_ENABLE_MT:
      drv3d_dx12::api_state.globalLock.enableMT();
      return 1;
      //  case DRV3D_COMMAND_DISABLE_MT:// ignore
      //    break;
    case DRV3D_COMMAND_ENTER_RESOURCE_LOCK_CS:
    case DRV3D_COMMAND_LEAVE_RESOURCE_LOCK_CS: break;
    case DRV3D_COMMAND_ACQUIRE_OWNERSHIP: drv3d_dx12::api_state.globalLock.lock(); break;
    case DRV3D_COMMAND_RELEASE_OWNERSHIP:
      drv3d_dx12::api_state.globalLock.unlock();
      break;
      //  case DRV3D_COMMAND_PREALLOCATE_RT:
      //    break;
      //  case DRV3D_COMMAND_SUSPEND:   // ignore
      //    break;
      //  case DRV3D_COMMAND_RESUME:  // ignore
      //    break;
      //  case DRV3D_COMMAND_GET_SUSPEND_COUNT: // ignore
      //    break;
      //  case DRV3D_COMMAND_GET_MEM_USAGE:
      //  case DRV3D_COMMAND_GET_AA_LEVEL:
      //    break;
      //  case DRV3D_COMMAND_THREAD_ENTER:
      //    break;
      //  case DRV3D_COMMAND_THREAD_LEAVE:
      //    break;
      //  case DRV3D_COMMAND_GET_GPU_FRAME_TIME:  // ignore
      //    break;
      //  case DRV3D_COMMAND_ENTER_RESOURCE_LOCK_CS:
      //  case DRV3D_COMMAND_LEAVE_RESOURCE_LOCK_CS:
      //    break;
      //  case DRV3D_COMMAND_INVALIDATE_STATES: // ignore
      //    break;
    case D3V3D_COMMAND_TIMESTAMPFREQ: *reinterpret_cast<uint64_t *>(par1) = drv3d_dx12::api_state.device.getGpuTimestampFrequency(); return 1;
    case D3V3D_COMMAND_TIMESTAMPISSUE:
    {
      Query **q = static_cast<Query **>(par1);
      if (!*q)
      {
        *q = drv3d_dx12::api_state.device.getQueryManager().newQuery();
      }
      drv3d_dx12::api_state.device.getContext().insertTimestampQuery(*q);
      return 1;
    }
    break;
    case D3V3D_COMMAND_TIMESTAMPGET:
    {
      Query *q = static_cast<Query *>(par1);
      if (q)
      {
        if (q->isFinalized())
        {
          *reinterpret_cast<uint64_t *>(par2) = q->getValue();
          return 1;
        }
      }
    }
    break;
    case D3V3D_COMMAND_TIMECLOCKCALIBRATION:
    {
      return drv3d_dx12::api_state.device.getGpuClockCalibration(reinterpret_cast<uint64_t *>(par2), reinterpret_cast<uint64_t *>(par1),
        reinterpret_cast<int *>(par3));
    }
    break;
    case DRV3D_COMMAND_RELEASE_QUERY:
      if (par1 && *static_cast<Query **>(par1))
      {
        drv3d_dx12::api_state.device.getContext().deleteQuery(*static_cast<Query **>(par1));
        *static_cast<Query **>(par1) = 0;
      }
      break;

      //  case DRV3D_COMMAND_GET_SECONDARY_BACKBUFFER:
      //    break;
      // case DRV3D_COMMAND_GET_VENDOR:
      //   break;

    case DRV3D_COMMAND_BEGIN_MRT_CLEAR_SEQUENCE:
      drv3d_dx12::api_state.state.beginMrtClear(reinterpret_cast<intptr_t>(par1));
      // tell caller that we implement this optimization
      if (par2)
        *reinterpret_cast<uint32_t *>(par2) = 1;
      break;
    case DRV3D_COMMAND_END_MRT_CLEAR_SEQUENCE:
    {
      drv3d_dx12::api_state.state.endMrtClear(drv3d_dx12::api_state.device.getContext());
      break;
    }
    case DRV3D_COMMAND_GET_DLSS_STATE:
    {
      return int(drv3d_dx12::api_state.device.getContext().getDlssState());
    }
    case DRV3D_COMMAND_GET_XESS_STATE:
    {
      return int(drv3d_dx12::api_state.device.getContext().getXessState());
    }
    case DRV3D_COMMAND_IS_DLSS_QUALITY_AVAILABLE_AT_RESOLUTION:
    {
#if _TARGET_PC_WIN
      nau::math::IVector2 targetResolution = handleAutoResolution(*(nau::math::IVector2 *)par1); // TODO: check if it works
      int dlssQuality = *(int *)par2;
      return drv3d_dx12::api_state.device.getContext().isDlssQualityAvailableAtResolution(targetResolution.getX(), targetResolution.getY(), dlssQuality);
#else
      return 0;
#endif
    }
    case DRV3D_COMMAND_IS_XESS_QUALITY_AVAILABLE_AT_RESOLUTION:
    {
#if _TARGET_PC_WIN
      nau::math::IVector2 targetResolution = handleAutoResolution(*(nau::math::IVector2 *)par1);
      int xessQuality = *(int *)par2;
      return drv3d_dx12::api_state.device.getContext().isXessQualityAvailableAtResolution(targetResolution.getX(), targetResolution.getY(), xessQuality);
#else
      return 0;
#endif
    }
    case DRV3D_COMMAND_GET_DLSS_RESOLUTION:
    {
      drv3d_dx12::api_state.device.getContext().getDlssRenderResolution(*(int *)par1, *(int *)par2);
      return 1;
    }
    case DRV3D_COMMAND_GET_XESS_RESOLUTION:
    {
      drv3d_dx12::api_state.device.getContext().getXessRenderResolution(*(int *)par1, *(int *)par2);
      return 1;
    }
    case DRV3D_COMMAND_EXECUTE_DLSS:
    {
      drv3d_dx12::api_state.device.getContext().executeDlss(*(DlssParams *)par1, par2 ? *(int *)par2 : 0);
      return 1;
    }
    case DRV3D_COMMAND_EXECUTE_XESS:
    {
      drv3d_dx12::api_state.device.getContext().executeXess(*(XessParams *)par1);
      return 1;
    }
    case DRV3D_COMMAND_EXECUTE_FSR2:
    {
      drv3d_dx12::api_state.device.getContext().executeFSR2(*(Fsr2Params *)par1);
      return 1;
    }
    case DRV3D_COMMAND_GET_FSR2_STATE:
    {
      return int(drv3d_dx12::api_state.device.getContext().getFsr2State());
    }
    case DRV3D_COMMAND_GET_FSR2_RESOLUTION:
    {
      drv3d_dx12::api_state.device.getContext().getFsr2RenderResolution(*(int *)par1, *(int *)par2);
      return 1;
    }
    case DRV3D_COMMAND_SET_XESS_VELOCITY_SCALE:
    {
      drv3d_dx12::api_state.device.getContext().setXessVelocityScale(*(float *)par1, *(float *)par2);
      return 1;
    }
    case DRV3D_COMMAND_PIX_GPU_BEGIN_CAPTURE:
#if _TARGET_XBOX
      drv3d_dx12::api_state.device.getContext().flushDraws();
#endif
      drv3d_dx12::api_state.device.getContext().beginCapture(reinterpret_cast<uintptr_t>(par1), reinterpret_cast<LPCWSTR>(par2));
      return 1;
    case DRV3D_COMMAND_PIX_GPU_END_CAPTURE:
#if _TARGET_XBOX
      drv3d_dx12::api_state.device.getContext().flushDraws();
#endif
      drv3d_dx12::api_state.device.getContext().endCapture();
      return 1;
    case DRV3D_COMMAND_PIX_GPU_CAPTURE_NEXT_FRAMES:
      drv3d_dx12::api_state.device.getContext().captureNextFrames(reinterpret_cast<uintptr_t>(par1), reinterpret_cast<LPCWSTR>(par2),
        reinterpret_cast<uintptr_t>(par3));
      return 1;
#if DX12_CAPTURE_AFTER_LONG_FRAMES
    case DRV3D_COMMAND_PIX_GPU_CAPTURE_AFTER_LONG_FRAMES:
    {
      const CaptureAfterLongFrameParams *params = reinterpret_cast<const CaptureAfterLongFrameParams *>(par1);
      drv3d_dx12::api_state.device.getContext().captureAfterLongFrames(params->thresholdUS, params->frames, params->captureCountLimit,
        params->flags);
      return 1;
    }
#endif
#if _TARGET_PC_WIN
    case DRV3D_COMMAND_GET_MONITORS:
    {
      eastl::vector<nau::string>& monitorList = *reinterpret_cast<eastl::vector<nau::string> *>(par1);
      drv3d_dx12::api_state.device.enumerateActiveMonitors(monitorList);
      return 1;
    }
    case DRV3D_COMMAND_GET_MONITOR_INFO:
    {
      const char *displayName = *reinterpret_cast<const char **>(par1);
      nau::string *friendlyName = reinterpret_cast<nau::string *>(par2);
      int *monitorIndex = reinterpret_cast<int *>(par3);
      return get_monitor_info(displayName, friendlyName, monitorIndex) ? 1 : 0;
    }
    case DRV3D_COMMAND_GET_RESOLUTIONS_FROM_MONITOR:
    {
      const char *displayName = resolve_monitor_name(*reinterpret_cast<const char **>(par1));
      eastl::vector<nau::string> &resolutions = *reinterpret_cast<eastl::vector<nau::string> *>(par2);
      clear_and_shrink(resolutions);

      ComPtr<IDXGIOutput> output = drv3d_dx12::api_state.device.getOutputMonitorByNameOrDefault(displayName);
      if (output)
      {
        drv3d_dx12::api_state.device.enumerateDisplayModesFromOutput(output.Get(), resolutions);
        return 1;
      }
      return 0;
    }
#endif

    case DRV3D_COMMAND_GET_VSYNC_REFRESH_RATE:
    {
#if _TARGET_PC_WIN
      // for fullscreen with vsync or for windowed mode
      DXGI_MODE_DESC modeDesc;
      HRESULT hr = drv3d_dx12::api_state.device.findClosestMatchingMode(&modeDesc);
      if (SUCCEEDED(hr))
      {
        double vsyncRefreshRate = (double)modeDesc.RefreshRate.Numerator / (double)Vectormath::max(modeDesc.RefreshRate.Denominator, 1u);
        *(double *)par1 = *(double *)&vsyncRefreshRate;
        return 1;
      }
#endif
      return 0;
    }

    case DRV3D_COMMAND_IS_HDR_AVAILABLE:
    {
#if _TARGET_XBOX
      return drv3d_dx12::is_hdr_available();
#else
      const char *displayName = par1 ? resolve_monitor_name(reinterpret_cast<const char *>(par1)) : nullptr;
      ComPtr<IDXGIOutput> output = drv3d_dx12::api_state.device.getOutputMonitorByNameOrDefault(displayName);
      return drv3d_dx12::is_hdr_available(output);
#endif
    }
#if _TARGET_PC_WIN
    case DRV3D_COMMAND_ENABLE_IMMEDIATE_FLUSH: return drv3d_dx12::api_state.device.getContext().enableImmediateFlush() ? 1 : 0;
    case DRV3D_COMMAND_DISABLE_IMMEDIATE_FLUSH: drv3d_dx12::api_state.device.getContext().disableImmediateFlush(); return 0;
#endif
    case DRV3D_COMMAND_IS_HDR_ENABLED: return static_cast<int>(drv3d_dx12::api_state.isHDREnabled);
    case DRV3D_COMMAND_INT10_HDR_BUFFER:
    {
#if _TARGET_XBOX
      return 1;
#else
      return !drv3d_dx12::api_state.device.getContext().getSwapchainColorFormat().isFloat();
#endif
    }
    case DRV3D_COMMAND_HDR_OUTPUT_MODE:
    {
      if (drv3d_dx12::api_state.isHDREnabled)
      {
#if _TARGET_XBOX
        return is_auto_gamedvr() ? static_cast<int>(HdrOutputMode::HDR10_ONLY) : static_cast<int>(HdrOutputMode::HDR10_AND_SDR);
#else
        bool hdr10 = driver_command(DRV3D_COMMAND_INT10_HDR_BUFFER, NULL, NULL, NULL);
        return static_cast<int>(hdr10 ? HdrOutputMode::HDR10_ONLY : HdrOutputMode::HDR_ONLY);
#endif
      }
      return static_cast<int>(HdrOutputMode::SDR_ONLY);
    }
    case DRV3D_COMMAND_GET_LUMINANCE:
    {
      *reinterpret_cast<float *>(par1) = drv3d_dx12::api_state.minLum;
      *reinterpret_cast<float *>(par2) = drv3d_dx12::api_state.maxLum;
      *reinterpret_cast<float *>(par3) = drv3d_dx12::api_state.maxFullFrameLum;
      return 1;
    }
    case DRV3D_COMMAND_MAKE_TEXTURE:
    {
      const Drv3dMakeTextureParams *makeParams = (const Drv3dMakeTextureParams *)par1;
      *(Texture **)par2 =
        drv3d_dx12::api_state.device.wrapD3DTex((ID3D12Resource *)makeParams->tex, makeParams->currentState, makeParams->name, makeParams->flg);
      return 1;
    }
    case DRV3D_COMMAND_GET_TEXTURE_HANDLE:
    {
      const Texture *texture = (const Texture *)par1;
      (*(void **)par2) = ((BaseTex *)texture)->tex.image->getHandle();
      return 1;
    }
    case DRV3D_COMMAND_PREPARE_TEXTURE_FOR_EXTERNAL_COPY:
    {
      if (!par1 || !par2 || !par3)
        return 0;
      ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
      auto *texture = cast_to_texture_base(static_cast<BaseTexture *>(par1));
      auto *image = texture->getDeviceImage();
      if (!image)
        return 0;
      const auto state = *static_cast<const bool *>(par2) ? RB_RW_COPY_DEST : RB_RO_COPY_SOURCE;
      auto &context = drv3d_dx12::api_state.device.getContext();
      context.textureBarrier(image, image->getSubresourceRangeForBarrier(0, 0), texture->cflg,
        state, GpuPipeline::GRAPHICS, true);
      *static_cast<void **>(par3) = image->getHandle();
      return 1;
    }
    case DRV3D_COMMAND_GET_RENDERING_COMMAND_QUEUE:
      *(ID3D12CommandQueue **)par1 = drv3d_dx12::api_state.device.getGraphicsCommandQueue();
      return 1;
    case DRV3D_COMMAND_REGISTER_FRAME_COMPLETION_EVENT:
      drv3d_dx12::api_state.device.getContext().registerFrameCompleteEvent(*static_cast<os_event_t *>(par1));
      return 1;
    case DRV3D_COMMAND_REGISTER_ONE_TIME_FRAME_EXECUTION_EVENT_CALLBACKS:
      drv3d_dx12::api_state.device.getContext().registerFrameEventCallbacks(static_cast<FrameEvents *>(par1), par2);
      return 1;
    case DRV3D_COMMAND_REGISTER_DEVICE_RESET_EVENT_HANDLER:
      drv3d_dx12::api_state.device.registerDeviceResetEventHandler(static_cast<DeviceResetEventHandler *>(par1));
      return 1;
    case DRV3D_COMMAND_UNREGISTER_DEVICE_RESET_EVENT_HANDLER:
      drv3d_dx12::api_state.device.unregisterDeviceResetEventHandler(static_cast<DeviceResetEventHandler *>(par1));
      return 1;
    case DRV3D_COMMAND_REGISTER_SHADER_DUMP:
      drv3d_dx12::api_state.shaderProgramDatabase.registerShaderBinDump(drv3d_dx12::api_state.device.getContext(),
        reinterpret_cast<ScriptedShadersBinDumpOwner *>(par1));
      return 1;
    case DRV3D_COMMAND_GET_SHADER:
      drv3d_dx12::api_state.shaderProgramDatabase.getBindumpShader(drv3d_dx12::api_state.device.getContext(), reinterpret_cast<uintptr_t>(par1),
        static_cast<ShaderCodeType>(reinterpret_cast<uintptr_t>(par2)), par3);
      return 1;

    case DRV3D_COMMAND_PREPARE_TEXTURES_FOR_VR_CONSUMPTION:
    {
      for (int ix = 0; ix < (intptr_t)par2; ++ix)
      {
        BaseTexture *tex = ((BaseTexture **)par1)[ix];
        auto baseTex = cast_to_texture_base(tex);
        if (!baseTex)
          continue;

        bool isDepth = baseTex->fmt.isDepth();
        auto &ctx = drv3d_dx12::api_state.device.getContext();
        auto image = baseTex->getDeviceImage();
        if (!image)
        {
          continue;
        }
        ctx.textureBarrier(image, image->getSubresourceRangeForBarrier(0, 0), baseTex->cflg,
          isDepth ? ResourceBarrier::RB_RW_DEPTH_STENCIL_TARGET : ResourceBarrier::RB_RW_RENDER_TARGET, GpuPipeline::GRAPHICS, true);
      }
      return 1;
    }

    case DRV3D_COMMAND_GET_VIDEO_MEMORY_BUDGET:
    {
      uint64_t dlssVramUsage = drv3d_dx12::api_state.device.getContext().getDlssVramUsage();
      return drv3d_dx12::api_state.device.getGpuMemUsageStats(dlssVramUsage, reinterpret_cast<uint32_t *>(par1),
        reinterpret_cast<uint32_t *>(par2), reinterpret_cast<uint32_t *>(par3));
    }

    case DRV3D_COMMAND_SET_FREQ_LEVEL:
    {
#if _TARGET_XBOX
      drv3d_dx12::api_state.device.getContext().updateFrameInterval(*reinterpret_cast<int32_t *>(par1));
#endif
      return 1;
    }
  };
  // silence compiler - we may not use it when some extensions are not compiled in
  G_UNUSED(par3);
  return 0;
}

extern bool dagor_d3d_force_driver_mode_reset;
bool d3d::device_lost(bool *can_reset_now)
{
  if (can_reset_now && (dagor_d3d_force_driver_reset || dagor_d3d_force_driver_mode_reset))
    *can_reset_now = true;
  return dagor_d3d_force_driver_reset || dagor_d3d_force_driver_mode_reset;
}
static bool device_is_being_reset = false;
bool d3d::is_in_device_reset_now() { return /*device_is_lost != S_OK || */ device_is_being_reset; }

#if _TARGET_PC_WIN
namespace
{
struct PushTextureAddressMode
{
  TextureInterfaceBase *target = nullptr;
  D3D12_TEXTURE_ADDRESS_MODE u = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  D3D12_TEXTURE_ADDRESS_MODE v = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
  D3D12_TEXTURE_ADDRESS_MODE w = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

  PushTextureAddressMode(TextureInterfaceBase *t) :
    target{t}, u{t->samplerState.getU()}, v{t->samplerState.getV()}, w{t->samplerState.getW()}
  {}
  ~PushTextureAddressMode()
  {
    target->samplerState.setU(u);
    target->samplerState.setV(v);
    target->samplerState.setW(w);
    target->notifySamplerChange();
  }
};
void recover_textures()
{
  drv3d_dx12::api_state.device.visitTextureObjects([](auto tex) //
    {
      //watchdog_kick();
      tex->resetTex();
      if (tex->rld)
      {
        PushTextureAddressMode pushMode{tex};
        tex->rld->reloadD3dRes(tex);
      }
      else if ((tex->cflg & TEXCF_SYSTEXCOPY) && data_size(tex->texCopy))
      {
        auto u = translate_texture_address_mode_to_engine(tex->samplerState.getU());
        auto v = translate_texture_address_mode_to_engine(tex->samplerState.getV());

        ddsx::Header &hdr = *(ddsx::Header *)tex->texCopy.data();
        int8_t sysCopyQualityId = hdr.hqPartLevels;
        unsigned flg = hdr.flags & ~(hdr.FLG_ADDRU_MASK | hdr.FLG_ADDRV_MASK);
        hdr.flags = flg | (u & hdr.FLG_ADDRU_MASK) | ((v << 4) & hdr.FLG_ADDRV_MASK);

        nau::iosys::InPlaceMemLoadCB mcrd(tex->texCopy.data() + sizeof(hdr), data_size(tex->texCopy) - (int)sizeof(hdr));
        d3d::load_ddsx_tex_contents(tex, hdr, mcrd, sysCopyQualityId);
      }
      else
        tex->recreate();
    });

  drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture()->updateDeviceSampler();
  drv3d_dx12::api_state.device.getContext().getSwapchainDepthStencilTextureAnySize()->updateDeviceSampler();
}
void recover_buffers()
{
  drv3d_dx12::api_state.device.visitBufferObjects([](auto buf) {
    //watchdog_kick();
    buf->recreate();
  });
  drv3d_dx12::api_state.device.visitBufferObjects([](auto buf) {
    //watchdog_kick();
    buf->restore();
  });
}
} // namespace
#endif

bool d3d::reset_device()
{
  STORE_RETURN_ADDRESS();
#if _TARGET_PC_WIN
  struct RaiiReset
  {
    RaiiReset() { device_is_being_reset = true; }
    ~RaiiReset() { device_is_being_reset = false; }
  } raii_reset;
  const nau::DataBlock &blk_dx = *dgs_get_settings()->getBlockByNameEx("dx12");

  POINT cursorPos{};
  //mouse_api_GetCursorPosRel(&cursorPos, drv3d_dx12::api_state.windowState.getMainWindow()); // TODO: fix it
  float cursorPosX = (float)cursorPos.x / drv3d_dx12::api_state.windowState.settings.resolutionX;
  float cursorPosY = (float)cursorPos.y / drv3d_dx12::api_state.windowState.settings.resolutionY;

  const char *displayName = get_monitor_name_from_settings();

  // don't show any window messages when we are in game
  bool savedExecuteQuiet = dgs_execute_quiet;
  dgs_execute_quiet = true;
  drv3d_dx12::api_state.windowState.getRenderWindowSettings(stereo_config_callback);
  dgs_execute_quiet = savedExecuteQuiet;

  if (!drv3d_dx12::api_state.windowState.setRenderWindowParams())
    return false;

  if (dagor_d3d_force_driver_reset)
  {
    drv3d_dx12::api_state.state.preRecovery();

    // this will tear down stuff as we need it, but keeps things that can be left alone
    // (like shader binaries and such)
    // capture the luid of the selected device and try to use it again
    auto luid = drv3d_dx12::api_state.device.preRecovery();

    auto featureLevel = make_feature_level(blk_dx.getInt("FeatureLevelMajor", min_major_feature_level),
      blk_dx.getInt("FeatureLevelMinor", min_minor_feature_level));
    ComPtr<IDXGIAdapter1> adapter1;
    if (SUCCEEDED(drv3d_dx12::api_state.dxgi14->EnumAdapterByLuid(luid, COM_ARGS(&adapter1))))
    {
      SwapchainCreateInfo sci{};
      set_sci_hdr_config(sci);
      sci.output = get_output_monitor_by_name_or_default(adapter1.Get(), displayName);
      drv3d_dx12::api_state.device.recover(drv3d_dx12::api_state.dxgi14.Get(), eastl::move(adapter1), featureLevel, drv3d_dx12::api_state.d3d12Env,
        reinterpret_cast<HWND>(drv3d_dx12::api_state.windowState.getMainWindow()), eastl::move(sci));
    }
    else
    {
      NAU_LOG_WARNING("DX12: EnumAdapterByLuid with previously used device LUID failed, device no longer "
              "available?");
      // TODO fallback to normal enumeration path to find a suitable device
    }

    // only try to restore state if recovery process did work
    if (!drv3d_dx12::api_state.device.isDead())
    {
      recover_textures();
      recover_buffers();
    }

    // finalizeRecovery will check if the device is dead or healthy and returns true if the device
    // can continue
    if (!drv3d_dx12::api_state.device.finalizeRecovery())
    {
      NAU_FAILURE("DX12: Observed an critical error while recovering from a previous critical error, can not continue");
      return false;
    }
  }
  else if (dagor_d3d_force_driver_mode_reset)
  {
    Extent2D bbres = drv3d_dx12::api_state.device.getContext().getSwapchainExtent();
    bool refreshSwapchain =
      bbres.width != drv3d_dx12::api_state.windowState.settings.resolutionX || bbres.height != drv3d_dx12::api_state.windowState.settings.resolutionY;

    drv3d_dx12::api_state.device.getContext().changePresentMode(get_presentation_mode_from_settings());

    // also update for possible change out of exclusive mode
    drv3d_dx12::api_state.device.getContext().changeFullscreenExclusiveMode(dgs_get_window_mode() == WindowMode::FULLSCREEN_EXCLUSIVE,
      drv3d_dx12::api_state.device.getOutputMonitorByNameOrDefault(displayName));

    // must refresh these after output (fullscreen mode) change
    if (refreshSwapchain)
    {
      bbres.width = drv3d_dx12::api_state.windowState.settings.resolutionX;
      bbres.height = drv3d_dx12::api_state.windowState.settings.resolutionY;
      drv3d_dx12::api_state.device.getContext().changeSwapchainExtents(bbres);
      drv3d_dx12::api_state.state.notifySwapchainChange();
    }

    if (d3d::get_driver_desc().caps.hasDLSS)
    {
      drv3d_dx12::api_state.device.getContext().releaseDlssFeature();
      int dlssQuality = dgs_get_settings()->getBlockByNameEx("video")->getInt("dlssQuality", -1);
      if (dlssQuality >= 0)
      {
        auto toExtent2D = [](auto size) { return Extent2D({static_cast<uint32_t>(size.width), static_cast<uint32_t>(size.height)}); };
        auto targetResolution = stereo_config_callback && stereo_config_callback->desiredStereoRender()
                                  ? toExtent2D(stereo_config_callback->desiredRendererSize())
                                  : drv3d_dx12::api_state.device.getContext().getSwapchainExtent();
        drv3d_dx12::api_state.device.getContext().createDlssFeature(dlssQuality, targetResolution,
          stereo_config_callback ? stereo_config_callback->desiredStereoRender() : false);
        drv3d_dx12::api_state.device.getContext().wait();
      }
    }

    if (d3d::get_driver_desc().caps.hasXESS)
    {
      drv3d_dx12::api_state.device.getContext().shutdownXess();
      drv3d_dx12::api_state.device.getContext().initXeSS();
    }

    drv3d_dx12::api_state.device.getContext().shutdownFsr2();
    drv3d_dx12::api_state.device.getContext().initFsr2();
  }

  cursorPos.x = cursorPosX * drv3d_dx12::api_state.windowState.settings.resolutionX;
  cursorPos.y = cursorPosY * drv3d_dx12::api_state.windowState.settings.resolutionY;
  //mouse_api_SetCursorPosRel(drv3d_dx12::api_state.windowState.getMainWindow(), &cursorPos); // TODO: fix it

  dagor_d3d_force_driver_mode_reset = false;
  dagor_d3d_force_driver_reset = false;
#endif
  return true;
}

namespace
{
uint32_t map_dx12_format_features_to_tex_usage(D3D12_FEATURE_DATA_FORMAT_SUPPORT support, FormatStore fmt, int res_type)
{
  uint32_t result = 0;
  UINT mask = static_cast<UINT>(support.Support1);
  if ((res_type == RES3D_TEX || res_type == RES3D_ARRTEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURE2D))
    return 0;
  if ((res_type == RES3D_VOLTEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURE3D))
    return 0;
  if ((res_type == RES3D_CUBETEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURECUBE))
    return 0;

  result = d3d::USAGE_TEXTURE | d3d::USAGE_VERTEXTEXTURE;
  if (fmt.isSrgbCapableFormatType())
    result |= d3d::USAGE_SRGBREAD;

  if (mask & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)
    result |= d3d::USAGE_DEPTH;
  if (mask & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)
  {
    // TODO: if mipgen is done by compute, check this
    // currently mips are generated by bliting
    result |= d3d::USAGE_RTARGET | d3d::USAGE_AUTOGENMIPS;
    if (fmt.isSrgbCapableFormatType())
      result |= d3d::USAGE_SRGBWRITE;
  }
  if (mask & D3D12_FORMAT_SUPPORT1_BLENDABLE)
    result |= d3d::USAGE_BLEND;

  if (mask & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE)
    result |= d3d::USAGE_FILTER;

  if (mask & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)
  {
    result |= d3d::USAGE_UNORDERED;
    if (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD)
    {
      result |= d3d::USAGE_UNORDERED_LOAD;
    }
  }

  if (support.Support2 & D3D12_FORMAT_SUPPORT2_TILED)
    result |= d3d::USAGE_TILED;

  return result | d3d::USAGE_PIXREADWRITE;
}

bool check_format_features(int cflg, D3D12_FEATURE_DATA_FORMAT_SUPPORT support, FormatStore fmt, int res_type)
{
  UINT mask = static_cast<UINT>(support.Support1);
  if ((res_type == RES3D_TEX || res_type == RES3D_ARRTEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURE2D))
    return false;
  if ((res_type == RES3D_VOLTEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURE3D))
    return false;
  if ((res_type == RES3D_CUBETEX) && !(mask & D3D12_FORMAT_SUPPORT1_TEXTURECUBE))
    return false;

  if ((cflg & TEXCF_UNORDERED) && (0 == (mask & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)))
    return false;

  if (fmt.isDepth() && (0 == (mask & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)))
    return false;

  bool isMultisampled = ((cflg & TEXCF_SAMPLECOUNT_MASK) != 0);

  if (isMultisampled && (0 == (mask & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RENDERTARGET)))
    return false;

  if (isMultisampled && (fmt.isColor() || d3d::get_driver_desc().caps.hasRenderPassDepthResolve) &&
      (0 == (mask & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_RESOLVE)))
    return false;

  if (isMultisampled && (fmt.isColor() || d3d::get_driver_desc().caps.hasReadMultisampledDepth) &&
      (0 == (mask & D3D12_FORMAT_SUPPORT1_MULTISAMPLE_LOAD)))
    return false;

  if ((cflg & TEXCF_TILED_RESOURCE) != 0 && (support.Support2 & D3D12_FORMAT_SUPPORT2_TILED) == 0)
    return false;

  return true;
}
} // namespace
bool d3d::check_texformat(int cflg)
{
  auto fmt = FormatStore::fromCreateFlags(cflg);
  if (!drv3d_dx12::api_state.device.isSamplesCountSupported(fmt.asDxGiFormat(), get_sample_count(cflg)))
    return false;
  auto features = drv3d_dx12::api_state.device.getFormatFeatures(fmt);
  return check_format_features(cflg, features, fmt, RES3D_TEX);
}

int d3d::get_max_sample_count(int cflg)
{
  auto dxgiFormat = FormatStore::fromCreateFlags(cflg).asDxGiFormat();
  for (int32_t numSamples = get_sample_count(TEXCF_SAMPLECOUNT_MAX); numSamples > 1; numSamples /= 2)
    if (drv3d_dx12::api_state.device.isSamplesCountSupported(dxgiFormat, numSamples))
      return numSamples;

  return 1;
}

bool d3d::issame_texformat(int cflg1, int cflg2)
{
  auto formatA = FormatStore::fromCreateFlags(cflg1);
  auto formatB = FormatStore::fromCreateFlags(cflg2);
  return formatA.asDxGiFormat() == formatB.asDxGiFormat();
}

bool d3d::check_cubetexformat(int cflg)
{
  auto fmt = FormatStore::fromCreateFlags(cflg);
  auto features = drv3d_dx12::api_state.device.getFormatFeatures(fmt);
  return check_format_features(cflg, features, fmt, RES3D_CUBETEX);
}

bool d3d::issame_cubetexformat(int cflg1, int cflg2) { return issame_texformat(cflg1, cflg2); }

bool d3d::check_voltexformat(int cflg)
{
  auto fmt = FormatStore::fromCreateFlags(cflg);
  auto features = drv3d_dx12::api_state.device.getFormatFeatures(fmt);
  return check_format_features(cflg, features, fmt, RES3D_VOLTEX);
}

bool d3d::issame_voltexformat(int cflg1, int cflg2) { return issame_texformat(cflg1, cflg2); }

void d3d::discard_managed_textures() {}

bool d3d::stretch_rect(BaseTexture *src, BaseTexture *dst, nau::math::RectInt *rsrc, nau::math::RectInt *rdst)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  auto &device = drv3d_dx12::api_state.device;
  auto &ctx = device.getContext();

  TextureInterfaceBase *srcTex = cast_to_texture_base(src);
  TextureInterfaceBase *dstTex = cast_to_texture_base(dst);
  if (!srcTex)
  {
    srcTex = drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture();
  }
  if (!dstTex)
  {
    dstTex = drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture();
  }
  if (!srcTex || !dstTex)
  {
    NAU_LOG_WARNING("DX12: d3d::stretch_rect({:p}, {:p}, ...) after swapchain / window destruction, ignonring", (void*)src, (void*)dst);
    return false;
  }

  ImageBlit blit;
  blit.srcSubresource.mipLevel = MipMapIndex::make(0);
  blit.srcSubresource.baseArrayLayer = ArrayLayerIndex::make(0);
  blit.dstSubresource.mipLevel = MipMapIndex::make(0);
  blit.dstSubresource.baseArrayLayer = ArrayLayerIndex::make(0);

  if (rsrc)
  {
    blit.srcOffsets[0].x = rsrc->left;
    blit.srcOffsets[0].y = rsrc->top;
    blit.srcOffsets[0].z = 0;
    blit.srcOffsets[1].x = rsrc->right;
    blit.srcOffsets[1].y = rsrc->bottom;
    blit.srcOffsets[1].z = 1;
  }
  else
  {
    blit.srcOffsets[0].x = 0;
    blit.srcOffsets[0].y = 0;
    blit.srcOffsets[0].z = 0;
    blit.srcOffsets[1] = toOffset(srcTex->getMipmapExtent(0));
    if (blit.dstOffsets[1].z < 1)
    {
      blit.dstOffsets[1].z = 1;
    }
  }

  if (rdst)
  {
    blit.dstOffsets[0].x = rdst->left;
    blit.dstOffsets[0].y = rdst->top;
    blit.dstOffsets[0].z = 0;
    blit.dstOffsets[1].x = rdst->right;
    blit.dstOffsets[1].y = rdst->bottom;
    blit.dstOffsets[1].z = 1;
  }
  else
  {
    // TODO: we could lift this restriction when a blit would be equal to a straight copy
    // but currently we don't do that.
    const bool isRT = dstTex->isRenderTarget();
    const bool isC = dstTex->getFormat().isColor();
    if (!isRT || !isC)
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as a destination for stretch_rect, but destinations have to "
             "be a color render targets. isRT={} | isC={}",
        (void*)dstTex, dstTex->getResName(), isRT, isC);
      return false;
    }

    blit.dstOffsets[0].x = 0;
    blit.dstOffsets[0].y = 0;
    blit.dstOffsets[0].z = 0;
    blit.dstOffsets[1] = toOffset(dstTex->getMipmapExtent(0));
    if (blit.dstOffsets[1].z < 1)
    {
      blit.dstOffsets[1].z = 1;
    }
  }

  ctx.blitImage(srcTex->getDeviceImage(), dstTex->getDeviceImage(), blit);
  return true;
}

bool d3d::copy_from_current_render_target(BaseTexture *to_tex)
{
  CHECK_MAIN_THREAD();
  d3d::stretch_rect(drv3d_dx12::api_state.state.getColorTarget(0), to_tex);
  return true;
}

// Texture states setup

unsigned d3d::get_texformat_usage(int cflg, int res_type)
{
  auto fmt = FormatStore::fromCreateFlags(cflg);
  auto features = drv3d_dx12::api_state.device.getFormatFeatures(fmt);
  return map_dx12_format_features_to_tex_usage(features, fmt, res_type);
}

VPROG d3d::create_vertex_shader(const uint32_t *native_code)
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase.newVertexShader(drv3d_dx12::api_state.device.getContext(), native_code).exportValue();
}

VPROG d3d::create_raw_vertex_shader(eastl::span<const uint8_t> byte_code, const dxil::ShaderResourceUsageTable& usageTable, VDECL inputLayout)
{
  dxil::ShaderHeader rawVSHeader = {};
  rawVSHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::VERTEX);
  rawVSHeader.resourceUsageTable = usageTable;

  InputLayout ilDef = drv3d_dx12::api_state.shaderProgramDatabase.getInputLayout(InputLayoutID(inputLayout));
  rawVSHeader.inOutSemanticMask = ilDef.vertexAttributeSet.locationMask;

  auto vs = drv3d_dx12::api_state.shaderProgramDatabase.newRawVertexShader(drv3d_dx12::api_state.device.getContext(), rawVSHeader, byte_code);

  return vs.exportValue();
}

NAU_RENDER_EXPORT VPROG d3d::create_raw_vs_hs_ds_gs(VertexHullDomainGeometryShadersCreationDesc desc)
{
    VsHsDsGsGroup group {};
    group.vsHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::VERTEX);
    group.vsHeader.resourceUsageTable = desc.vsTable;
    group.vs_byte_code = desc.vs_byte_code;

    group.hsHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::HULL);
    group.hsHeader.resourceUsageTable = desc.hsTable;
    group.hs_byte_code = desc.hs_byte_code;

    group.hsHeader.inputPrimitive = desc.primitiveType;

    group.dsHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::DOMAIN);
    group.dsHeader.resourceUsageTable = desc.dsTable;
    group.ds_byte_code = desc.ds_byte_code;

    group.gsHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::GEOMETRY);
    group.gsHeader.resourceUsageTable = desc.gsTable;
    group.gs_byte_code = desc.gs_byte_code;


    InputLayout ilDef = drv3d_dx12::api_state.shaderProgramDatabase.getInputLayout(InputLayoutID(desc.inputLayout));
    group.vsHeader.inOutSemanticMask = ilDef.vertexAttributeSet.locationMask;

    auto vs = drv3d_dx12::api_state.shaderProgramDatabase.newRawVHDGShaders(drv3d_dx12::api_state.device.getContext(), group);
    return vs.exportValue();
}

FSHADER d3d::create_raw_pixel_shader(eastl::span<const uint8_t> byte_code, const dxil::ShaderResourceUsageTable& usageTable)
{
    dxil::ShaderHeader psHeader = {};
    psHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::PIXEL);
    psHeader.resourceUsageTable = usageTable;
    psHeader.inOutSemanticMask = 0x0000000F;

    auto ps = drv3d_dx12::api_state.shaderProgramDatabase.newRawPixelShader(drv3d_dx12::api_state.device.getContext(), psHeader, byte_code);

    return ps.exportValue();
}

void d3d::delete_vertex_shader(VPROG vs)
{
  STORE_RETURN_ADDRESS();
  auto shader = ShaderID::importValue(vs);
  if (!shader)
    return;
  drv3d_dx12::api_state.shaderProgramDatabase.deleteVertexShader(drv3d_dx12::api_state.device.getContext(), shader);
}

int d3d::set_cs_constbuffer_size(int required_size)
{
  NAU_ASSERT(required_size >= 0, "Negative register count?");
  return drv3d_dx12::api_state.state.setComputeConstRegisterCount(required_size);
}

int d3d::set_vs_constbuffer_size(int required_size)
{
  NAU_ASSERT(required_size >= 0, "Negative register count?");
  return drv3d_dx12::api_state.state.setVertexConstRegisterCount(required_size);
}

FSHADER d3d::create_pixel_shader(const uint32_t *native_code)
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase.newPixelShader(drv3d_dx12::api_state.device.getContext(), native_code).exportValue();
}

void d3d::delete_pixel_shader(FSHADER ps)
{
  STORE_RETURN_ADDRESS();
  auto shader = ShaderID::importValue(ps);
  if (!shader)
    return;
  drv3d_dx12::api_state.shaderProgramDatabase.deletePixelShader(drv3d_dx12::api_state.device.getContext(), shader);
}

PROGRAM d3d::get_debug_program()
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase.getDebugProgram().exportValue();
}

PROGRAM d3d::create_program(VPROG vs, FSHADER fs, VDECL vdecl, unsigned *, unsigned)
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase
    .newGraphicsProgram(drv3d_dx12::api_state.device.getContext(), InputLayoutID(vdecl), ShaderID::importValue(vs), ShaderID::importValue(fs))
    .exportValue();
}

PROGRAM d3d::create_program(const uint32_t *vs, const uint32_t *ps, VDECL vdecl, unsigned *strides, unsigned streams)
{
  VPROG vprog = create_vertex_shader(vs);
  FSHADER fshad = create_pixel_shader(ps);
  return create_program(vprog, fshad, vdecl, strides, streams);
}

PROGRAM d3d::create_program_cs(const uint32_t *cs_native, CSPreloaded preloaded)
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase.newComputeProgram(drv3d_dx12::api_state.device.getContext(), cs_native, preloaded).exportValue();
}

NAU_RENDER_EXPORT PROGRAM d3d::create_raw_program_cs(eastl::span<const uint8_t> byte_code, const dxil::ShaderResourceUsageTable& usageTable, CSPreloaded preloaded)
{
    dxil::ShaderHeader CSHeader = {};
    CSHeader.shaderType = static_cast<uint16_t>(dxil::ShaderStage::COMPUTE);
    CSHeader.resourceUsageTable = usageTable;
    CSHeader.inOutSemanticMask = 0;

    return drv3d_dx12::api_state.shaderProgramDatabase.newRawComputeProgram(drv3d_dx12::api_state.device.getContext(), CSHeader, byte_code, preloaded).exportValue();
}

bool d3d::set_program(PROGRAM prog_id)
{
  auto prog = ProgramID::importValue(prog_id);
  if (ProgramID::Null() != prog)
  {
    if (prog.isCompute())
    {
      drv3d_dx12::api_state.state.setComputeProgram(prog);
    }
#if D3D_HAS_RAY_TRACING
    else if (prog.isRaytrace())
    {
      drv3d_dx12::api_state.state.setRaytraceProgram(prog);
    }
#endif
    else
    {
      drv3d_dx12::api_state.state.setGraphicsProgram(drv3d_dx12::api_state.shaderProgramDatabase.getGraphicsProgramForStateUpdate(prog));
    }
  }
  return true;
}

void d3d::delete_program(PROGRAM prog)
{
  STORE_RETURN_ADDRESS();
  auto pid = ProgramID::importValue(prog);
  if (ProgramID::Null() == pid)
    return;

  drv3d_dx12::api_state.shaderProgramDatabase.removeProgram(drv3d_dx12::api_state.device.getContext(), pid);
}

#if _TARGET_PC
VPROG d3d::create_vertex_shader_dagor(const VPRTYPE * /*tokens*/, int /*len*/)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}

VPROG d3d::create_vertex_shader_asm(const char * /*asm_text*/)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}

#if _TARGET_PC_WIN
VPROG d3d::create_vertex_shader_hlsl(const char *, unsigned, const char *, const char *, nau::string *)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}
#endif

FSHADER d3d::create_pixel_shader_dagor(const FSHTYPE * /*tokens*/, int /*len*/)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}

FSHADER d3d::create_pixel_shader_asm(const char * /*asm_text*/)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}

// NOTE: entry point should be removed from the interface
bool d3d::set_pixel_shader(FSHADER /*shader*/)
{
  NAU_ASSERT(false);
  return true;
}

// NOTE: entry point should be removed from the interface
bool d3d::set_vertex_shader(VPROG /*shader*/)
{
  NAU_ASSERT(false);
  return true;
}

VDECL d3d::get_program_vdecl(PROGRAM prog)
{
  return drv3d_dx12::api_state.shaderProgramDatabase.getInputLayoutForGraphicsProgram(ProgramID::importValue(prog)).get();
}
#endif

#if _TARGET_PC_WIN
FSHADER d3d::create_pixel_shader_hlsl(const char *, unsigned, const char *, const char *, nau::string *)
{
  NAU_ASSERT(false);
  return BAD_PROGRAM;
}
#endif

bool d3d::set_const(unsigned stage, unsigned first, const void *data, unsigned count)
{
  NAU_ASSERT(stage < STAGE_MAX);
  drv3d_dx12::api_state.state.setConstRegisters(stage, first,
    {reinterpret_cast<const ConstRegisterType *>(data), static_cast<decltype(eastl::dynamic_extent)>(count)});

  return true;
}

bool d3d::set_blend_factor(nau::math::E3DCOLOR color)
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.setBlendConstant(color);
  return true;
}

bool d3d::set_tex(unsigned shader_stage, unsigned unit, BaseTexture *tex, bool /*use_sampler*/)
{
  BaseTex *texture = cast_to_texture_base(tex);
  drv3d_dx12::api_state.state.setStageSRVTexture(shader_stage, unit, texture);
  return true;
}

bool d3d::set_rwtex(unsigned shader_stage, unsigned unit, BaseTexture *tex, uint32_t face, uint32_t mip_level, bool as_uint)
{
  BaseTex *texture = cast_to_texture_base(tex);
  ImageViewState view;
  if (texture)
  {
    if (!texture->isUav())
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as UAV texture, but has no UAV flag set", (void*)texture, texture->getResName());
      return false;
    }
    view = texture->getViewInfoUav(MipMapIndex::make(mip_level), ArrayLayerIndex::make(face), as_uint);
  }
  drv3d_dx12::api_state.state.setStageUAVTexture(shader_stage, unit, texture, view);
  return true;
}

bool d3d::clear_rwtexi(BaseTexture *tex, const unsigned val[4], uint32_t face, uint32_t mip_level)
{
  STORE_RETURN_ADDRESS();
  BaseTex *texture = cast_to_texture_base(tex);
  if (texture)
  {
    if (!texture->isUav())
    {
      NAU_LOG_ERROR("Texture {:p} <{}> cleared as UAV(i) texture, but has no UAV flag set", (void*)texture, texture->getResName());
      return false;
    }
    Image *image = texture->getDeviceImage();
    // false for is_uint is same as in DX11 backend
    drv3d_dx12::api_state.device.getContext().clearUAVTexture(image,
      texture->getViewInfoUav(MipMapIndex::make(mip_level), ArrayLayerIndex::make(face), false), val);
  }
  return true;
}

bool d3d::clear_rwtexf(BaseTexture *tex, const float val[4], uint32_t face, uint32_t mip_level)
{
  STORE_RETURN_ADDRESS();
  BaseTex *texture = cast_to_texture_base(tex);
  if (texture)
  {
    if (!texture->isUav())
    {
      NAU_LOG_ERROR("Texture {:p} <{}> cleared as UAV(f) texture, but has no UAV flag set", (void*)texture, texture->getResName());
      return false;
    }
    Image *image = texture->getDeviceImage();
    drv3d_dx12::api_state.device.getContext().clearUAVTexture(image,
      texture->getViewInfoUav(MipMapIndex::make(mip_level), ArrayLayerIndex::make(face), false), val);
  }
  return true;
}

bool d3d::clear_rwbufi(Sbuffer *buffer, const unsigned values[4])
{
  STORE_RETURN_ADDRESS();
  if (buffer)
  {
    NAU_ASSERT(buffer->getFlags() & SBCF_BIND_UNORDERED);
    GenericBufferInterface *vbuf = (GenericBufferInterface *)buffer;
    vbuf->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsUAVBuffer(); });
    drv3d_dx12::api_state.device.getContext().clearBufferInt(vbuf->getDeviceBuffer(), values);
  }
  return true;
}

bool d3d::clear_rwbuff(Sbuffer *buffer, const float values[4])
{
  STORE_RETURN_ADDRESS();
  if (buffer)
  {
    NAU_ASSERT(buffer->getFlags() & SBCF_BIND_UNORDERED);
    GenericBufferInterface *vbuf = (GenericBufferInterface *)buffer;
    vbuf->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsUAVBuffer(); });
    drv3d_dx12::api_state.device.getContext().clearBufferFloat(vbuf->getDeviceBuffer(), values);
  }
  return true;
}

bool d3d::set_buffer(unsigned shader_stage, unsigned unit, Sbuffer *buffer)
{
  NAU_ASSERT((nullptr == buffer) || (buffer->getFlags() & (SBCF_BIND_UNORDERED | SBCF_BIND_SHADER_RES))); // todo: remove
                                                                                                        // SBCF_BIND_UNORDEREDcheck!
#if DAGOR_DBGLEVEL > 0
                                                                                                        // todo: this check to be
                                                                                                        // removed
  if (buffer && (buffer->getFlags() & (SBCF_BIND_UNORDERED | SBCF_BIND_SHADER_RES)) == SBCF_BIND_UNORDERED)
    NAU_LOG_ERROR("buffer {} is without SBCF_BIND_SHADER_RES flag and can't be used in SRV. Deprecated, fixme!", buffer->getBufName());
#endif
  drv3d_dx12::api_state.state.setStageTRegisterBuffer(shader_stage, unit, buffer);
  return true;
}

bool d3d::set_rwbuffer(unsigned shader_stage, unsigned unit, Sbuffer *buffer)
{
  if (buffer)
  {
    NAU_ASSERT(buffer->getFlags() & (SBCF_BIND_UNORDERED | SBCF_BIND_SHADER_RES)); // todo: remove SBCF_BIND_SHADER_RES check!
#if DAGOR_DBGLEVEL > 0
                                                                                 // todo: this check to be removed
    if ((buffer->getFlags() & (SBCF_BIND_UNORDERED | SBCF_BIND_SHADER_RES)) == SBCF_BIND_SHADER_RES)
      NAU_LOG_ERROR("buffer {} is without SBCF_BIND_UNORDERED flag and can't be used in UAV. Deprecated, fixme!", buffer->getBufName());
#endif
    drv3d_dx12::api_state.state.removeTRegisterBuffer(buffer);
  }
  drv3d_dx12::api_state.state.setStageURegisterBuffer(shader_stage, unit, buffer);
  return true;
}

bool d3d::set_render_target()
{
  CHECK_MAIN_THREAD();
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  drv3d_dx12::api_state.state.resetColorTargetsToBackBuffer();
  drv3d_dx12::api_state.state.removeDepthStencilTarget(drv3d_dx12::api_state.device.getContext());
  drv3d_dx12::api_state.state.setUpdateViewportFromRenderTarget();
  return true;
}

bool d3d::set_depth(Texture *tex, DepthAccess access)
{
  CHECK_MAIN_THREAD();
  if (!tex)
  {
    drv3d_dx12::api_state.state.removeDepthStencilTarget(drv3d_dx12::api_state.device.getContext());
  }
  else
  {
    auto texture = cast_to_texture_base(tex);
    const bool isRT = texture->isRenderTarget();
    const bool isDS = texture->getFormat().isDepth();
    if (!isRT || !isDS)
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as depth/stencil target, but lacks the necessary properties: "
             "isRT={} | isDS={}",
        (void*)texture, texture->getResName(), isRT, isDS);
      return false;
    }
    drv3d_dx12::api_state.state.setDepthStencilTarget(drv3d_dx12::api_state.device.getContext(), texture, 0, access == DepthAccess::SampledRO);
  }
  return true;
}

bool d3d::set_depth(BaseTexture *tex, int layer, DepthAccess access)
{
  CHECK_MAIN_THREAD();
  if (!tex)
  {
    drv3d_dx12::api_state.state.removeDepthStencilTarget(drv3d_dx12::api_state.device.getContext());
  }
  else
  {
    auto texture = cast_to_texture_base(tex);
    const bool isRT = texture->isRenderTarget();
    const bool isDS = texture->getFormat().isDepth();
    if (!isRT || !isDS)
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as depth/stencil target, but lacks the necessary properties: "
             "isRT={} | isDS={}",
        (void*)texture, texture->getResName(), isRT, isDS);
      return false;
    }
    drv3d_dx12::api_state.state.setDepthStencilTarget(drv3d_dx12::api_state.device.getContext(), texture, layer, access == DepthAccess::SampledRO);
  }
  return true;
}

bool d3d::set_backbuf_depth()
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.resetDepthStencilToBackBuffer(drv3d_dx12::api_state.device.getContext());
  drv3d_dx12::api_state.state.setUpdateViewportFromRenderTarget();
  return true;
}

bool d3d::set_render_target(int ri, Texture *tex, int level)
{
  CHECK_MAIN_THREAD();
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (tex)
  {
    auto texture = cast_to_texture_base(tex);
    const bool isRT = texture->isRenderTarget();
    const bool isC = texture->getFormat().isColor();
    if (!isRT || !isC)
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as color target, but lacks the necessary properties: isRT={} | "
             "isC={}",
        (void*)texture, texture->getResName(), isRT, isC);
      return false;
    }
    drv3d_dx12::api_state.state.setColorTarget(ri, texture, level, 0);
  }
  else
  {
    drv3d_dx12::api_state.state.removeColorTarget(ri);
  }

  if (0 == ri)
  {
    drv3d_dx12::api_state.state.removeDepthStencilTarget(drv3d_dx12::api_state.device.getContext());

    drv3d_dx12::api_state.state.setUpdateViewportFromRenderTarget();
  }
  return true;
}

bool d3d::set_render_target(int ri, BaseTexture *tex, int layer, int level)
{
  CHECK_MAIN_THREAD();
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (tex)
  {
    auto texture = cast_to_texture_base(tex);
    const bool isRT = texture->isRenderTarget();
    const bool isC = texture->getFormat().isColor();
    if (!isRT || !isC)
    {
      NAU_LOG_ERROR("Texture {:p} <{}> used as color target, but lacks the necessary properties: isRT={} | "
             "isC={}",
        (void*)texture, texture->getResName(), isRT, isC);
      return false;
    }
    drv3d_dx12::api_state.state.setColorTarget(ri, texture, level, layer);
  }
  else
  {
    drv3d_dx12::api_state.state.removeColorTarget(ri);
  }

  if (0 == ri)
  {
    drv3d_dx12::api_state.state.removeDepthStencilTarget(drv3d_dx12::api_state.device.getContext());

    drv3d_dx12::api_state.state.setUpdateViewportFromRenderTarget();
  }
  return true;
}

bool d3d::set_render_target(const Driver3dRenderTarget &rt)
{
  CHECK_MAIN_THREAD();
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  drv3d_dx12::api_state.state.setRenderTargets(drv3d_dx12::api_state.device.getContext(), rt);
  drv3d_dx12::api_state.state.setUpdateViewportFromRenderTarget();
  return true;
}

void d3d::get_render_target(Driver3dRenderTarget &out_rt)
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.getRenderTargets([&](auto &rts) { out_rt = rts; });
}

bool d3d::get_target_size(int &w, int &h)
{
  CHECK_MAIN_THREAD();
  auto ext = drv3d_dx12::api_state.state.getFramebufferExtent(drv3d_dx12::api_state.device.getContext());
  w = ext.width;
  h = ext.height;
  return true;
}

bool d3d::get_render_target_size(int &w, int &h, BaseTexture *rt_tex, int lev)
{
  // NOTE: no thread check needed, this function should be removed as it can be implemented in a different (more clearer) way.
  if (!rt_tex)
  {
    auto size = drv3d_dx12::api_state.device.getContext().getSwapchainExtent();
    w = size.width;
    h = size.height;
  }
  else
  {
    auto size = cast_to_texture_base(*rt_tex).getMipmapExtent(lev);
    w = size.width;
    h = size.height;
  }
  return true;
}

bool d3d::setviews(eastl::span<const Viewport> viewports)
{
  CHECK_MAIN_THREAD();
  NAU_ASSERT(viewports.size() < Viewport::MAX_VIEWPORT_COUNT);
  drv3d_dx12::api_state.state.setViewports(make_span_const(reinterpret_cast<const ViewportState *>(viewports.data()), int(viewports.size())));
  return true;
}

bool d3d::setview(int x, int y, int w, int h, float minz, float maxz)
{
  Viewport viewport = {x, y, w, h, minz, maxz};
  return setviews(make_span_const(&viewport, 1));
}

bool d3d::getview(int &x, int &y, int &w, int &h, float &minz, float &maxz)
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.getViewport(drv3d_dx12::api_state.device.getContext(), [&](auto &viewport) //
    {
      x = viewport.x;
      y = viewport.y;
      w = viewport.width;
      h = viewport.height;
      minz = viewport.minZ;
      maxz = viewport.maxZ;
    });
  return true;
}

bool d3d::setscissor(int x, int y, int w, int h)
{
  ScissorRect s = {x, y, w, h};
  return setscissors(make_span_const(&s, 1));
}

bool d3d::setscissors(eastl::span<const ScissorRect> scissorRects)
{
  CHECK_MAIN_THREAD();
  NAU_ASSERT(scissorRects.size() < Viewport::MAX_VIEWPORT_COUNT);
  D3D12_RECT rects[Viewport::MAX_VIEWPORT_COUNT];
  eastl::transform(scissorRects.begin(), scissorRects.end(), rects, [](auto &scissor) {
    return D3D12_RECT{scissor.x, scissor.y, scissor.x + scissor.w, scissor.y + scissor.h};
  });
  drv3d_dx12::api_state.state.setScissorRects(make_span_const(rects, scissorRects.size()));
  return true;
}

bool d3d::clearview(int what, nau::math::E3DCOLOR color, float z, uint32_t stencil)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  what &= ~CLEAR_DISCARD;
  if (what)
  {
    drv3d_dx12::api_state.state.clearView(drv3d_dx12::api_state.device.getContext(), what, color, z, stencil);
  }
  return true;
}

bool d3d::update_screen(bool app_active)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();

  if (!drv3d_dx12::api_state.device.getContext().wasCurrentFramePresentSubmitted())
  {
    drv3d_dx12::api_state.state.onFrameEnd(drv3d_dx12::api_state.device.getContext());
    drv3d_dx12::api_state.device.getContext().present(OutputMode::PRESENT);
    drv3d_dx12::api_state.device.processDebugLog();
  }

  if (!drv3d_dx12::api_state.device.getContext().swapchainPresentFromMainThread())
  {
    return false;
  }

#if _TARGET_PC_WIN
  if (!app_active && drv3d_dx12::api_state.windowOcclusionCheckEnabled)
    drv3d_dx12::api_state.windowState.updateWindowOcclusionState();
#else
  G_UNUSED(app_active);
#endif

  //if (::dgs_on_swap_callback) // TODO: do something about it
  //  ::dgs_on_swap_callback();
  return true;
}

bool d3d::is_window_occluded()
{
#if _TARGET_PC_WIN
  return drv3d_dx12::api_state.windowState.isWindowOccluded();
#else
  return false;
#endif
}

bool d3d::should_use_compute_for_image_processing(std::initializer_list<unsigned>) { return false; }

bool d3d::setvsrc_ex(int stream, Vbuffer *vb, int ofs, int stride_bytes)
{
  if (vb)
  {
    drv3d_dx12::api_state.state.setVertexBuffer(stream, vb, ofs, stride_bytes);
  }
  else
  {
    drv3d_dx12::api_state.state.setVertexBuffer(stream, nullptr, 0, 0);
  }
  return true;
}

bool d3d::setind(Ibuffer *ib)
{
  drv3d_dx12::api_state.state.setIndexBuffer(ib);
  return true;
}

VDECL d3d::create_vdecl(VSDTYPE *vsd)
{
  drv3d_dx12::InputLayout layout;
  layout.fromVdecl(vsd);
  return drv3d_dx12::api_state.shaderProgramDatabase.registerInputLayout(drv3d_dx12::api_state.device.getContext(), layout).get();
}

void d3d::delete_vdecl(VDECL vdecl)
{
  (void)vdecl;
  // ignore delete request, we keep it as a 'optimization'
}

bool d3d::setvdecl(VDECL vdecl)
{
  drv3d_dx12::api_state.state.setInputLayout(InputLayoutID(vdecl));
  return true;
}

namespace
{
// stolen from dx11 backend
uint32_t nprim_to_nverts(uint32_t prim_type, uint32_t numprim)
{
  // table look-up: 4 bits per entry [2b mul 2bit add]
  constexpr uint64_t table = (0x0ULL << (4 * PRIM_POINTLIST))   //*1+0 00/00
                             | (0x4ULL << (4 * PRIM_LINELIST))  //*2+0 01/00
                             | (0x1ULL << (4 * PRIM_LINESTRIP)) //*1+1 00/01
                             | (0x8ULL << (4 * PRIM_TRILIST))   //*3+0 10/00
                             | (0x2ULL << (4 * PRIM_TRISTRIP))  //*1+2 00/10
                             | (0x8ULL << (4 * PRIM_TRIFAN))    //*1+2 00/10
#if _TARGET_XBOX
                             | (0xcULL << (4 * PRIM_QUADLIST)) //*4+0 11/00
#endif
                             | (0xcULL << (4 * PRIM_4_CONTROL_POINTS)) //*4+0 11/00
    //| (0x3LL << 4*PRIM_QUADSTRIP);   //*1+3 00/11
    ;

  if(prim_type == PRIM_3_CONTROL_POINTS)
  {
      prim_type = PRIM_TRILIST;
  }

  const uint32_t code = uint32_t((table >> (prim_type * 4)) & 0x0f);
  return numprim * ((code >> 2) + 1) + (code & 3);
}
} // namespace

bool d3d::draw_base(int type, int start, int numprim, uint32_t num_instances, uint32_t start_instance)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(type);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW))
  {
    return true;
  }
  drv3d_dx12::api_state.device.getContext().draw(topology, start, nprim_to_nverts(type, numprim), start_instance, num_instances);

  //Stat3D::updateDrawPrim();
  //Stat3D::updateTriangles(numprim * num_instances);
  //if (num_instances > 1)
    //Stat3D::updateInstances(num_instances);
  return true;
}

bool d3d::drawind_base(int type, int startind, int numprim, int base_vertex, uint32_t num_instances, uint32_t start_instance)
{
  STORE_RETURN_ADDRESS();
  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(type);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW_INDEXED))
  {
    return true;
  }
  NAU_ASSERT(num_instances > 0);
  drv3d_dx12::api_state.device.getContext().drawIndexed(topology, startind, nprim_to_nverts(type, numprim), Vectormath::max(base_vertex, 0), start_instance,
    num_instances);

  //Stat3D::updateDrawPrim();
  //Stat3D::updateTriangles(numprim * num_instances);
  //if (num_instances > 1)
    //Stat3D::updateInstances(num_instances);
  return true;
}

bool d3d::draw_up(int type, int numprim, const void *ptr, int stride_bytes)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(type);
  auto primCount = nprim_to_nverts(type, numprim);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW_UP))
  {
    return true;
  }
  drv3d_dx12::api_state.device.getContext().drawUserData(topology, primCount, stride_bytes, ptr);

  //Stat3D::updateDrawPrim();
  //Stat3D::updateTriangles(numprim);
  return true;
}

bool d3d::drawind_up(int type, int minvert, int numvert, int numprim, const uint16_t *ind, const void *ptr, int stride_bytes)
{
  G_UNUSED(minvert);
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(type);
  auto primCount = nprim_to_nverts(type, numprim);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW_INDEXED_UP))
  {
    return true;
  }
  drv3d_dx12::api_state.device.getContext().drawIndexedUserData(topology, primCount, stride_bytes, ptr, numvert, ind);

  //Stat3D::updateDrawPrim();
  //Stat3D::updateTriangles(numprim);
  return true;
}

bool d3d::dispatch(uint32_t x, uint32_t y, uint32_t z, GpuPipeline gpu_pipeline)
{
  G_UNUSED(gpu_pipeline);
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  drv3d_dx12::api_state.state.flushCompute(drv3d_dx12::api_state.device.getContext());
  drv3d_dx12::api_state.device.getContext().dispatch(x, y, z);
  return true;
}

bool d3d::draw_indirect(int prim_type, Sbuffer *args, uint32_t byte_offset)
{
  STORE_RETURN_ADDRESS();
  return d3d::multi_draw_indirect(prim_type, args, 1, sizeof(D3D12_DRAW_ARGUMENTS), byte_offset);
}

bool d3d::draw_indexed_indirect(int prim_type, Sbuffer *args, uint32_t byte_offset)
{
  STORE_RETURN_ADDRESS();
  return d3d::multi_draw_indexed_indirect(prim_type, args, 1, sizeof(D3D12_DRAW_INDEXED_ARGUMENTS), byte_offset);
}

bool d3d::multi_draw_indirect(int prim_type, Sbuffer *args, uint32_t draw_count, uint32_t stride_bytes, uint32_t byte_offset)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  NAU_ASSERT(args != nullptr, "multi_draw_indirect with nullptr buffer is invalid");
  GenericBufferInterface *buffer = (GenericBufferInterface *)args;
  NAU_ASSERT(buffer->getFlags() & SBCF_MISC_DRAWINDIRECT, "multi_draw_indirect buffer is not usable as indirect buffer");

  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(prim_type);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  buffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });

  BufferResourceReferenceAndRange bufferRef //
    {get_any_buffer_ref(buffer), byte_offset, draw_count * stride_bytes};

  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW))
  {
    return true;
  }
  drv3d_dx12::api_state.device.getContext().drawIndirect(topology, draw_count, bufferRef, stride_bytes);

  //Stat3D::updateDrawPrim();
  return true;
}

bool d3d::multi_draw_indexed_indirect(int prim_type, Sbuffer *args, uint32_t draw_count, uint32_t stride_bytes, uint32_t byte_offset)
{
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  NAU_ASSERT(args != nullptr, "multi_draw_indexed_indirect with nullptr buffer is invalid");
  GenericBufferInterface *buffer = (GenericBufferInterface *)args;
  NAU_ASSERT(buffer->getFlags() & SBCF_MISC_DRAWINDIRECT, "multi_draw_indexed_indirect buffer is not usable as indirect buffer");

  D3D12_PRIMITIVE_TOPOLOGY topology = translate_primitive_topology_to_dx12(prim_type);

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  buffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });

  BufferResourceReferenceAndRange bufferRef //
    {get_any_buffer_ref(buffer), byte_offset, draw_count * stride_bytes};

  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DRAW_INDEXED))
  {
    return true;
  }
  drv3d_dx12::api_state.device.getContext().drawIndexedIndirect(topology, draw_count, bufferRef, stride_bytes);

  //Stat3D::updateDrawPrim();
  return true;
}

bool d3d::dispatch_indirect(Sbuffer *args, uint32_t byte_offset, GpuPipeline gpu_pipeline)
{
  G_UNUSED(gpu_pipeline);
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();
  NAU_ASSERT(args != nullptr, "dispatch_indirect with nullptr buffer is invalid");
  NAU_ASSERT(args->getFlags() & SBCF_BIND_UNORDERED, "dispatch_indirect buffer without SBCF_BIND_UNORDERED flag");
  GenericBufferInterface *buffer = (GenericBufferInterface *)args;
  NAU_ASSERT(buffer->getFlags() & SBCF_MISC_DRAWINDIRECT, "dispatch_indirect buffer is not usable as indirect buffer");

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  buffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });
  BufferResourceReferenceAndRange bufferRef //
    {get_any_buffer_ref(buffer), byte_offset, sizeof(D3D12_DISPATCH_ARGUMENTS)};

  drv3d_dx12::api_state.state.flushCompute(drv3d_dx12::api_state.device.getContext());
  drv3d_dx12::api_state.device.getContext().dispatchIndirect(bufferRef);
  return true;
}

void d3d::dispatch_mesh(uint32_t thread_group_x, uint32_t thread_group_y, uint32_t thread_group_z)
{
#if _TARGET_XBOXONE
  NAU_ASSERT(false, "DX12: dispatch_mesh on XB1 is unsupported");
  G_UNUSED(thread_group_x);
  G_UNUSED(thread_group_y);
  G_UNUSED(thread_group_z);
#else
  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};

  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DISPATCH_MESH))
  {
    return;
  }

  drv3d_dx12::api_state.device.getContext().dispatchMesh(thread_group_x, thread_group_y, thread_group_z);
#endif
}

void d3d::dispatch_mesh_indirect(Sbuffer *args, uint32_t dispatch_count, uint32_t stride_bytes, uint32_t byte_offset)
{
#if _TARGET_XBOXONE
  NAU_ASSERT(false, "DX12: dispatch_mesh_indirect on XB1 is unsupported");
  G_UNUSED(args);
  G_UNUSED(dispatch_count);
  G_UNUSED(stride_bytes);
  G_UNUSED(byte_offset);
#else
  NAU_ASSERT_RETURN(args, , "DX12: dispatch_mesh args parameter can not be null");
  GenericBufferInterface *buffer = (GenericBufferInterface *)args;
  NAU_ASSERT_RETURN(buffer->getFlags() & SBCF_MISC_DRAWINDIRECT, ,
    "DX12: dispatch_mesh_indirect buffer is not usable as indirect buffer");

  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};

  buffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });
  BufferResourceReferenceAndRange bufferRef{get_any_buffer_ref(buffer), byte_offset, stride_bytes * dispatch_count};

  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DISPATCH_MESH))
  {
    return;
  }

  drv3d_dx12::api_state.device.getContext().dispatchMeshIndirect(bufferRef, stride_bytes, {}, dispatch_count);
#endif
}

void d3d::dispatch_mesh_indirect_count(Sbuffer *args, uint32_t args_stride_bytes, uint32_t args_byte_offset, Sbuffer *count,
  uint32_t count_byte_offset, uint32_t max_count)
{
#if _TARGET_XBOXONE
  NAU_ASSERT(false, "DX12: dispatch_mesh_indirect_count on XB1 is unsupported");
  G_UNUSED(args);
  G_UNUSED(args_stride_bytes);
  G_UNUSED(args_byte_offset);
  G_UNUSED(count);
  G_UNUSED(count_byte_offset);
  G_UNUSED(max_count);
#else
  NAU_ASSERT_RETURN(args, , "DX12: dispatch_mesh args parameter can not be null");
  NAU_ASSERT_RETURN(count, , "DX12: dispatch_mesh count parameter can not be null");
  GenericBufferInterface *argsBuffer = (GenericBufferInterface *)args;
  GenericBufferInterface *countBuffer = (GenericBufferInterface *)count;
  NAU_ASSERT_RETURN(argsBuffer->getFlags() & SBCF_MISC_DRAWINDIRECT, ,
    "DX12: dispatch_mesh_indirect_count args buffer is not usable as indirect buffer");
  NAU_ASSERT_RETURN(countBuffer->getFlags() & SBCF_MISC_DRAWINDIRECT, ,
    "DX12: dispatch_mesh_indirect_count count buffer is not usable as indirect buffer");

  STORE_RETURN_ADDRESS();
  CHECK_MAIN_THREAD();

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};

  argsBuffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });
  BufferResourceReferenceAndRange argsBufferRef{get_any_buffer_ref(argsBuffer), args_byte_offset, args_stride_bytes * max_count};

  countBuffer->updateDeviceBuffer([](auto &buf) { buf.resourceId.markUsedAsIndirectBuffer(); });
  BufferResourceReferenceAndRange countBufferRef{get_any_buffer_ref(countBuffer), count_byte_offset, sizeof(uint32_t)};

  if (!drv3d_dx12::api_state.state.flushGraphics(drv3d_dx12::api_state.device, FrontendState::GraphicsMode::DISPATCH_MESH))
  {
    return;
  }

  drv3d_dx12::api_state.device.getContext().dispatchMeshIndirect(argsBufferRef, args_stride_bytes, countBufferRef, max_count);
#endif
}

GPUFENCEHANDLE d3d::insert_fence(GpuPipeline /*gpu_pipeline*/) { return BAD_GPUFENCEHANDLE; }
void d3d::insert_wait_on_fence(GPUFENCEHANDLE & /*fence*/, GpuPipeline /*gpu_pipeline*/) {}

bool d3d::set_const_buffer(uint32_t stage, uint32_t unit, Sbuffer *buffer, uint32_t consts_offset, uint32_t consts_size)
{
  NAU_ASSERT((nullptr == buffer) || (buffer->getFlags() & SBCF_BIND_CONSTANT));

  drv3d_dx12::api_state.state.setStageBRegisterBuffer(stage, unit, buffer, consts_offset, consts_size);
  return true;
}

bool d3d::setantialias(int aa_type) { return aa_type == 0; }

int d3d::getantialias() { return 0; }

bool d3d::setstencil(uint32_t ref)
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.setStencilReference(ref);
  return true;
}

bool d3d::setwire(bool wire)
{
  CHECK_MAIN_THREAD();
  drv3d_dx12::api_state.state.setPolygonLine(wire);
  return false;
}

bool d3d::setgamma(float power)
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().setGamma(power);
  return true;
}

bool d3d::set_msaa_pass() { return true; }

bool d3d::set_depth_resolve() { return true; }

bool d3d::isVcolRgba() { return true; }

float d3d::get_screen_aspect_ratio() { return drv3d_dx12::api_state.windowState.settings.aspect; }

void d3d::change_screen_aspect_ratio(float) {}

void *d3d::fast_capture_screen(int &w, int &h, int &stride_bytes, int &format)
{
  auto sz = drv3d_dx12::api_state.device.getContext().getSwapchainExtent();
  format = CAPFMT_X8R8G8B8;
  w = sz.width;
  h = sz.height;
  void *ptr = nullptr;
  get_backbuffer_tex()->lockimg(&ptr, stride_bytes, 0, TEXLOCK_READ);
  return ptr;
}

void d3d::end_fast_capture_screen() { get_backbuffer_tex()->unlockimg(); }

TexPixel32 *d3d::capture_screen(int &w, int &h, int &stride_bytes)
{
  int fmt;
  void *ptr = fast_capture_screen(w, h, stride_bytes, fmt);
  drv3d_dx12::api_state.screenCaptureBuffer.resize(w * h * sizeof(TexPixel32));
  if (CAPFMT_X8R8G8B8 == fmt)
  {
    // if device was reseted, then ptr is null, just return a black image then, to be safe
    if (ptr)
      memcpy(drv3d_dx12::api_state.screenCaptureBuffer.data(), ptr, drv3d_dx12::api_state.screenCaptureBuffer.size());
    else
      memset(drv3d_dx12::api_state.screenCaptureBuffer.data(), 0, drv3d_dx12::api_state.screenCaptureBuffer.size());
  }
  else
    NAU_ASSERT(false);
  end_fast_capture_screen();
  return (TexPixel32 *)drv3d_dx12::api_state.screenCaptureBuffer.data();
}

void d3d::release_capture_buffer() { drv3d_dx12::api_state.screenCaptureBuffer.clear(); }

void d3d::get_screen_size(int &w, int &h)
{
  auto sz = drv3d_dx12::api_state.device.getContext().getSwapchainExtent();
  w = sz.width;
  h = sz.height;
}

void d3d::set_screen_size(unsigned int w, unsigned int h)
{
  drv3d_dx12::api_state.device.getContext().changeSwapchainExtents(Extent2D{w, h});
}

void d3d::set_screen_size(unsigned int w, unsigned int h, SWAPID swapID)
{
    if (swapID == DEFAULT_SWAPID)
    {
        drv3d_dx12::api_state.device.getContext().changeSwapchainExtents(Extent2D{w, h});
        return;
    }
    drv3d_dx12::api_state.device.getContext().changeAdditionalSwapchainExtents(Extent2D{w, h}, swapID);
}

bool d3d::set_srgb_backbuffer_write(bool on) { return drv3d_dx12::api_state.state.setBackbufferSrgb(on); }

void d3d::beginEvent(const char *name)
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().pushEvent(name);

  d3dhang::hang_if_requested(name);
}

void d3d::endEvent()
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().popEvent();
}

bool d3d::set_depth_bounds(float zmin, float zmax)
{
  zmin = Vectormath::clamp(zmin, 0.f, 1.f);
  zmax = Vectormath::clamp(zmax, zmin, 1.f);
  drv3d_dx12::api_state.state.setDepthBoundsRange(zmin, zmax);
  return true;
}

bool d3d::supports_depth_bounds() { return drv3d_dx12::api_state.driverDesc.caps.hasDepthBoundsTest; }

bool d3d::begin_survey(int name)
{
  STORE_RETURN_ADDRESS();
  if (-1 == name)
    return false;

  drv3d_dx12::api_state.device.getContext().beginSurvey(name);
  return true;
}

void d3d::end_survey(int name)
{
  STORE_RETURN_ADDRESS();
  if (-1 != name)
    drv3d_dx12::api_state.device.getContext().endSurvey(name);
}

int d3d::create_predicate() { return drv3d_dx12::api_state.device.createPredicate(); }

void d3d::free_predicate(int name)
{
  if (-1 != name)
    drv3d_dx12::api_state.device.deletePredicate(name);
}

void d3d::begin_conditional_render(int name)
{
  STORE_RETURN_ADDRESS();
  if (-1 != name)
    drv3d_dx12::api_state.device.getContext().beginConditionalRender(name);
}

void d3d::end_conditional_render(int name)
{
  STORE_RETURN_ADDRESS();
  if (-1 != name)
    drv3d_dx12::api_state.device.getContext().endConditionalRender();
}

bool d3d::get_vrr_supported() { return drv3d_dx12::api_state.device.getContext().isVrrSupported(); }

bool d3d::get_vsync_enabled() { return drv3d_dx12::api_state.device.getContext().isVsyncOn(); }

bool d3d::enable_vsync(bool enable)
{
  STORE_RETURN_ADDRESS();
  PresentationMode mode;
  if (enable)
    mode = PresentationMode::VSYNCED;
  else if (::dgs_get_settings()->getBlockByNameEx("video")->getBool("adaptive_vsync", false))
    mode = PresentationMode::CONDITIONAL_VSYNCED;
  else
    mode = PresentationMode::UNSYNCED;

  drv3d_dx12::api_state.device.getContext().changePresentMode(mode);
  return true;
}

#if _TARGET_PC_WIN
bool d3d::pcwin32::set_capture_full_frame_buffer(bool /*ison*/) { return false; }

void d3d::pcwin32::set_present_wnd(void *) {}
#endif

d3d::EventQuery *d3d::create_event_query()
{
  // A fence is just a uint64_t that records the progress of the current work item when issued
  // Check for completion is to compare its current value with completed progress counter and if
  // the completed progress is equal or larger than the sored value, the fence has passed.
  // Initial progress of 0 will always pass, as first progress pushed to GPU will be 1 and initially is 0.
  auto event = eastl::make_unique<uint64_t>(0);
  return (d3d::EventQuery *)(event.release());
}

void d3d::release_event_query(d3d::EventQuery *fence) { eastl::unique_ptr<uint64_t> ptr{reinterpret_cast<uint64_t *>(fence)}; }

bool d3d::issue_event_query(d3d::EventQuery *fence)
{
  if (fence)
  {
    // We just record the progress of the current work item to the fence. When everything has
    // been executed and cleaned up of that work item, then the progress advances to that work
    // item.
    *reinterpret_cast<uint64_t *>(fence) = drv3d_dx12::api_state.device.getContext().getRecordingFenceProgress();
  }
  return true;
}

bool d3d::get_event_query_status(d3d::EventQuery *fence, bool flush)
{
  STORE_RETURN_ADDRESS();

  if (fence)
  {
    if (flush)
    {
      if (*reinterpret_cast<uint64_t *>(fence) == drv3d_dx12::api_state.device.getContext().getRecordingFenceProgress())
      {
        drv3d_dx12::api_state.device.getContext().flushDraws();
      }
      // Super simple, just compare expected progress of the fence with the currently completed progress.
      if (*reinterpret_cast<uint64_t *>(fence) > drv3d_dx12::api_state.device.getContext().getCompletedFenceProgress())
      {
        // Check if the GPU made any progress and update current progress counter.
        drv3d_dx12::api_state.device.getContext().updateFenceProgress();
      }
    }
    return *reinterpret_cast<uint64_t *>(fence) <= drv3d_dx12::api_state.device.getContext().getCompletedFenceProgress();
  }
  return true;
}

#if _TARGET_PC
void d3d::get_video_modes_list(eastl::vector<nau::string> &list) { drv3d_dx12::api_state.device.enumerateDisplayModes(list); }
#endif

Vbuffer* d3d::create_vb(int size, int flg, const char8_t *name)
{
  validate_sbuffer_flags(flg | SBCF_BIND_VERTEX, name);
  return drv3d_dx12::api_state.device.newBufferObject(0, size, flg | SBCF_BIND_VERTEX, 0, name);
}

Ibuffer* d3d::create_ib(int size, int flg, const char8_t *stat_name)
{
  validate_sbuffer_flags(flg | SBCF_BIND_INDEX, stat_name);
  return drv3d_dx12::api_state.device.newBufferObject(0, size, flg | SBCF_BIND_INDEX, 0, stat_name);
}

Sbuffer* d3d::create_cb(int size, int flg, const char8_t* stat_name)
{
    validate_sbuffer_flags(flg | SBCF_BIND_CONSTANT, stat_name);
    return drv3d_dx12::api_state.device.newBufferObject(0, size, flg | SBCF_BIND_CONSTANT, 0, stat_name);
}

Vbuffer *d3d::create_sbuffer(int struct_size, int elements, unsigned flags, unsigned format, const char8_t *name)
{
  validate_sbuffer_flags(flags, name);
  return drv3d_dx12::api_state.device.newBufferObject(struct_size, elements, flags, format, name);
}

void drv3d_dx12::notify_delete(Sbuffer *buffer) { drv3d_dx12::api_state.state.notifyDelete(buffer); }

void drv3d_dx12::notify_discard(Sbuffer *buffer, bool check_vb, bool check_const, bool check_tex, bool check_storage)
{
  drv3d_dx12::api_state.state.notifyDiscard(buffer, check_vb, check_const, check_tex, check_storage);
}

Texture *d3d::get_backbuffer_tex() { return drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture(); }

Texture *d3d::get_secondary_backbuffer_tex() { return drv3d_dx12::api_state.device.getContext().getSwapchainSecondaryColorTexture(); }

Texture *d3d::get_backbuffer_tex_depth() { return drv3d_dx12::api_state.device.getContext().getSwapchainDepthStencilTextureAnySize(); }

#if D3D_HAS_RAY_TRACING
RaytraceBottomAccelerationStructure *d3d::create_raytrace_bottom_acceleration_structure(RaytraceGeometryDescription *desc,
  uint32_t count, RaytraceBuildFlags flags)
{
  STORE_RETURN_ADDRESS();
  return (RaytraceBottomAccelerationStructure *)drv3d_dx12::api_state.device.createRaytraceAccelerationStructure(desc, count, flags);
}

void d3d::delete_raytrace_bottom_acceleration_structure(RaytraceBottomAccelerationStructure *as)
{
  if (as)
    drv3d_dx12::api_state.device.getContext().deleteRaytraceBottomAccelerationStructure(as);
}

RaytraceTopAccelerationStructure *d3d::create_raytrace_top_acceleration_structure(uint32_t elements, RaytraceBuildFlags flags)
{
  STORE_RETURN_ADDRESS();
  return (RaytraceTopAccelerationStructure *)drv3d_dx12::api_state.device.createRaytraceAccelerationStructure(elements, flags);
}

void d3d::delete_raytrace_top_acceleration_structure(RaytraceTopAccelerationStructure *as)
{
  if (as)
    drv3d_dx12::api_state.device.getContext().deleteRaytraceTopAccelerationStructure(as);
}

void d3d::set_top_acceleration_structure(ShaderStage stage, uint32_t index, RaytraceTopAccelerationStructure *as)
{
  drv3d_dx12::api_state.state.setStageTRegisterRaytraceAccelerationStructure(stage, index, (RaytraceAccelerationStructure *)as);
}

PROGRAM d3d::create_raytrace_program(const int *shaders, uint32_t shader_count, const RaytraceShaderGroup *shader_groups,
  uint32_t shader_group_count, uint32_t max_recursion_depth)
{
  STORE_RETURN_ADDRESS();
  return drv3d_dx12::api_state.shaderProgramDatabase
    .newRaytraceProgram(drv3d_dx12::api_state.device.getContext(), (ShaderID *)shaders, shader_count, shader_groups, shader_group_count,
      max_recursion_depth)
    .exportValue();
}

void d3d::trace_rays(Sbuffer *ray_gen_table, uint32_t ray_gen_offset, Sbuffer *miss_table, uint32_t miss_offset, uint32_t miss_stride,
  Sbuffer *hit_table, uint32_t hit_offset, uint32_t hit_stride, Sbuffer *callable_table, uint32_t callable_offset,
  uint32_t callable_stride, uint32_t width, uint32_t height, uint32_t depth)
{
  STORE_RETURN_ADDRESS();
  auto rayGenBuf = (GenericBufferInterface *)ray_gen_table;
  auto missBuf = (GenericBufferInterface *)miss_table;
  auto hitBuf = (GenericBufferInterface *)hit_table;
  auto callableBuf = (GenericBufferInterface *)callable_table;

  BufferResourceReferenceAndRange reyGenDeviceBuf{get_any_buffer_ref(rayGenBuf), ray_gen_offset};
  BufferResourceReferenceAndRange missDeviceBuf{get_any_buffer_ref(missBuf), miss_offset};
  BufferResourceReferenceAndRange hitDeviceBuf{get_any_buffer_ref(hitBuf), hit_offset};
  auto callableDeviceBuf = callable_table ? BufferResourceReferenceAndRange{get_any_buffer_ref(callableBuf), callable_offset}
                                          : BufferResourceReferenceAndRange{};

  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};

  drv3d_dx12::api_state.state.flushRaytrace(drv3d_dx12::api_state.device.getContext());
  drv3d_dx12::api_state.device.getContext().traceRays(reyGenDeviceBuf, missDeviceBuf, miss_stride, hitDeviceBuf, hit_stride, callableDeviceBuf,
    callable_stride, width, height, depth);
}

void d3d::build_bottom_acceleration_structure(RaytraceBottomAccelerationStructure *as, RaytraceGeometryDescription *desc,
  uint32_t count, RaytraceBuildFlags flags, bool update)
{
  STORE_RETURN_ADDRESS();
  if (as)
  {
    drv3d_dx12::api_state.device.getContext().raytraceBuildBottomAccelerationStructure(as, desc, count, flags, update,
      drv3d_dx12::api_state.device.getRaytraceScratchBuffer());
  }
}

void d3d::build_top_acceleration_structure(RaytraceTopAccelerationStructure *as, Sbuffer *index_buffer, uint32_t index_count,
  RaytraceBuildFlags flags, bool update)
{
  STORE_RETURN_ADDRESS();
  if (as)
  {
    auto buf = (GenericBufferInterface *)index_buffer;
    drv3d_dx12::api_state.device.getContext().raytraceBuildTopAccelerationStructure(as, get_any_buffer_ref(buf), index_count, flags, update,
      drv3d_dx12::api_state.device.getRaytraceScratchBuffer());
  }
}

void d3d::copy_raytrace_shader_handle_to_memory(PROGRAM prog, uint32_t first_group, uint32_t group_count, uint32_t size,
  Sbuffer *buffer, uint32_t offset)
{
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().copyRaytraceShaderGroupHandlesToMemory(ProgramID::importValue(prog), first_group, group_count, size,
    ((GenericBufferInterface *)buffer)->getDeviceBuffer(), offset);
}

void d3d::write_raytrace_index_entries_to_memory(uint32_t count, const RaytraceGeometryInstanceDescription *desc, void *ptr)
{
  static_assert(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) == sizeof(RaytraceGeometryInstanceDescription));
  memcpy(ptr, desc, count * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
  auto tptr = reinterpret_cast<D3D12_RAYTRACING_INSTANCE_DESC *>(ptr);
  for (uint32_t i = 0; i < count; ++i)
  {
    tptr[i].AccelerationStructure = desc[i].accelerationStructure
                                      ? ((RaytraceAccelerationStructure *)desc[i].accelerationStructure)->getGPUPointer()
                                      : (D3D12_GPU_VIRTUAL_ADDRESS)0;
  }
}

int d3d::create_raytrace_shader(RaytraceShaderType type, const uint32_t *data, uint32_t data_size)
{
  G_UNUSED(type);
  G_UNUSED(data);
  G_UNUSED(data_size);
#if 0
  // TODO: No support yet
  VkShaderStageFlagBits stg;
  switch (type)
  {
    case RaytraceShaderType::RAYGEN:
      stg = VK_SHADER_STAGE_RAYGEN_BIT_NV;
      break;
    case RaytraceShaderType::ANY_HIT:
      stg = VK_SHADER_STAGE_ANY_HIT_BIT_NV;
      break;
    case RaytraceShaderType::CLOSEST_HIT:
      stg = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV;
      break;
    case RaytraceShaderType::MISS:
      stg = VK_SHADER_STAGE_MISS_BIT_NV;
      break;
    case RaytraceShaderType::INTERSECTION:
      stg = VK_SHADER_STAGE_INTERSECTION_BIT_NV;
      break;
    case RaytraceShaderType::CALLABLE:
      stg = VK_SHADER_STAGE_CALLABLE_BIT_NV;
      break;
    default:
      return -1;
  }
  return create_shader_for_stage(data, stg, data_size);
#else
  return 0;
#endif
}

void d3d::delete_raytrace_shader(int shader)
{
  G_UNUSED(shader);
#if 0
  drv3d_dx12::api_state.shaderProgramDatabase.deleteShader(drv3d_dx12::api_state.device.getContext(), ShaderID::importValue(shader));
#endif
}
#endif

#if _TARGET_PC_WIN
APISupport get_dx12_support_status(bool use_any_device = true)
{
  const nau::DataBlock &dxCfg = *::dgs_get_settings()->getBlockByNameEx("dx12");
  const nau::DataBlock *gpuCfg = dxCfg.getBlockByName("gpuPreferences");

  if (!gpuCfg && !use_any_device)
  {
    return APISupport::NO_DEVICE_FOUND;
  }

  OSVERSIONINFOEXW osvi{};
  //get_version_ex(&osvi);
  GetVersionExW((OSVERSIONINFOW *)&osvi);

  if (osvi.dwMajorVersion < 10)
  {
    NAU_LOG_DEBUG("DX12: Unsupported OS version {}", osvi.dwMajorVersion);
    return APISupport::NO_DEVICE_FOUND;
  }

  D3D12SDKVersion = dxCfg.getInt("sdkVersion", D3D12SDKVersion);
  D3D12SDKPath = dxCfg.getStr("sdkPath", D3D12SDKPath);

  Direct3D12Enviroment d3d12Env;
  if (!d3d12Env.setup())
  {
    return APISupport::NO_DEVICE_FOUND;
  }

  ComPtr<DXGIFactory> dxgi14;
  if (auto hr = d3d12Env.CreateDXGIFactory2(0, COM_ARGS(&dxgi14)); FAILED(hr))
  {
    NAU_LOG_DEBUG("DX12: CreateDXGIFactory2 for DXGI 1.4 interface failed, {}", dxgi_error_code_to_string(hr));
    return APISupport::NO_DEVICE_FOUND;
  }

  auto featureLevel = make_feature_level(dxCfg.getInt("FeatureLevelMajor", min_major_feature_level),
    dxCfg.getInt("FeatureLevelMinor", min_minor_feature_level));

  APISupport apiSupport = APISupport::NO_DEVICE_FOUND;
  ComPtr<IDXGIFactory6> dxgi6;
  ComPtr<IDXGIAdapter1> adapter1;
  if (SUCCEEDED(dxgi14.As(&dxgi6)))
  {
    NAU_LOG_DEBUG("DX12: Scanning for viable devices in performance order...");

    UINT index = 0;
    if (dxgi6->EnumAdapterByGpuPreference(index, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, COM_ARGS(&adapter1)) != DXGI_ERROR_NOT_FOUND)
    {
      apiSupport = check_adapter(d3d12Env, featureLevel, gpuCfg, use_any_device, adapter1);
      if (apiSupport == APISupport::FULL_SUPPORT)
        return apiSupport;
    }
  }
  else
  {
    NAU_LOG_DEBUG("DX12: Scanning for viable devices...");

    eastl::vector<Device::AdapterInfo> adapterList;
    for (UINT index = 0; dxgi14->EnumAdapters1(index, &adapter1) != DXGI_ERROR_NOT_FOUND; ++index)
      check_and_add_adapter(d3d12Env, featureLevel, nullptr, adapter1, adapterList);

    if (!adapterList.empty())
    {
      if (!gpuCfg)
        return APISupport::FULL_SUPPORT;

      sort_adapters_by_perf(adapterList);

      if (!use_any_device && !is_prefered_device(*gpuCfg, adapterList.front().info.VendorId, adapterList.front().info.DeviceId))
      {
        NAU_LOG_DEBUG("DX12: Rejected, because the driver mode is \"auto\" and the device isn't a prefered one");
        return APISupport::NO_DEVICE_FOUND;
      }

      auto version = get_driver_version_from_registry(adapterList.front().info.AdapterLuid);
      apiSupport = check_driver_version(adapterList.front().info, version, *gpuCfg);
      switch (apiSupport)
      {
        case APISupport::OUTDATED_DRIVER:
          NAU_LOG_DEBUG("DX12: Rejected, because inadequate gpu driver, the {}.{}.{}.{} is outdated", version.productVersion,
            version.majorVersion, version.minorVersion, version.buildNumber);
          break;
        case APISupport::BLACKLISTED_DRIVER:
          NAU_LOG_DEBUG("DX12: Rejected, because inadequate gpu driver, the {}.{}.{}.{} is blacklisted", version.productVersion,
            version.majorVersion, version.minorVersion, version.buildNumber);
          break;
        default: return apiSupport;
      }
    }
  }

  NAU_LOG_DEBUG("DX12: No viable device found, DX12 is unavailable!");
  return apiSupport;
}
#endif

bool d3d::set_immediate_const(unsigned stage, const uint32_t *data, unsigned num_words)
{
  STORE_RETURN_ADDRESS();
  // immediate const directly bypasses all state tracking, only stateful command buffer
  // will check for changes and only send updates out if anything did change
  NAU_ASSERT(num_words <= MAX_ROOT_CONSTANTS);
  NAU_ASSERT(data || !num_words);
  auto &ctx = drv3d_dx12::api_state.device.getContext();

#define PUSH_WORDS(target)                                             \
  if (num_words)                                                       \
  {                                                                    \
    for (uint32_t i = 0; i < num_words && i < MAX_ROOT_CONSTANTS; ++i) \
    {                                                                  \
      ctx.target(i, data[i]);                                          \
    }                                                                  \
  }                                                                    \
  else                                                                 \
  {                                                                    \
    for (uint32_t i = 0; i < MAX_ROOT_CONSTANTS; ++i)                  \
    {                                                                  \
      ctx.target(i, 0);                                                \
    }                                                                  \
  }

  if (STAGE_CS == stage)
  {
    PUSH_WORDS(setComputeRootConstant);
  }
  else if (STAGE_VS == stage)
  {
    PUSH_WORDS(setVertexRootConstant);
  }
  else if (STAGE_PS == stage)
  {
    PUSH_WORDS(setPixelRootConstant);
  }
#if D3D_HAS_RAY_TRACING
  else if (STAGE_RAYTRACE == stage)
  {
    PUSH_WORDS(setRaytraceRootConstant);
  }
#endif

#undef PUSH_WORDS
  return true;
}

shaders::DriverRenderStateId d3d::create_render_state(const shaders::RenderState &state)
{
  STORE_RETURN_ADDRESS();
  auto id = drv3d_dx12::api_state.device.getRenderStateSystem().registerState(drv3d_dx12::api_state.device.getContext(), state);
  return shaders::DriverRenderStateId{id};
}

bool d3d::set_render_state(shaders::DriverRenderStateId state_id)
{
  drv3d_dx12::api_state.state.setDynamicAndStaticState(
    drv3d_dx12::api_state.device.getRenderStateSystem().getDynamicAndStaticState(static_cast<uint32_t>(state_id)));
  return true;
}

void d3d::clear_render_states()
{
  // do nothing
}

#if _TARGET_XBOXONE
void d3d::set_variable_rate_shading(unsigned, unsigned, VariableRateShadingCombiner, VariableRateShadingCombiner) {}

void d3d::set_variable_rate_shading_texture(BaseTexture *) {}
#else
void d3d::set_variable_rate_shading(unsigned rate_x, unsigned rate_y,
  VariableRateShadingCombiner vertex_combiner /*= VariableRateShadingCombiner::VRS_PASSTHROUGH*/,
  VariableRateShadingCombiner pixel_combiner /*= VariableRateShadingCombiner::VRS_PASSTHROUGH*/)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasVariableRateShading, , "Variable Shading Rate is unsupported on this device");

  drv3d_dx12::api_state.state.setVariableShadingRate(make_shading_rate_from_int_values(rate_x, rate_y),
    map_shading_rate_combiner_to_dx12(vertex_combiner), map_shading_rate_combiner_to_dx12(pixel_combiner));
}
void d3d::set_variable_rate_shading_texture(BaseTexture *rate_texture)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasVariableRateShadingTexture, , "Can not use shading rate texture on this device");
  drv3d_dx12::api_state.state.setVariableShadingRateTexture(rate_texture);
}
#endif

namespace
{
// does just rudimentary barrier validation, nothing state dependent. The backend will do this
// as it has to do some state house keeping anyway.
// NOTE: we check here states that we don't need to, so that we have consisted checks on any API
//       that we implement this on.
void validate_buffer_barrier(ResourceBarrier barrier, GpuPipeline q)
{
  // noop to turn off uav flush check
  if (RB_NONE == barrier)
    return;

  if (RB_NONE != ((RB_ALIAS_FROM | RB_ALIAS_TO | RB_ALIAS_TO_AND_DISCARD | RB_ALIAS_ALL) & barrier))
  {
    // flush is a special case and can end up in buffer enum, with the single barrier parameter constructor.
    if (RB_ALIAS_ALL != barrier)
    {
      NAU_LOG_ERROR("DX12: Aliasing barriers are not needed for buffers");
    }
    return;
  }

  if (RB_NONE != (RB_FLUSH_UAV & barrier))
  {
    if (RB_NONE == ((RB_STAGE_VERTEX | RB_STAGE_PIXEL | RB_STAGE_COMPUTE | RB_STAGE_RAYTRACE) & barrier))
    {
      NAU_LOG_ERROR("DX12: A UAV barrier requires a destination stage");
    }
    if (RB_NONE == ((RB_SOURCE_STAGE_VERTEX | RB_SOURCE_STAGE_PIXEL | RB_SOURCE_STAGE_COMPUTE | RB_SOURCE_STAGE_RAYTRACE) & barrier))
    {
      NAU_LOG_ERROR("DX12: A UAV barrier requires a source stage");
    }
    if (RB_NONE != ((~(RB_FLUSH_UAV | RB_STAGE_VERTEX | RB_STAGE_PIXEL | RB_STAGE_COMPUTE | RB_STAGE_RAYTRACE |
                       RB_SOURCE_STAGE_VERTEX | RB_SOURCE_STAGE_PIXEL | RB_SOURCE_STAGE_COMPUTE | RB_SOURCE_STAGE_RAYTRACE)) &
                     barrier))
    {
      NAU_LOG_ERROR("DX12: A UAV barrier can not combined with any other transition");
    }
  }
  if (RB_NONE != (RB_FLAG_DONT_PRESERVE_CONTENT & barrier))
  {
    NAU_LOG_ERROR("DX12: Buffers do not support destructive transition");
  }
  if (RB_NONE != ((RB_FLAG_SPLIT_BARRIER_BEGIN | RB_FLAG_SPLIT_BARRIER_END) & barrier))
  {
    NAU_LOG_ERROR("DX12: Can not request a split barrier for a buffer barrier");
  }
  if (RB_NONE != ((RB_RO_BLIT_SOURCE | RB_RW_BLIT_DEST) & barrier))
  {
    NAU_LOG_ERROR("DX12: A buffer can neither be a blit source nor destination");
  }
  if (RB_NONE != (RB_RO_VARIABLE_RATE_SHADING_TEXTURE & barrier))
  {
    NAU_LOG_ERROR("DX12: A buffer can not be used as variable rate shading texture");
  }
  if (RB_NONE != (RB_RW_RENDER_TARGET & barrier))
  {
    NAU_LOG_ERROR("DX12: A buffer can not be used as render target");
  }
  if (RB_NONE != (RB_RW_UAV & barrier))
  {
    if (RB_NONE != ((RB_RW_COPY_DEST | RB_RO_SRV | RB_RO_CONSTANT_BUFFER | RB_RO_VERTEX_BUFFER | RB_RO_INDEX_BUFFER |
                      RB_RO_INDIRECT_BUFFER | RB_RO_COPY_SOURCE | RB_RO_RAYTRACE_ACCELERATION_BUILD_SOURCE) &
                     barrier))
    {
      NAU_LOG_ERROR("DX12: A write state can not be combined with any other states");
    }
  }
  if (RB_NONE != (RB_RW_COPY_DEST & barrier))
  {
    if (RB_NONE != ((RB_RW_UAV | RB_RO_SRV | RB_RO_CONSTANT_BUFFER | RB_RO_VERTEX_BUFFER | RB_RO_INDEX_BUFFER | RB_RO_INDIRECT_BUFFER |
                      RB_RO_COPY_SOURCE | RB_RO_RAYTRACE_ACCELERATION_BUILD_SOURCE) &
                     barrier))
    {
      NAU_LOG_ERROR("DX12: A write state can not be combined with any other states");
    }
  }
  if (RB_NONE == ((RB_RW_COPY_DEST | RB_RO_COPY_SOURCE | RB_RO_INDIRECT_BUFFER | RB_RO_INDEX_BUFFER | RB_RO_VERTEX_BUFFER) & barrier))
  {
    // shader related state require a stage to be defined, where they are used.
    if (RB_NONE == ((RB_STAGE_VERTEX | RB_STAGE_PIXEL | RB_STAGE_COMPUTE | RB_STAGE_RAYTRACE) & barrier))
    {
      NAU_LOG_ERROR("DX12: Transitioned state requires a target stage");
    }
  }
  if (GpuPipeline::GRAPHICS == q)
  {
    // nothing specific here (yet?)
  }
  else if (GpuPipeline::ASYNC_COMPUTE == q)
  {
    // compute queue, can only handle compute and ray trace related shaders
    if (RB_NONE != (RB_STAGE_VERTEX & barrier))
    {
      NAU_LOG_ERROR("DX12: Can not target vertex shader stage on compute queue");
    }
    if (RB_NONE != (RB_STAGE_PIXEL & barrier))
    {
      NAU_LOG_ERROR("DX12: Can not target pixel shader stage on compute queue");
    }
    if (RB_NONE != (RB_STAGE_RAYTRACE & barrier))
    {
      // as soon as the ray trace interface has the ability to target the compute
      // queue we can remove this
      NAU_LOG_ERROR("DX12: Can not target ray trace shader stage on compute queue (yet!)");
    }
    if (RB_NONE != (RB_RO_VERTEX_BUFFER & barrier))
    {
      NAU_LOG_ERROR("DX12: Can not transition to vertex buffer on compute queue");
    }
    if (RB_NONE != (RB_RO_INDEX_BUFFER & barrier))
    {
      NAU_LOG_ERROR("DX12: Can not transition to index buffer on compute queue");
    }
  }
}
// Returns false if the barrier has to be skipped
bool validate_texture_barrier(ResourceBarrier barrier, bool is_depth, bool is_rt, bool is_uav, GpuPipeline q)
{
  // noop to turn off uav flush check
  if (RB_NONE == barrier)
    return true;

  bool isOkayToExecute = true;
  auto reportError = [&isOkayToExecute](auto value) {
    NAU_LOG_ERROR(value);
    isOkayToExecute = false;
  };

  if (RB_NONE != ((RB_ALIAS_FROM | RB_ALIAS_TO | RB_ALIAS_TO_AND_DISCARD | RB_ALIAS_ALL) & barrier))
  {
    if ((RB_ALIAS_FROM != barrier) && (RB_ALIAS_TO != barrier) && (RB_ALIAS_TO_AND_DISCARD != barrier) && (RB_ALIAS_ALL != barrier))
    {
      reportError("DX12: Aliasing barriers can only RB_ALIAS_FROM, RB_ALIAS_TO, "
                  "RB_ALIAS_TO_AND_DISCARD or RB_ALIAS_ALL");
    }
  }

  if (!is_uav)
  {
    if (RB_NONE != (RB_FLUSH_UAV & barrier))
    {
      reportError("DX12: RB_FLUSH_UAV barrier requires a resource with the TEXCF_UNORDERED "
                  "creation flag to be set");
    }
    if (RB_NONE != (RB_RW_UAV & barrier))
    {
      reportError("DX12: RB_RW_UAV barrier requires a resource with the TEXCF_UNORDERED creation "
                  "flag to be set");
    }
  }

  if (!is_rt)
  {
    if (RB_NONE != (RB_RW_RENDER_TARGET & barrier))
    {
      reportError("DX12: RB_RW_RENDER_TARGET barrier requires a resource with the TEXCF_RTARGET "
                  "creation flag to be set");
    }
  }

  if (!is_uav && !is_rt)
  {
    reportError("DX12: Barriers for textures without TEXCF_RTARGET and/or TEXCF_UNORDERED creation "
                "flags are unneccesary");
  }

  if (RB_NONE != (RB_FLUSH_UAV & barrier))
  {
    if (RB_NONE == ((RB_STAGE_VERTEX | RB_STAGE_PIXEL | RB_STAGE_COMPUTE | RB_STAGE_RAYTRACE) & barrier))
    {
      reportError("DX12: A UAV barrier requires a destination stage");
    }
    if (RB_NONE == ((RB_SOURCE_STAGE_VERTEX | RB_SOURCE_STAGE_PIXEL | RB_SOURCE_STAGE_COMPUTE | RB_SOURCE_STAGE_RAYTRACE) & barrier))
    {
      reportError("DX12: A UAV barrier requires a source stage");
    }
    if (RB_NONE != ((~(RB_FLUSH_UAV | RB_STAGE_VERTEX | RB_STAGE_PIXEL | RB_STAGE_COMPUTE | RB_STAGE_RAYTRACE |
                       RB_SOURCE_STAGE_VERTEX | RB_SOURCE_STAGE_PIXEL | RB_SOURCE_STAGE_COMPUTE | RB_SOURCE_STAGE_RAYTRACE)) &
                     barrier))
    {
      reportError("DX12: A UAV barrier can not combined with any other transition");
    }
  }
  if (RB_NONE != (RB_RO_CONSTANT_BUFFER & barrier))
  {
    reportError("DX12: A texture can not be a constant buffer");
  }
  if (RB_NONE != (RB_RO_VERTEX_BUFFER & barrier))
  {
    reportError("DX12: A texture can not be a vertex buffer");
  }
  if (RB_NONE != (RB_RO_INDEX_BUFFER & barrier))
  {
    reportError("DX12: A texture can not be a index buffer");
  }
  if (RB_NONE != (RB_RO_INDIRECT_BUFFER & barrier))
  {
    reportError("DX12: A texture can not be a indirect buffer");
  }
  if (RB_NONE != (RB_RO_RAYTRACE_ACCELERATION_BUILD_SOURCE & barrier))
  {
    reportError("DX12: A texture can not be a source to build a acceleration structures");
  }
  if (RB_NONE != (RB_RW_UAV & barrier))
  {
    if (RB_NONE != ((RB_RO_GENERIC_READ_TEXTURE | RB_RW_COPY_DEST | RB_RW_RENDER_TARGET | RB_RW_BLIT_DEST) & barrier))
    {
      reportError("DX12: A write state can not be combined with any other states");
    }
  }
  if (RB_NONE != (RB_RW_COPY_DEST & barrier))
  {
    if (RB_NONE != ((RB_RO_GENERIC_READ_TEXTURE | RB_RW_UAV | RB_RW_RENDER_TARGET | RB_RW_BLIT_DEST) & barrier))
    {
      reportError("DX12: A write state can not be combined with any other states");
    }
  }
  if (is_depth && (RB_RO_CONSTANT_DEPTH_STENCIL_TARGET == (RB_RO_CONSTANT_DEPTH_STENCIL_TARGET & barrier)))
  {
    if (RB_NONE == ((RB_STAGE_VERTEX | RB_STAGE_PIXEL) & barrier))
    {
      reportError("DX12: Constant depth stencil state requires the target stage to be vertex "
                  "and/or pixel shader");
    }
    if (RB_NONE != ((RB_RO_COPY_SOURCE | RB_RO_BLIT_SOURCE | RB_RO_VARIABLE_RATE_SHADING_TEXTURE | RB_RW_UAV | RB_RW_COPY_DEST |
                      RB_RW_BLIT_DEST) &
                     barrier))
    {
      reportError("DX12: Constant depth stencil state can not be combined with any other states");
    }
  }
  else if (RB_NONE != (RB_RW_RENDER_TARGET & barrier))
  {
    if (RB_NONE != ((RB_RO_GENERIC_READ_TEXTURE | RB_RW_UAV | RB_RW_COPY_DEST | RB_RW_BLIT_DEST) & barrier))
    {
      reportError("DX12: A write state can not be combined with any other states");
    }
  }
  if (RB_NONE != (RB_RW_BLIT_DEST & barrier))
  {
    if (RB_NONE != ((RB_RO_GENERIC_READ_TEXTURE | RB_RW_UAV | RB_RW_COPY_DEST | RB_RW_RENDER_TARGET) & barrier))
    {
      reportError("DX12: A write state can not be combined with any other states");
    }
  }
  if (RB_NONE == ((RB_RW_RENDER_TARGET | RB_RW_COPY_DEST | RB_RW_BLIT_DEST | RB_RO_VARIABLE_RATE_SHADING_TEXTURE | RB_RO_COPY_SOURCE |
                    RB_RO_BLIT_SOURCE | RB_ALIAS_FROM | RB_ALIAS_TO) &
                   barrier))
  {
    if (RB_NONE == (RB_STAGE_ALL_SHADERS & barrier))
    {
      reportError("DX12: Transitioned state requires a target stage");
    }
  }
  if (
    (RB_FLAG_SPLIT_BARRIER_BEGIN | RB_FLAG_SPLIT_BARRIER_END) == ((RB_FLAG_SPLIT_BARRIER_BEGIN | RB_FLAG_SPLIT_BARRIER_END) & barrier))
  {
    reportError("DX12: A barrier can not be the beginning and the end of a split barrier at the "
                "same time");
  }
  if (GpuPipeline::GRAPHICS == q)
  {
    // nothing specific here (yet?)
  }
  else if (GpuPipeline::ASYNC_COMPUTE == q)
  {
    // compute queue, can only handle compute and ray trace related shaders
    if (RB_NONE != (RB_STAGE_VERTEX & barrier))
    {
      reportError("DX12: Can not target vertex shader stage on compute queue");
    }
    if (RB_NONE != (RB_STAGE_PIXEL & barrier))
    {
      reportError("DX12: Can not target pixel shader stage on compute queue");
    }
    if (RB_NONE != (RB_STAGE_RAYTRACE & barrier))
    {
      // as soon as the ray trace interface has the ability to target the compute
      // queue we can remove this
      reportError("DX12: Can not target ray trace shader stage on compute queue (yet!)");
    }
    if (RB_NONE != (RB_RW_BLIT_DEST & barrier))
    {
      reportError("DX12: Can not transition to blit target on compute queue");
    }
    if (RB_NONE != (RB_RO_BLIT_SOURCE & barrier))
    {
      reportError("DX12: Can not transition to blit source on compute queue");
    }
    if (RB_NONE != (RB_RO_VARIABLE_RATE_SHADING_TEXTURE & barrier))
    {
      reportError("DX12: Can not transition to variable rate shading texture on compute queue");
    }
  }
  return isOkayToExecute;
}
} // namespace

void d3d::resource_barrier(ResourceBarrierDesc desc, GpuPipeline gpu_pipeline /* = GpuPipeline::GRAPHICS*/)
{
  STORE_RETURN_ADDRESS();
  ScopedCommitLock ctxLock{drv3d_dx12::api_state.device.getContext()};
  desc.enumerateBufferBarriers([gpu_pipeline](auto buf, auto state) //
    {
      validate_buffer_barrier(state, gpu_pipeline);
      auto gbuf = (GenericBufferInterface *)buf;
      BufferResourceReference ref;
      if (gbuf)
      {
        if (RB_NONE == (RB_RW_UAV & state))
        {
          gbuf->updateDeviceBuffer([](auto &buf) { buf.resourceId.removeMarkedAsUAVBuffer(); });
        }
        ref = gbuf->getDeviceBuffer();
      }

      drv3d_dx12::api_state.device.getContext().bufferBarrier(ref, state, gpu_pipeline);
    });
  desc.enumerateTextureBarriers([gpu_pipeline](auto tex, auto state, auto res_index, auto res_range) {
    if (!tex)
    {
      NAU_LOG_ERROR("DX12: Texture barrier with nullptr for texture!");
      return;
    }

    auto btex = cast_to_texture_base(tex);
    if (!validate_texture_barrier(state, btex->getFormat().isDepth(), btex->isRenderTarget(), btex->isUav(), gpu_pipeline))
    {
      NAU_LOG_ERROR("DX12: Barrier validation resulted in skipped barrier for {}", btex->getResName());
      return;
    }

    auto image = btex->getDeviceImage();
    auto range = image->getSubresourceRangeForBarrier(res_index, res_range);
    if (!range.isValidRange())
    {
      NAU_LOG_ERROR("DX12: Barrier with invalid subresource range resulted in skipped barrier {}", btex->getResName());
      return;
    }

    drv3d_dx12::api_state.device.getContext().textureBarrier(image, range, btex->cflg, state, gpu_pipeline, false);
  });
}

d3d::SamplerHandle d3d::create_sampler(const d3d::SamplerInfo &info)
{
  return drv3d_dx12::api_state.device.createSampler(SamplerState::fromSamplerInfo(info));
}
void d3d::destroy_sampler(d3d::SamplerHandle handle) { drv3d_dx12::api_state.device.deleteSampler(handle); }

void d3d::set_sampler(unsigned shader_stage, unsigned slot, d3d::SamplerHandle handle)
{
  drv3d_dx12::api_state.state.setStageSampler(shader_stage, slot, handle);
}

uint32_t d3d::register_bindless_sampler(BaseTexture *texture)
{
  STORE_RETURN_ADDRESS();
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, 0, "Bindless resources are not supported on this hardware");
  NAU_ASSERT_RETURN(texture != nullptr, 0, "d3d::register_bindless_sampler texture can not be null");
  return drv3d_dx12::api_state.device.registerBindlessSampler((BaseTex *)texture);
}

void drv3d_dx12::dirty_srv_no_lock(BaseTex *texture, uint32_t stage, Bitset<dxil::MAX_T_REGISTERS> slots)
{
  drv3d_dx12::api_state.state.dirtySRVNoLock(texture, stage, slots);
}

void drv3d_dx12::dirty_srv(BaseTex *texture, uint32_t stage, Bitset<dxil::MAX_T_REGISTERS> slots)
{
  drv3d_dx12::api_state.state.dirtySRV(texture, stage, slots);
}

void drv3d_dx12::dirty_sampler(BaseTex *texture, uint32_t stage, Bitset<dxil::MAX_T_REGISTERS> slots)
{
  drv3d_dx12::api_state.state.dirtySampler(texture, stage, slots);
}

void drv3d_dx12::dirty_srv_and_sampler_no_lock(BaseTex *texture, uint32_t stage, Bitset<dxil::MAX_T_REGISTERS> slots)
{
  drv3d_dx12::api_state.state.dirtySRVandSamplerNoLock(texture, stage, slots);
}

void drv3d_dx12::dirty_uav_no_lock(BaseTex *texture, uint32_t stage, Bitset<dxil::MAX_U_REGISTERS> slots)
{
  drv3d_dx12::api_state.state.dirtyUAVNoLock(texture, stage, slots);
}

void drv3d_dx12::dirty_rendertarget_no_lock(BaseTex *texture, Bitset<Driver3dRenderTarget::MAX_SIMRT> slots)
{
  drv3d_dx12::api_state.state.dirtyRendertTargetNoLock(texture, slots);
}

void drv3d_dx12::notify_delete(BaseTex *texture, const Bitset<dxil::MAX_T_REGISTERS> *srvs, const Bitset<dxil::MAX_U_REGISTERS> *uavs,
  Bitset<Driver3dRenderTarget::MAX_SIMRT> rtvs, bool dsv)
{
  drv3d_dx12::api_state.state.notifyDelete(texture, srvs, uavs, rtvs, dsv);
}

namespace
{
// Disable some PVS checks for validate_resource_description as they either are wrong or
// they would make code less readable.
// 547: Expression is always false, this is okay as on some platforms a flag constant will be 0.
// 616: A constant is 0 and was used in a bitwise op, this is okay as those will do what we expect.
//-V:NAU_ASSERT_DO_AND_LOG:616, 547
bool validate_resource_description(const BasicResourceDescription& desc, const char *what)
{
  G_UNUSED(desc);
  G_UNUSED(what);
  return true;
}

bool validate_resource_description(const BufferResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const BasicResourceDescription &>(desc), what);

  NAU_ASSERT(0 == (SBCF_DYNAMIC & desc.cFlags), "DX12: cFlags of {} had incompatible SBCF_DYNAMIC flag", what);
  NAU_ASSERT(0 == (SBCF_FRAMEMEM & desc.cFlags), "DX12: cFlags of {} had incompatible SBCF_FRAMEMEM flag", what);
  NAU_ASSERT(0 == (SBCF_ZEROMEM & desc.cFlags), "DX12: cFlags of {} had incompatible SBCF_ZEROMEM flag", what);

  hadError =
      (0 != (SBCF_DYNAMIC & desc.cFlags)) ||
      (0 != (SBCF_FRAMEMEM & desc.cFlags)) ||
      (0 != (SBCF_ZEROMEM & desc.cFlags));

  return hadError;
}

bool validate_resource_description(const BasicTextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const BasicResourceDescription &>(desc), what);

  NAU_ASSERT(0 == (TEXCF_SYSTEXCOPY & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_SYSTEXCOPY flag", what);
  NAU_ASSERT(0 == (TEXCF_DYNAMIC & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_DYNAMIC flag", what);
  NAU_ASSERT(0 == (TEXCF_SYSMEM & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_SYSMEM flag", what);
  NAU_ASSERT(0 == (TEXCF_MOVABLE_ESRAM & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_MOVABLE_ESRAM flag", what);
  NAU_ASSERT(0 == (TEXCF_CLEAR_ON_CREATE & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_CLEAR_ON_CREATE flag", what);
  NAU_ASSERT(0 == (TEXCF_TILED_RESOURCE & desc.cFlags), "DX12: cFlags of {} had incompatible TEXCF_TILED_RESOURCE flag", what);

  hadError =
      (0 != (TEXCF_SYSTEXCOPY & desc.cFlags)) ||
      (0 != (TEXCF_DYNAMIC & desc.cFlags)) ||
      (0 != (TEXCF_SYSMEM & desc.cFlags)) ||
      (0 != (TEXCF_MOVABLE_ESRAM & desc.cFlags)) ||
      (0 != (TEXCF_CLEAR_ON_CREATE & desc.cFlags)) ||
      (0 != (TEXCF_TILED_RESOURCE & desc.cFlags));

  return hadError;
}

bool validate_resource_description(const TextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const BasicTextureResourceDescription &>(desc), what);
  return hadError;
}

bool validate_resource_description(const VolTextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const TextureResourceDescription &>(desc), what);
  return hadError;
}

bool validate_resource_description(const ArrayTextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const TextureResourceDescription &>(desc), what);
  return hadError;
}

bool validate_resource_description(const CubeTextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const BasicTextureResourceDescription &>(desc), what);
  return hadError;
}

bool validate_resource_description(const ArrayCubeTextureResourceDescription &desc, const char *what)
{
  bool hadError = validate_resource_description(static_cast<const CubeTextureResourceDescription &>(desc), what);
  return hadError;
}

bool validate_resource_description(const ResourceDescription &desc, const char *what)
{
  switch (desc.resType)
  {
    case RES3D_TEX: return validate_resource_description(desc.asTexRes, what);
    case RES3D_CUBETEX: return validate_resource_description(desc.asCubeTexRes, what);
    case RES3D_VOLTEX: return validate_resource_description(desc.asVolTexRes, what);
    case RES3D_ARRTEX: return validate_resource_description(desc.asArrayTexRes, what);
    case RES3D_CUBEARRTEX: return validate_resource_description(desc.asArrayCubeTexRes, what);
    case RES3D_SBUF: return validate_resource_description(desc.asBufferRes, what);
  }
  return false;
}
} // namespace

ResourceAllocationProperties d3d::get_resource_allocation_properties(const ResourceDescription &desc)
{
  if (!validate_resource_description(desc, "'desc' of get_resource_allocation_properties"))
  {
    return {};
  }
  return drv3d_dx12::api_state.device.getResourceAllocationProperties(desc);
}

ResourceHeap *d3d::create_resource_heap(ResourceHeapGroup *heap_group, size_t size, ResourceHeapCreateFlags flags)
{
  return drv3d_dx12::api_state.device.newUserHeap(heap_group, size, flags);
}

void d3d::destroy_resource_heap(ResourceHeap *heap)
{
  NAU_ASSERT_RETURN(nullptr != heap, , "DX12: 'heap' of destroy_resource_heap was nullptr");
  drv3d_dx12::api_state.device.getContext().freeUserHeap(heap);
}

Sbuffer *d3d::place_buffere_in_resource_heap(ResourceHeap *heap, const ResourceDescription &desc, size_t offset,
  const ResourceAllocationProperties &alloc_info, const char8_t *name)
{
  STORE_RETURN_ADDRESS();
  NAU_ASSERT_RETURN(nullptr != heap, nullptr, "DX12: 'heap' of place_buffere_in_resource_heap was nullptr");
  // validate already throws asserts, no need to do it again
  if (!validate_resource_description(desc, "'desc' of place_buffere_in_resource_heap"))
  {
    return nullptr;
  }

  auto buffer = drv3d_dx12::api_state.device.placeBufferInHeap(heap, desc, offset, alloc_info, name);
  if (!buffer)
  {
    return nullptr;
  }
  return drv3d_dx12::api_state.device.newBufferObject(eastl::move(buffer), desc.asBufferRes.elementSizeInBytes, desc.asBufferRes.elementCount,
    desc.asBasicRes.cFlags, desc.asBufferRes.viewFormat, name);
}

BaseTexture *d3d::place_texture_in_resource_heap(ResourceHeap *heap, const ResourceDescription &desc, size_t offset,
  const ResourceAllocationProperties &alloc_info, const char8_t *name)
{
  STORE_RETURN_ADDRESS();
  NAU_ASSERT_RETURN(nullptr != heap, nullptr, "DX12: 'heap' of place_texture_in_resource_heap was nullptr");
  // validate already throws asserts, no need to do it again
  if (!validate_resource_description(desc, "'desc' of place_texture_in_resource_heap"))
  {
    return nullptr;
  }

  auto image = drv3d_dx12::api_state.device.placeTextureInHeap(heap, desc, offset, alloc_info, name);
  if (!image)
  {
    return nullptr;
  }
  auto tex = drv3d_dx12::api_state.device.newTextureObject(desc.resType, desc.asBasicRes.cFlags);
  tex->tex.image = image;
  tex->tex.realMipLevels = image->getMipLevelRange().count();
  tex->setParams(image->getBaseExtent().width, image->getBaseExtent().height,
    image->getType() == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? image->getBaseExtent().depth : image->getArrayLayers().count(),
    image->getMipLevelRange().count(), name);
  return tex;
}

ResourceHeapGroupProperties d3d::get_resource_heap_group_properties(ResourceHeapGroup *heap_group)
{
  return drv3d_dx12::api_state.device.getResourceHeapGroupProperties(heap_group);
}

void d3d::map_tile_to_resource(BaseTexture *tex, ResourceHeap *heap, const TileMapping *mapping, size_t mapping_count)
{
  NAU_ASSERT_RETURN(tex, );
  NAU_ASSERT_RETURN(mapping, );
  NAU_ASSERT_RETURN(mapping_count, );
  NAU_ASSERT_RETURN(tex->restype() != RES3D_VOLTEX || d3d::get_driver_desc().caps.hasTiled3DResources, );
  NAU_ASSERT_RETURN(tex->restype() != RES3D_TEX || d3d::get_driver_desc().caps.hasTiled2DResources, );
  NAU_ASSERT_RETURN(tex->restype() != RES3D_CUBETEX || d3d::get_driver_desc().caps.hasTiled2DResources, );
  NAU_ASSERT_RETURN(tex->restype() != RES3D_ARRTEX || d3d::get_driver_desc().caps.hasTiled2DResources, );
  NAU_ASSERT_RETURN(tex->restype() != RES3D_CUBEARRTEX || d3d::get_driver_desc().caps.hasTiled2DResources, );
  for (int ix = 0; ix < mapping_count; ++ix)
    NAU_ASSERT_RETURN(mapping[ix].heapTileSpan > 0, );

  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().mapTileToResource(cast_to_texture_base(tex), heap, mapping, mapping_count);
}

TextureTilingInfo d3d::get_texture_tiling_info(BaseTexture *tex, size_t subresource)
{
  NAU_ASSERT_RETURN(tex, TextureTilingInfo{});
  return drv3d_dx12::api_state.device.getTextureTilingInfo(cast_to_texture_base(tex), subresource);
}

namespace
{
bool check_buffer_activation(ResourceActivationAction action)
{
  switch (action)
  {
    case ResourceActivationAction::REWRITE_AS_COPY_DESTINATION:
    case ResourceActivationAction::REWRITE_AS_UAV:
    case ResourceActivationAction::CLEAR_F_AS_UAV:
    case ResourceActivationAction::CLEAR_I_AS_UAV:
    case ResourceActivationAction::DISCARD_AS_UAV: return true;

    case ResourceActivationAction::REWRITE_AS_RTV_DSV:
    case ResourceActivationAction::CLEAR_AS_RTV_DSV:
    case ResourceActivationAction::DISCARD_AS_RTV_DSV: return false;
  }
  return false;
}
} // namespace

void d3d::activate_buffer(Sbuffer *buf, ResourceActivationAction action, const ResourceClearValue &value,
  GpuPipeline gpu_pipeline /*= GpuPipeline::GRAPHICS*/)
{
  CHECK_MAIN_THREAD();
  NAU_ASSERT_RETURN(nullptr != buf, , "DX12: 'buf' of activate_buffer was nullptr");
  NAU_ASSERT_RETURN(check_buffer_activation(action), , "DX12: 'activation' of activate_buffer was invalid");
  STORE_RETURN_ADDRESS();
  decltype(auto) buffer = ((GenericBufferInterface *)buf)->getDeviceBuffer();
  drv3d_dx12::api_state.device.getContext().activateBuffer(buffer, drv3d_dx12::api_state.device.getResourceMemoryForBuffer(buffer), action, value,
    gpu_pipeline);
}
void d3d::activate_texture(BaseTexture *tex, ResourceActivationAction action, const ResourceClearValue &value,
  GpuPipeline gpu_pipeline /*= GpuPipeline::GRAPHICS*/)
{
  CHECK_MAIN_THREAD();
  NAU_ASSERT_RETURN(nullptr != tex, , "DX12: 'tex' of activate_texture was nullptr");
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.getContext().activateTexture(cast_to_texture_base(tex), action, value, gpu_pipeline);
}

void d3d::deactivate_buffer(Sbuffer *buf, GpuPipeline gpu_pipeline /*= GpuPipeline::GRAPHICS*/)
{
  CHECK_MAIN_THREAD();
  STORE_RETURN_ADDRESS();
  if (buf)
  {
    decltype(auto) buffer = ((GenericBufferInterface *)buf)->getDeviceBuffer();
    drv3d_dx12::api_state.device.getContext().deactivateBuffer(buffer, drv3d_dx12::api_state.device.getResourceMemoryForBuffer(buffer), gpu_pipeline);
  }
  else
  {
    drv3d_dx12::api_state.device.getContext().aliasFlush(gpu_pipeline);
  }
}

void d3d::deactivate_texture(BaseTexture *tex, GpuPipeline gpu_pipeline /*= GpuPipeline::GRAPHICS*/)
{
  CHECK_MAIN_THREAD();
  STORE_RETURN_ADDRESS();
  if (tex)
  {
    drv3d_dx12::api_state.device.getContext().deactivateTexture(cast_to_texture_base(tex)->getDeviceImage(), gpu_pipeline);
  }
  else
  {
    drv3d_dx12::api_state.device.getContext().aliasFlush(gpu_pipeline);
  }
}

IMPLEMENT_D3D_RENDER_PASS_API_USING_GENERIC()

#if DAGOR_DBGLEVEL > 0

#include <gui/dag_imgui.h>

namespace
{
void memory_overlay() { drv3d_dx12::api_state.device.memoryDebugOverlay(); }

void resource_use_overlay() { drv3d_dx12::api_state.device.resourceUseOverlay(); }
} // namespace
REGISTER_IMGUI_WINDOW("DX12", "DX12 Memory##DX12-Memory", memory_overlay);
REGISTER_IMGUI_WINDOW("DX12", "DX12 Resource Use / Barriers##DX12-Resource-Use-Barriers", resource_use_overlay);

#endif


uint32_t d3d::allocate_bindless_resource_range(uint32_t, uint32_t count)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, 0, "Bindless resources are not supported on this hardware");
  NAU_ASSERT_RETURN(count > 0, 0, "d3d::allocate_bindless_resource_range: 'count' must be larger than 0");
  return drv3d_dx12::api_state.device.allocateBindlessResourceRange(count);
}

uint32_t d3d::resize_bindless_resource_range(uint32_t, uint32_t index, uint32_t current_count, uint32_t new_count)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, 0, "Bindless resources are not supported on this hardware");
  STORE_RETURN_ADDRESS();
  if (current_count > 0)
  {
    return drv3d_dx12::api_state.device.resizeBindlessResourceRange(index, current_count, new_count);
  }
  else
  {
    return drv3d_dx12::api_state.device.allocateBindlessResourceRange(new_count);
  }
}

void d3d::free_bindless_resource_range(uint32_t, uint32_t index, uint32_t count)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, , "Bindless resources are not supported on this hardware");
  if (count > 0)
  {
    drv3d_dx12::api_state.device.freeBindlessResourceRange(index, count);
  }
}

void d3d::update_bindless_resource(uint32_t index, D3dResource *res)
{
  STORE_RETURN_ADDRESS();
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, , "Bindless resources are not supported on this hardware");
  NAU_ASSERT_RETURN(res != nullptr, , "d3d::update_bindless_resource: 'res' can not be null");
  auto resType = res->restype();
  if (RES3D_SBUF == resType)
  {
    drv3d_dx12::api_state.device.updateBindlessBuffer(index, (GenericBufferInterface *)res);
  }
  else
  {
    drv3d_dx12::api_state.device.updateBindlessTexture(index, (BaseTex *)res);
  }
}

void d3d::update_bindless_resources_to_null(uint32_t resource_type, uint32_t index, uint32_t count)
{
  NAU_ASSERT_RETURN(d3d::get_driver_desc().caps.hasBindless, , "Bindless resources are not supported on this hardware");
  STORE_RETURN_ADDRESS();
  drv3d_dx12::api_state.device.updateBindlessNull(resource_type, index, count);
}




SWAPID d3d::create_swapchain(void* hWnd)
{
    SWAPID id = drv3d_dx12::api_state.device.createAdditionalSwachain(hWnd, drv3d_dx12::api_state.dxgi14.Get());

    return id;
}

void d3d::remove_swapchain(SWAPID swapID)
{
    drv3d_dx12::api_state.device.removeAdditionalSwapchain(swapID);
    return ;
}

NAU_RENDER_EXPORT void d3d::finish_render_commands()
{
    auto &ctx = api_state.device.getContext();
    ctx.finish();
    return;
}

BaseTexture* d3d::get_back_buffer_rt(SWAPID id)
{
    static int t = 0;
    if (id == DEFAULT_SWAPID)
    {
        return drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture();
    }

    auto res = drv3d_dx12::api_state.device.getContext().getSwapchainColorTexture(id);;

    return res;
}
