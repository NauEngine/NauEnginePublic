// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "nau/gui/dag_imgui.h"
#include "imguiRenderer.h"
#include "../render_window/dof_controls.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include "nau/3d/dag_drv3d.h"
#include "nau/3d/dag_drv3dReset.h"
#include "nau/perfMon/dag_cpuFreq.h"
//#include "nau/dataBlock/dag_dataBlock.h"
//#include "nau/dataBlock/dag_dataBlockUtils.h"
#include "nau/osApiWrappers/dag_direct.h"
#include "nau/diag/logging.h"
#include <mutex>
#include <thread>



static ImGuiState imgui_state = ImGuiState::OFF;
static ImGuiState requested_state = ImGuiState::OFF;
static bool is_state_change_requested = false;
static eastl::unique_ptr<eastl::vector<OnStateChangeHandlerFunc>> on_state_change_functions;

static bool is_initialized = false;
static eastl::unique_ptr<DagImGuiRenderer> renderer;
//static eastl::unique_ptr<nau::DataBlock> imgui_blk;
//static eastl::unique_ptr<nau::DataBlock> override_imgui_blk;
static const char *imgui_blk_path = "imgui.blk";
static eastl::string full_blk_path;
static eastl::string full_ini_path;
static eastl::string full_log_path;

static float active_window_bg_alpha = 1.0f;
static float overlay_window_bg_alpha = 0.5f;
static eastl::unique_ptr<ImFontConfig> requested_font_cfg;
static constexpr float MIN_SCALE = 1.0f;
static constexpr float MAX_SCALE = 4.0f;
static bool frameEnded = true;

// ImGui is lazily initialized from the render thread (debug_gui node declaration calls imgui_update()/imgui_endframe()),
// while the main thread starts calling imgui_cache_render_data()/imgui_update() as soon as imgui_get_state() != OFF.
// imgui_mutex serializes state changes, init and frame begin/end between these threads.
// Recursive because state change handlers are called under it.
static std::recursive_mutex imgui_mutex;
static std::thread::id frame_thread; // thread that started the current ImGui frame

static bool imguiSubmenuEnabled = true;

ImDrawData* cachedDrawData = nullptr;
ImDrawData* copiedDrawData = nullptr;

//void imgui_set_override_blk(const nau::DataBlock &imgui_blk_)
//{
//  NAU_ASSERT(!is_initialized, "imgui_set_override_blk() should be called before init_on_demand()");
//  override_imgui_blk = eastl::make_unique<nau::DataBlock>();
//  override_imgui_blk->setFrom(&imgui_blk_);
//}


void deleteDrawData(ImDrawData* data)
{
    NAU_ASSERT(data != nullptr);

    for(int i = 0; i < data->CmdListsCount; ++i)
    {
        IM_DELETE(data->CmdLists[i]);
    }

    delete[] data->CmdLists;

    delete data;
}



void imgui_enable_imgui_submenu(bool enabled) { imguiSubmenuEnabled = enabled; }

static float get_default_scale()
{
  int w, h;
  d3d::get_screen_size(w, h);
  float scale = roundf(h / 1080.0f); // - Rounded scale on purpose, because non-integer scale produces blurry text with
                                     //   default ImGui font.
                                     // - We should determine default scaling based on DPI instead of raw resolution
                                     //   (e.g. by querying the built in scaling in Windows).
  return nau::math::clamp(scale, MIN_SCALE, MAX_SCALE);
}

