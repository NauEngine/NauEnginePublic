// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "nau/render/cascadeShadows.h"
#include "nau/3d/dag_drv3d.h"
#include "nau/3d/dag_resPtr.h"
#include "nau/math/dag_color.h"
#include "nau/debugRenderer/debug_render_system.h"
#include "nau/shaders/shader_globals.h"

using namespace nau::math;

namespace {
    inline Matrix4 screen_to_tex_scale_tm_xy(float texelOfsX, float texelOfsY)
    {
        return Matrix4(
            Vector4(0.5f, 0.0f, 0.0f, 0.0f),
            Vector4(0.0f, -0.5f, 0.0f, 0.0f),
            Vector4(0.0f, 0.0f, 1.0f, 0.0f),
            Vector4(0.5f + texelOfsX, 0.5f + texelOfsY, 0.0f, 1.0f));
    }

    inline __m128 v_perm_xyzd(__m128 xyzw, __m128 abcd) // TODO: move this functions and functions from NauFrustum to separate file
    {
        __m128 zzdd = _mm_shuffle_ps(xyzw, abcd, _MM_SHUFFLE(3, 3, 2, 2));
        return _mm_shuffle_ps(xyzw, zzdd, _MM_SHUFFLE(3, 0, 1, 0));
    }

    inline __m128 v_perm_xycw(__m128 xyzw, __m128 abcd)
    {
        __m128 wwcc = _mm_shuffle_ps(xyzw, abcd, _MM_SHUFFLE(2, 2, 3, 3));
        return _mm_shuffle_ps(xyzw, wwcc, _MM_SHUFFLE(0, 3, 1, 0));
    }

    inline __m128 v_min(__m128 a, __m128 b)
    {
        return _mm_min_ps(a, b);
    }

    inline __m128 v_max(__m128 a, __m128 b)
    {
        return _mm_max_ps(a, b);
    }


    constexpr float VerySmallNumber = ((real)4e-19);

    float safediv(float a, float b) { return b > VerySmallNumber ? a / b : (b < -VerySmallNumber ? a / b : 0.f); }
}

float shadow_render_expand_mul = 1.0f; // up to 1.f for full shadow length expansion if any occur.
float shadow_render_expand_to_sun_mul = 0.0f;
float shadow_render_expand_from_sun_mul = 0.0f;


#define SHADOW_CULLING_POS_EXPAND_MUL 0.0f   // No artifacts without expansions, multipliers can be increased
// if we support different culling from rendering matrix - shadow_render_expand_mul should be 0

#define SHADOW_FAR_CASCADE_DEPTH_MUL 2.0f   // Trade more shadow distance for less depth quality. 
                                            // Is roughly a projection of the last cascade width to virtual ground in light space.

#define USE_SHADOW_DEPTH_CLAMP 1

#define SHADOW_SAMPLING_MAX_OFFSET 2.5f 
        // 2 texels from FXAA, 0.5 texels from PCF. Should be multiplied by sqrt(2) for the worst case, but looks good even multiplied
        // by shadowDepthSlopeBias.

namespace nau::csm
{

    static const bool SHADOW_ROTATION_STABILITY = false;

    /**
     * @brief Forces to redraw cascade shadow maps each frame.
     */
    static const bool FORCE_UPDATE_SHADOWS = false;

    /**
     * @brief Forces to not redraw cascade shadow maps. It is stronger than FORCE_UPDATE_SHADOWS.
     */
    static const bool FORCE_NO_UPDATE_SHADOWS = false;

    class CascadeShadowsPrivate
    {
    public:
        CascadeShadowsPrivate(ICascadeShadowsClient* in_client, const CascadeShadows::Settings& in_settings);

        ~CascadeShadowsPrivate();

        void prepareShadowCascades(const CascadeShadows::ModeSettings& mode_settings, const Vector3& dir_to_sun, const Matrix4& view_matrix,
            const Vector3& camera_pos, const Matrix4& proj_tm, const NauFrustum& view_frustum, const Vector2& scene_z_near_far,
            float z_near_for_cascade_distribution);

        const CascadeShadows::Settings& getSettings() const { return settings; }
        void setDepthBiasSettings(const CascadeShadows::Settings& set)
        {
            settings.shadowDepthBias = set.shadowDepthBias;
            settings.shadowConstDepthBias = set.shadowConstDepthBias;
            settings.shadowDepthSlopeBias = set.shadowDepthSlopeBias;
            settings.zRangeToDepthBiasScale = set.zRangeToDepthBiasScale;
        }
        void setCascadeWidth(int width)
        {
            if (settings.cascadeWidth != width)
            {
                settings.cascadeWidth = width;
                createDepthShadow(settings.splitsW, settings.splitsH, settings.cascadeWidth, settings.cascadeWidth,
                    settings.cascadeDepthHighPrecision);
            }
        }
        void renderShadowsCascades();
        void renderShadowsCascadesCb(csm_render_cascades_cb_t render_cascades_cb);
        void renderShadowCascadeDepth(int cascadeNo, bool clearPerView);
        void calcTMs();
        void setCascadesToShader(MaterialAssetView::Ptr resolveMaterial);
        CascadeShadows::ShaderSnapshot snapshotShaderConstants();
        void setCascadesToShader(MaterialAssetView::Ptr resolveMaterial, const CascadeShadows::ShaderSnapshot& snapshot);
        CascadeShadows::CascadeViewport getCascadeViewport(int cascade_no) const
        {
            const IBBox2& viewport = shadowSplits[cascade_no].viewport;
            CascadeShadows::CascadeViewport result;
            result.x = viewport[0].getX();
            result.y = viewport[0].getY();
            result.width = viewport.width().getX();
            result.height = viewport.width().getY();
            return result;
        }
        void getCascadeDepthBias(int cascade_no, float& const_bias, float& slope_bias) const
        {
            NAU_ASSERT(cascade_no >= 0 && unsigned(cascade_no) < settings.splitsW * settings.splitsH);
            const shaders::OverrideState state = shaders::overrides::get(cascadeOverride[cascade_no]);
            const_bias = state.zBias;
            slope_bias = state.slopeZBias;
        }
        CascadeShadows::AtlasInfo getAtlasInfo() const
        {
            CascadeShadows::AtlasInfo info;
            if (shadowCascades)
            {
                TextureInfo texInfo;
                shadowCascades->getinfo(texInfo);
                info.width = texInfo.w;
                info.height = texInfo.h;
                info.format = texInfo.cflg & TEXFMT_MASK;
            }
            return info;
        }
        void disable() { numCascadesToRender = 0; }
        bool isEnabled() const { return numCascadesToRender != 0; }
        void invalidate()
        {
            for (unsigned int cascadeNo = 0; cascadeNo < shadowSplits.size(); cascadeNo++)
            {
                shadowSplits[cascadeNo].frames = 0xFFFF;
            }
        }

        int getNumCascadesToRender() const { return numCascadesToRender; }
        const Vector2& getZnZf(int cascade_no) const { return shadowSplits[cascade_no].znzf; }
        const NauFrustum& getFrustum(int cascade_no) const { return shadowSplits[cascade_no].frustum; }
        const Vector3& getRenderCameraWorldViewPos(int cascade_no) const { return shadowSplits[cascade_no].viewPos; }
        const Matrix4& getShadowViewItm(int cascade_no) const { return shadowSplits[cascade_no].shadowViewItm; }
        const Matrix4& getCameraRenderMatrix(int cascade_no) const { return shadowSplits[cascade_no].cameraRenderMatrix; }
        const Matrix4& getWorldCullingMatrix(int cascade_no) const { return shadowSplits[cascade_no].worldCullingMatrix; }
        const Matrix4& getWorldRenderMatrix(int cascade_no) const { return shadowSplits[cascade_no].worldRenderMatrix; }

