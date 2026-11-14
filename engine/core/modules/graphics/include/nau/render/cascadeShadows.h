// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include <EASTL/array.h>
#include <EASTL/fixed_function.h>

#include "nau/math/dag_color.h"
#include "nau/math/dag_IBBox2.h"
#include "nau/math/dag_IBBox3.h"
#include "nau/math/dag_frustum.h"
#include "nau/3d/dag_drv3d.h"
#include "graphics_assets/material_asset.h"


namespace nau::csm
{
    typedef eastl::fixed_function<sizeof(void *) * 4, void(int num_cascades_to_render, bool clear_per_view)> csm_render_cascades_cb_t;


    class ICascadeShadowsClient
    {
    public:
	    virtual void renderCascadeShadowDepth(int cascade, const nau::math::Vector2 &znzf) = 0;
        virtual void prepareRenderShadowCascades(){};
        virtual void getCascadeShadowAnchorPoint(float cascade_from, nau::math::Vector3& out_anchor) = 0;
        virtual void getCascadeShadowSparseUpdateParams(int cascade_no, const nau::math::NauFrustum& cascade_frustum, float &out_min_sparse_dist,
        int &out_min_sparse_frame) = 0;
    };

    class CascadeShadowsPrivate;

    class CascadeShadows
    {
    public:
        static constexpr unsigned MAX_CASCADES = 6;
        static constexpr unsigned SSSS_CASCADES = 3;

        struct Settings
        {
            unsigned cascadeWidth = 2048;
            bool cascadeDepthHighPrecision = false;
            unsigned splitsW = 2;
            unsigned splitsH = 2;
            float fadeOutMul = 1;
            float shadowFadeOut = 10.f;
            float shadowDepthBias = 0.01;
            float shadowConstDepthBias = 0.00002;
            float shadowDepthSlopeBias = 0.83;
            float zRangeToDepthBiasScale = 1e-4;
            // Skip rendering to CSM any destructable whose bounding box radius is less than
            // (static shadow texel size) * (this multiplier)
            float destructablesMinBboxRadiusTexelMul = 0.f;

            float minimalSparseDistance = 100000.0f;
            float minimalSparseFrame = -1000.0f;
        };

        struct ModeSettings
        {
            ModeSettings();

            float powWeight;                /** < Alpha value used to linearly interpolate between linear and logarithmic values when calculating distances between cascades. */
            float maxDist;                  /** < Max distance from the camera at which shadows are visible*/
            float shadowStart;              /** < */
            unsigned numCascades;           /** < Actual number of cascades to use.*/
            float shadowCascadeZExpansion;  /** < */
            float shadowCascadeRotationMargin; /** < */
            float cascade0Dist;             /** < If positive, might be used to override nearest cascade distance. This might contribute to higher quality shadows very close to camera.*/
            // if cascade0Dist is >0, it will be used as minimum of auto calculated cascade dist and this one. This is to
            // artificially create high quality cascade for 'cockpit'
            float overrideZNearForCascadeDistribution; /** < If positive, overrides near plane z-value for the nearest cascade. */
        };

        /** Cascade rectangle in the shadow atlas, in pixels: the d3d::setview arguments used by renderShadowCascadeDepth. */
        struct CascadeViewport
        {
            int x = 0;
            int y = 0;
            int width = 0;
            int height = 0;
            float minZ = 0.f;
            float maxZ = 1.f;
        };

        /** Shadow atlas texture (getShadowsCascade()). */
        struct AtlasInfo
        {
            int width = 0;
            int height = 0;
            uint32_t format = 0; /** < TEXFMT_DEPTH16 or TEXFMT_DEPTH32. */
        };

        /** Contents of the SB_CSMBuffer constant buffer (CSMBuffer in csm.hlsli), in its layout. */
        struct ShaderConstants
        {
            nau::math::Vector4 pcfLerp;
            eastl::array<nau::math::Color4, MAX_CASCADES * 4> shadowCascadeTmTransp;
            eastl::array<nau::math::Color4, MAX_CASCADES> shadowCascadeTcMulOffset;
        };

