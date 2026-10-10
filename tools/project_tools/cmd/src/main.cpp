// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include <iostream>
#include <filesystem>
#include <string>
#include <vector>

#include "nau/shared/args.h"
#include "nau/project_tools/project_api.h"
#include "argparse/argparse.hpp"
#include "nau/shared/file_system.h"
#include "nau/shared/error_codes.h"
#include "nau/shared/logger.h"

namespace nau
{
    int parse(int argc, char **const &argv)
    {
        argparse::ArgumentParser programArgs("project_tool", NAU_VERSION);
        programArgs.add_argument("--log")
            .help("Log output path or current exe directory");
        programArgs.add_argument("--verbose")
            .default_value(false)
            .implicit_value(true)
            .help("Log level");
        programArgs.add_argument("--tools")
            .help("Path to tools directory");
 
        argparse::ArgumentParser initProjectArgs("init");
        initProjectArgs.add_description("Generates a game project from the template and optionally with console arguments provided generates CMake code project.");
        initProjectArgs.add_argument("--project")
            .required()
            .help("Project path");
        initProjectArgs.add_argument("--preset")
            .default_value("win_vs2022_x64_dll")
            .help("CMake preset name");
        initProjectArgs.add_argument("--name")
            .default_value("MyProject")
            .help("Project name");
        initProjectArgs.add_argument("--template")
            .required()
            .help("Template name");
        initProjectArgs.add_argument("--contentOnly")
            .default_value(false)
            .implicit_value(true)
            .help("Inits content-only type of project");
        initProjectArgs.add_argument("--generate")
            .default_value(false)
            .implicit_value(true)
            .help("Generates solution and project files");
        initProjectArgs.add_argument("--openIde")
            .default_value(false)
            .implicit_value(true)
            .help("Should open IDE on end?");

        argparse::ArgumentParser rebuildProjectArgs("clean_rebuild");
        rebuildProjectArgs.add_description("Regenerates solution and project files.");
        rebuildProjectArgs.add_argument("--preset")
            .default_value("win_vs2022_x64")
            .help("CMake preset name");
        rebuildProjectArgs.add_argument("--project")
            .required()
            .help("Project path");
        rebuildProjectArgs.add_argument("--openIde")
            .default_value(false)
            .implicit_value(true)
            .help("Should open IDE on end?");
        rebuildProjectArgs.add_argument("--name")
            .default_value("MyProject")
            .help("Project name");

        argparse::ArgumentParser upgradeProjectArgs("upgrade");
        upgradeProjectArgs.add_description("Upgrades project version.");
        upgradeProjectArgs.add_argument<std::string>("--v")
            .required()
            .help("Which version should this save use?");
        upgradeProjectArgs.add_argument("--name")
            .default_value("MyProject")
            .help("Project name");
        upgradeProjectArgs.add_argument("--project")
            .required()
            .help("Project path");
        upgradeProjectArgs.add_argument("--do-not-upgrade")
            .default_value(false)
            .help("If provided, asset builder will not be called.");

        argparse::ArgumentParser saveProjectArgs("save");
        saveProjectArgs.add_description("Writes and merge configs of nauproject file.");
        saveProjectArgs.add_argument("--cfg")
            .required()
            .help("Escaped JSON params string with new values.");
        saveProjectArgs.add_argument("--do-not-upgrade")
            .default_value(false)
            .help("If provided, asset builder will not be called.");
        saveProjectArgs.add_argument("--name")
            .default_value("MyProject")
            .help("Project name");
        saveProjectArgs.add_argument("--project")
            .required()
            .help("Project path");

        programArgs.add_subparser(initProjectArgs);
        programArgs.add_subparser(rebuildProjectArgs);
        programArgs.add_subparser(upgradeProjectArgs);
        programArgs.add_subparser(saveProjectArgs);

        try
        {
            programArgs.parse_args(argc, argv);
        }
        catch (const std::exception &err)
        {
            std::cout << "Fatal! Could not parse arguments!" << std::endl;
            std::cerr << err.what() << std::endl;
            std::cerr << programArgs;

            return ErrorCode::invalidArgumentsError;
        }

        const std::string logPath = programArgs.is_used("--log") ? programArgs.get<std::string>("--log") : pathToUtf8(std::filesystem::current_path());
        const bool verbose = programArgs.is_used("--verbose");
        const std::string toolsPath = programArgs.is_used("--tools") ? programArgs.get<std::string>("--tools") : FileSystemExtensions::findDirectoryInUpperRecursive(pathToUtf8(std::filesystem::current_path()), "project_templates");

        logger::init(logPath, verbose);

        // For console output
        logger::addConsoleOutput(verbose);

        if (toolsPath.empty())
        {
            LOG_ERROR("Could not find tools directory!\nProvide corrent path with --tools or check your build directory!");
            return ErrorCode::invalidPathError;
        }

        try
        {
            if (programArgs.is_subcommand_used("init"))
            {
                auto args = std::make_unique<nau::InitProjectArguments>();

                args->projectPath = initProjectArgs.get<std::string>("--project");
                args->templateName = initProjectArgs.get<std::string>("--template");
                args->projectName = initProjectArgs.get<std::string>("--name");
                args->contentOnly = initProjectArgs.get<bool>("--contentOnly");
                args->generateSolutionFile = initProjectArgs.get<bool>("--generate");
                args->cMakePreset = initProjectArgs.get<std::string>("--preset");
                args->openIde = initProjectArgs.get<bool>("--openIde");
                args->toolsPath = toolsPath;

                LOG_FASSERT(args->contentOnly && args->generateSolutionFile, "Content only and generate solution files are mutually exclusive!");
                LOG_INFO("Init project {} at path {} ", args->projectName, args->projectPath);

                return initProject(args.get());
            }
            else if (programArgs.is_subcommand_used("clean_rebuild"))
            {
                auto args = std::make_unique<nau::RebuildProjectArguments>();

                args->projectPath = rebuildProjectArgs.get<std::string>("--project");
                args->projectName = rebuildProjectArgs.get<std::string>("--name");
                args->cMakePreset = rebuildProjectArgs.get<std::string>("--preset");
                args->openIde = rebuildProjectArgs.get<bool>("--openIde");
                args->toolsPath = toolsPath;

                return rebuildProject(args.get());
            }
            else if (programArgs.is_subcommand_used("upgrade"))
            {
                auto args = std::make_unique<nau::UpgradeProjectArguments>();

                args->projectPath = upgradeProjectArgs.get<std::string>("--project");
                args->projectName = upgradeProjectArgs.get<std::string>("--name");
                args->version = upgradeProjectArgs.get<std::string>("--v");
                args->dontUpgrade = upgradeProjectArgs.get<bool>("--do-not-upgrade");
                args->toolsPath = toolsPath;

                return upgradeProject(args.get());
            }
            else if (programArgs.is_subcommand_used("save"))
            {
                auto args = std::make_unique<nau::SaveProjectArguments>();

                args->projectPath = saveProjectArgs.get<std::string>("--project");
                args->projectName = saveProjectArgs.get<std::string>("--name");
                args->config = saveProjectArgs.get<std::string>("--cfg");
                args->dontUpgrade = saveProjectArgs.get<bool>("--do-not-upgrade");
                args->toolsPath = toolsPath;

                return saveProject(args.get());
            }
            else
            {
                LOG_ERROR("Unknown command {}.", std::to_string(ErrorCode::invalidArgumentsError));
                return ErrorCode::invalidArgumentsError;
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("Failed to initialize project: {}", e.what());

            return ErrorCode::invalidArgumentsError;
        }

        return ErrorCode::success;
    }
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    std::vector<std::string> arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i)
    {
        arguments.push_back(nau::pathToUtf8(std::filesystem::path(argv[i])));
    }
    std::vector<char*> utf8Argv;
    utf8Argv.reserve(argc + 1);
    for (auto& argument : arguments)
    {
        utf8Argv.push_back(argument.data());
    }
    utf8Argv.push_back(nullptr);
    return nau::parse(argc, utf8Argv.data());
}
#else
int main(int argc, char** argv)
{
    return nau::parse(argc, argv);
}
#endif