static void apply_style_from_blk()
{
  float imguiScale = get_default_scale(); //imgui_blk->getReal("imgui_scale", get_default_scale());
  imguiScale = nau::math::clamp(imguiScale, MIN_SCALE, MAX_SCALE);

  ImGuiStyle scaledStyle;
  scaledStyle.ScaleAllSizes(imguiScale);
  ImGui::GetStyle() = scaledStyle;
  ImGui::StyleColorsDark(); // TODO: Apply custom style here if we ever wish to support it. Right now we use Dark style,
                            //       which is also the default.
  active_window_bg_alpha = ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w;
  overlay_window_bg_alpha = active_window_bg_alpha * 0.5f;

  requested_font_cfg = eastl::make_unique<ImFontConfig>();
  requested_font_cfg->OversampleH = requested_font_cfg->OversampleV = 1;
  requested_font_cfg->PixelSnapH = true; // some fonts are blurry without this
  // FIXME: Non-integer amount of imguiScale produces blurry text with default ImGui font. We should consider using our
  //        own font.
  requested_font_cfg->SizePixels = 13.0f * imguiScale; //floor(imgui_blk->getReal("imgui_font_size", 13.0f) * imguiScale);
  requested_font_cfg->GlyphRanges = ImGui::GetIO().Fonts->GetGlyphRangesCyrillic();
}

static bool init()
{
  if (is_initialized)
    return true;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();

  ImGuiIO &io = ImGui::GetIO();

  // Load blk
  //imgui_blk = eastl::make_unique<nau::DataBlock>();

#if _TARGET_ANDROID || _TARGET_C3
  full_blk_path = String(0, "%s/%s", folders::get_gamedata_dir(), imgui_blk_path);
  full_ini_path = String(0, "%s/%s", folders::get_temp_dir(), io.IniFilename);
  full_log_path = String(0, "%s/%s", folders::get_temp_dir(), io.LogFilename);
#else
  full_blk_path = eastl::string(eastl::string::CtorSprintf(), "%s", imgui_blk_path);
  full_ini_path = eastl::string(eastl::string::CtorSprintf(), "%s", io.IniFilename);
  full_log_path = eastl::string(eastl::string::CtorSprintf(), "%s", io.LogFilename);
#endif
  //if (dd_file_exist(full_blk_path.c_str()))
  //  imgui_blk->load(full_blk_path.c_str());

  //if (override_imgui_blk)
  //  merge_data_block(*imgui_blk, *override_imgui_blk);

  apply_style_from_blk();

  io.IniFilename = full_ini_path.c_str();
  io.LogFilename = full_log_path.c_str();

  // Init our own renderer backend
  renderer = eastl::make_unique<DagImGuiRenderer>();
  renderer->setBackendFlags(io);

  is_initialized = true;
  return true;
}

bool init_on_demand()
{
  std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
  if (is_initialized)
    return true;
  bool success = init();
  if (!success)
  {
    NAU_LOG_ERROR("Failed to initialize imgui.");
    return false;
  }
  return true;
}

static void handle_state_change_request()
{
  if (!is_state_change_requested)
    return;

  if (requested_state == imgui_state)
  {
    is_state_change_requested = false;
    return;
  }

  if (!init_on_demand())
    return;

  ImGuiState oldState = imgui_state;
  imgui_state = requested_state;
  is_state_change_requested = false;

  ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w =
    imgui_state == ImGuiState::OVERLAY ? overlay_window_bg_alpha : active_window_bg_alpha;

  if (on_state_change_functions != nullptr)
    for (OnStateChangeHandlerFunc &func : *on_state_change_functions)
      func(oldState, imgui_state);
}

void imgui_shutdown()
{
  std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
  if (is_initialized)
  {
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
  }

  imgui_save_blk();

  is_initialized = false;
  renderer.reset();

  if(cachedDrawData)
  {
      deleteDrawData(cachedDrawData);
      cachedDrawData = nullptr;
  }

  if(copiedDrawData)
  {
      deleteDrawData(copiedDrawData);
      copiedDrawData = nullptr;
  }

  //imgui_blk.reset();
  //override_imgui_blk.reset();
  requested_font_cfg.reset();
  on_state_change_functions.reset();
}

ImGuiState imgui_get_state() { return imgui_state; }

bool imgui_want_capture_mouse() { return ImGui::GetIO().WantCaptureMouse; }

void imgui_request_state_change(ImGuiState new_state)
{
  std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
  requested_state = new_state;
  is_state_change_requested = true;
}

void imgui_register_on_state_change_handler(OnStateChangeHandlerFunc func)
{
  if (on_state_change_functions == nullptr)
    on_state_change_functions = eastl::make_unique<eastl::vector<OnStateChangeHandlerFunc>>();
  on_state_change_functions->push_back(func);
}

