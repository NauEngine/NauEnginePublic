// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/module/module.h"

#include "myapi_impl.h"
#include "rotator.h"
#include "mover.h"

class MainGameModule final : public nau::DefaultModuleImpl
{
    nau::string getModuleName() override
    {
        return "MainGameModule";
    }

    void initialize() override
    {
        NAU_MODULE_EXPORT_SERVICE(MyApiImpl);
        nau::getServiceProvider().addClass<MyRotator>();
        nau::getServiceProvider().addClass<MyMover>();
    }
};


IMPLEMENT_MODULE(MainGameModule)
