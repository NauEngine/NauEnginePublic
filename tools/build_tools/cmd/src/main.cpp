// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include <filesystem>
#include <vector>
#include <string>

#include "argparse/argparse.hpp"
#include "iostream"
#include "nau/build_tool/build_config.h"
#include "nau/build_tool/build_tool.h"
#include "nau/shared/args.h"
#include "nau/shared/error_codes.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"

namespace nau
{
    int buildProject(const BuildProjectArguments& args);
    int compileProject(const BuildProjectArguments& args);

    int parse(int argc, char** const& argv)
    {
        argparse::ArgumentParser programArgs("build_tool", "0.3.0");
        programArgs.add_argument("--log")
            .help("Log output path or current exe directory");
        programArgs.add_argument("--verbose")
            .default_value(false)
            .implicit_value(true)
            .help("Log level");

        argparse::ArgumentParser build("build");
        build.add_description("Compiles and built assets into package.");
        build.add_argument("--project")
            .required()
            .help("Project path");
        build.add_argument("--config")
            .default_value("Debug")
            .help("Config for building binaries (if none, Debug will be used).");
        build.add_argument("--targetDir")
            .required()
            .help("Target directory for building binaries and assets.");
        build.add_argument("--preset")
            .default_value("win_vs2022_x64_dll")
            .help("CMake preset name");
        build.add_argument("--openAfterBuild")
            .default_value(false)
            .implicit_value(true)
            .help("Open project directory after build.");
        build.add_argument("--skipAssetsCompilation")
            .default_value(false)
            .implicit_value(true)
            .help("Skips compiling assets.");
        build.add_argument("--skipSourcesCompilation")
            .default_value(false)
            .implicit_value(true)
            .help("Skips compiling sources.");
        build.add_argument("--postBuildCopy")
            .default_value(false)
            .implicit_value(true)
            .help("Manually copies bin folder to target dir.");

        argparse::ArgumentParser compile("compile");
        compile.add_description("Compiles sources.");
        compile.add_argument("--project")
            .required()
            .help("Project path");
        compile.add_argument("--config")
            .default_value("Debug")
            .help("Config for building binaries (if none, Debug will be used).");
        compile.add_argument("--preset")
            .default_value("win_vs2022_x64_dll")
            .help("CMake preset name");

        programArgs.add_subparser(build);
        programArgs.add_subparser(compile);

        try
        {
            programArgs.parse_args(argc, argv);
        }
        catch (const std::exception& err)
        {
            std::cout << "Fatal! Could not parse arguments!" << std::endl;
            std::cerr << err.what() << std::endl;
            std::cerr << programArgs;

            return ErrorCode::invalidArgumentsError;
        }

        const std::string logPath = programArgs.is_used("--log") ? programArgs.get<std::string>("--log") : pathToUtf8(std::filesystem::current_path());
        const bool verbose = programArgs.is_used("--verbose");
        logger::init(logPath, verbose);

        // For console output
        logger::addConsoleOutput(verbose);

        try
        {
            if (programArgs.is_subcommand_used("build"))
            {
                BuildProjectArguments args;

                args.projectPath = build.get<std::string>("--project");
                args.config = build.get<std::string>("--config");
                args.targetDirectory = build.get<std::string>("--targetDir");
                args.openAfterBuild = build.get<bool>("--openAfterBuild");
                args.cMakePreset = build.get<std::string>("--preset");
                args.compileAssets = !build.get<bool>("--skipAssetsCompilation");
                args.compileSources = !build.get<bool>("--skipSourcesCompilation");
                args.copyBinaries = build.get<bool>("--postBuildCopy") && !args.compileSources;

                LOG_INFO("Build project at path {} to {}", args.projectPath, args.targetDirectory);

                return buildProject(args);
            }
            else if (programArgs.is_subcommand_used("compile"))
            {
                BuildProjectArguments args;

                args.projectPath = compile.get<std::string>("--project");
                args.config = compile.get<std::string>("--config");
                args.cMakePreset = compile.get<std::string>("--preset");

                LOG_INFO("Compile project at path {}", args.projectPath);

                return compileProject(args);
            }
            else
            {
                LOG_ERROR("Unknown command {}", std::to_string(ErrorCode::invalidArgumentsError));
                return ErrorCode::invalidArgumentsError;
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Failed processing project: {}", e.what());

            return ErrorCode::invalidArgumentsError;
        }

        return ErrorCode::success;
    }
    int buildProject(const BuildProjectArguments& args)
    {
        std::shared_ptr<IBuildTool> tool = IBuildTool::get();
        BuildConfig config;
        config.buildConfiguration = args.config;
        config.targetDestination = args.targetDirectory;
        config.projectPath = args.projectPath;
        config.openAfterBuild = args.openAfterBuild;
        config.compileSources = args.compileSources;
        config.compileAssets = args.compileAssets;
        config.preset = args.cMakePreset;
        config.forceCopyBinaries = args.copyBinaries;

        BuildResult code = BuildResult::Invalid;

        nau::BuildResultCallback result = [&](const BuildResult& result, const std::string& msg)
        {
            code = result;
            LOG_INFO(msg);
        };

        nau::ProgressCallback progress = [&](int progress)
        {

        };

        tool->build(config, progress, result);

        LOG_FASSERT(code == BuildResult::Invalid, "Could not run build tool");
        return code == BuildResult::Success ? ErrorCode::success : ErrorCode::projectBuildFailed;
    }

    int compileProject(const BuildProjectArguments& args)
    {
        std::shared_ptr<IBuildTool> tool = IBuildTool::get();

        BuildConfig config;

        config.buildConfiguration = args.config;
        config.projectPath = args.projectPath;
        config.preset = args.cMakePreset;

        BuildResult code = BuildResult::Invalid;

        nau::BuildResultCallback result = [&](const BuildResult& result, const std::string& msg)
        {
            code = result;
            LOG_INFO(msg);
        };

        nau::ProgressCallback progress = [&](int progress)
        {

        };

        [[maybe_unused]] const auto result_ = tool->compile(config, progress, result);
        return code == BuildResult::Success ? ErrorCode::success : ErrorCode::projectBuildFailed;
    }
}  // namespace nau

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