static void end_frame()
{
  ImGui::EndFrame();
  frameEnded = true;
}

void imgui_update()
{
  std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
  handle_state_change_request();

  if (imgui_state == ImGuiState::OFF)
    return;

  ImGuiIO &io = ImGui::GetIO();

  if (requested_font_cfg)
  {
    io.Fonts->Clear();
    if (const char *fontName = nullptr)//imgui_blk->getStr("imgui_font_name", nullptr))
      io.FontDefault = io.Fonts->AddFontFromFileTTF(fontName, requested_font_cfg->SizePixels, requested_font_cfg.get());
    else
      io.FontDefault = io.Fonts->AddFontDefault(requested_font_cfg.get());

    renderer->createAndSetFontTexture(io);
    requested_font_cfg.reset();
  }

  int w, h;
  d3d::get_screen_size(w, h);
  io.DisplaySize = ImVec2(w, h);

  static int64_t reft = ref_time_ticks();
  int64_t curt = ref_time_ticks();
  io.DeltaTime = ref_time_delta_to_usec(curt - reft) * 1e-6f;
  reft = curt;

  // work-around to handle issue when fullscreen game loses focus and throw endless assertion:
  // "(g.FrameCount == 0 || g.FrameCountEnded == g.FrameCount) && "Forgot to call Render() or EndFrame() at the end of the previous
  // frame?""
  if (!frameEnded)
    end_frame();
  ImGui::NewFrame();
  frameEnded = false;
  frame_thread = std::this_thread::get_id();
}

void imgui_endframe()
{
  std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
  // By the time the render thread ends the frame it began during lazy init, the main thread
  // may have already rendered it and begun its own frame. Don't end a frame another thread is building.
  if (frame_thread != std::this_thread::get_id())
    return;
  end_frame();
}

void imgui_render()
{
  ImGui::Render();
  renderer->render(ImGui::GetDrawData());
  frameEnded = true;
}


ImDrawData* copyDrawData(ImDrawData* data)
{
    NAU_ASSERT(data != nullptr);

    ImDrawData* res = new ImDrawData();
    res->Valid = true;
    res->CmdListsCount = data->CmdListsCount;
    res->TotalIdxCount = data->TotalIdxCount;
    res->TotalVtxCount = data->TotalVtxCount;
    res->DisplayPos       = data->DisplayPos;
    res->DisplaySize      = data->DisplaySize;
    res->FramebufferScale = data->FramebufferScale;

    res->CmdLists = new ImDrawList*[res->CmdListsCount];
    for(int i = 0; i < res->CmdListsCount; ++i)
    {
        res->CmdLists[i] = data->CmdLists[i]->CloneOutput();
    }

    return res;
}


void imgui_cache_render_data()
{
    std::lock_guard<std::recursive_mutex> lock(imgui_mutex);
    if (imgui_state != ImGuiState::OFF && !frameEnded)
        nau::render::drawDofControls();

    if(cachedDrawData)
    {
        deleteDrawData(cachedDrawData);
        cachedDrawData = nullptr;
    }

    ImGui::Render();
    cachedDrawData = copyDrawData(ImGui::GetDrawData());
    frameEnded = true;
}

void imgui_copy_render_data()
{
    if(cachedDrawData == nullptr)
    {
        return;
    }

    if(copiedDrawData)
    {
        deleteDrawData(copiedDrawData);
        copiedDrawData = nullptr;
    }

    copiedDrawData = std::move(cachedDrawData);
    cachedDrawData = nullptr;
}


// Not under imgui_mutex: uses only copiedDrawData (swapped in syncSceneState while the render thread waits),
// and DagImGuiRenderer::render() may block on the main thread executor.
void imgui_render_copied_data()
{
    if(copiedDrawData)
    {
        renderer->render(copiedDrawData);
    }
}

//nau::DataBlock *imgui_get_blk() { return imgui_blk.get(); }

void imgui_save_blk()
{
  //if (imgui_blk != nullptr)
  //  imgui_blk->saveToTextFile(full_blk_path.c_str());
}

