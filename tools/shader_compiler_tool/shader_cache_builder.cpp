// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_cache_builder.h"
#include "utf8_path.h"

#include <fstream>

#include <windows.h>

#include "nau/dataBlock/dag_dataBlock.h"
#include "nau/io/file_system.h"
#include "nau/io/nau_container.h"
#include "nau/memory/bytes_buffer.h"
#include "nau/platform/windows/utils/uid.h"
#include "nau/serialization/runtime_value_builder.h"

namespace nau
{
    namespace
    {
        constexpr auto ConfigsBlockName = "compile_configs";
        constexpr auto PermutationsBlockName = "permutations";
        constexpr auto InputLayoutBlockName = "input_layout";
        constexpr auto VsdRegBlockName = "vsd_reg";
        constexpr auto DefinesBlockName = "defines";
        constexpr auto Stage = "stage";
        constexpr auto Entry = "entry";
        constexpr auto Stream = "stream";
        constexpr auto Number = "number";
        constexpr auto SemanticName = "semantic_name";
        constexpr auto Type = "type";
        constexpr auto Name = "name";

        constexpr LPCSTR Targets[] = {
            "vs",
            "ps",
            "gs",
            "hs",
            "ds",
            "cs",
        };
        static_assert(
            std::size(Targets) == static_cast<size_t>(ShaderTarget::Count),
            "Targets array size does not match the number of ShaderTarget enum values"
        );

        Result<ShaderTarget> stringToShaderTarget(std::string_view target)
        {
            const auto* it = std::ranges::find(Targets, target);
            if (it != std::end(Targets))
            {
                const ptrdiff_t index = std::distance(std::begin(Targets), it);
                return static_cast<ShaderTarget>(index);
            }

            return NauMakeError("Invalid shader target: {}", target);
        }

        struct ShaderBytecodeEntry
        {
            eastl::string shaderName;
            size_t blobOffset;
            size_t blobSize;

            NAU_CLASS_FIELDS(
                CLASS_FIELD(shaderName),
                CLASS_FIELD(blobOffset),
                CLASS_FIELD(blobSize)
            )
        };

        struct ShaderPackContainerData
        {
            std::vector<Shader> shaders;
            std::vector<ShaderBytecodeEntry> byteCode;

            NAU_CLASS_FIELDS(
                CLASS_FIELD(shaders),
                CLASS_FIELD(byteCode)
            )
        };

        Result<> writeShadersPack(const io::IStreamWriter::Ptr& outStream, std::vector<Shader>&& shaders)
        {
            ShaderPackContainerData containerData;
            containerData.shaders = std::move(shaders);

            auto bytecodeStream = io::createMemoryStream(BytesBuffer{});

            for (const Shader& shader : containerData.shaders)
            {
                ShaderBytecodeEntry& entry = containerData.byteCode.emplace_back();
                entry.shaderName = shader.name;
                entry.blobOffset = bytecodeStream->getPosition();
                entry.blobSize = shader.bytecode.size();

                bytecodeStream->write(shader.bytecode.data(), shader.bytecode.size()).ignore();
            }

            bytecodeStream->setPosition(io::OffsetOrigin::Begin, 0);

            io::writeContainerHeader(outStream, "nau-shader-pack", makeValueRef(containerData));
            NauCheckResult(io::copyStream(outStream->as<io::IStreamWriter&>(), bytecodeStream->as<io::IStreamReader&>()));

            return ResultSuccess;
        }

        Result<> validatePaths(const fs::path& shadersPath, const fs::path& metafilesPath)
        {
            if (!fs::exists(shadersPath))
            {
                return NauMakeError("File or directory does not exist: {}", nau::shader_tools::pathToUtf8(shadersPath));
            }

            if (!fs::exists(metafilesPath))
            {
                return NauMakeError("File or directory does not exist: {}", nau::shader_tools::pathToUtf8(metafilesPath));
            }

            if (fs::is_regular_file(shadersPath) && fs::is_directory(metafilesPath)
                || fs::is_directory(shadersPath) && fs::is_regular_file(metafilesPath))
            {
                return NauMakeError("Shaders and metafiles must either both be directories or both be files:\n{}\n{}", nau::shader_tools::pathToUtf8(shadersPath), nau::shader_tools::pathToUtf8(metafilesPath));
            }

            return ResultSuccess;
        }
    } // anonymous namespace

    Result<> ShaderCacheBuilder::makeCache(StreamFactory streamFactory, const Arguments& args)
    {
        auto shaderInfos = collectShaderInfo(args.shadersPath, args.metafilesPath);
        if (shaderInfos.isError())
        {
            return NauMakeError(shaderInfos.getError()->getMessage());
        }

        auto result = compileShaders(*shaderInfos, args);
        NauCheckResult(result);

        io::IStreamWriter::Ptr outStream = streamFactory(args.shaderCacheName);
        NAU_FATAL(outStream);

        return writeShadersPack(outStream, *std::move(result));
    }

