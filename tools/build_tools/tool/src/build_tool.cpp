// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/build_tool/build_tool.h"

#include "nau/build_tool/build_config.h"
#if defined(_WIN32) || defined(_WIN64)
    #include "nau/shared/platform/win/process.h"
#elif defined(__linux__) || defined(__linux)
    #include "nau/shared/platform/linux/process.h"
#else
    #include "nau/shared/platform/mac/process.h"
#endif

#include <nau/io/special_paths.h>
#include <nau/module/module_manager.h>
#include <nau/project_tools/project_info.h>
#include <nau/service/service_provider.h>
#include <nau/shared/macro.h>
#include <nau/shared/util.h>

#include <format>
#include <fstream>
#include <vector>
#include <stdexcept>
#include "build_progress.h"

#include "nau/app/application.h"
#include "nau/app/application_services.h"
#include "nau/app/platform_window.h"
#include "nau/app/window_manager.h"
#include "nau/asset_pack/asset_pack_builder.h"
#include "nau/asset_tools/asset_api.h"
#include "nau/asset_tools/asset_manager.h"
#include "nau/asset_tools/db_manager.h"
#include "nau/build_tool/interface/build_tool.h"
#include "nau/io/memory_stream.h"
#include "nau/io/virtual_file_system.h"
#include "nau/serialization/json_utils.h"
#include "nau/serialization/serialization.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"
#include "nau/shared/platform/win/utils.h"

namespace nau
{
#define NAU_PROGRESS_LOG(progress) m_progressCallback(progress);
#define NAU_MKDIR(dir)                                                      \
    {                                                                       \
        std::error_code err;                                                \
        fs.createDirectoryRecursive(dir, err);                              \
        if (err)                                                            \
        {                                                                   \
            fail("Could not create build directory!", BuildResult::Failed); \
            return BuildResult::Failed;                                     \
        }                                                                   \
    }
#define NAU_ERROR(err, ...)                                              \
    {                                                                    \
        app->stop();                                                     \
        while (app->step())                                              \
        {                                                                \
            std::this_thread::yield();                                   \
        };                                                               \
        return fail(std::format(err, __VA_ARGS__), BuildResult::Failed); \
    }

#define NAU_CHECK_IS_CANCELLED       \
    if (m_cancelled)                 \
    {                                \
        LOG_INFO("Build cancelled"); \
        return;                      \
    }

    void copyDirectory(const std::filesystem::path& source, const std::filesystem::path& destination,
                       const std::vector<std::string>& extensionsBlacklist, const char* stage)
    {
        using namespace std::filesystem;
        const auto sourceRoot = weakly_canonical(source);
        const auto targetRoot = weakly_canonical(destination);
        const auto inside = targetRoot.lexically_relative(sourceRoot);
        if (!inside.empty() && *inside.begin() != "..")
            throw std::runtime_error("Build copy destination must be outside its source directory");
        build_progress::report(stage);
        std::vector<path> files;
        uint64_t totalBytes = 0;
        create_directories(destination);
        for (const auto& entry : recursive_directory_iterator(source))
        {
            if (std::find(extensionsBlacklist.begin(), extensionsBlacklist.end(), entry.path().extension().string()) != extensionsBlacklist.end()) continue;
            if (entry.is_directory()) create_directories(destination / relative(entry.path(), source));
            else if (entry.is_regular_file())
            {
                files.push_back(entry.path());
                totalBytes += entry.file_size();
            }
        }
        const bool bytes = totalBytes != 0;
        const uint64_t total = bytes ? totalBytes : files.size();
        uint64_t completed = 0;
        build_progress::report(stage, 0, total, bytes ? "bytes" : "files");
        std::vector<char> buffer(1024 * 1024);
        for (const auto& file : files)
        {
            const auto relativePath = relative(file, source);
            const auto target = destination / relativePath;
            create_directories(target.parent_path());
            std::ifstream input(file, std::ios::binary);
            std::ofstream output(target, std::ios::binary | std::ios::trunc);
            if (!input || !output) throw std::runtime_error("Could not open file for build copy: " + file.string());
            while (input)
            {
                input.read(buffer.data(), buffer.size());
                const auto count = input.gcount();
                if (count > 0)
                {
                    output.write(buffer.data(), count);
                    if (!output) throw std::runtime_error("Could not write build copy: " + target.string());
                    if (bytes) completed += count;
                    build_progress::report(stage, completed, total, bytes ? "bytes" : "files", build_progress::utf8(relativePath));
                }
            }
            if (!input.eof()) throw std::runtime_error("Could not read build copy: " + file.string());
            output.close();
            if (!output) throw std::runtime_error("Could not finish build copy: " + target.string());
            permissions(target, status(file).permissions());
            if (!bytes) ++completed;
            build_progress::report(stage, completed, total, bytes ? "bytes" : "files", build_progress::utf8(relativePath));
        }
        build_progress::report(stage, completed, total, bytes ? "bytes" : "files");
    }