        /** Shader constants of one frame, to be bound later together with the atlas rendered for that frame. */
        struct ShaderSnapshot
        {
            ShaderConstants constants = {};
            int numCascades = 0; /** < getNumCascadesToRender() of that frame. */
            AtlasInfo atlas;     /** < Atlas the constants were computed for. */
        };


    private:
        //CascadeShadows(ICascadeShadowsClient* in_client, const Settings& in_settings);

    public:
        static CascadeShadows* make(ICascadeShadowsClient* client, const Settings& settings);
        ~CascadeShadows();

        void prepareShadowCascades(const CascadeShadows::ModeSettings& modeSettings, const nau::math::Vector3& invLightDir,
            const nau::math::Matrix4& view, const nau::math::Vector3& cameraPos, const nau::math::Matrix4& proj,
            const nau::math::NauFrustum& viewFrustum, const nau::math::Vector2& sceneNearFarZ, float nearZForCascadeDistribution);
        const Settings& getSettings() const;
        void setDepthBiasSettings(const Settings& set);
        void setCascadeWidth(int cascadeWidth);
        void renderShadowsCascades();
        void renderShadowsCascadesCb(csm_render_cascades_cb_t cb);
        void renderShadowCascadeDepth(int cascadeNo, bool clearPerView);

        void setCascadesToShader(MaterialAssetView::Ptr resolveMaterial);

        // Rendering the cascades outside of Nau. Valid after prepareShadowCascades of the frame; use together with
        // getWorldRenderMatrix() and shouldUpdateCascade().
        CascadeViewport getCascadeViewport(int cascade_no) const;
        void getCascadeDepthBias(int cascade_no, float& const_bias, float& slope_bias) const; // zBias and slopeZBias of the cascade override state
        AtlasInfo getAtlasInfo() const;

        // Constants setCascadesToShader(resolveMaterial) would upload now (call after prepareShadowCascades).
        ShaderSnapshot snapshotShaderConstants();
        // Same as setCascadesToShader(resolveMaterial), but uploads the constants of the snapshot.
        void setCascadesToShader(MaterialAssetView::Ptr resolveMaterial, const ShaderSnapshot& snapshot);

        void disable();
        bool isEnabled() const;
        void invalidate(); // Reset sparse counters.

        int getNumCascadesToRender() const;
        const nau::math::NauFrustum& getFrustum(int cascade_no) const;
        const nau::math::Vector3& getRenderCameraWorldViewPos(int cascade_no) const;
        const nau::math::Matrix4& getShadowViewItm(int cascade_no) const;
        const nau::math::Matrix4& getCameraRenderMatrix(int cascade_no) const;
        const nau::math::Matrix4& getWorldCullingMatrix(int cascade_no) const;
        const nau::math::Matrix4& getWorldRenderMatrix(int cascade_no) const;
        const nau::math::Matrix4& getRenderViewMatrix(int cascade_no) const;
        const nau::math::Matrix4& getRenderProjMatrix(int cascade_no) const;
        const nau::math::Vector3& shadowWidth(int cascade_no) const;
        const nau::math::BBox3& getWorldBox(int cascade_no) const;
        bool shouldUpdateCascade(int cascade_no) const;
        bool isCascadeValid(int cascade_no) const;
        void copyFromSparsed(int cascade_no);
        float getMaxDistance() const;
        float getCascadeDistance(int cascade_no) const;
        float getMaxShadowDistance() const;
        const nau::math::NauFrustum& getWholeCoveredFrustum() const;
        BaseTexture* getShadowsCascade() const;
        const nau::math::Vector2& getZnZf(int cascade_no) const;

        const eastl::string& setShadowCascadeDistanceDbg(const nau::math::Vector2& scene_z_near_far, int tex_size, int splits_w, int splits_h,
            float shadow_distance, float pow_weight);

        void debugSetParams(float shadow_depth_bias, float shadow_const_depth_bias, float shadow_depth_slope_bias);

        void debugGetParams(float& shadow_depth_bias, float& shadow_const_depth_bias, float& shadow_depth_slope_bias);
        void setNeedSsss(bool need_ssss);

    private:
        CascadeShadowsPrivate* d;
    };
}//namespace nau::render::csm