    Result<> ShaderCacheBuilder::makeCacheFiles(StreamFactory streamFactory, const Arguments& args)
    {
        auto shaderInfos = collectShaderInfo(args.shadersPath, args.metafilesPath);
        if (shaderInfos.isError())
        {
            return NauMakeError(shaderInfos.getError()->getMessage());
        }

        auto result = compileShaders(*shaderInfos, args);
        NauCheckResult(result);

        std::vector<Shader> shaders;

        for (Shader& shader : *result)
        {
            const std::string_view shaderName{shader.name.data(), shader.name.size()};
            io::IStreamWriter::Ptr outStream = streamFactory(shaderName);
            NAU_FATAL(outStream);

            shaders.emplace_back(std::move(shader));
            NauCheckResult(writeShadersPack(outStream, std::move(shaders)));

            shaders.clear();
        }

        return {};
    }

    Result<std::vector<ShaderCacheBuilder::ShaderInfo>> ShaderCacheBuilder::collectShaderInfo(const fs::path& shadersPath, const fs::path& metafilesPath)
    {
        auto validation = validatePaths(shadersPath, metafilesPath);
        NauCheckResult(validation);

        std::vector<ShaderInfo> shaderInfos;

        if (fs::is_regular_file(shadersPath))
        {
            shaderInfos.emplace_back(shadersPath, metafilesPath);
        }
        else
        {
            auto shaders = collectFiles(shadersPath, ".hlsl");
            NauCheckResult(shaders);

            auto metafiles = collectFiles(metafilesPath, ".blk");
            NauCheckResult(metafiles);

            for (const auto& shader : *shaders)
            {
                for (const auto& meta : *metafiles)
                {
                    if (shader.stem() == meta.stem())
                    {
                        shaderInfos.emplace_back(shader, meta);
                    }
                }
            }
        }

        return shaderInfos;
    }

    Result<std::vector<fs::path>> ShaderCacheBuilder::collectFiles(const fs::path& directory, std::string_view extension)
    {
        if (extension.empty() || !extension.starts_with('.'))
        {
            return NauMakeError("This is not a file extension: {}", extension);
        }

        std::vector<fs::path> files;

        for (const auto& entry : fs::recursive_directory_iterator(directory))
        {
            if (entry.is_regular_file() && entry.path().extension() == extension)
            {
                files.emplace_back(entry);
            }
        }

        if (files.empty())
        {
            return NauMakeError("No files collected: {}", nau::shader_tools::pathToUtf8(directory));
        }

        return files;
    }

    Result<std::vector<Shader>> ShaderCacheBuilder::compileShaders(const std::vector<ShaderInfo>& shaderInfos, const Arguments& args)
    {
        std::vector<std::wstring> includes;
        includes.reserve(args.includeDirs.size());

        for (const auto& dir : args.includeDirs)
        {
            includes.emplace_back(dir.c_str());
        }

        std::vector<Shader> shaders;

        ShaderCompiler compiler;

        for (const auto& [shader, metafile] : shaderInfos)
        {
            auto meta = getShaderMeta(metafile);
            NauCheckResult(meta);

            auto result = compileShader(&compiler, shader, *meta, includes, args.debugOutputDir, args.embedDebugInfo);
            NauCheckResult(result);

            shaders.reserve(shaders.size() + result->size());
            std::move(result->begin(), result->end(), std::back_inserter(shaders));
        }

        return shaders;
    }

    Result<std::vector<Shader>> ShaderCacheBuilder::compileShader(
        ShaderCompiler* compiler,
        const fs::path& filename,
        const ShaderMeta& meta,
        const std::vector<std::wstring>& includeDirs,
        const std::optional<fs::path>& pdbDir,
        bool needEmbedDebug)
    {
        compiler->reset();

        auto result = compiler->loadFile(filename);
        if (result.isError())
        {
            return NauMakeError(result.getError()->getMessage());
        }

        std::vector<Shader> shaders;

        for (const auto& config : meta.configs)
        {
            for (const auto& permutation : meta.permutations)
            {
                std::string shaderName = nau::shader_tools::pathToUtf8(filename.stem());
                shaderName += ".";

                if (permutation.name != "regular")
                {
                    shaderName += permutation.name + ".";
                }

                std::string ep = config.entry;
                std::ranges::transform(ep, ep.begin(), [](unsigned char c) { return std::tolower(c); });

                shaderName += Targets[static_cast<size_t>(config.stage)];
                shaderName += "." + ep;

                std::optional<fs::path> pdbFilename = std::nullopt;
                if (pdbDir.has_value())
                {
                    const std::string pdbName = shaderName + ".pdb";
                    pdbFilename = *pdbDir / fs::u8path(pdbName);
                }

                NauCheckResult(compiler->compile(config.stage, config.entry, permutation.defines, includeDirs, pdbFilename, needEmbedDebug));

                Shader& shader = shaders.emplace_back(*compiler->getResult());
                shader.name.assign(shaderName.data(), shaderName.size());
                shader.vsd.assign(meta.vsd.begin(), meta.vsd.end());
            }
        }

        return shaders;
    }

