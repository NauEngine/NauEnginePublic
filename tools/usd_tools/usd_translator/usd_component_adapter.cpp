// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "usd_component_adapter.h"

#include <pxr/usd/usdGeom/primvarsAPI.h>

#include "nau/diag/logging.h"
#include "nau/scene/components/internal/missing_component.h"
#include "nau/usd_wrapper/usd_attribute_wrapper.h"
#include "usd_prim_translator.h"

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "NauComponent";
    }

    ComponentAdapter::ComponentAdapter(PXR_NS::UsdPrim prim) :
        IPrimAdapter(prim)
    {
    }

    ComponentAdapter::~ComponentAdapter() = default;

    std::string_view ComponentAdapter::getType() const
    {
        return g_typeName;
    }

    nau::async::Task<nau::scene::ObjectWeakRef<nau::scene::SceneObject>> ComponentAdapter::initializeSceneObject(nau::scene::ObjectWeakRef<nau::scene::SceneObject> dest)
    {
        m_obj = dest;
        m_obj->addComponent(getComponentTypeFromPrim(), [this](nau::scene::Component& component)
        {
            m_component = component;
            applyAttributesToComponent();
        });

        co_return m_obj;
    }

    nau::async::Task<> ComponentAdapter::update()
    {
        if (!m_component)
        {
            co_return;
        }

        applyAttributesToComponent();
    }

    bool ComponentAdapter::isValid() const
    {
        return m_obj && m_component;
    }

    void ComponentAdapter::destroySceneObject()
    {
        if (isValid())
        {
            m_obj->removeComponent(m_component);
            // it is expected that all references to the removed component should be invalidated immediately
            NAU_ASSERT(!m_component);
        }

        m_obj = nullptr;
    }

    void ComponentAdapter::applyAttributesToComponent()
    {
        if (!m_component || m_component->is<nau::scene::IMissingComponent>())
        {
            NAU_LOG_WARNING("Trying to apply attributes to invalid component");
            setError("Specified component type not registered");
            getPrim().SetKind("InvalidComponent"_tftoken);
            return;
        }
        else
        {
            clearError();
        }

        auto componentAsRuntimeObject = m_component->as<nau::DynamicObject*>();
        auto componentAttributes = getPrim().GetAttributes();
        for (auto attribute : componentAttributes)
        {
            std::string attributeName = attribute.GetName();

            if (attributeName == "xformOp:transform")
            {
                auto sceneComponent = m_component->as<nau::scene::SceneComponent*>();
                if (sceneComponent)
                    translateWorldTransform(getPrim(), *sceneComponent);
                continue;
            }

            if (!componentAsRuntimeObject->containsKey(attributeName))
            {
                continue;
            }

            auto runtimeValue = nau::attributeAsRuntimeValue(attribute);
            if (!runtimeValue)
            {
                NAU_LOG_WARNING("Can't apply runtime value ({})", attributeName);
                continue;
            }
            auto result = componentAsRuntimeObject->setValue(attributeName, runtimeValue);
            if (!result)
            {
                NAU_LOG_WARNING("Can't apply runtime value ({})", attributeName);
            }
        }

        for (size_t i = 0; i < componentAsRuntimeObject->getSize(); ++i)
        {
            std::string_view keyValue = componentAsRuntimeObject->getKey(i);
            const nau::RuntimeValue::Ptr& value = componentAsRuntimeObject->getValue(keyValue);
            if (auto res = createAttributeByValue(getPrim(), PXR_NS::TfToken(std::string(keyValue)), value); !res)
            {
                NAU_LOG_WARNING("Can't create attribute by value({})", keyValue);
            }
        }
    }

    nau::scene::ObjectWeakRef<nau::scene::SceneObject> ComponentAdapter::getSceneObject() const
    {
        return m_obj;
    }

    nau::scene::ObjectWeakRef<nau::scene::Component> ComponentAdapter::getComponent() const
    {
        return m_component;
    }

    nau::rtti::TypeInfo ComponentAdapter::getComponentTypeFromPrim() const
    {
        PXR_NS::UsdAttribute componentTypeNameAttribute = getPrim().GetAttribute("componentTypeName"_tftoken);
        NAU_ASSERT(componentTypeNameAttribute);

        PXR_NS::VtValue componentTypeNameValue;
        componentTypeNameAttribute.Get(&componentTypeNameValue);
        NAU_ASSERT(componentTypeNameValue.IsHolding<std::string>());

        std::string componentTypeName = componentTypeNameValue.Get<std::string>();
        return nau::rtti::makeTypeInfoFromName(componentTypeName.c_str());
    }

    DEFINE_TRANSLATOR(ComponentAdapter, "NauComponent"_tftoken);
}  // namespace UsdTranslator
