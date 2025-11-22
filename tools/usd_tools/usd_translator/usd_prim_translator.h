// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once 

#include "usd_translator/usd_translator_api.h"
#include "usd_translator/usd_prim_adapter.h"
#include "usd_translator/ui_translator/usd_ui_prim_adapter.h"

#include <pxr/usd/usd/stage.h>
#include <nau/scene/scene.h>
#include <functional>

namespace UsdTranslator
{
    class USD_TRANSLATOR_API PrimTranslator final
    {
    public:
        using AdapterFactory = std::function<IPrimAdapter::Ptr(PXR_NS::UsdPrim)>;
        using UIAdapterFactory = std::function<IUIPrimAdapter::Ptr(PXR_NS::UsdPrim)>;

        static PrimTranslator& instance();

        bool registerAdapter(const PXR_NS::TfToken& typeName, AdapterFactory adapterFactory, bool doOverride = false);
        bool registerUIAdapter(const PXR_NS::TfToken& typeName, UIAdapterFactory adapterFactory, bool doOverride = false);

        IPrimAdapter::Ptr createAdapter(PXR_NS::UsdPrim prim) const;
        IUIPrimAdapter::Ptr createUIAdapter(PXR_NS::UsdPrim prim) const;
        PXR_NS::TfToken findAdapterType(PXR_NS::UsdPrim prim) const;
        std::vector<PXR_NS::TfToken> registeredAdapters() const;
        std::vector<PXR_NS::TfToken> registeredUIAdapters() const;

        ~PrimTranslator();

    private:
        PrimTranslator() = default;
        PrimTranslator(const PrimTranslator&) = default;
        PrimTranslator(PrimTranslator&&) = default;
        PrimTranslator& operator=(const PrimTranslator&) = default;
        PrimTranslator& operator=(PrimTranslator&&) = default;

        PXR_NS::TfHashMap<PXR_NS::TfToken, AdapterFactory, PXR_NS::TfHash> m_adapters;
        PXR_NS::TfHashMap<PXR_NS::TfToken, UIAdapterFactory, PXR_NS::TfHash> m_uiAdapters;
    };

    USD_TRANSLATOR_API void translateWorldTransform(PXR_NS::UsdPrim fromPrim, nau::scene::SceneObject& toObject);
    USD_TRANSLATOR_API void translateWorldTransform(PXR_NS::UsdPrim fromPrim, nau::scene::SceneComponent& toComponent);
}

#define DEFINE_TRANSLATOR(ClassName, PrimType) bool ClassName##AdapterIsRegestred =             \
UsdTranslator::PrimTranslator::instance().registerAdapter(PrimType, [](PXR_NS::UsdPrim prim)    \
    {                                                                                           \
        return std::make_shared<ClassName>(prim);                                               \
    })

#define DEFINE_UI_TRANSLATOR(ClassName, PrimType) bool ClassName##AdapterIsRegestred =            \
UsdTranslator::PrimTranslator::instance().registerUIAdapter(PrimType, [](PXR_NS::UsdPrim prim)    \
    {                                                                                             \
        return std::make_shared<ClassName>(prim);                                                 \
    })
