// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/project_tools/project_manager.h"

#include <format>
#include <sstream>
#include <string>

#include "nau/project_tools/project_template.h"
#include "nau/project_tools/string_processor.h"
#include "nau/shared/args.h"
#include "nau/shared/error_codes.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"
#include "nau/shared/util.h"

#if defined(_WIN32) || defined(_WIN64)
    #include "nau/shared/platform/win/process.h"
#elif defined(__linux__) || defined(__linux)
    #include "nau/shared/platform/linux/process.h"
#else
    #include "nau/shared/platform/mac/process.h"
#endif
#include <nau/shared/macro.h>

#include "nlohmann/json.hpp"

namespace nau
{
    constexpr static char g_pattern[] = "%%\\((.*?)\\)%%";
    constexpr static int g_intent = 4;

    static bool compileShaders(const std::filesystem::path& shadersDir, FileSystem& fs)
    {
        if (std::filesystem::exists(shadersDir))
        {
            const std::filesystem::path shadersMeta = shadersDir / "meta";
            const std::filesystem::path shadersSrc = shadersDir / "src";
            const std::filesystem::path shadersOut = shadersDir / "cache";

            LOG_INFO("Making shader cache at path {}!", pathToUtf8(shadersDir));

            std::string shadersInclude = getShadersIncludeDir(shadersDir);
            std::replace(shadersInclude.begin(), shadersInclude.end(), '\\', '/');

            const std::string makeArgs = std::format("ShaderCompilerTool.exe -o \"{}\" -s \"{}\" -m \"{}\" -i {} -c {}",
                                                     pathToUtf8(shadersOut),
                                                     pathToUtf8(shadersSrc),
                                                     pathToUtf8(shadersMeta),
                                                     shadersInclude,
                                                     "shader_cache.nsbc");
            IProcessWorker process;

            LOG_INFO("Compiling shaders {}", makeArgs);

            return process.runProcess(makeArgs) == 0;
        }

        LOG_WARN("Could not find shaders directory at path {}!", pathToUtf8(shadersDir));

        return false;
    }

    static int openIde(const std::filesystem::path& projectPath, const std::string& projectName, const std::string& preset, FileSystem& fs)
    {
        const std::filesystem::path idePath = projectPath / "build" / std::format("{}.{}", projectName, util::getIdeExtension(preset));

        if (!fs.exist(idePath))
        {
            LOG_WARN("Could not find IDE executable!\nIDE executable is not found at path {}", idePath.string());
            return 1;
        }

        LOG_INFO("Opening IDE...");

        const std::string ideArgs = std::format("start {}", idePath.string());

        return std::system(ideArgs.c_str());
    }