    void configureVirtualFileSystem(io::IVirtualFileSystem& vfs, const BuildConfig& config)
    {
        auto contentFs = io::createNativeFileSystem(config.projectPath);
        vfs.mount("/project", std::move(contentFs)).ignore();
    }

    std::shared_ptr<IBuildTool> IBuildTool::get()
    {
        static std::shared_ptr<IBuildTool> instance = std::make_shared<BuildTool>();
        return instance;
    }

    void BuildTool::build(const BuildConfig& config, ProgressCallback& progressCallback, BuildResultCallback& resultCallback)
    {
        build_progress::Scope events;
        build_progress::report("prepare");
        m_progressCallback = progressCallback;
        m_resultCallback = resultCallback;
        m_buildConfig = std::make_unique<BuildConfig>(config);
        m_failed = false;

        const std::filesystem::path pathToProject = std::filesystem::path(m_buildConfig->projectPath);

        NAU_ASSERT(std::filesystem::exists(pathToProject), "Project path does not exist {}", config.projectPath);
        NAU_ASSERT(nau::util::validateEnvironment(), "Invalid environment!");

        auto app = createApplication([config]
        {
            loadModulesList(NAU_MODULES_LIST).ignore();
            configureVirtualFileSystem(getServiceProvider().get<io::IVirtualFileSystem>(), config);
            return nau::ResultSuccess;
        });

        app->startupOnCurrentThread();

        FileSystem fs;

        if (!fs.exist(pathToProject) || fs.isEmpty(pathToProject))
        {
            NAU_ERROR("Project path does not exist or is empty {}", m_buildConfig->projectPath);
        }

        const std::filesystem::path targetDestination = std::filesystem::path(m_buildConfig->targetDestination);

        if (pathToProject.compare(targetDestination) == 0)
        {
            NAU_ERROR("Project path and target destination are the same {}!", m_buildConfig->projectPath);
        }

        NAU_CHECK_IS_CANCELLED

        NAU_PROGRESS_LOG(10);

        if (config.compileSources)
        {
            LOG_INFO("Compile project sources at path {}", m_buildConfig->projectPath);

            {
                auto result = compileSources();

                if (result != BuildResult::Success)
                {
                    NAU_ERROR("Could not compile sources at path {}", m_buildConfig->projectPath);
                }
            }
        }
        else if (config.forceCopyBinaries)
        {
            LOG_INFO("Copying project binaries at path {}", m_buildConfig->projectPath);

            const std::filesystem::path buildPath = std::filesystem::path(m_buildConfig->targetDestination) / "bin";
            const std::filesystem::path cmakeFiles = std::filesystem::path(m_buildConfig->projectPath) / "build";

            if (!fs.exist(buildPath))
            {
                std::error_code err;

                fs.createDirectoryRecursive(buildPath, err);

                if (err)
                {
                    LOG_ERROR("Could not create firectory {}!", buildPath.string());
                }
            }

            if (!copyBinaries(buildPath, cmakeFiles))
            {
                NAU_ERROR("Could not copy binaries from {}!", buildPath.string());
            }
        }
        else
        {
            LOG_INFO("Skipping project sources compilation at path {}", m_buildConfig->projectPath);
        }

        NAU_CHECK_IS_CANCELLED
        NAU_PROGRESS_LOG(50);

        if (config.compileAssets)
        {
            LOG_INFO("Compile assets at path {}", m_buildConfig->projectPath);

            {
                auto result = compileAssets();

                if (result != BuildResult::Success)
                {
                    NAU_ERROR("Could not compile assets at path {}", m_buildConfig->projectPath);
                }
            }
        }

        NAU_CHECK_IS_CANCELLED
        NAU_PROGRESS_LOG(75)

        LOG_INFO("Create package at path {}", m_buildConfig->projectPath);

        {
            auto result = createPackage();

            if (result != BuildResult::Success)
            {
                NAU_ERROR("Could not create package at path {}", m_buildConfig->projectPath);
            }
        }

        NAU_CHECK_IS_CANCELLED
        NAU_PROGRESS_LOG(95)

        LOG_INFO("Copying resources...");

        const std::filesystem::path resourcesContentPath = std::filesystem::path(m_buildConfig->projectPath) /* getAssetsSubfolderDefaultName() */ / "resources";
        const std::filesystem::path resourcesTargetDir = std::filesystem::path(m_buildConfig->targetDestination) / "resources";

        if (!fs.exist(resourcesContentPath))
        {
            LOG_WARN("Could not find resources folder {}", resourcesContentPath.string());
        }
        else
        {
            std::error_code err;
            fs.createDirectoryRecursive(resourcesTargetDir, err);

            if (err)
            {
                NAU_ERROR("Could not create resources folder {}", resourcesTargetDir.string());
            }
            else
            {
                copyDirectory(resourcesContentPath, resourcesTargetDir, {".nausd"}, "copy-resources");

                NAU_LOG("Resources copied to {}", resourcesTargetDir.string());
            }
        }

        LOG_INFO("Copying config...");

        const std::filesystem::path configPath = std::filesystem::path(m_buildConfig->targetDestination) / "config";
        const std::filesystem::path sourceConfigPath = std::filesystem::path(m_buildConfig->projectPath) / "config";

        if (!fs.exist(sourceConfigPath))
        {
            NAU_ERROR("Could not find config folder {}", configPath.string());
        }

        NAU_CHECK_IS_CANCELLED
        LOG_INFO("Creating config files {}", configPath.string());

        {
            std::error_code err;
            fs.createDirectoryRecursive(configPath, err);

            if (err)
            {
                NAU_ERROR("Could not create config folder {}", configPath.string());
            }
            else
            {
                copyDirectory(sourceConfigPath, configPath, {}, "copy-config");
            }
        }

        build_progress::report("finalize");
        app->stop();

        while (app->step())
        {
            std::this_thread::yield();
        }

        if (!m_failed)
        {
            m_resultCallback(BuildResult::Success, "Done!");

            if (config.openAfterBuild)
            {
                IPlatformUtils::openFolder(m_buildConfig->targetDestination);
            }
        }

        success();

        NAU_PROGRESS_LOG(100)
    }