        const Matrix4& getRenderViewMatrix(int cascade_no) const { return shadowSplits[cascade_no].renderViewMatrix; }
        const Matrix4& getRenderProjMatrix(int cascade_no) const { return shadowSplits[cascade_no].renderProjMatrix; }
        const Vector3& shadowWidth(int cascade_no) const { return shadowSplits[cascade_no].shadowWidth; }

        const BBox3& getWorldBox(int cascade_no) const { return shadowSplits[cascade_no].worldBox; }
        bool shouldUpdateCascade(int cascade_no) const { return shadowSplits[cascade_no].shouldUpdate; }
        bool isCascadeValid(int cascade_no) const { return shadowSplits[cascade_no].to > shadowSplits[cascade_no].from; }

        void copyFromSparsed(int cascade_no)
        {
            shadowSplits[cascade_no] = sparsedShadowSplits[cascade_no];
            shadowSplits[cascade_no].frustum.construct(shadowSplits[cascade_no].worldCullingMatrix);
        }

        float getMaxDistance() const { return modeSettings.maxDist; }

        float getMaxShadowDistance() const { return csmDistance; }

        float getCascadeDistance(int cascade_no) const { return shadowSplits[cascade_no].to; }

        const NauFrustum& getWholeCoveredFrustum() const { return wholeCoveredSpaceFrustum; }

        const eastl::string& setShadowCascadeDistanceDbg(const Vector2& scene_z_near_far, int tex_size, int splits_w, int splits_h,
            float shadow_distance, float pow_weight);

        void debugSetParams(float shadow_depth_bias, float shadow_const_depth_bias, float shadow_depth_slope_bias);

        void debugGetParams(float& shadow_depth_bias, float& shadow_const_depth_bias, float& shadow_depth_slope_bias);


        BaseTexture* getShadowsCascade() const { return shadowCascades.get(); }

        void setNeedSsss(bool need_ssss) { needSsss = need_ssss; }

    private:
        struct ShadowSplit
        {
            ShadowSplit() : from(0), to(1), frames(0xFFFF), shouldUpdate(1) {}
            float from, to;
            Vector2 znzf;
            Vector3 shadowWidth;
            Vector3 viewPos; // for shadowFromCamldViewProjMatrix
            Matrix4 shadowViewItm;
            Matrix4 cameraCullingMatrix; // made with camViewTm
            Matrix4 cameraRenderMatrix;
            Matrix4 worldCullingMatrix;
            Matrix4 worldRenderMatrix;
            Matrix4 renderViewMatrix;
            Matrix4 renderProjMatrix;
            NauFrustum frustum;
            BBox3 worldBox;
            IBBox2 viewport;
            uint16_t frames; // how many frames it was not updated
            uint16_t shouldUpdate;
        };

        ICascadeShadowsClient* client;
        CascadeShadows::Settings settings;
        CascadeShadows::ModeSettings modeSettings;
        bool dbgModeSettings;
        NauFrustum wholeCoveredSpaceFrustum;


        ResPtr<BaseTexture> shadowCascades;
        ResPtr<BaseTexture> shadowCascadesFakeRT;

        IVector2 shadowCascadesTexInfo;
        d3d::SamplerHandle csmSampler;

        d3d::RenderPass* mobileAreaUpdateRP;

        int numCascadesToRender = 0;
        eastl::array<ShadowSplit, CascadeShadows::MAX_CASCADES> shadowSplits;
        eastl::array<ShadowSplit, CascadeShadows::MAX_CASCADES> sparsedShadowSplits;
        eastl::array<Color4, CascadeShadows::MAX_CASCADES * 3> shadowCascadeTm; // ???

        float csmDistance = 0.0f;
        eastl::array<shaders::UniqueOverrideStateId, CascadeShadows::MAX_CASCADES> cascadeOverride;
        void destroyOverrides();
        void createOverrides();

        //void createMobileRP(uint32_t depth_fmt, uint32_t rt_fmt);
        void createDepthShadow(int splits_w, int splits_h, int width, int height, bool high_precision_depth);
        void closeDepthShadow();
        IBBox2 getViewPort(int cascade, const IVector2& tex_width) const;
        Matrix4 getShadowViewMatrix(const Vector3& dir_to_sun, const Vector3& camera_pos, bool world_space);
        void setFadeOutToShaders(float max_dist);

        void buildShadowProjectionMatrix(uint32_t cascadeNo, const Vector3& dir_to_sun, const Matrix4& view_matrix, const Vector3& camera_pos,
            const Matrix4& projtm, float z_near, float z_far, float next_z_far, const Vector3& anchor, ShadowSplit& split);

        Sbuffer* csmBuffer;

        struct csmBufferData
        {
            Vector4 pcfLerp;
            eastl::array<Color4, CascadeShadows::MAX_CASCADES * 4> transposed;
            eastl::array<Color4, CascadeShadows::MAX_CASCADES> shadowCascadeTcMulOffset;
        };

        csmBufferData csmConstData;

        bool needSsss = false;
    };

    void CascadeShadowsPrivate::destroyOverrides()
    {
        for (auto& s : cascadeOverride)
        {
            shaders::overrides::destroy(s);
        }
    }

    void CascadeShadowsPrivate::createOverrides()
    {
        for (int ss = 0; ss < settings.splitsW * settings.splitsH; ++ss)
        {
            IBBox2 viewport = getViewPort(ss, shadowCascadesTexInfo);
            shaders::OverrideState state;
            state.set(shaders::OverrideState::Z_BIAS);
            state.set(shaders::OverrideState::Z_CLAMP_ENABLED);
            state.set(shaders::OverrideState::Z_FUNC);
            state.zFunc = CMPF_LESSEQUAL;
            state.slopeZBias = settings.shadowDepthSlopeBias * SHADOW_SAMPLING_MAX_OFFSET;
            state.zBias = settings.shadowConstDepthBias + settings.shadowDepthBias / viewport.width().getX();
            shaders::OverrideState oldState = shaders::overrides::get(cascadeOverride[ss]);
            if (oldState.zBias == state.zBias && oldState.slopeZBias == state.slopeZBias && oldState.bits == state.bits) // optimized of if
                // (oldState == state)
                continue;
            cascadeOverride[ss].reset(shaders::overrides::create(state)); // will just increase reference if same
        }
    }