    int NauInitProjectJob::run(const CommonArguments* const params)
    {
        const InitProjectArguments* args = static_cast<const InitProjectArguments*>(params);
        const std::filesystem::path pathToTemplate = std::filesystem::u8path(args->toolsPath) / "project_templates" / std::filesystem::u8path(args->templateName);
        const std::filesystem::path projectPath = std::filesystem::u8path(args->projectPath) / std::filesystem::u8path(args->projectName);

        FileSystem fs;

        if (!fs.exist(pathToTemplate))
        {
            return result(std::format("Template not found at path {}", pathToUtf8(pathToTemplate)), ErrorCode::invalidPathError);
        }

        if (fs.exist(projectPath))
        {
            return result(std::format("Project already exists at path {}", pathToUtf8(projectPath)), ErrorCode::invalidPathError);
        }

        // Init default assets folder for project
        std::string assetsPath = pathToUtf8(projectPath / "assets");
        std::replace(assetsPath.begin(), assetsPath.end(), '\\', '/');

        // This is used in StringProcessor to replace regex matches
        NauProjectParams projectParams = {
            {         "ProjectPath",                          args->projectPath},
            {             "Version",                    getVersion().toString()},
            {          "AssetsPath",                                 assetsPath},
            {         "ProjectName",                          args->projectName},
            {        "TemplateName",                         args->templateName},
            {         "CMakePreset",                          args->cMakePreset},
            {         "ContentOnly",          std::to_string(args->contentOnly)},
            {"GenerateSolutionFile", std::to_string(args->generateSolutionFile)}
        };

        const NauProjectTemplate projectTemplate = NauProjectTemplate(args->templateName, pathToTemplate, projectParams, &fs);

        if (!projectTemplate.isValid())
        {
            return result("Could not intialize project!", ErrorCode::invalidPathError);
        }

        StringProcessor processor;

        std::filesystem::path finalPath = std::filesystem::u8path(args->projectPath) / std::filesystem::u8path(args->projectName);

        LOG_INFO("Creating project at path {}", pathToUtf8(finalPath));

        std::error_code error;

        // Load project template files
        for (auto& fileInfo : projectTemplate.getFiles())
        {
            std::filesystem::path to = finalPath / std::filesystem::u8path(fileInfo.subpath);

            if (!fs.createDirectoryRecursive(to, error))
            {
                return result(error.message(), ErrorCode::internalError);
            }

            std::string finalName;

            if (fileInfo.rename)
            {
                finalName = args->projectName + fileInfo.extension;

                if (fileInfo.lowercase)
                {
                    processor.toLower(finalName);
                }
            }
            else
            {
                finalName = fileInfo.name;
            }

            LOG_INFO("Saving file {} at path {}", finalName, pathToUtf8(to));

            std::filesystem::path savePath = to / std::filesystem::u8path(finalName);

            fs.copyFile(std::filesystem::u8path(fileInfo.path), savePath);

            // Update file content, replace via regex
            if (fileInfo.updateContent)
            {
                std::stringstream content;

                if (!fs.readFile(savePath, content))
                {
                    return result("Could not read saved file!", ErrorCode::internalError);
                }

                // Used to be ensure that all matches are processed
                int counter = 0;

                std::string processedContent;

                try
                {
                    processedContent = processor.processRegexMatches(g_pattern, content.str(), projectParams, counter);
                }
                catch (const std::exception& e)
                {
                    return result(std::format("Could not process regex in file {}\n{}", pathToUtf8(savePath), e.what()), ErrorCode::internalError);
                }

                if (counter != 0)
                {
                    return result(std::format("Could not process regex in file {}", pathToUtf8(savePath)), ErrorCode::internalError);
                }

                LOG_INFO("Processed file {}", pathToUtf8(savePath));

                fs.writeFile(savePath, processedContent);

                LOG_INFO("Saved file {}", pathToUtf8(savePath));
            }
        }

        NAU_ASSERT(nau::util::checkEnvironmentVariables(), "Invalid environment!");

        {
            const std::filesystem::path shadersResourcesDir = finalPath / "resources" / "shaders";

            LOG_INFO("Compiling shaders...");

            if (!compileShaders(shadersResourcesDir, fs))
            {
                return result("Could not compile shaders!", ErrorCode::internalError);
            }

            LOG_INFO("Compiling ui shaders...");

            if (!compileShaders(finalPath / "resources" / "ui" / "shaders", fs))
            {
                return result("Could not compile ui shaders!", ErrorCode::internalError);
            }

            LOG_INFO("Shaders compiled!");
        }

        NAU_ASSERT(nau::util::validateEnvironment(), "Invalid environment!");

        // Generate CMake project if argument is set
        // Starts a new popen process and read output
        if (args->generateSolutionFile)
        {
            const std::filesystem::path buildPath = projectPath / "build";

            LOG_INFO("Generating solution file at path {}", buildPath.string());

            const std::string makeArgs = std::format("cmake -B {} -S {} --preset {} -A x64", buildPath.string(), projectPath.string(), args->cMakePreset);

            IProcessWorker process;

            if (const int processResult = process.runProcess(makeArgs); processResult != 0)
            {
                return result("Could not generate solution file!", ErrorCode::internalError);
            }

            LOG_INFO("Solution generated at path {}", buildPath.string());

            if (args->openIde)
            {
                openIde(finalPath, args->projectName, args->cMakePreset, fs);
            }
        }

        return 0;
    }

