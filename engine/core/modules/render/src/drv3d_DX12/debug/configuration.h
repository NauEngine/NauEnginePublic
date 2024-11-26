// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include <EASTL/string_view.h>
#include "nau/dataBlock/dag_dataBlock.h"


namespace drv3d_dx12::debug
{
union Configuration
{
  struct
  {
    bool loadPIXCapturer : 1;
    bool enableAftermath : 1;
    bool enableShaderErrorReporting : 1;
    bool enableGPUDumps : 1;
    bool enableDRED : 1;
    bool ignoreDREDAvailability : 1;
    bool trackPageFaults : 1;
    bool enableDagorGPUTrace : 1;
    bool enableCPUValidation : 1;
    bool enableGPUValidation : 1;
  };

  bool anyValidation() const { return enableGPUValidation || enableCPUValidation; }

  void applyDefaults()
  {
#if !NAU_DEBUG
    applyReleaseDefaults();
#elif NAU_DEBUG
    applyDebugDefaults();
#else
    applyDevDefaults();
#endif
  }

  void applyReleaseDefaults()
  {
    loadPIXCapturer = false;
    enableAftermath = true;
    enableDagorGPUTrace = false;
    trackPageFaults = false;
    enableShaderErrorReporting = false;
    enableDRED = false;
    ignoreDREDAvailability = false;
    // no words on how not helpful the pvs message here is
    enableGPUDumps = enableAftermath; // -V547
    enableCPUValidation = false;
    enableGPUValidation = false;
  }

  void applyDevDefaults()
  {
    loadPIXCapturer = true;
    enableAftermath = true;
    enableDagorGPUTrace = true;
    trackPageFaults = true;
    enableShaderErrorReporting = false;
    enableDRED = true;
    ignoreDREDAvailability = false;
    // no words on how not helpful the pvs message here is
    enableGPUDumps = enableAftermath; // -V547
    enableCPUValidation = false;
    enableGPUValidation = false;
  }

  void applyDebugDefaults()
  {
    loadPIXCapturer = true;
    enableAftermath = true;
    enableDagorGPUTrace = true;
    trackPageFaults = true;
    enableShaderErrorReporting = true;
    enableDRED = true;
    ignoreDREDAvailability = false;
    // no words on how not helpful the pvs message here is
    enableGPUDumps = enableAftermath; // -V547
    enableCPUValidation = true;
    enableGPUValidation = false;
  }

  void applyPIXProfile() { loadPIXCapturer = true; }

  void applyRenderDocProfile() { loadPIXCapturer = false; }

  void applyGenericCaptureToolProfile() { loadPIXCapturer = true; }

  void applyShaderDebugProfile()
  {
    enableAftermath = true;
    enableDagorGPUTrace = true;
    trackPageFaults = true;
    enableShaderErrorReporting = true;
    enableDRED = true;
    ignoreDREDAvailability = false;
    enableGPUDumps = true;
    enableCPUValidation = false;
    enableGPUValidation = true;
  }

  void applyTraceProfile()
  {
    enableAftermath = true;
    enableDagorGPUTrace = true;
    trackPageFaults = true;
    enableShaderErrorReporting = true;
    enableDRED = true;
    ignoreDREDAvailability = false;
    enableGPUDumps = true;
    enableCPUValidation = false;
    enableGPUValidation = false;
  }

  void applyAftermathProfile()
  {
    enableAftermath = true;
    enableDagorGPUTrace = false;
    trackPageFaults = true;
    enableShaderErrorReporting = true;
    enableDRED = false;
    ignoreDREDAvailability = false;
    enableGPUDumps = true;
    enableCPUValidation = false;
    enableGPUValidation = false;
  }

  void applyDREDProfile()
  {
    enableAftermath = false;
    enableDagorGPUTrace = false;
    trackPageFaults = true;
    enableDRED = true;
    ignoreDREDAvailability = true;
    enableGPUDumps = false;
  }

  void applyDagorTraceProfile()
  {
    enableAftermath = false;
    enableDagorGPUTrace = true;
    trackPageFaults = true;
    enableDRED = false;
    ignoreDREDAvailability = false;
    enableGPUDumps = false;
  }