    void BuildTool::cancel()
    {
        m_cancelled = true;
    }

    bool BuildTool::failed() const
    {
        return m_failed;
    }

    bool BuildTool::compile(const BuildConfig& config, ProgressCallback& progressCallback, BuildResultCallback& resultCallback)
    {
        build_progress::Scope events;
        build_progress::report("prepare");
        m_progressCallback = progressCallback;
        m_resultCallback = resultCallback;
        m_buildConfig = std::make_unique<BuildConfig>(config);
        m_failed = false;

        NAU_VERIFY(std::filesystem::exists(m_buildConfig->projectPath), "Project path is empty!");

        const std::filesystem::path cmakeFiles = std::filesystem::path(m_buildConfig->projectPath) / "build";
        const std::filesystem::path buildPath = std::filesystem::path(m_buildConfig->projectPath) / "bin";

        {
            build_progress::report("configure");
            auto result = runProcess(std::format("cmake -B\"{}\" -S\"{}\" --preset {} -DCMAKE_INSTALL_PREFIX={} -DNAU_CORE_TOOLS=OFF -DNAU_PACKAGE_BUILD=ON -DNAU_CORE_TESTS=OFF -DNAU_CORE_SAMPLES=OFF -DNAU_FORCE_ENABLE_SHADER_COMPILER_TOOL=ON", cmakeFiles.string(), m_buildConfig->projectPath, m_buildConfig->preset, buildPath.string()));

            if (result != BuildResult::Success)
            {
                fail("Could not generate solution file!", result);
                return false;
            }

            LOG_INFO("Solution file generated at path {}", cmakeFiles.string());
        }

        {
            build_progress::report("compile-sources");
            auto result = runProcess(std::format("cmake --build \"{}\" --config {}", cmakeFiles.string(), m_buildConfig->buildConfiguration));

            if (result != BuildResult::Success)
            {
                fail("Could not build project!", result);
                return false;
            }

            LOG_INFO("Project built at path {}", cmakeFiles.string());
        }

        // Reuturn code
        success();

        return true;
    }