    int NauRebuildProjectJob::run(const CommonArguments* const params)
    {
        const RebuildProjectArguments* args = static_cast<const RebuildProjectArguments*>(params);

        FileSystem fs;

        std::filesystem::path pathToBuild = std::filesystem::path(args->projectPath) / "build";
        const std::filesystem::path projectPath = std::filesystem::path(args->projectPath);

        if (fs.exist(pathToBuild))
        {
            LOG_INFO("Deleting old build directory at path {}", pathToBuild.string());

            fs.deleteDirectory(pathToBuild);
        }

        const std::string makeArgs = std::format("cmake -B {} -S {} --preset {} -A x64", pathToBuild.string(), projectPath.string(), args->cMakePreset);

        IProcessWorker process;

        if (const int processResult = process.runProcess(makeArgs); processResult != 0)
        {
            return result("Could not generate solution file!", ErrorCode::internalError);
        }

        if (args->openIde)
        {
            openIde(projectPath, args->projectName, args->cMakePreset, fs);
        }

        return 0;
    }

    int NauUpgradeProjectJob::run(const CommonArguments* const params)
    {
        const UpgradeProjectArguments* args = static_cast<const UpgradeProjectArguments*>(params);

        FileSystem fs;

        nlohmann::json json;
        std::stringstream ss;

        const std::filesystem::path projectPath = std::filesystem::path(args->projectPath);
        const std::string projectConfig = args->projectName + FileSystemExtensions::g_configExtension;
        const std::filesystem::path configPath = projectPath / projectConfig;

        if (!fs.exist(configPath))
        {
            return result("Could not find project config file!", ErrorCode::invalidPathError);
        }

        fs.readFile(configPath, ss);

        json = nlohmann::json::parse(ss.str());

        if (!json.is_object())
        {
            return result("Could not parse project config file!", ErrorCode::internalError);
        }

        try
        {
            const std::string projectVersion = json["ProjectVersion"].get<std::string>();

            NauVersion currentVersion = NauVersion(projectVersion);
            NauVersion newVersion = NauVersion(args->version);

            LOG_FASSERT(currentVersion >= newVersion, "Current version is greater than new version!");

            json["ProjectVersion"] = args->version;

            LOG_INFO("Upgraded project version from {} to {}", projectVersion, args->version);

            return fs.writeFile(configPath, json.dump(g_intent)) ? ErrorCode::success : ErrorCode::internalError;
        }
        catch (const std::exception& e)
        {
            return result(std::format("Failed to upgrade project config file!\n{}", e.what()), ErrorCode::internalError);
        }

        return 0;
    }

    int NauSaveProjectJob::run(const CommonArguments* const params)
    {
        const SaveProjectArguments* args = static_cast<const SaveProjectArguments*>(params);

        FileSystem fs;

        nlohmann::json newjson, config;
        std::stringstream ss;

        const std::filesystem::path projectPath = std::filesystem::u8path(args->projectPath);
        const std::string projectConfig = args->projectName + FileSystemExtensions::g_configExtension;
        const std::filesystem::path configPath = projectPath / std::filesystem::u8path(projectConfig);

        if (!fs.exist(configPath))
        {
            return result("Could not find project config file!", ErrorCode::invalidPathError);
        }

        fs.readFile(configPath, ss);

        config = nlohmann::json::parse(ss.str());
        newjson = nlohmann::json::parse(args->config);

        if (!config.is_object() || !newjson.is_object())
        {
            return result("Could not parse project config file!", ErrorCode::internalError);
        }

        if (newjson.contains("Project"))
        {
            LOG_FASSERT(config["Project"] == newjson["Project"], "Project name does not match!");
        }

        bool requiresRecompile = false;

        try
        {
            if (newjson.contains("ProjectVersion"))
            {
                NauVersion currentVersion = NauVersion(config["ProjectVersion"].get<std::string>());
                NauVersion newVersion = NauVersion(newjson["ProjectVersion"].get<std::string>());

                LOG_FASSERT(currentVersion > newVersion, "Current version is greater than new version! Aborting...");

                requiresRecompile = currentVersion < newVersion;

                LOG_COND(requiresRecompile, "Versions do not match, asset compiler will be run...");
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed to parse project version: {}", e.what());
            return ErrorCode::invalidArgumentsError;
        }

        util::mergeJsonRecursive(config, newjson);

        return fs.writeFile(configPath, config.dump(g_intent)) ? ErrorCode::success : ErrorCode::internalError;
    }
}  // namespace nau
