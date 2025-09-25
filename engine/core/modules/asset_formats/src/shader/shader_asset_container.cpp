// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "shader_asset_container.h"

#include "nau/io/nau_container.h"
#include "nau/rtti/weak_ptr.h"
#include "nau/service/service_provider.h"

namespace nau
{
    namespace
    {
        class ShaderAssetContainer;

        class ShaderAssetAccessor final : public IShaderAssetAccessor
        {
            NAU_CLASS_(nau::ShaderAssetAccessor, IShaderAssetAccessor)

        public:
            ShaderAssetAccessor(ShaderAssetContainer& shaderContainer, eastl::string_view shaderName);

        private:
            Result<> fillShader(Shader& shader) const override;

            mutable nau::WeakPtr<ShaderAssetContainer> m_shaderContainerRef;
            eastl::string_view m_shaderName;
        };

        struct ShaderBytecodeEntry
        {
            eastl::string shaderName;
            size_t blobOffset;
            size_t blobSize;

            NAU_CLASS_FIELDS(
                CLASS_FIELD(shaderName),
                CLASS_FIELD(blobOffset),
                CLASS_FIELD(blobSize), )
        };

        struct ShaderPackContainerData
        {
            eastl::vector<Shader> shaders;
            eastl::vector<ShaderBytecodeEntry> byteCode;

            NAU_CLASS_FIELDS(
                CLASS_FIELD(shaders),
                CLASS_FIELD(byteCode))
        };

        /**
         */
        class ShaderAssetContainer final : public IAssetContainer
        {
            NAU_CLASS_(nau::ShaderAssetContainer, IAssetContainer)

        public:
            ShaderAssetContainer(io::IStreamReader::Ptr shaderPackStream) :
                m_stream(shaderPackStream)
            {
                using namespace nau::io;

                auto [packHeader, blobStartOffset] = *io::readContainerHeader(m_stream);

                RuntimeValue::assign(makeValueRef(m_shadersPackData), packHeader).ignore();

                // fix bytecode offset:
                // orginally blobOffset stored as offset from start of the binary section, but not actual file.
                for(ShaderBytecodeEntry& blobEntry : m_shadersPackData.byteCode)
                {
                    blobEntry.blobOffset += blobStartOffset;
                }
            }

            Result<> fillShader(eastl::string_view shaderName, Shader& outShader)
            {
                const Shader* const shader = findShader(shaderName);
                if(!shader)
                {
                    return NauMakeError("Shader not found ({})", std::string_view{shaderName.data(), shaderName.size()});
                }

                outShader = *shader;
                outShader.bytecode = readShaderBytecode(shader->name);
                NAU_FATAL(outShader.bytecode && !outShader.bytecode.empty());

                return ResultSuccess;
            }

        private:
            ReadOnlyBuffer readShaderBytecode(eastl::string_view shaderName)
            {
                auto blobEntry = std::find_if(m_shadersPackData.byteCode.begin(), m_shadersPackData.byteCode.end(), [shaderName](const ShaderBytecodeEntry& entry)
                {
                    return entry.shaderName == shaderName;
                });
                NAU_FATAL(blobEntry != m_shadersPackData.byteCode.end(), "Invalid shader pack data, no shader ({}) bytecode found", shaderName);

                BytesBuffer bytecode(blobEntry->blobSize);

                // cases of parallel data reading are potentially possible,
                // need to protect m_stream that can be shared between multiple read operations
                lock_(m_mutex);

                m_stream->setPosition(io::OffsetOrigin::Begin, blobEntry->blobOffset);
                [[maybe_unused]] auto readResult = m_stream->read(bytecode.data(), bytecode.size());
                NAU_FATAL(readResult && *readResult == blobEntry->blobSize, "shader pack data is broken");

                return ReadOnlyBuffer{std::move(bytecode)};
            }

            nau::Ptr<> getAsset(eastl::string_view path) override
            {
                auto* const shader = findShader(path);
                if(!shader)
                {
                    NAU_LOG_WARNING(u8"Shader ({}) not exists", path);
                    return nullptr;
                }

                return rtti::createInstance<ShaderAssetAccessor>(*this, shader->name);
            }

            eastl::vector<eastl::string> getContent() const override
            {
                eastl::vector<eastl::string> shaderNames;
                shaderNames.reserve(m_shadersPackData.shaders.size());

                for(const auto& shader : m_shadersPackData.shaders)
                {
                    shaderNames.emplace_back(shader.name);
                }

                return shaderNames;
            }

            Shader* findShader(eastl::string_view shaderName)
            {
                auto& shaders = m_shadersPackData.shaders;

                if(shaderName.empty())
                {
                    // container's default content
                    if(shaders.size() != 1)
                    {
                        NAU_LOG_WARNING("Requesting default shader, but there is more shaders");
                    }

                    return !shaders.empty() ? &shaders.front() : nullptr;
                }

                auto iter = eastl::find_if(shaders.begin(), shaders.end(), [shaderName](const Shader& shader)
                {
                    return shader.name == shaderName;
                });

                return iter != shaders.end() ? &(*iter) : nullptr;
            }

            io::IStreamReader::Ptr m_stream;
            ShaderPackContainerData m_shadersPackData;
            std::mutex m_mutex;
        };

        ShaderAssetAccessor::ShaderAssetAccessor(ShaderAssetContainer& shaderContainer, eastl::string_view shaderName) :
            m_shaderContainerRef{&shaderContainer},
            m_shaderName(shaderName)
        {
        }

        Result<> ShaderAssetAccessor::fillShader(Shader& shader) const
        {
            nau::Ptr<ShaderAssetContainer> container = m_shaderContainerRef.lock();
            NAU_ASSERT(container, "Invalid logic, asset accessor can not live longer that host container");
            if(!container)
            {
                return NauMakeError("Invalid asset container");
            }

            return container->fillShader(m_shaderName, shader);
        }
    }  // anonymous namespace

    eastl::vector<eastl::string_view> ShaderAssetContainerLoader::getSupportedAssetKind() const
    {
        return {"Shader/*", "nsbc"};
    }

    async::Task<IAssetContainer::Ptr> ShaderAssetContainerLoader::loadFromStream(io::IStreamReader::Ptr stream, [[maybe_unused]] AssetContentInfo)
    {
        auto shaderPackContainer = rtti::createInstance<ShaderAssetContainer>(std::move(stream));
        co_return shaderPackContainer;
    }

    RuntimeReadonlyDictionary::Ptr ShaderAssetContainerLoader::getDefaultImportSettings() const
    {
        return nullptr;
    }
}  // namespace nau