    CascadeShadowsPrivate::CascadeShadowsPrivate(ICascadeShadowsClient* in_client, const CascadeShadows::Settings& in_settings) :
        client(in_client), settings(in_settings), dbgModeSettings(false), mobileAreaUpdateRP(nullptr)
    {
        NAU_ASSERT(client);

        d3d::SamplerInfo csmSamplerInfo;
        csmSamplerInfo.isCompare = true;
        csmSamplerInfo.mip_map_mode = d3d::MipMapMode::Point;
        csmSamplerInfo.filter_mag_mode = d3d::FilterMode::Linear;
        csmSamplerInfo.filter_min_mode = d3d::FilterMode::Linear;
        csmSamplerInfo.address_mode_u = d3d::AddressMode::Clamp;
        csmSamplerInfo.address_mode_v = d3d::AddressMode::Clamp;
        csmSamplerInfo.address_mode_w = d3d::AddressMode::Clamp;

        csmSampler = d3d::create_sampler(csmSamplerInfo);

        //nau::shader_globals::addStruct("shadow_cascade_tm_transp", CascadeShadows::MAX_CASCADES * 4 * sizeof(Color4), nullptr);
        //nau::shader_globals::addStruct("shadow_cascade_tc_mul_offset", CascadeShadows::MAX_CASCADES * sizeof(Color4), nullptr);

        NAU_ASSERT(settings.splitsW * settings.splitsH <= CascadeShadows::MAX_CASCADES);
        createDepthShadow(settings.splitsW, settings.splitsH, settings.cascadeWidth, settings.cascadeWidth,
            settings.cascadeDepthHighPrecision);

        int csmBufferSize = sizeof(Vector4) + CascadeShadows::MAX_CASCADES * 4 * sizeof(Color4) + CascadeShadows::MAX_CASCADES * sizeof(Color4);
        csmBuffer = d3d::create_cb(csmBufferSize, SBCF_DYNAMIC, u8"csm buffer");

        csmConstData = {};
        csmConstData.pcfLerp = Vector4(0.00049f, 0.00049f, 0.00098f, 0.00098f);
    }

    void CascadeShadowsPrivate::createDepthShadow(int splits_w, int splits_h, int width, int height, bool high_precision_depth)
    {
        closeDepthShadow();

        const uint32_t format = high_precision_depth ? TEXFMT_DEPTH32 : TEXFMT_DEPTH16;
        shadowCascades = dag::create_tex(NULL, splits_w * width, splits_h * height,
            format | TEXCF_RTARGET | TEXCF_TC_COMPATIBLE, 1, "shadowCascadeDepthTex2D");

        if (d3d::get_driver_desc().issues.hasRenderPassClearDataRace)
        {
            NAU_FAILURE();
        }

        d3d_err(shadowCascades.get());
        TextureInfo texInfo;
        shadowCascades->getinfo(texInfo);
        shadowCascadesTexInfo.setX(texInfo.w);
        shadowCascadesTexInfo.setY(texInfo.h);
        NAU_LOG_DEBUG("2d texture for shadows created");
        shadowCascades->texfilter(TEXFILTER_COMPARE);
        shadowCascades->texaddr(TEXADDR_CLAMP);

        // sometimes we use this target as SRV while not writing something to it
        // causing it be in initial clear RT/DS state
        d3d::resource_barrier({ shadowCascades.get(), RB_RO_SRV | RB_STAGE_PIXEL, 0, 0 });
    }


    CascadeShadowsPrivate::~CascadeShadowsPrivate()
    {
        closeDepthShadow();
        destroyOverrides();

        if (csmBuffer)
        {
            csmBuffer->destroy();
            csmBuffer = nullptr;
        }
    }


    void CascadeShadowsPrivate::closeDepthShadow()
    {
        shadowCascades.close();
        shadowCascadesFakeRT.close();
    }


    void CascadeShadowsPrivate::renderShadowsCascadesCb(csm_render_cascades_cb_t render_cascades_cb)
    {
        //SCOPE_RENDER_TARGET;
        //SCOPE_VIEW_PROJ_MATRIX;

        d3d::set_render_target();
        d3d::set_render_target(nullptr, 0);
        bool clearPerView = false;
        for (int cascadeNo = numCascadesToRender - 1; cascadeNo >= 0; cascadeNo--)
        {
            if (!shadowSplits[cascadeNo].shouldUpdate)
            {
                clearPerView = true;
                break;
            }
        }

        d3d::set_depth(shadowCascades.get(), DepthAccess::RW);

        if (!clearPerView)
        {
            d3d::clearview(CLEAR_ZBUFFER, 0, 1.f, 0);
        }

        if (numCascadesToRender == 0)
        {
            return;
        }

        shaders::OverrideStateId curStateId = shaders::overrides::get_current();
        if (curStateId)
        {
            shaders::overrides::reset();
        }

        client->prepareRenderShadowCascades();

        render_cascades_cb(numCascadesToRender, clearPerView);

        if (curStateId)
        {
            shaders::overrides::set(curStateId);
        }

        d3d::set_depth(nullptr, DepthAccess::RW);
        d3d::resource_barrier({ shadowCascades.get(), RB_RO_SRV | RB_STAGE_PIXEL, 0, 0 });
    }

    void CascadeShadowsPrivate::renderShadowsCascades()
    {
        renderShadowsCascadesCb([&](int num_cascades_to_render, bool clear_per_view) {
            for (int i = 0; i < num_cascades_to_render; ++i)
            {
                renderShadowCascadeDepth(i, clear_per_view);
            }
        });
    }

    static void calculate_cascades(float dist, float weight, int cascades, float* distances, float zn)
    {
        for (int sliceIt = 0; sliceIt < cascades; sliceIt++)
        {
            float f = float(sliceIt + 1) / cascades;
            float logDistance = zn * pow(dist / zn, f);
            float uniformDistance = zn + (dist - zn) * f;
            distances[sliceIt] = lerp(uniformDistance, logDistance, weight);
        }
    }


    IBBox2 CascadeShadowsPrivate::getViewPort(int cascade, const IVector2& tex_width) const
    {
        int cascadeWidth = tex_width.getX(), cascadeHeight = tex_width.getY();
        cascadeWidth /= settings.splitsW;
        cascadeHeight /= settings.splitsH;
        IBBox2 view;
        view[0] = IVector2((cascade % settings.splitsW) * cascadeWidth, (cascade / settings.splitsW) * cascadeHeight);
        view[1] = view[0] + IVector2(cascadeWidth, cascadeHeight);
        return view;
    }


    bool force_no_update_shadows = false;
    bool force_update_shadows = false;

