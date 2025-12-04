// Copyright 2024 N-GINN LLC. All rights reserved.

// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "nau/render/deferredRT.h"

namespace nau::render
{
    namespace
    {
        inline void initShaderVar(eastl::string_view name, size_t size)
        {
            if (nau::shader_globals::containsName(name))
            {
                return;
            }
            nau::shader_globals::addVariable(name, size);
        }
    }  // namespace

    void DeferredRT::close()
    {
        for (int i = 0; i < m_numRt; ++i)
        {
            m_mrts[i].close();
        }
        m_depth.close();
        m_numRt = 0;
    }

    void DeferredRT::setRt()
    {
        d3d::set_render_target();
        for (int i = 0; i < m_numRt; ++i)
        {
            d3d::set_render_target(i, m_mrts[i].get(), 0);
        }
        if (m_depth.get())
        {
            d3d::set_depth(m_depth.get(), DepthAccess::RW);
        }
    }

    uint32_t DeferredRT::recreateDepthInternal(uint32_t targetFmt)
    {
        if (!(d3d::get_texformat_usage(targetFmt) & d3d::USAGE_DEPTH))
        {
            NAU_LOG_DEBUG("not supported m_depth format 0x%08x, fallback to TEXFMT_DEPTH32", targetFmt);
            targetFmt = (targetFmt & (~TEXFMT_MASK)) | TEXFMT_DEPTH32;
        }
        uint32_t currentFmt = 0;
        if (m_depth.get())
        {
            TextureInfo tinfo;
            m_depth.get()->getinfo(tinfo, 0);
            currentFmt = tinfo.cflg & (TEXFMT_MASK | TEXCF_SAMPLECOUNT_MASK | TEXCF_TC_COMPATIBLE);
            targetFmt |= currentFmt & (~TEXFMT_MASK);
        }
        if (currentFmt == targetFmt)
        {
            return currentFmt;
        }
        m_depth.close();

        math::IVector2 cs = calcCreationSize();

        const uint32_t flags = TEXCF_RTARGET;
        nau::string m_depthName("{}_intzDepthTex", m_name);
        TexPtr m_depthTex = dag::create_tex(NULL, cs.getX(), cs.getY(), targetFmt | flags, 1, reinterpret_cast<const char*>(m_depthName.c_str()));

        if (!m_depthTex && (targetFmt & TEXFMT_MASK) != TEXFMT_DEPTH32)
        {
            NAU_LOG_DEBUG("can't create m_depth format 0x%08x, fallback to TEXFMT_DEPTH32", targetFmt);
            targetFmt = (targetFmt & (~TEXFMT_MASK)) | TEXFMT_DEPTH32;
            m_depthTex = dag::create_tex(NULL, cs.getX(), cs.getY(), targetFmt | flags, 1, reinterpret_cast<const char*>(m_depthName.c_str()));
        }

        if (!m_depthTex)
        {
            NAU_FAILURE("can't create intzDepthTex (INTZ, DF24, RAWZ) due to err '%s'", d3d::get_last_error());
        }

        m_depth = eastl::move(m_depthTex);

        m_depth.get()->texfilter(TEXFILTER_POINT);
        m_depth.get()->texaddr(TEXADDR_CLAMP);

        return targetFmt;
    }

    math::IVector2 DeferredRT::calcCreationSize() const
    {
        switch (m_stereoMode)
        {
            case StereoMode::MonoOrMultipass:
            {
                return math::IVector2(m_width, m_height);
            }
            case StereoMode::SideBySideHorizontal:
            {
                return math::IVector2(m_width * 2, m_height);
            }
            case StereoMode::SideBySideVertical:
            {
                return math::IVector2(m_width, m_height * 2);
            }
        }

        return math::IVector2(1, 1);
    }

    uint32_t DeferredRT::recreateDepth(uint32_t targetFmt)
    {
        return recreateDepthInternal(targetFmt);
    }

    DeferredRT::DeferredRT(const char* name, int w, int h, StereoMode stereoMode, unsigned msaaFlag, int numRT, const unsigned texFmt[MAX_NUM_MRT], uint32_t depthFmt) :
        m_stereoMode(stereoMode),
        m_name(name)
    {
        uint32_t currentFmt = depthFmt;
        close();
        m_width = w;
        m_height = h;

        if (depthFmt)
        {
            recreateDepthInternal(currentFmt | msaaFlag);
        }

        auto cs = calcCreationSize();

        m_numRt = numRT;
        for (int i = m_numRt - 1; i >= 0; --i)
        {
            nau::string mrtName("{}_mrt_{}", name, i);
            unsigned mrtFmt = texFmt ? texFmt[i] : TEXFMT_A8R8G8B8;
            auto mrtTex = dag::create_tex(NULL, cs.getX(), cs.getY(), mrtFmt | TEXCF_RTARGET | msaaFlag, 1, reinterpret_cast<const char*>(mrtName.c_str()));
            d3d_err(!!mrtTex);
            mrtTex->texaddr(TEXADDR_CLAMP);
            mrtTex->texfilter(TEXFILTER_POINT);
            m_mrts[i] = ResizableResPtrTex(std::move(mrtTex));
        }

        d3d::SamplerInfo sampInfo;
        m_defaultSampler = d3d::create_sampler(sampInfo);
    }

    void DeferredRT::changeResolution(const int w, const int h)
    {
        m_width = w;
        m_height = h;

        auto cs = calcCreationSize();

        for (int i = 0; i < m_numRt; ++i)
        {
            m_mrts[i].resize(cs.getX(), cs.getY());
        }
        m_depth.resize(cs.getX(), cs.getY());
    }
}  // namespace nau::render