  void applyDebugProfile(eastl::string_view profile)
  {
    if (profile.empty())
    {
      return;
    }
    if (0 == profile.compare("release"))
    {
      applyReleaseDefaults();
    }
    else if (0 == profile.compare("dev"))
    {
      applyDevDefaults();
    }
    else if (0 == profile.compare("debug"))
    {
      applyDevDefaults();
    }
    else if (0 == profile.compare("pix"))
    {
      applyPIXProfile();
    }
    else if (0 == profile.compare("renderdoc"))
    {
      applyRenderDocProfile();
    }
    else if (0 == profile.compare("capture"))
    {
      applyGenericCaptureToolProfile();
    }
    else if ((0 == profile.compare("hung")) || (0 == profile.compare("hang")))
    {
      applyTraceProfile();
    }
    else if (0 == profile.compare("shaders"))
    {
      applyShaderDebugProfile();
    }
    else if (0 == profile.compare("trace"))
    {
      applyTraceProfile();
    }
    else if (0 == profile.compare("postmortem"))
    {
      applyTraceProfile();
    }
    else if (0 == profile.compare("aftermath"))
    {
      applyAftermathProfile();
    }
    else if (0 == profile.compare("dred"))
    {
      applyDREDProfile();
    }
    else if (0 == profile.compare("dagor"))
    {
      applyDagorTraceProfile();
    }
  }

  void applyLegacySettings(const nau::DataBlock *settings)
  {
    int debugLevel = settings->getInt("debugLevel", 0);

    enableAftermath = settings->getBool("enableAftermath", enableAftermath);
    trackPageFaults = settings->getBool("trackPageFaults", trackPageFaults || debugLevel > 0 || enableAftermath);
    enableDRED = settings->getBool("allowDRED", enableDRED);
    loadPIXCapturer = settings->getBool("loadPixCapturer", loadPIXCapturer);
    enableDagorGPUTrace = settings->getBool("enableDagorGPUTrace", enableDagorGPUTrace);
    enableShaderErrorReporting = settings->getBool("enableShaderErrorReporting", enableShaderErrorReporting);
    ignoreDREDAvailability = settings->getBool("ignoreDREDAvailability", ignoreDREDAvailability);
    enableGPUDumps = settings->getBool("gpuDumps", enableAftermath);
    enableCPUValidation = settings->getBool("cpuValidation", enableCPUValidation || debugLevel > 1);
    enableGPUValidation = settings->getBool("gpuValidation", enableGPUValidation || debugLevel > 2);

    applyDebugProfile(settings->getStr("debugProfile", ""));
  }

  void applyModernSettings(const nau::DataBlock *settings)
  {
    // modern settings are all in debug sub-block
    auto modernSettings = settings->getBlockByName("debug");
    if (!modernSettings)
    {
      return;
    }

    int debugLevel = modernSettings->getInt("level", 0);
    enableAftermath = modernSettings->getBool("aftermath", enableAftermath);
    trackPageFaults = modernSettings->getBool("pageFaults", trackPageFaults || debugLevel > 0 || enableAftermath);
    enableDRED = modernSettings->getBool("DRED", enableDRED);
    loadPIXCapturer = modernSettings->getBool("loadPixCapturer", loadPIXCapturer);
    enableDagorGPUTrace = modernSettings->getBool("dagorGPUTrace", enableDagorGPUTrace);
    enableShaderErrorReporting = modernSettings->getBool("shaderErrorReporting", enableShaderErrorReporting);
    ignoreDREDAvailability = modernSettings->getBool("ignoreDREDAvailability", ignoreDREDAvailability);
    enableGPUDumps = modernSettings->getBool("gpuDumps", enableAftermath);
    enableCPUValidation = settings->getBool("cpuValidation", enableCPUValidation || debugLevel > 1);
    enableGPUValidation = settings->getBool("gpuValidation", enableGPUValidation || debugLevel > 2);

    applyDebugProfile(modernSettings->getStr("profile", ""));
  }

  void applySettings(const nau::DataBlock *settings)
  {
    applyLegacySettings(settings);
    applyModernSettings(settings);
  }
};

} // namespace drv3d_dx12::debug
