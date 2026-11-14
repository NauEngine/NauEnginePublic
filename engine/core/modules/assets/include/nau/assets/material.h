// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
 

#pragma once

#include <EASTL/optional.h>
#include <EASTL/string.h>
#include <EASTL/unordered_map.h>
#include "nau/memory/bytes_buffer.h"
#include "nau/serialization/runtime_value.h"
#include "nau/utils/enum/enum_reflection.h"

#include "shader.h"

namespace nau
{
    NAU_DEFINE_ENUM_(
        CullMode,
            None,
            Clockwise,
            CounterClockwise
    );

    NAU_DEFINE_ENUM_(
        DepthMode,
            Default,
            ReadOnly,
            WriteOnly,
            Disabled
    );

    NAU_DEFINE_ENUM_(
        BlendMode,
            Opaque,
            Masked,
            Translucent,
            Additive,
            PremultipliedAlpha,
            InverseDestinationAlpha,
            AlphaBlend,
            MaxBlend,
            PremultipliedOver
    );

    NAU_DEFINE_ENUM_(
        ComparisonFunc,
            Disabled,
            Never,
            Less,
            Equal,
            LessEqual,
            Greater,
            NotEqual,
            GreaterEqual,
            Always
    );

    /**
     * @brief Encapsulates compiled shaders data.
     * 
     * In metadata files user can specify settings for the associated shaders compilation. 
     * By using preprocessing techniques user can create multiple compiled versions of the same .hlsl shader (i.e. shader assets).
     * For a render pass, user has to specify concrete compiled shader versions for each stage. Material pipeline encapsulates them.
     */
    struct MaterialPipeline
    {
        /**
         * @brief A collection of all shader inputs (i.e. constants, textures, buffers, etc.).
         */
        eastl::unordered_map<eastl::string, RuntimeValue::Ptr> properties;

        /**
         * @brief A collection of handles to the compiled shaders.
         */
        eastl::vector<eastl::string> shaders;

        eastl::optional<CullMode> cullMode;
        eastl::optional<DepthMode> depthMode;
        eastl::optional<BlendMode> blendMode;
        eastl::optional<bool> isScissorsEnabled;
        eastl::optional<ComparisonFunc> stencilCmpFunc;

        NAU_CLASS_FIELDS(
            CLASS_FIELD(properties),
            CLASS_FIELD(shaders),
            CLASS_FIELD(cullMode),
            CLASS_FIELD(depthMode),
            CLASS_FIELD(blendMode),
            CLASS_FIELD(isScissorsEnabled),
            CLASS_FIELD(stencilCmpFunc)
        )
    };

    /**
     * @brief Encapsulates a rendering material, which is a named collection of material pipelines (see MaterialPipeline). 
     * 
     * A material provides an opportunity to switch between various pipelines in runtime.
     */
    struct Material
    {
        eastl::string name;

        /**
         * @brief A handle to the master material.
         * 
         * It is not set if the material itself is master. 
         */
        eastl::optional<eastl::string> master;

        /**
         * @brief A collection of named pipelines to switch between.
         */
        eastl::unordered_map<eastl::string, MaterialPipeline> pipelines;

        NAU_CLASS_FIELDS(
            CLASS_FIELD(name),
            CLASS_FIELD(master),
            CLASS_FIELD(pipelines)
        )
    };
} // namespace nau
