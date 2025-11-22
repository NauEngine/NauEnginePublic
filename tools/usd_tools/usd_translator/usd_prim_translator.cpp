// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "usd_prim_translator.h"

#include <nau/scene/scene_factory.h>
#include <nau/service/service_provider.h>
#include <pxr/usd/usdGeom/xform.h>
#include <pxr/usd/usdGeom/xformCache.h>

#include "usd_proxy/usd_proxy.h"

using namespace nau;
using namespace PXR_NS;

namespace UsdTranslator
{
    namespace
    {
        constexpr std::string_view g_typeName = "DefaultPrimAdapter";
    }

    PrimTranslator& PrimTranslator::instance()
    {
        static PrimTranslator inst;
        return inst;
    }

    bool PrimTranslator::registerAdapter(const PXR_NS::TfToken& typeName, AdapterFactory adapterFactory, bool doOverride)
    {
        auto& dest = m_adapters[typeName];
        if (!doOverride && dest)
            return false;

        dest = adapterFactory;
        return true;
    }

    bool PrimTranslator::registerUIAdapter(const PXR_NS::TfToken& typeName, UIAdapterFactory adapterFactory, bool doOverride /*= false*/)
    {
        auto& dest = m_uiAdapters[typeName];
        if (!doOverride && dest)
            return false;

        dest = adapterFactory;
        return true;
    }

    IPrimAdapter::Ptr PrimTranslator::createAdapter(PXR_NS::UsdPrim prim) const
    {
        auto& typeInfo = prim.GetPrimTypeInfo();

        for (auto apiType : typeInfo.GetAppliedAPISchemas())
        {
            auto it = m_adapters.find(apiType);
            if (it != m_adapters.end())
                return it->second(prim);
        }

        auto it = m_adapters.find(typeInfo.GetTypeName());
        if (it == m_adapters.end())
            it = m_adapters.find(typeInfo.GetSchemaTypeName());
        if (it == m_adapters.end())
            it = m_adapters.find(PXR_NS::TfToken(g_typeName.data()));

        return it->second(prim);
    }

    UsdTranslator::IUIPrimAdapter::Ptr PrimTranslator::createUIAdapter(PXR_NS::UsdPrim prim) const
    {
        auto& typeInfo = prim.GetPrimTypeInfo();

        for (auto apiType : typeInfo.GetAppliedAPISchemas())
        {
            auto it = m_uiAdapters.find(apiType);
            if (it != m_uiAdapters.end())
                return it->second(prim);
        }

        auto it = m_uiAdapters.find(typeInfo.GetTypeName());
        if (it == m_uiAdapters.end())
            it = m_uiAdapters.find(typeInfo.GetSchemaTypeName());
        if (it == m_uiAdapters.end())
            it = m_uiAdapters.find("%default"_tftoken);

        return it->second(prim);
    }

    PXR_NS::TfToken PrimTranslator::findAdapterType(PXR_NS::UsdPrim prim) const
    {
        auto& typeInfo = prim.GetPrimTypeInfo();

        auto isAdapterExist = [this](auto type) 
        {
            auto it = m_adapters.find(type);
            return it != m_adapters.end();
        };

        for (auto apiType : typeInfo.GetAppliedAPISchemas())
        {
            auto it = m_adapters.find(apiType);
            if (isAdapterExist(apiType))
            {
                return apiType;
            }
        }

        if (isAdapterExist(typeInfo.GetTypeName()))
        {
            return typeInfo.GetTypeName();
        }
        if (isAdapterExist(typeInfo.GetSchemaTypeName()))
        {
            return typeInfo.GetSchemaTypeName();
        }

        return PXR_NS::TfToken(g_typeName.data());
    }

    std::vector<PXR_NS::TfToken> PrimTranslator::registeredAdapters() const
    {
        std::vector<PXR_NS::TfToken> out;
        for (auto item : m_adapters)
        {
            out.push_back(item.first);
        }

        return out;
    }

    std::vector<PXR_NS::TfToken> PrimTranslator::registeredUIAdapters() const
    {
        std::vector<PXR_NS::TfToken> out;
        for (auto item : m_uiAdapters)
            out.push_back(item.first);

        return out;
    }

    PrimTranslator::~PrimTranslator()
    {
    }

    class DefaultPrimAdapter : public IPrimAdapter
    {
    public:
        DefaultPrimAdapter(PXR_NS::UsdPrim prim) :
            IPrimAdapter(prim)
        {
        }

        bool isValid() const override
        {
            return !!m_obj;
        }

        nau::async::Task<> update() override
        {
            if (!m_obj)
            {
                co_return;
            }

            translateWorldTransform(getPrim(), *m_obj);
        }

        std::string_view getType() const override
        {
            return g_typeName;
        }