static bool load_window_opened(const char *window_name)
{
  return false; //imgui_blk && imgui_blk->getBlockByNameEx("windows")->getBool(window_name, false);
}

static void save_window_opened(const char *window_name, bool opened)
{
  //if (imgui_blk)
  //  imgui_blk->addBlock("windows")->setBool(window_name, opened);
  imgui_save_blk();
}

void imgui_window_set_visible(const char *, const char *name, const bool visible)
{
  if (init_on_demand()) // instantiate imgui_blk
    save_window_opened(name, visible);
}

void imgui_cascade_windows()
{
  if (!init_on_demand())
    return;

  if (ImGuiContext *ctx = ImGui::GetCurrentContext())
  {
    eastl::vector<ImGuiWindow *> windows;
    for (ImGuiWindow *window : ctx->Windows)
    {
      if (window->IsFallbackWindow || window->Hidden || window->RootWindow != window)
        continue;
      if (window->Flags & ImGuiWindowFlags_NoMove &&
          (window->Collapsed || window->Flags & ImGuiWindowFlags_NoResize || window->Flags & ImGuiWindowFlags_AlwaysAutoResize))
        continue;
      windows.push_back(window);
    }
    if (!windows.empty())
    {
      ImGuiWindow *prevWindow = ctx->CurrentWindow;
      const ImVec2 topLeftIm = ImGui::GetMainViewport()->WorkPos;
      const ImVec2 viewSizeIm = ImGui::GetMainViewport()->WorkSize;

      nau::math::Vector2 topLeft  = nau::math::Vector2(topLeftIm.x, topLeftIm.y);
      nau::math::Vector2 viewSize = nau::math::Vector2(viewSizeIm.x, viewSizeIm.y);

      const nau::math::Vector2 maxStep = (viewSize * 0.6f) / float(windows.size() + 1);
      const nau::math::Vector2 step    = nau::math::Vector2(eastl::min(maxStep.getX(), 50.f), eastl::min(maxStep.getY(), 50.f));
      int i = 1;
      for (ImGuiWindow *window : windows)
      {
        ctx->CurrentWindow = window;
        const ImVec2 windowSizeIm = ImGui::GetWindowSize();
        nau::math::Vector2 pos = nau::math::Vector2(windowSizeIm.x, windowSizeIm.y);
        if (!(window->Flags & ImGuiWindowFlags_NoMove))
        {
          pos = topLeft + step * i;
          ImGui::SetWindowPos(ImVec2(pos.getX(), pos.getY()));
        }
        if (!(window->Collapsed || window->Flags & ImGuiWindowFlags_NoResize || window->Flags & ImGuiWindowFlags_AlwaysAutoResize))
        {
          const ImVec2 size = ImGui::GetWindowSize();
          const ImVec2 newSize = ImVec2(
              eastl::clamp(size.x, 50.f, viewSize.getX() - pos.getX()),
              eastl::clamp(size.y, 50.f, viewSize.getY() - pos.getY()));
          ImGui::SetWindowSize(newSize);
        }
        i += 1;
      }
      ctx->CurrentWindow = prevWindow;
    }
  }
}

bool imgui_window_is_visible(const char *, const char *name) { return load_window_opened(name); }

