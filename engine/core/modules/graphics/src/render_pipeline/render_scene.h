// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "render_manager.h"
#include "render_view.h"
#include "billboards_manager.h"


#include "nau/math/math.h"
#include "nau/render/cascadeShadows.h"


namespace nau
{
    class BillboardsManager;

    using RenderViews = eastl::vector<eastl::shared_ptr<RenderView>>;

    class RenderScene : public  IRefCounted
    {
        NAU_CLASS_(nau::RenderScene, IRefCounted);
    public:
        using Ptr = nau::Ptr<RenderScene>;

        async::Task<> initialize();

        void addManager(nau::Ptr<IRenderManager> manager);

        template<class T>
        nau::Ptr<T> getManagerTyped()
        {
            for (auto manager : m_managers)
            {
                if (manager->is<T>())
                {
                    return manager;
                }
            }
            return nullptr;
        }

        nau::Ptr<BillboardsManager> getBillboardsManager();

        void updateView(RenderView& view, const math::Matrix4& vp, const math::Vector3& cameraPosition, eastl::span<const Uid> excluded);
        void updateManagers();
        void renderScene(const RenderViews& views, const math::Matrix4& vp);
        void renderDepth(const RenderViews& views, const math::Matrix4& vp);
        void renderTranslucency(const RenderViews& views, const math::Matrix4& vp);
        void renderOutlineMask(const RenderViews& views, const math::Matrix4& vp);

        void renderBillboards(const math::Matrix4& vp, float aspectRatio, eastl::span<const Uid> excluded);

        MaterialAssetView::Ptr getZPrepassMaterial();

        struct Tags
        {
            static constexpr RenderTag opaqueTag = nau::strings::constHash("opaque");
            static constexpr RenderTag translucentTag = nau::strings::constHash("translucent");
            static constexpr RenderTag shadowCascadeTag = nau::strings::constHash("shadow_cascade");
        };

    private:
        eastl::vector<nau::Ptr<IRenderManager>> m_managers;

        nau::Ptr<BillboardsManager> m_billboardsManager;

        MaterialAssetView::Ptr m_zPrepassMaterial;
        MaterialAssetView::Ptr m_outlineMaterial;

        friend class RendferWindowImpl;
    };
}  // namespace nau