    void CascadeShadowsPrivate::prepareShadowCascades(const CascadeShadows::ModeSettings& mode_settings, const Vector3& dir_to_sun,
        const Matrix4& view_matrix, const Vector3& camera_pos, const Matrix4& proj_tm, const NauFrustum& view_frustum,
        const Vector2& scene_z_near_far, float z_near_for_cascade_distribution)
    {
        NAU_ASSERT(client);
        if (!dbgModeSettings)
        {
            modeSettings = mode_settings;
        }

        if (modeSettings.numCascades <= 0)
        {
            numCascadesToRender = 0;
            csmDistance = 0.f;
            return;
        }

        if (modeSettings.overrideZNearForCascadeDistribution >= 0)
        {
            z_near_for_cascade_distribution = modeSettings.overrideZNearForCascadeDistribution;
        }

        NAU_ASSERT(modeSettings.numCascades <= settings.splitsW * settings.splitsH);

        eastl::array<float, CascadeShadows::MAX_CASCADES> distances;
        int cascades = modeSettings.numCascades;
        float znear = scene_z_near_far.getX();
        float shadowStart = max(znear, modeSettings.shadowStart);
        float zNearForCascadeDistribution = max(z_near_for_cascade_distribution, shadowStart);

        NAU_ASSERT(scene_z_near_far.getX() > 0.f);

        NAU_ASSERT(cascades <= distances.size());
        calculate_cascades(modeSettings.maxDist, modeSettings.powWeight, cascades, distances.data(), zNearForCascadeDistribution);
        numCascadesToRender = cascades;

        if (modeSettings.cascade0Dist > 0.f && cascades > 1)
        {
            distances[0] = min(modeSettings.cascade0Dist + modeSettings.shadowStart, distances[0]);
        }

        for (unsigned int cascadeNo = 0; cascadeNo < numCascadesToRender; cascadeNo++)
        {
            ShadowSplit ss;
            ss.frames = 0;
            ss.from = (cascadeNo > 0) ? distances[cascadeNo - 1] : shadowStart;
            ss.from = max(ss.from, znear);
            ss.to = (cascadeNo < cascades) ? distances[cascadeNo] : scene_z_near_far.getY();
            ss.to = min(ss.to, scene_z_near_far.getY());
            float nextSplitTo = (cascadeNo + 1 < cascades) ? distances[cascadeNo + 1] : SHADOW_FAR_CASCADE_DEPTH_MUL * ss.to;
            nextSplitTo = min(nextSplitTo, scene_z_near_far.getY());

            NAU_ASSERT(ss.to > ss.from);

            ss.viewport = getViewPort(cascadeNo, shadowCascadesTexInfo);

            Vector3 anchor;
            client->getCascadeShadowAnchorPoint(cascadeNo < cascades - 1 ? ss.from : FLT_MAX, anchor); // Do not anchor last cascade to hero -
            // on low settings it is dangerously
            // near.

            buildShadowProjectionMatrix(cascadeNo, dir_to_sun, view_matrix, camera_pos, proj_tm, ss.from, ss.to, nextSplitTo, anchor, ss);

            if (needSsss && cascadeNo < CascadeShadows::SSSS_CASCADES)
            {
                NAU_FAILURE();
            }

            if (force_update_shadows)
            {
                shadowSplits[cascadeNo].frames = 0xFFFF;
            }

            float minSparseDist;
            int minSparseFrame;
            ss.frustum.construct(ss.worldCullingMatrix);
            client->getCascadeShadowSparseUpdateParams(cascadeNo, ss.frustum, minSparseDist, minSparseFrame);

            if ((ss.from < minSparseDist || shadowSplits[cascadeNo].frames >= minSparseFrame + cascadeNo) && !force_no_update_shadows)
            {
                shadowSplits[cascadeNo] = ss;
                sparsedShadowSplits[cascadeNo] = ss;
            }
            else
            {
                bool shouldUpdate = false;
                if (minSparseDist >= 0.f) // Negative value indicates the camera direction may be ignored.
                {
                    NauFrustum shadowFrustum = view_frustum;
                    Vector4 curViewPos = Vector4(ss.viewPos);
                    shadowFrustum.camPlanes[NauFrustum::NEARPLANE] =
                        expand_znear_plane(shadowFrustum.camPlanes[NauFrustum::NEARPLANE], curViewPos, Vector4(ss.from));
                    shadowFrustum.camPlanes[NauFrustum::FARPLANE] =
                        shrink_zfar_plane(shadowFrustum.camPlanes[NauFrustum::FARPLANE], curViewPos, Vector4(ss.to));

                    Vector3 frustumPoints[8];
                    shadowFrustum.generateAllPointFrustm(frustumPoints);

                    for (int pt = 0; pt < 8; ++pt)
                    {
                        Vector3 v_pt = frustumPoints[pt];
                        Vector4 invisible = Vector4(0, 0, 0, 0);
                        for (int plane = 0; plane < 6; ++plane)
                        {
                            invisible = Vector4(
                                _mm_or_ps(invisible.get128(),
                                    Vector4(distFromPlane(Point3(v_pt), shadowSplits[cascadeNo].frustum.camPlanes[plane])).get128()));
                        }
                        //invisible = v_and(invisible, v_msbit());
                        //if (!v_test_vec_x_eqi_0(invisible))
                        if (int(invisible.getX()) != 0.0f)
                        {
                            shouldUpdate = true;
                            break;
                        }
                    }
                }

                sparsedShadowSplits[cascadeNo] = ss;

                if (shouldUpdate && !force_no_update_shadows)
                {
                    shadowSplits[cascadeNo] = ss;
                }
                else
                {
                    shadowSplits[cascadeNo].frames++;
                    shadowSplits[cascadeNo].shouldUpdate = 0;
                    Matrix4 tm = Matrix4::identity();
                    tm.setTranslation(ss.viewPos - shadowSplits[cascadeNo].viewPos);
                    shadowSplits[cascadeNo].cameraRenderMatrix = shadowSplits[cascadeNo].cameraRenderMatrix * tm;
                    shadowSplits[cascadeNo].viewPos = ss.viewPos;
                }
            }
        }

        {
            ShadowSplit ss;
            ss.frames = 0;
            ss.from = max(shadowStart, znear);
            ss.to = min(modeSettings.maxDist, scene_z_near_far.getY());
            Vector3 anchor;
            client->getCascadeShadowAnchorPoint(FLT_MAX, anchor);
            buildShadowProjectionMatrix(500, dir_to_sun, view_matrix, camera_pos, proj_tm, ss.from, ss.to,
                min(SHADOW_FAR_CASCADE_DEPTH_MUL * ss.to, scene_z_near_far.getY()), anchor, ss);
            wholeCoveredSpaceFrustum.construct(ss.worldCullingMatrix);
        }

        for (int cascadeNo = 0; cascadeNo < cascades; cascadeNo++)
        {
            shadowSplits[cascadeNo].frustum.construct(shadowSplits[cascadeNo].worldCullingMatrix);
        }
        createOverrides();

        Vector3 frustumPoints[8];
        shadowSplits[numCascadesToRender - 1].frustum.generateAllPointFrustm(frustumPoints);
        Matrix4 globtm;
        globtm = proj_tm * view_matrix;

        Vector4 farPt = Vector4(0, 0, 0, 0);
        for (int i = 0; i < 8; ++i)
        {
            Vector4 point = globtm * Vector4(frustumPoints[i], 1.0f);
            farPt = select(farPt, point, (bool)Vector4(_mm_cmpgt_ps(point.get128(), farPt.get128())).getW());
        }
        csmDistance = farPt.getW();
    }


    void CascadeShadowsPrivate::calcTMs()
    {
        if (numCascadesToRender <= 0)
        {
            setFadeOutToShaders(0.f);
            return;
        }
        NAU_ASSERT(numCascadesToRender <= CascadeShadows::MAX_CASCADES);
        setFadeOutToShaders(modeSettings.maxDist * settings.fadeOutMul);
        //ShaderGlobal::set_real(csm_distanceVarId, csmDistance);
        //ShaderGlobal::set_int(num_of_cascadesVarId, numCascadesToRender);

        int shadowTexW = shadowCascadesTexInfo.getX(), shadowTexH = shadowCascadesTexInfo.getY();
        for (unsigned int cascadeNo = 0; cascadeNo < numCascadesToRender; cascadeNo++)
        {
            const ShadowSplit& ss = shadowSplits[cascadeNo];
            Matrix4 texTm = screen_to_tex_scale_tm_xy(HALF_TEXEL_OFSF / shadowTexW, HALF_TEXEL_OFSF / shadowTexH) * ss.cameraRenderMatrix;
            shadowCascadeTm[cascadeNo * 3 + 0] = Color4(texTm.getElem(0, 0), texTm.getElem(1, 0), texTm.getElem(2, 0), texTm.getElem(3, 0) - 0.5);
            shadowCascadeTm[cascadeNo * 3 + 1] = Color4(texTm.getElem(0, 1), texTm.getElem(1, 1), texTm.getElem(2, 1), texTm.getElem(3, 1) - 0.5);
            shadowCascadeTm[cascadeNo * 3 + 2] = Color4(texTm.getElem(0, 2), texTm.getElem(1, 2), texTm.getElem(2, 2), texTm.getElem(3, 2) - 0.5);
            csmConstData.shadowCascadeTcMulOffset[cascadeNo] = Color4(float(ss.viewport.width().getX()) / shadowTexW, float(ss.viewport.width().getY()) / shadowTexH,
                float(ss.viewport[0].getX()) / shadowTexW + float(0.5 * ss.viewport.width().getX()) / shadowTexW,
                float(ss.viewport[0].getY()) / shadowTexH + float(0.5 * ss.viewport.width().getY()) / shadowTexH);
        }
    }