void imgui_perform_registered()
{
  // Construct main menu bar
  if (ImGui::BeginMainMenuBar())
  {
    // Function queue
    if (ImGuiFunctionQueue::functionHead != nullptr)
    {
      const char *currentGroup = ImGuiFunctionQueue::functionHead->group;
      bool currentGroupOpened = ImGui::BeginMenu(currentGroup);
      for (ImGuiFunctionQueue *q = ImGuiFunctionQueue::functionHead; q; q = q->next)
      {
        if (stricmp(currentGroup, q->group) != 0)
        {
          if (currentGroupOpened)
            ImGui::EndMenu();
          currentGroup = q->group;
          currentGroupOpened = ImGui::BeginMenu(currentGroup);
        }
        if (currentGroupOpened)
        {
          if (ImGui::MenuItem(q->name, q->hotkey))
          {
            NAU_ASSERT(q->function, "Registered ImGui function is null: {}/{}", q->group, q->name); // G_ASSERTF_CONTINUE
            q->function();
          }
        }
      }
      if (currentGroupOpened)
        ImGui::EndMenu();
    }

    if (imguiSubmenuEnabled && ImGui::BeginMenu("ImGui"))
    {
      ImGui::Separator();

      const float defaultScale = get_default_scale();
      float imguiScale = defaultScale; //imgui_blk->getReal("imgui_scale", defaultScale);
      if (ImGui::Button(eastl::string(eastl::string::CtorSprintf(), "Default ({})", defaultScale).c_str()))
      {
        imguiScale = defaultScale;
        //imgui_blk->setReal("imgui_scale", imguiScale);
        imgui_save_blk();
        apply_style_from_blk();
      }
      ImGui::SameLine();
      if (ImGui::DragFloat("ImGui scale", &imguiScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.1f"))
      {
        imguiScale = nau::math::clamp(imguiScale, MIN_SCALE, MAX_SCALE);
        //imgui_blk->setReal("imgui_scale", imguiScale);
        imgui_save_blk();
        apply_style_from_blk();
      }
      ImGui::EndMenu();
    }

    // Window queue
    if (ImGuiFunctionQueue::windowHead != nullptr)
    {
      if (ImGui::BeginMenu("Window"))
      {
        const char *currentGroup = ImGuiFunctionQueue::windowHead->group;
        bool currentGroupOpened = ImGui::BeginMenu(currentGroup);
        for (ImGuiFunctionQueue *q = ImGuiFunctionQueue::windowHead; q; q = q->next)
        {
          if (stricmp(currentGroup, q->group) != 0)
          {
            if (currentGroupOpened)
              ImGui::EndMenu();
            currentGroup = q->group;
            currentGroupOpened = ImGui::BeginMenu(currentGroup);
          }
          if (currentGroupOpened)
          {
            q->opened = load_window_opened(q->name);
            bool oldOpened = q->opened;
            ImGui::MenuItem(q->name, q->hotkey, &q->opened);
            if (q->opened != oldOpened)
              save_window_opened(q->name, q->opened);
          }
        }
        if (currentGroupOpened)
          ImGui::EndMenu();
        ImGui::EndMenu();
      }
    }

    ImGui::EndMainMenuBar();
  }

  // Execute window functions
  for (ImGuiFunctionQueue *q = ImGuiFunctionQueue::windowHead; q; q = q->next)
  {
    q->opened = load_window_opened(q->name);
    if (q->opened)
    {
      NAU_ASSERT("Registered ImGui window function is null: {}/{}", q->group, q->name); // G_ASSERTF_CONTINUE
      bool oldOpened = q->opened;
      ImGui::Begin(q->name, &q->opened, q->flags);
      if (q->opened != oldOpened)
        save_window_opened(q->name, q->opened);
      q->function();
      ImGui::End();
    }
  }
}

ImGuiFunctionQueue *ImGuiFunctionQueue::windowHead = nullptr;
ImGuiFunctionQueue *ImGuiFunctionQueue::functionHead = nullptr;

ImGuiFunctionQueue::ImGuiFunctionQueue(const char *group_, const char *name_, const char *hotkey_, int priority_, int flags_,
  ImGuiFuncPtr func, bool is_window) :
  group(group_), name(name_), hotkey(hotkey_), priority(priority_), flags(flags_), function(func)
{
  ImGuiFunctionQueue **head = is_window ? &windowHead : &functionHead;
  if (*head == nullptr)
  {
    *head = this;
    return;
  }

  ImGuiFunctionQueue *n = *head, *p = nullptr;
  for (; n; p = n, n = n->next)
  {
    int cmp = stricmp(group, n->group);
    if (cmp < 0 || (cmp == 0 && priority < n->priority))
    {
      // insert before
      next = n;
      if (p)
        p->next = this;
      else
        *head = this;
      return;
    }
  }
  p->next = this;
}

static void after_device_reset(bool full_reset)
{
  if (full_reset && renderer)
    renderer->createAndSetFontTexture(ImGui::GetIO());
}
