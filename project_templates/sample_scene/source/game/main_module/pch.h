// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include <nau/core_defines.h>

#ifdef _WIN32
    #include "nau/platform/windows/windows_headers.h"
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <random>
#include <regex>
#include <thread>
#include "nau/diag/assertion.h"
#include "nau/diag/logging.h"
#include "nau/service/service_provider.h"