    BuildResult BuildTool::compileSources()
    {
        const std::filesystem::path buildPath = std::filesystem::path(m_buildConfig->targetDestination) / "bin";
        const std::filesystem::path cmakeFiles = std::filesystem::path(m_buildConfig->projectPath) / "build";

        LOG_INFO("Generating solution file at path {}", buildPath.string());

        FileSystem fs;

        if (!fs.exist(cmakeFiles))
        {
            NAU_MKDIR(cmakeFiles);
        }

        if (fs.exist(buildPath))
        {
            fs.deleteDirectory(buildPath);
            NAU_MKDIR(buildPath);
        }
        else
        {
            NAU_MKDIR(buildPath);
        }

        {
            build_progress::report("configure");
            auto result = runProcess(std::format("cmake -B\"{}\" -S\"{}\" --preset {} -DCMAKE_INSTALL_PREFIX={} -DNAU_CORE_TOOLS=OFF -DNAU_PACKAGE_BUILD=ON -DNAU_CORE_TESTS=OFF -DNAU_CORE_SAMPLES=OFF -DNAU_FORCE_ENABLE_SHADER_COMPILER_TOOL=ON", cmakeFiles.string(), m_buildConfig->projectPath, m_buildConfig->preset, buildPath.string()));

            if (result != BuildResult::Success)
            {
                fail("Could not generate solution file!", result);
                return result;
            }

            LOG_INFO("Solution file generated at path {}", cmakeFiles.string());
        }

        {
            build_progress::report("compile-sources");
            auto result = runProcess(std::format("cmake --build \"{}\" --config {}", cmakeFiles.string(), m_buildConfig->buildConfiguration));

            if (result != BuildResult::Success)
            {
                fail("Could not build project!", result);
                return result;
            }

            LOG_INFO("Project built at path {}", cmakeFiles.string());
        }

        if (!copyBinaries(buildPath, cmakeFiles))
        {
            LOG_ERROR("Could not copy binaries from {}!", buildPath.string());
            return BuildResult::Failed;
        }

        return BuildResult::Success;
    }

    BuildResult BuildTool::compileAssets()
    {
        build_progress::report("compile-assets");
        ImportAssetsArguments args;

        args.projectPath = m_buildConfig->projectPath;

        std::unique_ptr<IJob> job = std::make_unique<NauImportAssetsJob>();

        LOG_INFO("Importing assets... {}", m_buildConfig->projectPath);

        auto result = job->run(&args);

        if (result != 0)
        {
            return BuildResult::Failed;
        }

        LOG_INFO("Assets imported at path {}", m_buildConfig->projectPath);

        return BuildResult::Success;
    }