    const eastl::string& CascadeShadowsPrivate::setShadowCascadeDistanceDbg(const Vector2& scene_z_near_far, int tex_size, int splits_w,
        int splits_h, float shadow_distance, float pow_weight)
    {
        if (tex_size > 0 && splits_w > 0 && splits_h > 0 && splits_w * splits_h <= CascadeShadows::MAX_CASCADES && pow_weight >= 0 &&
            pow_weight <= 1 && shadow_distance > 0)
        {
            dbgModeSettings = true;

            settings.splitsW = splits_w;
            settings.splitsH = splits_h;
            settings.cascadeWidth = tex_size;
            createDepthShadow(settings.splitsW, settings.splitsH, settings.cascadeWidth, settings.cascadeWidth,
                settings.cascadeDepthHighPrecision);

            modeSettings.maxDist = shadow_distance;
            modeSettings.powWeight = pow_weight;
            modeSettings.numCascades = settings.splitsW * settings.splitsH;
        }

        eastl::array<float, CascadeShadows::MAX_CASCADES> distances;
        int cascades = settings.splitsW * settings.splitsH;
        NAU_ASSERT(cascades <= distances.size());
        calculate_cascades(modeSettings.maxDist, modeSettings.powWeight, cascades, distances.data(), scene_z_near_far.getX());

        static eastl::string dbg;
        dbg = eastl::string(eastl::string::CtorSprintf(), "%dx%d, %g weight): cascades = ", shadowCascadesTexInfo.getX(), shadowCascadesTexInfo.getY(), modeSettings.powWeight);
        for (unsigned int cascadeNo = 0; cascadeNo < cascades; cascadeNo++)
        {
            dbg += eastl::string(eastl::string::CtorSprintf(), "%g%s", distances[cascadeNo], (cascadeNo == cascades - 1) ? "\n" : ", ");
        }

        return dbg;
    }


    Matrix4 CascadeShadowsPrivate::getShadowViewMatrix(const Vector3& dir_to_sun, const Vector3& camera_pos, bool world_space)
    {
        Point3 dirToSun = Point3(-dir_to_sun);

        Matrix4 shadowViewMatrix = Matrix4::lookAtRH(Point3(0, 0, 0), dirToSun, Vector3(0.f, 1.f, 0.f)); //::matrix_look_at_lh(ZERO<Point3>(), -dirToSun, Point3(0.f, 1.f, 0.f));


        if (world_space)
        {
            Matrix4 worldToCamldMatrix = Matrix4::identity();
            worldToCamldMatrix.setTranslation(Vector3(-camera_pos.getX(), -camera_pos.getY(), -camera_pos.getZ()));

            shadowViewMatrix = shadowViewMatrix * worldToCamldMatrix; // ???????????
        }

        return shadowViewMatrix;
    }

    bool shadow_rotation_stability = false;