    Result<ShaderCacheBuilder::ShaderMeta> ShaderCacheBuilder::getShaderMeta(const fs::path& filename)
    {
        const std::ifstream file(filename.c_str());
        if (!file)
        {
            return NauMakeError("Metafile is corrupted or not found: {}", nau::shader_tools::pathToUtf8(filename));
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        std::string text = buffer.str();

        DataBlock metadata;
        if (!metadata.loadText(text.data(), text.length()))
        {
            return NauMakeError("Can not parse file: {}", nau::shader_tools::pathToUtf8(filename));
        }

        ShaderMeta meta;

        const DataBlock* configs = metadata.getBlockByNameEx(ConfigsBlockName);
        if (!configs || configs->blockCount() == 0)
        {
            return NauMakeError("Metafile does not contain block '{}' or is missing: {}", ConfigsBlockName, nau::shader_tools::pathToUtf8(filename));
        }

        for (auto i = 0; i < configs->blockCount(); ++i)
        {
            CompileConfig& config = meta.configs.emplace_back();

            const char* stageStr = configs->getBlock(i)->getStr(Stage);
            if (!stageStr || strlen(stageStr) == 0)
            {
                return NauMakeError("Field '{}' is required for config: {}", Stage, nau::shader_tools::pathToUtf8(filename));
            }

            const char* entry = configs->getBlock(i)->getStr(Entry);
            if (!entry || strlen(entry) == 0)
            {
                return NauMakeError("Field '{}' is required for config: {}", Entry, nau::shader_tools::pathToUtf8(filename));
            }

            auto stage = stringToShaderTarget(stageStr);
            NauCheckResult(stage);

            config.stage = *stage;
            config.entry = entry;
        }

        const DataBlock* inputLayout = metadata.getBlockByNameEx(InputLayoutBlockName);
        if (inputLayout)
        {
            meta.vsd.reserve(inputLayout->blockCount());
            for (auto i = 0; i < inputLayout->blockCount(); ++i)
            {
                VertexShaderDeclaration& vsd = meta.vsd.emplace_back();
                const DataBlock* vsdBlock = inputLayout->getBlock(i);

                const char* stream = vsdBlock->getStr(Stream);
                if (!stream || strlen(stream) == 0)
                {
                    return NauMakeError("Field '{}' is required for vertex shader declaration: {}", Stream, nau::shader_tools::pathToUtf8(filename));
                }

                const int32_t number = vsdBlock->getInt(Number);
                if (number < 0)
                {
                    return NauMakeError("Field '{}' must be positive: {}", Number, nau::shader_tools::pathToUtf8(filename));
                }

                vsd.stream = stream;
                vsd.number = number;

                if (vsdBlock->blockCount() == 0)
                {
                    return NauMakeError("Block '{}' is required for vertex shader declaration and must contain at least one register: {}", VsdRegBlockName, nau::shader_tools::pathToUtf8(filename));
                }

                vsd.vsdReg.reserve(vsdBlock->blockCount());
                for (auto j = 0; j < vsdBlock->blockCount(); ++j)
                {
                    VertexShaderDeclarationRegister& reg = vsd.vsdReg.emplace_back();

                    const char* semanticName = vsdBlock->getBlock(j)->getStr(SemanticName);
                    if (!semanticName || strlen(semanticName) == 0)
                    {
                        return NauMakeError("Field '{}' is required for register: {}", SemanticName, nau::shader_tools::pathToUtf8(filename));
                    }

                    const char* type = vsdBlock->getBlock(j)->getStr(Type);
                    if (!type || strlen(type) == 0)
                    {
                        return NauMakeError("Field '{}' is required for register: {}", Type, nau::shader_tools::pathToUtf8(filename));
                    }

                    reg.semanticName = semanticName;
                    reg.type = type;
                }
            }
        }

        const DataBlock* permutations = metadata.getBlockByNameEx(PermutationsBlockName);
        if (!permutations || permutations->blockCount() == 0)
        {
            return NauMakeError("Block '{}' is required and must contain at least the 'Regular' permutation: {}", PermutationsBlockName, nau::shader_tools::pathToUtf8(filename));
        }

        for (auto i = 0; i < permutations->blockCount(); ++i)
        {
            ShaderPermutation& permutation = meta.permutations.emplace_back();

            const char* name = permutations->getBlock(i)->getStr(Name);
            if (!name || strlen(name) == 0)
            {
                return NauMakeError("Field '{}' is required for permutation: {}", Name, nau::shader_tools::pathToUtf8(filename));
            }

            permutation.name = name;

            const DataBlock* defines = permutations->getBlock(i)->getBlockByNameEx(DefinesBlockName);
            for (auto j = 0; j < defines->paramCount(); ++j)
            {
                std::string_view define = defines->getStr(j);
                permutation.defines.emplace_back(define.begin(), define.end());
            }
        }

        return meta;
    }
} // namespace nau
