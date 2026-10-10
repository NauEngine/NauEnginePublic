// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/asset_tools/compilers/shader_compilers.h"

#include "nau/asset_tools/asset_compiler.h"
#if defined(_WIN32) || defined(_WIN64)
    #include "nau/shared/platform/win/process.h"
#elif defined(__linux__) || defined(__linux)
    #include "nau/shared/platform/linux/process.h"
#else
    #include "nau/shared/platform/mac/process.h"
#endif
#include <format>

#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"

namespace nau
{
    namespace compilers
    {
        std::filesystem::path findMeta(std::string path, const std::string& fileName)
        {
            FileSystem fs;
            std::replace(path.begin(), path.end(), '\\', '/');
            if (fs.existInFolder(std::filesystem::u8path(path), fileName, false))
            {
                return std::filesystem::u8path(std::format("{}/{}.blk", path, fileName));
            }
            return std::filesystem::path();
        }

        bool HlslAssetCompiler::canCompile(const std::string& path) const
        {
            FileSystem fs;
            const std::string fileName = pathToUtf8(std::filesystem::u8path(path).stem());
            std::string shaderMetaFolder = path.substr(0, path.find("shaders") + strlen("shaders")) + "/meta";
            std::replace(shaderMetaFolder.begin(), shaderMetaFolder.end(), '\\', '/');
            const bool canCompile = fs.existInFolder(std::filesystem::u8path(shaderMetaFolder), fileName, false);
            if (!canCompile)
            {
                LOG_WARN("Shader {} cannot be compiled because no metafile found in directory {}!", fileName, shaderMetaFolder);
            }
            return canCompile;
        }

        nau::Result<AssetMetaInfo> HlslAssetCompiler::compile(PXR_NS::UsdStageRefPtr stage, const std::string& outputPath, const std::string& projectRootPath, const nau::UsdMetaInfo& metaInfo, int folderIndex)
        {
            FileSystem fs;

            auto extraInfo = reinterpret_cast<ExtraInfoShader*>(metaInfo.extraInfo.get());

            const std::string path = extraInfo->path;

            const std::filesystem::path shadersIn = std::filesystem::u8path(path.substr(0, path.find("shaders") + strlen("shaders")));
            const std::filesystem::path shaderOut = std::filesystem::u8path(outputPath) / std::to_string(folderIndex) / std::string(toString(metaInfo.uid) + ext().data());
            const std::string fileName = FileSystemExtensions::removeExtension(FileSystemExtensions::nameFromPath(path));

            if (!std::filesystem::exists(shaderOut.parent_path()))
            {
                std::filesystem::create_directories(shaderOut.parent_path());
            }

            const std::filesystem::path shadersMeta = findMeta(pathToUtf8(shadersIn) + "/meta", fileName);

            if (shadersMeta.empty())
            {
                return NauMakeError("Failed to compile shader {} because no metafile found in directory {}!", fileName, pathToUtf8(shadersIn) + "/meta");
            }

            std::string shadersInclude = getShadersIncludeDir(shadersIn);
            std::replace(shadersInclude.begin(), shadersInclude.end(), '\\', '/');
            const std::string makeArgs = std::format("ShaderCompilerTool.exe -o \"{}\" -s \"{}\" -m \"{}\" -i {} -c {}", pathToUtf8(shaderOut.parent_path()), path, pathToUtf8(shadersMeta), shadersInclude, toString(metaInfo.uid));

            IProcessWorker process;

            if (const int processResult = process.runProcess(makeArgs); processResult != 0)
            {
                return NauMakeError("Failed to compile shader {}, ShaderCompilerTool.exe returned exit code {}!", fileName, processResult);
            }

            const std::string output = std::format("{}/{}{}", folderIndex, toString(metaInfo.uid), ext().data());

            return makeAssetMetaInfo(path, metaInfo.uid, output, "hlsl", "shader");
        }
    }  // namespace compilers

}  // namespace nau