    BuildResult BuildTool::createPackage()
    {
        FileSystem fs;
        PackBuildOptions options;
        options.contentType = "application/json";
        options.description = "Assets package";
        options.version = "0.1";

        LOG_INFO("Creating package... {}", m_buildConfig->targetDestination);

        eastl::vector<PackInputFileData> packData;

        std::filesystem::path assetDbPath = std::filesystem::path(m_buildConfig->projectPath) / getAssetsDBfolderName();
        std::filesystem::path assetsPackPath = std::filesystem::path(m_buildConfig->targetDestination) / getAssetsSubfolderDefaultName();

        NAU_VERIFY(fs.exist(assetDbPath), "Could not find asset database at path {}", assetDbPath.string());

        AssetDatabaseManager& db = AssetDatabaseManager::instance();
        NAU_VERIFY(db.load(assetDbPath.string()), "Could not load asset database at path {}", assetDbPath.string());

        auto& assets = db.assets();

        const std::string assetsDbPath = std::format("{}/{}", getAssetsDBfolderName(), getAssetsDbName());

        // Add asset.db file into package
        const std::string assetDb = "project/" + assetsDbPath;
        {
            PackInputFileData& data = packData.emplace_back();
            data.filePathInPack = assetsDbPath.c_str();
            data.stream = [assetDb]()
            {
                using namespace io;
                IFileSystem& fileSystem = getServiceProvider().get<IFileSystem>();
                auto file = fileSystem.openFile(assetDb, AccessMode::Read, OpenFileMode::OpenExisting);
                if (!file)
                {
                    NAU_ASSERT(false, "Could not open file {}", assetDb);
                }
                return file->createStream();
            };
        }

        std::vector<FileInfo> subFiles;

        for (int i = 0; i < assets.size(); i++)
        {
            const AssetMetaInfo& metafile = assets[i];

            PackInputFileData& data = packData.emplace_back();

            const std::string uid = toString(metafile.uid);
            const std::filesystem::path folderPath = assetDbPath / std::filesystem::path(metafile.dbPath.c_str()).parent_path();
            const std::string compiledExtension = std::filesystem::path(metafile.dbPath.c_str()).extension().string();

            if (compiledExtension.empty())
            {
                LOG_WARN("Could not find compiled extension for asset {}", uid);
                continue;
            }

            fs.findAllFilesByName(folderPath, toString(metafile.uid), subFiles, compiledExtension);

            const std::string filePathInPack = metafile.dbPath.c_str();
            data.filePathInPack = std::format("{}/{}", "assets_database", filePathInPack).c_str();
            std::string relativePath = std::format("project/assets_database/{}", filePathInPack);
            LOG_INFO("Adding {} to package", filePathInPack);
            data.stream = [relativePath, metafile]()
            {
                using namespace io;
                IFileSystem& fileSystem = getServiceProvider().get<IFileSystem>();
                auto file = fileSystem.openFile(relativePath, AccessMode::Read, OpenFileMode::OpenExisting);
                if (!file)
                {
                    NAU_ASSERT(false, "Could not open file {}", relativePath);
                }
                return file->createStream();
            };
        }

        if (subFiles.size() > 0)
        {
            LOG_INFO("Adding subfiles {} to package", subFiles.size());

            for (int j = 0; j < subFiles.size(); j++)
            {
                PackInputFileData& subFileData = packData.emplace_back();

                const std::string fileName = subFiles[j].name;
                const std::string fileExt = subFiles[j].extension;

                // substr from last / to end
                const std::string parentPath = std::filesystem::path(subFiles[j].path).parent_path().string();
                const std::string parentIndex = parentPath.substr(parentPath.find_last_of("/"), std::string::npos);
                const std::string subfileRelativePath = std::format("project/assets_database{}/{}{}", parentIndex, fileName, fileExt);

                subFileData.filePathInPack = std::format("{}{}/{}{}", "assets_database", parentIndex, fileName, fileExt).c_str();

                LOG_INFO("Adding {}/{}{} to package", parentPath, fileName, fileExt);

                subFileData.stream = [subfileRelativePath]()
                {
                    using namespace io;
                    IFileSystem& fileSystem = getServiceProvider().get<IFileSystem>();
                    auto file = fileSystem.openFile(subfileRelativePath, AccessMode::Read, OpenFileMode::OpenExisting);
                    if (!file)
                    {
                        NAU_ASSERT(false, "Could not open file {}", subfileRelativePath);
                    }
                    return file->createStream();
                };
            }
        }

        NAU_ASSERT(assets.size() > 0, "Asset database is empty, something is wrong!");

        const std::filesystem::path packDest = assetsPackPath / "content.assets";

        NAU_MKDIR(assetsPackPath);
        fs.createFile(packDest);

        io::IStreamWriter::Ptr tempStream = io::createNativeFileStream(packDest.string().c_str(), io::AccessMode::Write, io::OpenFileMode::CreateAlways);
        const auto packed = buildAssetPackageWithProgress(packData, options, tempStream,
            [](const PackBuildProgress& progress)
            {
                const bool writing = progress.stage == PackBuildStage::WritingPackage;
                build_progress::report(writing ? "write-package" : "package-assets", progress.completed,
                    progress.total, writing ? "bytes" : "files", progress.file);
            });
        if (packed.isError())
        {
            fail("Could not write asset package", BuildResult::Failed);
            return BuildResult::Failed;
        }

        LOG_INFO("Package created at path {}", assetsPackPath.string());

        return BuildResult::Success;
    }

