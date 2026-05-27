// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/rtti/rtti_object.h"

struct NAU_ABSTRACT_TYPE IMyApi1 : nau::IRttiObject
{
    NAU_INTERFACE(IMyApi1, nau::IRttiObject)

    virtual void myApiFunction() = 0;
};

