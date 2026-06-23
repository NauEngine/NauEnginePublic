// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "myapi.h"
#include "nau/rtti/rtti_impl.h"

class MyApiImpl : public IMyApi1
{
    NAU_RTTI_CLASS(MyApiImpl , IMyApi1)

public:
    MyApiImpl();

    void myApiFunction() override;

};
