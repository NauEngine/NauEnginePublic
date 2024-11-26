// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once
#include "nau/app/application_delegate.h"

namespace nau
{
    namespace app_utils
    {
        async::Task<> loadStartupScene();
        nau::Result<> parseAppConfigs(const std::string&);
    }  // namespace app_utils
}  // namespace nau
