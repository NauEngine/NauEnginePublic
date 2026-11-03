// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "render_pipeline/render_manager.h"


namespace nau
{
    class BillboardHandle;

    class BillboardsManager : public IRenderManager
    {
        NAU_CLASS_(nau::BillboardsManager, IRenderManager);

        struct BillboardInfo
        {
            nau::math::Vector3 worldPosition;
            ReloadableAssetView::Ptr texture;
            float screenPercentageSize;
            bool isVisible;
            nau::Uid uid;
        };

    public:
        using Ptr = nau::Ptr<BillboardsManager>;

        BillboardsManager(nau::Ptr<nau::MaterialAssetView> material);
        ~BillboardsManager();

        nau::async::Task<eastl::shared_ptr<nau::BillboardHandle>> addBillboard(ReloadableAssetView::Ptr texture, nau::math::Vector3 position, nau::Uid uid, float screenPercentageSize = 0.1f);

        void render(const math::Matrix4& viewProj, float aspectRatio, eastl::span<const Uid> excluded);

        // Inherited via IRenderManager
        RenderList::Ptr getRenderList(const nau::math::Vector3& viewerPosition,
            eastl::function<bool(const InstanceInfo&)>& filterFunc,
            eastl::function<bool(const nau::MaterialAssetView::Ptr)>& materialFilter, eastl::span<const Uid> excluded) override;

        void update() override;

    protected:
        eastl::vector<eastl::weak_ptr<BillboardInfo>> m_billboards;

        nau::Ptr<nau::MaterialAssetView> m_billboardMaterial;
        bool m_isBillboardsDirty = false;

        Sbuffer* dataBuffer = nullptr;
    };

    class BillboardHandle
    {
    public:
        using Ptr = eastl::shared_ptr<BillboardHandle>;

        BillboardHandle();

        bool isValid() const;

        void setWorldPos(const nau::math::Vector3& position);
        nau::math::Vector3 getWorldPos();

        void setScreenPercentageSize(float screenPercentageSize);
        float getScreenPercentageSize();

        void setTexture(ReloadableAssetView::Ptr texture);
        nau::Ptr<nau::TextureAssetView> getTexture();

        void setVisibility(bool isVisible);
        bool getVisibility() const;

        void setUid(const nau::Uid& uid);
        nau::Uid getUid() const;


        ~BillboardHandle();

    private:
        eastl::shared_ptr<BillboardsManager::BillboardInfo> m_billboard;

        BillboardsManager::Ptr m_manager;

        friend class BillboardsManager;
    };
} // namespace nau

