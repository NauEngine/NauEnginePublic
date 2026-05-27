// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include <nau/core_defines.h>

#include <iostream>

#ifdef NAU_PLATFORM_WIN32
    #include "nau/platform/windows/windows_headers.h"
#endif

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <atomic>
#include <iostream>

#include "nau/app/application.h"
#include "nau/app/application_services.h"
#include "nau/app/window_manager.h"
#include "nau/app/platform_window.h"
#include "nau/rtti/ptr.h"
#include "nau/service/service_provider.h"
#include "nau/utils/scope_guard.h"
#include "nau/service/service_provider.h"