        nau::async::Task<nau::scene::ObjectWeakRef<nau::scene::SceneObject>> initializeSceneObject(nau::scene::ObjectWeakRef<nau::scene::SceneObject> dest) override
        {
            auto& sceneFactory = getServiceProvider().get<scene::ISceneFactory>();
            auto newChild = sceneFactory.createSceneObject();
            m_obj = *newChild;
            const auto name = getPrim().GetName().GetString();
            m_obj->setName({name.data(), name.length()});

            co_await update();
            co_return co_await dest->attachChildAsync(std::move(newChild));
        }

        nau::scene::ObjectWeakRef<nau::scene::SceneObject> getSceneObject() const override
        {
            return m_obj;
        }

    protected:
        void destroySceneObject() override
        {
            m_obj->destroy();
        }

    private:
        nau::scene::ObjectWeakRef<nau::scene::SceneObject> m_obj = nullptr;
    };
    DEFINE_TRANSLATOR(DefaultPrimAdapter, PXR_NS::TfToken(g_typeName.data()));

    class DefaultUIPrimAdapter : public IUIPrimAdapter
    {
    public:
        DefaultUIPrimAdapter(PXR_NS::UsdPrim prim) :
            IUIPrimAdapter(prim)
        {
        }

        bool isValid() const override
        {
            return !!m_node;
        }

        void update() override
        {
        }

        nau::Uid getUid() const override
        {
            return id;
        }

        nau::ui::Node* initializeNode() override
        {
            const auto& name = getPrim().GetName().GetString();
            
            m_node = nau::ui::Node::create();
            m_node->retain();
            m_node->nau_setName(name.c_str());
            id = m_node->getUid();

            return m_node;
        }

        nau::ui::Node* getNode() const override
        {
            return m_node;
        }

        void addChildInternal(nau::ui::Node* node) override
        {
            m_node->addChild(node);
        }

        virtual void serializeChildren(nau::DataBlock &blk) override
        {
            for (auto& [name, adapter] : getChildren())
            {
                if (!adapter)
                {
                    continue;
                }

                nau::DataBlock* childBlock = blk.addNewBlock("element");
                adapter->toBlk(*childBlock);
            }
        }

    protected:
        void destroyNode() override
        {
            m_node->removeFromParent();
            m_node->release();
        }

    private:
        nau::ui::Node* m_node = nullptr;
        nau::Uid id;

    };
    DEFINE_UI_TRANSLATOR(DefaultUIPrimAdapter, "%default"_tftoken);

    inline void translateTransform(GfMatrix4d usdTransform, nau::scene::SceneObject& toObject)
    {
        // decompose matrix
        auto translation = usdTransform.ExtractTranslation();
        auto scale = GfVec3d(usdTransform.GetRow3(0).GetLength(), usdTransform.GetRow3(1).GetLength(), usdTransform.GetRow3(2).GetLength());
        usdTransform.Orthonormalize(false);
        auto rotation = usdTransform.ExtractRotationQuat();

        toObject.setScale({(float)scale[0], (float)scale[1], (float)scale[2]});
        toObject.setTranslation({(float)translation[0], (float)translation[1], (float)translation[2]});
        auto rotxyz = rotation.GetImaginary();
        toObject.setRotation({(float)rotxyz[0], (float)rotxyz[1], (float)rotxyz[2], (float)rotation.GetReal()});
    }

    inline void translateTransform(GfMatrix4d usdTransform, nau::scene::SceneComponent& toComponent)
    {
        // decompose matrix
        auto translation = usdTransform.ExtractTranslation();
        auto scale = GfVec3d(usdTransform.GetRow3(0).GetLength(), usdTransform.GetRow3(1).GetLength(), usdTransform.GetRow3(2).GetLength());
        usdTransform.Orthonormalize(false);
        auto rotation = usdTransform.ExtractRotationQuat();

        toComponent.setScale({(float)scale[0], (float)scale[1], (float)scale[2]});
        toComponent.setTranslation({(float)translation[0], (float)translation[1], (float)translation[2]});
        auto rotxyz = rotation.GetImaginary();
        toComponent.setRotation({(float)rotxyz[0], (float)rotxyz[1], (float)rotxyz[2], (float)rotation.GetReal()});
    }

    void translateWorldTransform(PXR_NS::UsdPrim fromPrim, nau::scene::SceneObject& toObject)
    {
        PXR_NS::UsdGeomXformCache cache;
        bool resetsXformStack = false;
        translateTransform(cache.GetLocalTransformation(fromPrim, &resetsXformStack), toObject);
    }

    void translateWorldTransform(PXR_NS::UsdPrim fromPrim, nau::scene::SceneComponent& toComponent)
    {
        PXR_NS::UsdGeomXformCache cache;
        bool resetsXformStack = false;
        translateTransform(cache.GetLocalTransformation(fromPrim, &resetsXformStack), toComponent);
    }
}  // namespace UsdTranslator