    void CascadeShadowsPrivate::buildShadowProjectionMatrix(uint32_t cascadeNo, const Vector3& dir_to_sun, const Matrix4& view_matrix, const Vector3& camera_pos,
        const Matrix4& projTM, float z_near, float z_far, float next_z_far, const Vector3& anchor, ShadowSplit& split)
    {
        if (!shadowCascades)
        {
            return;
        }

        float expandZ = min(2.f * modeSettings.shadowCascadeZExpansion, safediv(modeSettings.shadowCascadeZExpansion, dir_to_sun.getY()));

        Matrix4 shadowViewMatrix = getShadowViewMatrix(dir_to_sun, camera_pos, false); // always the same for all splits! Depends on light
        // direction only
        Matrix3 shadowViewMatrix3 = shadowViewMatrix.getUpper3x3(); // always the same for all splits! Depends on light direction only
        Matrix4 shadowWorldViewMatrix = getShadowViewMatrix(dir_to_sun, camera_pos, true);

        const float det = determinant(view_matrix); //.det();
        split.viewPos = fabs(det) > 1e-5 ? inverse(view_matrix).getTranslation() : Vector3(0, 0, 0);

        Matrix4 camViewTm = view_matrix;
        camViewTm.setTranslation(Vector3(0.f, 0.f, 0.f));
        Matrix4 camViewProjTm = projTM * camViewTm;

        NauFrustum frustum;
        frustum.construct(camViewProjTm);
        // debug("zn = %g zf=%g <- %g %g", v_extract_w(frustum.camPlanes[5]), v_extract_w(frustum.camPlanes[4]), z_near, z_far);
        frustum.camPlanes[4] = Vector4(v_perm_xyzd(frustum.camPlanes[4].get128(), Vector4(z_far).get128()));
        frustum.camPlanes[5] = Vector4(v_perm_xyzd(frustum.camPlanes[5].get128(), Vector4(-z_near).get128()));
        Vector3 frustumPoints[8];
        frustum.generateAllPointFrustm(frustumPoints);
        Vector3 frustumPointsInLS[8];
        Matrix3 v_shadowView = shadowViewMatrix3;

        for (int i = 0; i < 8; ++i)
        {
            frustumPointsInLS[i] = v_shadowView * frustumPoints[i];
        }

        BBox3 v_frustumInLSBox;
        v_frustumInLSBox.lim[0] = v_frustumInLSBox.lim[1] = frustumPointsInLS[0];

        for (int i = 1; i < 8; ++i)
        {
            v_frustumInLSBox += frustumPointsInLS[i];
        }

        if (next_z_far > z_far) // Extend box along the z-axis to include next cascade frustum.
        {                       // Helps to avoid early cascade switch due to an insufficient depth range.
            frustum.camPlanes[4] = Vector4(v_perm_xyzd(frustum.camPlanes[4].get128(), Vector4(next_z_far).get128()));
            frustum.generateAllPointFrustm(frustumPoints);
            for (int i = 0; i < 8; ++i)
            {
                Vector3 frustumPointInLS = v_shadowView * frustumPoints[i];// v_mat33_mul_vec3(v_shadowView, frustumPoints[i]);
                v_frustumInLSBox.lim[0] = Vector3(v_perm_xycw(v_frustumInLSBox.lim[0].get128(), v_min(v_frustumInLSBox.lim[0].get128(), frustumPointInLS.get128())));
                v_frustumInLSBox.lim[1] = Vector3(v_perm_xycw(v_frustumInLSBox.lim[1].get128(), v_max(v_frustumInLSBox.lim[1].get128(), frustumPointInLS.get128())));
            }
        }

        BBox3 shadowProjectionBox;
        shadowProjectionBox[0] = v_frustumInLSBox.lim[0];
        shadowProjectionBox[1] = v_frustumInLSBox.lim[1];

        if (shadow_rotation_stability)
        {
            Vector3 avgCenterLS = frustumPointsInLS[0];
            for (int i = 1; i < 8; ++i)
            {
                avgCenterLS = avgCenterLS + frustumPointsInLS[i];
            }
            avgCenterLS = mulPerElem(avgCenterLS, Vector3(1.0f / 8.0f));

            float radius2d = 0;
            for (int i = 0; i < 8; ++i)
            {
                radius2d = max(radius2d, (float)lengthSqr(frustumPointsInLS[i] - avgCenterLS));
            }
            radius2d = floorf(sqrtf(radius2d) * 100.) / 100.;
            // debug("zmin = %g zmax=%g", zmin, zmax);
            float texelWidth = radius2d * 2.f / split.viewport.width().getX();
            float texelHeight = radius2d * 2.f / split.viewport.width().getY();

            Vector3 anchorPoint = shadowViewMatrix3 * anchor;
            Vector2 sphereCenter = Vector2(anchorPoint.getX() + floorf((avgCenterLS.getX() - anchorPoint.getX()) / texelWidth) * texelWidth,
                anchorPoint.getY() + floorf((avgCenterLS.getY() - anchorPoint.getY()) / texelHeight) * texelHeight);

            shadowProjectionBox.lim[0].setX(sphereCenter.getX() - radius2d);
            shadowProjectionBox.lim[0].setY(sphereCenter.getY() - radius2d);
            shadowProjectionBox.lim[1].setX(sphereCenter.getX() + radius2d);
            shadowProjectionBox.lim[1].setY(sphereCenter.getY() + radius2d);
        }
        else
        {
            if (!split.viewport.isEmpty())
            {
                // Align box with shadow texels.
                const int borderPixels = 4;
                float texelWidth = shadowProjectionBox.width().getX() / split.viewport.width().getX(); // Add border pixels before adding the reserve for
                // camera rotation to measure this reserve in
                // constant units.
                float texelHeight = shadowProjectionBox.width().getY() / split.viewport.width().getY();
                shadowProjectionBox.lim[0].setX(shadowProjectionBox.lim[0].getX() - borderPixels * texelWidth);
                shadowProjectionBox.lim[0].setY(shadowProjectionBox.lim[0].getY() - borderPixels * texelHeight);
                shadowProjectionBox.lim[1].setX(shadowProjectionBox.lim[1].getX() + borderPixels * texelWidth);
                shadowProjectionBox.lim[1].setY(shadowProjectionBox.lim[1].getY() + borderPixels * texelHeight);

                float step = modeSettings.shadowCascadeRotationMargin * z_far;
                shadowProjectionBox.lim[0].setX(step * floorf(shadowProjectionBox.lim[0].getX() / step));
                shadowProjectionBox.lim[0].setY(step * floorf(shadowProjectionBox.lim[0].getY() / step));
                shadowProjectionBox.lim[1].setX(step * ceilf(shadowProjectionBox.lim[1].getX() / step));
                shadowProjectionBox.lim[1].setY(step * ceilf(shadowProjectionBox.lim[1].getY() / step));

                Vector3 anchorPoint = shadowViewMatrix3 * anchor;
                texelWidth = shadowProjectionBox.width().getX() / split.viewport.width().getX(); // Box size was changed, recalculate the exact texel size
                // to snap to pixel.
                texelHeight = shadowProjectionBox.width().getY() / split.viewport.width().getY();
                shadowProjectionBox.lim[0].setX(anchorPoint.getX() + floorf((shadowProjectionBox.lim[0].getX() - anchorPoint.getX()) / texelWidth) * texelWidth);
                shadowProjectionBox.lim[0].setY(
                    anchorPoint.getY() + floorf((shadowProjectionBox.lim[0].getY() - anchorPoint.getY()) / texelHeight) * texelHeight);
                shadowProjectionBox.lim[1].setX(anchorPoint.getX() + ceilf((shadowProjectionBox.lim[1].getX() - anchorPoint.getX()) / texelWidth) * texelWidth);
                shadowProjectionBox.lim[1].setY(anchorPoint.getY() + ceilf((shadowProjectionBox.lim[1].getY() - anchorPoint.getY()) / texelHeight) * texelHeight);
            }
        }

        // Shadow projection matrix.
        split.znzf = Vector2(shadowProjectionBox.lim[0].getZ() - expandZ, shadowProjectionBox.lim[1].getZ() + SHADOW_CULLING_POS_EXPAND_MUL * expandZ);

        Matrix4 shadowProjectionCullingMatrix = Matrix4::orthographicRHOffCenter( // ::matrix_ortho_off_center_lh(
            shadowProjectionBox.lim[0].getX(), shadowProjectionBox.lim[1].getX(),
            shadowProjectionBox.lim[0].getY(), shadowProjectionBox.lim[1].getY(),
            split.znzf.getX(), split.znzf.getY());

        Matrix4 shadowProjectionRenderMatrix;
#if USE_SHADOW_DEPTH_CLAMP
        shadowProjectionBox.lim[0].setZ(shadowProjectionBox.lim[0].getZ() - shadow_render_expand_mul * expandZ + shadow_render_expand_to_sun_mul * (split.to - split.from));
        shadowProjectionBox.lim[1].setZ(shadowProjectionBox.lim[1].getZ() + shadow_render_expand_mul * expandZ + shadow_render_expand_from_sun_mul * (split.to - split.from));

        split.znzf = Vector2(shadowProjectionBox.lim[0].getZ(), shadowProjectionBox.lim[1].getZ());

        shadowProjectionRenderMatrix = Matrix4::orthographicRHOffCenter( // ::matrix_ortho_off_center_lh(
            shadowProjectionBox.lim[0].getX(), shadowProjectionBox.lim[1].getX(),
            shadowProjectionBox.lim[0].getY(), shadowProjectionBox.lim[1].getY(),
            split.znzf.getX(), split.znzf.getY());
#else
        shadowProjectionRenderMatrix = shadowProjectionCullingMatrix;
#endif

        split.shadowWidth = shadowProjectionBox.width();
        split.cameraCullingMatrix = shadowProjectionCullingMatrix * shadowViewMatrix;
        split.cameraRenderMatrix = shadowProjectionRenderMatrix * shadowViewMatrix;
        split.worldCullingMatrix = shadowProjectionCullingMatrix * shadowWorldViewMatrix;
        split.worldRenderMatrix = shadowProjectionRenderMatrix * shadowWorldViewMatrix;

        split.renderViewMatrix = shadowWorldViewMatrix;
        split.renderProjMatrix = shadowProjectionRenderMatrix;

        Matrix4 shadowViewMatrix3But4 = Matrix4::identity();
        shadowViewMatrix3But4.setUpper3x3(shadowViewMatrix3);
        Matrix4 invShadowViewMatrix = orthoInverse(shadowViewMatrix3But4); // always the same for all splits! Depends on light
        // direction only
        split.shadowViewItm = invShadowViewMatrix; // it is actually split independent Depends on light direction only

        NauFrustum shadowFrustum;
        shadowFrustum.construct(split.cameraCullingMatrix);

        BBox3 frustumWorldBox;
        shadowFrustum.calcFrustumBBox(frustumWorldBox);
        split.worldBox[0] = frustumWorldBox.lim[0] + camera_pos;
        split.worldBox[1] = frustumWorldBox.lim[1] + camera_pos;
    }