    void BuildTool::fail(const std::string& msg, BuildResult reason)
    {
        LOG_ERROR("Build failed: {}", msg);
        m_failed = true;
        m_resultCallback(reason, msg);
    }

    void BuildTool::success()
    {
        m_resultCallback(BuildResult::Success, "Done!");
        m_failed = false;
    }

    bool BuildTool::copyBinaries(const std::filesystem::path& buildPath, const std::filesystem::path& cMakeFiles)
    {
        FileSystem fs;

        const auto binaryPath = std::format("{}/bin/{}", m_buildConfig->projectPath, m_buildConfig->buildConfiguration);

        if (!fs.exist(binaryPath))
        {
            fail(std::format("Could not find binary at path {}", binaryPath), BuildResult::Failed);
            return false;
        }

        LOG_INFO("Copying binary from {} to {}", binaryPath, buildPath.string());

        copyDirectory(binaryPath, buildPath, {}, "copy-binaries");

        LOG_INFO("Binaries generated at path {}", buildPath.string());

        // Read app info from .nauproject
        std::filesystem::path appConfigPath = std::filesystem::path(m_buildConfig->projectPath) / fs.findFirst(m_buildConfig->projectPath, FileSystemExtensions::g_configExtension);

        std::stringstream ss;
        fs.readFile(appConfigPath, ss);

        auto appConfig = nlohmann::json::parse(ss.str());
        auto info = std::make_shared<ProjectInfo>(appConfig.template get<ProjectInfo>());

        // TODO: Make with reading main game module from .nauproject
        const std::string exePath = std::format("{}/{}Main.{}", buildPath.string(), info->ProjectName, "exe");

        NAU_ASSERT(exePath != "", "Could not find generated executable in path: {}", buildPath.string());

        const std::filesystem::path exeDir = std::filesystem::path(exePath);
        const std::string shortcutName = exeDir.stem().string() + ".lnk";
        const std::filesystem::path lnkPath = std::filesystem::path(m_buildConfig->targetDestination) / shortcutName;

        LOG_INFO("Creating shortcut {}", shortcutName);

        if (!IPlatformUtils::createLink(exePath, lnkPath.string()))
        {
            LOG_WARN("Could not create shortcut {}", shortcutName);
        }

        return true;
    }

    BuildResult BuildTool::runProcess(std::string makeArgs)
    {
        std::replace(makeArgs.begin(), makeArgs.end(), '\\', '/');

        IProcessWorker process;

        if (const int processResult = process.runProcess(makeArgs); processResult != 0)
        {
            return BuildResult::Failed;
        }

        return BuildResult::Success;
    }
}  // namespace nau