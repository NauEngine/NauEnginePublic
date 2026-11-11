// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "render_pipeline/billboards_manager.h"
#include "graphics_impl.h"
#include <EASTL/algorithm.h>

namespace nau
{
    struct BillboardData
    {
        nau::math::Vector4 worldPositionScPercentsize;
        nau::Uid uid;
        nau::math::float4 aspectRatio;
    };


    BillboardsManager::BillboardsManager(nau::Ptr<nau::MaterialAssetView> material)
    {
        m_billboardMaterial = material;
        dataBuffer = d3d::create_cb(sizeof(BillboardData), SBCF_DYNAMIC, u8"billboard buffer");
    }

    BillboardsManager::~BillboardsManager()
    {
        if (dataBuffer)
        {
            dataBuffer->destroy();
            dataBuffer = nullptr;
        }
    }


    nau::async::Task<eastl::shared_ptr<nau::BillboardHandle>> BillboardsManager::addBillboard(
        ReloadableAssetView::Ptr texture,
        nau::math::Vector3 position, 
        nau::Uid uid,
        float screenPercentageSize)
    {
        auto& graphics = getServiceProvider().get<GraphicsImpl>();
        ASYNC_SWITCH_EXECUTOR(graphics.getPreRenderExecutor());

        eastl::shared_ptr<nau::BillboardHandle> billboard = eastl::make_shared<nau::BillboardHandle>();
        billboard->setWorldPos(position);
        billboard->setTexture(texture);
        billboard->setScreenPercentageSize(screenPercentageSize);
        billboard->setUid(uid);
        
        m_billboards.emplace_back(billboard->m_billboard);

        co_return billboard;
    }

    void BillboardsManager::render(const math::Matrix4& viewProj, float aspectRatio, eastl::span<const Uid> excluded)
    {
        if (m_billboards.size() == 0)
        {
            return;
        }

        NAU_ASSERT(m_billboardMaterial);

        nau::shader_globals::setVariable("vp", &viewProj);

        d3d::setvsrc(0, nullptr, 0);
        d3d::setind(nullptr);

        BillboardData data;
        data.aspectRatio = nau::math::float4(aspectRatio, 0.0f, 0.0f, 0.0f);

        for (const auto billboard : m_billboards)
        {
            if (const auto bill = billboard.lock())
            {
                if (!bill->isVisible || eastl::find(excluded.begin(), excluded.end(), bill->uid) != excluded.end())
                {
                    continue;
                }

                NAU_ASSERT(bill->texture);
                nau::Ptr<TextureAssetView> textureView;
                bill->texture->getTyped<TextureAssetView>(textureView);
                NAU_ASSERT(textureView->getTexture());

                data.worldPositionScPercentsize = nau::math::Vector4(bill->worldPosition, bill->screenPercentageSize);
                data.uid = bill->uid;

                dataBuffer->updateDataWithLock(0, sizeof(BillboardData), &data, VBLOCK_DISCARD);

                m_billboardMaterial->setCBuffer("default", "SB_BillboardBuffer", dataBuffer);
                m_billboardMaterial->setTexture("default", "tex", textureView->getTexture());

                m_billboardMaterial->bind();

                d3d::draw(PRIM_TRISTRIP, 0, 2);
            }
            else
            {
                m_isBillboardsDirty = true;
            }
        }
    }

    RenderList::Ptr BillboardsManager::getRenderList(const nau::math::Vector3& viewerPosition,
        eastl::function<bool(const InstanceInfo&)>& filterFunc,
        eastl::function<bool(const nau::MaterialAssetView::Ptr)>& materialFilter, eastl::span<const Uid> excluded)
    {
        NAU_FAILURE("Not implemented yet!");
        return RenderList::Ptr();
    }

    void BillboardsManager::update()
    {
        if (m_isBillboardsDirty)
        {
            eastl::erase_if(m_billboards, [](auto& ptr) {
                return ptr.expired();
            });

            m_isBillboardsDirty = false;
        }
    }

    BillboardHandle::BillboardHandle()
    {
        m_billboard = eastl::make_shared<BillboardsManager::BillboardInfo>();
    }

    bool BillboardHandle::isValid() const
    {
        return static_cast<bool>(m_billboard);
    }

    void BillboardHandle::setWorldPos(const nau::math::Vector3& position)
    {
        m_billboard->worldPosition = position;
    }

    nau::math::Vector3 BillboardHandle::getWorldPos()
    {
        return m_billboard->worldPosition;
    }

    void BillboardHandle::setScreenPercentageSize(float screenPercentageSize)
    {
        m_billboard->screenPercentageSize = screenPercentageSize;
    }

    float BillboardHandle::getScreenPercentageSize()
    {
        return m_billboard->screenPercentageSize;
    }

    void BillboardHandle::setTexture(ReloadableAssetView::Ptr texture)
    {
        m_billboard->texture = texture;
    }

    nau::Ptr<nau::TextureAssetView> BillboardHandle::getTexture()
    {
        nau::Ptr<TextureAssetView> textureView;
        m_billboard->texture->getTyped<TextureAssetView>(textureView);
        return textureView;
    }

    void BillboardHandle::setVisibility(bool isVisible)
    {
        m_billboard->isVisible = isVisible;
    }

    bool BillboardHandle::getVisibility() const
    {
        return m_billboard->isVisible;
    }

    void BillboardHandle::setUid(const nau::Uid& uid)
    {
        m_billboard->uid = uid;
    }

    nau::Uid BillboardHandle::getUid() const
    {
        return m_billboard->uid;
    }

    BillboardHandle::~BillboardHandle()
    {
        m_billboard.reset();
    }

} //namespace nau