    void CascadeShadowsPrivate::renderShadowCascadeDepth(int cascadeNo, bool clearPerView)
    {
        const ShadowSplit& ss = shadowSplits[cascadeNo];
        NAU_ASSERT(ss.to > ss.from);

        if (ss.shouldUpdate)
        {
            d3d::setview(ss.viewport[0].getX(), ss.viewport[0].getY(), ss.viewport.width().getX(), ss.viewport.width().getY(), 0.0f, 1.0f);
            if (clearPerView && !mobileAreaUpdateRP)
            {
#if DAGOR_DBGLEVEL > 0 // don't waste perf in release on searching perf marker names
                char name[64];
                SNPRINTF(name, sizeof(name), "clear_cascade_%d", cascadeNo);
                TIME_D3D_PROFILE_NAME(renderShadowCascade, name);
#else
                //TIME_D3D_PROFILE(clear_cascade);
#endif
                d3d::clearview(CLEAR_ZBUFFER, 0, 1.0f, 0);
            }
            if (shadowCascades)
            {
#if DAGOR_DBGLEVEL > 0 // don't waste perf in release on searching perf marker names
                char name[64] = "shadow_cascade_0_render";
                name[sizeof("shadow_cascade_") - 1] += cascadeNo;
                TIME_D3D_PROFILE_NAME(renderShadowCascade, name);
#else
                //TIME_D3D_PROFILE(shadow_cascade_render);
#endif
                auto clientRenderDepth = [&]() {
                    shaders::overrides::set(cascadeOverride[cascadeNo]);
                    client->renderCascadeShadowDepth(cascadeNo, ss.znzf);
                    shaders::overrides::reset();
                    };

                clientRenderDepth();
            }
        }
    }


    void CascadeShadowsPrivate::setFadeOutToShaders(float max_dist)
    {
        NAU_ASSERT(settings.shadowFadeOut > 0.f);
    }

    void CascadeShadowsPrivate::setCascadesToShader(MaterialAssetView::Ptr resolveMaterial)
    {
        calcTMs();

        //eastl::array<Color4, CascadeShadows::MAX_CASCADES * 4> transposed;
        for (int i = 0; i < numCascadesToRender; ++i)
        {
            csmConstData.transposed[i * 4 + 0] = Color4(shadowCascadeTm[i * 3 + 0].r, shadowCascadeTm[i * 3 + 1].r, shadowCascadeTm[i * 3 + 2].r, 0);
            csmConstData.transposed[i * 4 + 1] = Color4(shadowCascadeTm[i * 3 + 0].g, shadowCascadeTm[i * 3 + 1].g, shadowCascadeTm[i * 3 + 2].g, 0);
            csmConstData.transposed[i * 4 + 2] = Color4(shadowCascadeTm[i * 3 + 0].b, shadowCascadeTm[i * 3 + 1].b, shadowCascadeTm[i * 3 + 2].b, 0);
            csmConstData.transposed[i * 4 + 3] = Color4(shadowCascadeTm[i * 3 + 0].a, shadowCascadeTm[i * 3 + 1].a, shadowCascadeTm[i * 3 + 2].a, 0);
        }

        //resolveMaterial->setProperty("Regular", "pcf_lerp",
        //    Vector4(0.00049f, 0.00049f, 0.00098f, 0.00098f));
        //resolveMaterial->setTexture("Regular", "shadow_cascade_depth_tex", shadowCascades.get());

        //nau::shader_globals::setStruct("shadow_cascade_tm_transp", CascadeShadows::MAX_CASCADES * 4 * sizeof(Color4), (void*)transposed.data());
        //nau::shader_globals::setStruct("shadow_cascade_tc_mul_offset", CascadeShadows::MAX_CASCADES * sizeof(Color4), (void*)shadowCascadeTcMulOffset.data());

        csmBuffer->updateDataWithLock(0, sizeof(csmConstData), &csmConstData, VBLOCK_DISCARD);


        resolveMaterial->setCBuffer("Regular", "SB_CSMBuffer", csmBuffer);

        d3d::settex(8, shadowCascades.get());
        d3d::set_sampler(STAGE_PS, 8, csmSampler);
    }

    CascadeShadows::ShaderSnapshot CascadeShadowsPrivate::snapshotShaderConstants()
    {
        // calcTMs() writes only what setCascadesToShader() recomputes before every upload anyway,
        // the transposed matrices go to the snapshot and csmConstData.transposed stays as is.
        calcTMs();

        CascadeShadows::ShaderSnapshot snapshot;
        snapshot.constants.pcfLerp = csmConstData.pcfLerp;
        snapshot.constants.shadowCascadeTmTransp = csmConstData.transposed;
        snapshot.constants.shadowCascadeTcMulOffset = csmConstData.shadowCascadeTcMulOffset;
        for (int i = 0; i < numCascadesToRender; ++i)
        {
            snapshot.constants.shadowCascadeTmTransp[i * 4 + 0] = Color4(shadowCascadeTm[i * 3 + 0].r, shadowCascadeTm[i * 3 + 1].r, shadowCascadeTm[i * 3 + 2].r, 0);
            snapshot.constants.shadowCascadeTmTransp[i * 4 + 1] = Color4(shadowCascadeTm[i * 3 + 0].g, shadowCascadeTm[i * 3 + 1].g, shadowCascadeTm[i * 3 + 2].g, 0);
            snapshot.constants.shadowCascadeTmTransp[i * 4 + 2] = Color4(shadowCascadeTm[i * 3 + 0].b, shadowCascadeTm[i * 3 + 1].b, shadowCascadeTm[i * 3 + 2].b, 0);
            snapshot.constants.shadowCascadeTmTransp[i * 4 + 3] = Color4(shadowCascadeTm[i * 3 + 0].a, shadowCascadeTm[i * 3 + 1].a, shadowCascadeTm[i * 3 + 2].a, 0);
        }
        snapshot.numCascades = numCascadesToRender;
        snapshot.atlas = getAtlasInfo();
        return snapshot;
    }

    void CascadeShadowsPrivate::setCascadesToShader(MaterialAssetView::Ptr resolveMaterial, const CascadeShadows::ShaderSnapshot& snapshot)
    {
        static_assert(sizeof(CascadeShadows::ShaderConstants) == sizeof(csmBufferData));
        static_assert(offsetof(CascadeShadows::ShaderConstants, shadowCascadeTmTransp) == offsetof(csmBufferData, transposed));
        static_assert(offsetof(CascadeShadows::ShaderConstants, shadowCascadeTcMulOffset) == offsetof(csmBufferData, shadowCascadeTcMulOffset));
        NAU_ASSERT(snapshot.atlas.width == shadowCascadesTexInfo.getX() && snapshot.atlas.height == shadowCascadesTexInfo.getY(),
            "CSM snapshot was taken for another shadow atlas size");

        csmBuffer->updateDataWithLock(0, sizeof(snapshot.constants), &snapshot.constants, VBLOCK_DISCARD);

        resolveMaterial->setCBuffer("Regular", "SB_CSMBuffer", csmBuffer);

        d3d::settex(8, shadowCascades.get());
        d3d::set_sampler(STAGE_PS, 8, csmSampler);
    }

    void CascadeShadowsPrivate::debugSetParams(float shadow_depth_bias, float shadow_const_depth_bias, float shadow_depth_slope_bias)
    {
        settings.shadowDepthBias = shadow_depth_bias;
        settings.shadowConstDepthBias = shadow_const_depth_bias;
        settings.shadowDepthSlopeBias = shadow_depth_slope_bias;
    }


    void CascadeShadowsPrivate::debugGetParams(float& shadow_depth_bias, float& shadow_const_depth_bias, float& shadow_depth_slope_bias)
    {
        shadow_depth_bias = settings.shadowDepthBias;
        shadow_const_depth_bias = settings.shadowConstDepthBias;
        shadow_depth_slope_bias = settings.shadowDepthSlopeBias;
    }


    //
    // CascadeShadows
    //

    CascadeShadows::ModeSettings::ModeSettings()
    {
        memset(this, 0, sizeof(ModeSettings));
        powWeight = 0.99f;
        maxDist = 1000.f;
        shadowStart = 0.f;
        numCascades = 4;
        shadowCascadeZExpansion = 100.f;
        shadowCascadeRotationMargin = 0.1f;
        cascade0Dist = -1;
        overrideZNearForCascadeDistribution = -1;
    }

    /*static*/
    CascadeShadows* CascadeShadows::make(ICascadeShadowsClient* in_client, const Settings& in_settings)
    {
        CascadeShadows* ret = new CascadeShadows();
        ret->d = new CascadeShadowsPrivate(in_client, in_settings);

        return ret;
    }

    CascadeShadows::~CascadeShadows() { delete d; }

    void CascadeShadows::prepareShadowCascades(const CascadeShadows::ModeSettings& mode_settings, const Vector3& dir_to_sun,
        const Matrix4& view_matrix, const Vector3& camera_pos, const Matrix4& proj_tm, const NauFrustum& view_frustum,
        const Vector2& scene_z_near_far, float z_near_for_cascade_distribution)
    {
        d->prepareShadowCascades(mode_settings, dir_to_sun, view_matrix, camera_pos, proj_tm, view_frustum, scene_z_near_far,
            z_near_for_cascade_distribution);
    }

    void CascadeShadows::renderShadowsCascadesCb(csm_render_cascades_cb_t render_cascades_cb)
    {
        d->renderShadowsCascadesCb(render_cascades_cb);
    }

    void CascadeShadows::renderShadowsCascades() { d->renderShadowsCascades(); }

    void CascadeShadows::renderShadowCascadeDepth(int cascadeNo, bool clearPerView)
    {
        d->renderShadowCascadeDepth(cascadeNo, clearPerView);
    }

    void CascadeShadows::setCascadesToShader(MaterialAssetView::Ptr resolveMaterial) { d->setCascadesToShader(resolveMaterial); }

    CascadeShadows::CascadeViewport CascadeShadows::getCascadeViewport(int cascade_no) const { return d->getCascadeViewport(cascade_no); }

    void CascadeShadows::getCascadeDepthBias(int cascade_no, float& const_bias, float& slope_bias) const
    {
        d->getCascadeDepthBias(cascade_no, const_bias, slope_bias);
    }

    CascadeShadows::AtlasInfo CascadeShadows::getAtlasInfo() const { return d->getAtlasInfo(); }

    CascadeShadows::ShaderSnapshot CascadeShadows::snapshotShaderConstants() { return d->snapshotShaderConstants(); }

    void CascadeShadows::setCascadesToShader(MaterialAssetView::Ptr resolveMaterial, const ShaderSnapshot& snapshot)
    {
        d->setCascadesToShader(resolveMaterial, snapshot);
    }

    void CascadeShadows::disable() { d->disable(); }

    bool CascadeShadows::isEnabled() const { return d->isEnabled(); }

    void CascadeShadows::invalidate() { d->invalidate(); }

    int CascadeShadows::getNumCascadesToRender() const { return d->getNumCascadesToRender(); }

    const NauFrustum& CascadeShadows::getFrustum(int cascade_no) const { return d->getFrustum(cascade_no); }
    const Vector3& CascadeShadows::getRenderCameraWorldViewPos(int cascade_no) const { return d->getRenderCameraWorldViewPos(cascade_no); }

    const Matrix4& CascadeShadows::getShadowViewItm(int cascade_no) const { return d->getShadowViewItm(cascade_no); }

    const Matrix4& CascadeShadows::getCameraRenderMatrix(int cascade_no) const { return d->getCameraRenderMatrix(cascade_no); }

    const Matrix4& CascadeShadows::getWorldCullingMatrix(int cascade_no) const { return d->getWorldCullingMatrix(cascade_no); }

    const Matrix4& CascadeShadows::getWorldRenderMatrix(int cascade_no) const { return d->getWorldRenderMatrix(cascade_no); }

    const Matrix4& CascadeShadows::getRenderViewMatrix(int cascade_no) const { return d->getRenderViewMatrix(cascade_no); }

    const Vector3& CascadeShadows::shadowWidth(int cascade_no) const { return d->shadowWidth(cascade_no); }

    const Matrix4& CascadeShadows::getRenderProjMatrix(int cascade_no) const { return d->getRenderProjMatrix(cascade_no); }

    const BBox3& CascadeShadows::getWorldBox(int cascade_no) const { return d->getWorldBox(cascade_no); }

    bool CascadeShadows::shouldUpdateCascade(int cascade_no) const { return d->shouldUpdateCascade(cascade_no); }

    bool CascadeShadows::isCascadeValid(int cascade_no) const { return d->isCascadeValid(cascade_no); }

    void CascadeShadows::copyFromSparsed(int cascade_no) { d->copyFromSparsed(cascade_no); }

    float CascadeShadows::getMaxDistance() const { return d->getMaxDistance(); }

    float CascadeShadows::getMaxShadowDistance() const { return d->getMaxShadowDistance(); }

    float CascadeShadows::getCascadeDistance(int cascade_no) const { return d->getCascadeDistance(cascade_no); }

    const NauFrustum& CascadeShadows::getWholeCoveredFrustum() const { return d->getWholeCoveredFrustum(); }

    const eastl::string& CascadeShadows::setShadowCascadeDistanceDbg(const Vector2& scene_z_near_far, int tex_size, int splits_w, int splits_h,
        float shadow_distance, float pow_weight)
    {
        return d->setShadowCascadeDistanceDbg(scene_z_near_far, tex_size, splits_w, splits_h, shadow_distance, pow_weight);
    }

    void CascadeShadows::debugSetParams(float shadow_depth_bias, float shadow_const_depth_bias, float shadow_depth_slope_bias)
    {
        d->debugSetParams(shadow_depth_bias, shadow_const_depth_bias, shadow_depth_slope_bias);
    }

    void CascadeShadows::debugGetParams(float& shadow_depth_bias, float& shadow_const_depth_bias, float& shadow_depth_slope_bias)
    {
        d->debugGetParams(shadow_depth_bias, shadow_const_depth_bias, shadow_depth_slope_bias);
    }

    void CascadeShadows::setNeedSsss(bool need_ssss) { d->setNeedSsss(need_ssss); }

    const CascadeShadows::Settings& CascadeShadows::getSettings() const { return d->getSettings(); }
    void CascadeShadows::setDepthBiasSettings(const CascadeShadows::Settings& set) { return d->setDepthBiasSettings(set); }
    void CascadeShadows::setCascadeWidth(int width) { return d->setCascadeWidth(width); }

    BaseTexture* CascadeShadows::getShadowsCascade() const { return d->getShadowsCascade(); }

    const Vector2& CascadeShadows::getZnZf(int cascade_no) const { return d->getZnZf(cascade_no); }


} //namesapce nau::csm