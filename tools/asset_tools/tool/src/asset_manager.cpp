// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/asset_manager.h"

#include <nau/asset_tools/usd_meta_processor.h>
#include <nau/service/service_provider.h>
#include <nau/shared/util.h>
#include <nau/usd_meta_tools/usd_meta_generator.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/relationship.h>

#include <format>
#include <sstream>

#include "EASTL/string.h"
#include "nau/asset_tools/asset_api.h"
#include "nau/asset_tools/asset_compiler.h"
#include "nau/asset_tools/asset_utils.h"
#include "nau/asset_tools/db_manager.h"
#include "nau/io/io_constants.h"
#include "nau/io/virtual_file_system.h"
#include "nau/shared/args.h"
#include "nau/shared/error_codes.h"
#include "nau/shared/file_system.h"
#include "nau/shared/logger.h"
#include "nau/usd_meta_tools/usd_meta_manager.h"
#include "nlohmann/json.hpp"
#include "usd_proxy/usd_proxy.h"

namespace nau
{
    namespace iterators
    {
        void iteratePrims(PXR_NS::UsdStageRefPtr stage, std::function<void(PXR_NS::UsdPrim, const std::string&)> func)
        {
            if (!stage)
            {
                return;
            }

            std::function<void(PXR_NS::UsdPrim)> traverse = [&](PXR_NS::UsdPrim prim)
            {
                if (!prim.IsValid())
                {
                    return;
                }

                auto& typeName = prim.GetTypeName();

                func(prim, typeName);

                for (auto it : prim.GetChildren())
                {
                    traverse(it);
                }
            };

            auto root = stage->GetPseudoRoot();

            for (auto it : root.GetChildren())
            {
                traverse(it);
            }
        }
        void iterateMeta(std::vector<nau::UsdMetaInfo>& metaArray, std::function<void(nau::UsdMetaInfo& meta)> func)
        {
            std::function<void(nau::UsdMetaInfo&)> traverse = [&](nau::UsdMetaInfo& meta)
            {
                if (!meta.isValid)
                {
                    return;
                }

                func(meta);

                for (auto& it : meta.children)
                {
                    traverse(it);
                }
            };

            for (auto& it : metaArray)
            {
                traverse(it);
            }
        }
    }  // namespace iterators

    static void updateMetaPath(nau::UsdMetaInfoArray& metaArray, const std::filesystem::path& assetSourcePath, const std::filesystem::path& assetPath)
    {
        iterators::iterateMeta(metaArray, [&](nau::UsdMetaInfo& metaInfo)
        {
            metaInfo.assetPath = metaInfo.assetSourcePath.empty() 
                ? assetPath.string() 
                : (assetSourcePath / metaInfo.assetSourcePath).string();

            if (std::filesystem::path(metaInfo.assetPath).extension().string().empty())
            {
                LOG_WARN("Asset {} has no extension, adding .nausd by default!", metaInfo.assetPath);
                metaInfo.assetPath += ".nausd";
            }

            std::replace(metaInfo.assetPath.begin(), metaInfo.assetPath.end(), '\\', '/');
        });
    };

    void checkExtension(nau::UsdMetaInfo& metaInfo)
    {
        if (metaInfo.assetPath.rfind('.') == std::string::npos)
        {
            LOG_WARN("Asset {} has no extension, adding .nausd by default!", metaInfo.assetPath);
            metaInfo.assetPath += ".nausd";
        }
    }

    bool isDirtyAsset(nau::UsdMetaInfo& metaInfo, AssetDatabaseManager& dbManager)
    {
        auto dbMeta = dbManager.get(metaInfo.uid);
        auto& sourcePath = metaInfo.assetPath;
        auto lastTimeWrite = std::filesystem::last_write_time(sourcePath).time_since_epoch().count();

        const bool dirty = dbMeta.isError() || lastTimeWrite > dbMeta->lastModified || !dbManager.exist(metaInfo.uid) || !dbManager.compiled(metaInfo.uid);

        return dirty;
    }

    int NauImportAssetsJob::run(const CommonArguments* const params)
    {
        const ImportAssetsArguments* args = static_cast<const ImportAssetsArguments*>(params);

        FileSystem fs;

        const std::filesystem::path assets = std::filesystem::path(args->projectPath) / getAssetsSubfolderDefaultName();
        const std::filesystem::path assetsDb = std::filesystem::path(args->projectPath) / getAssetsDBfolderName();
        const std::filesystem::path dbFilePath = assetsDb / getAssetsDbName();

        // Set assets path, different to executable path
        auto& paths = Paths::instance();
        paths.setPath("assets", assets.string());

        NAU_ASSERT(nau::util::validateEnvironment(), "Invalid environment!");

        LOG_INFO("Loading USD plugins...");

        nau::loadPlugins();

        if (!fs.exist(args->projectPath) || fs.isEmpty(args->projectPath))
        {
            return result(std::format("Project not found at path {}", args->projectPath), ErrorCode::invalidPathError);
        }

        if (!fs.exist(assets) || fs.isEmpty(assets))
        {
            return result(std::format("Assets not found at path {}", assets.string()), ErrorCode::invalidPathError);
        }

        const std::filesystem::path assetPath = std::filesystem::path(args->assetPath);

        AssetDatabaseManager& dbManager = AssetDatabaseManager::instance();

        NAU_VERIFY(dbManager.load(assetsDb.string()), "Failed to load assets database!");
        NAU_LOG("Assets database loaded, {} assets registered!", dbManager.size());

        if (!assetPath.empty())
        {
            if (!fs.exist(assetPath) || fs.isEmpty(assetPath))
            {
                return result(std::format("Asset not found at path {}", assetPath.string()), ErrorCode::invalidPathError);
            }

            std::vector<AssetMetaInfo> assetsList;

            FileInfo file = fs.getFileInfo(assetPath.string());

            if (file.extension == ".nausd")
            {
                const std::filesystem::path path = std::filesystem::path(assetPath.string());

                if (compileSingleAsset(fs.getFileInfo(path.string()), assetsDb.string(), args->projectPath, dbManager, fs, assetsList))
                {
                    dbManager.save();

                    LOG_INFO("File {} imported and compiled successfully", assetPath.string());
                }
            }
            else
            {
                if (importSingleAsset(file, assetsDb.string(), dbManager, fs))
                {
                    const std::filesystem::path metafilePath = std::filesystem::path(assetPath.string() + ".nausd");

                    if (compileSingleAsset(fs.getFileInfo(metafilePath.string()), assetsDb.string(), args->projectPath, dbManager, fs, assetsList))
                    {
                        dbManager.save();

                        LOG_INFO("File {} imported and compiled successfully", assetPath.string());
                    }
                }
            }
        }
        else
        {
            std::vector<AssetMetaInfo> assetsList;

            if (importAssets(args, fs, dbManager) != 0)
            {
                return ErrorCode::internalError;
            }

            LOG_INFO("Imported {} assets", dbManager.size());

            if (compileAssets(args, fs, dbManager, assetsList) != 0)
            {
                return ErrorCode::internalError;
            }

            // TODO: int assetsRemoved = dbManager.update(assetsList);
            // LOG_INFO("Project {} cache updated, {} assets removed", args->projectPath, assetsRemoved);
        }

        dbManager.save();

        return ErrorCode::success;
    }

    nau::Result<AssetMetaInfo> NauImportAssetsJob::compileAsset(PXR_NS::UsdStageRefPtr stage, const nau::UsdMetaInfo& metaInfo, const std::string& dbPath, const std::string& projectRootPath, int folderIndex)
    {
        std::string err;
        auto result = processMeta(stage, dbPath, projectRootPath, metaInfo, folderIndex);
        return result;
    }

    nau::Result<AssetMetaInfo> NauImportAssetsJob::updateAsset(PXR_NS::UsdStageRefPtr stage, nau::UsdMetaInfo& meta, const std::filesystem::path& dbPath, const std::string& projectRootPath, AssetDatabaseManager& db, FileSystem& fs)
    {
        if (!fs.exist(meta.assetPath))
        {
            return NauMakeError("Asset source file not found at path {}, skipping...", meta.assetPath);
        }

        if (!meta.isValid || meta.type == "group")
        {
            LOG_INFO("Asset prim {} is not valid, skipping...", meta.assetPath);
            return NauMakeError("Asset prim {} is not valid, skipping...", meta.assetPath);
        }

        nau::Result<int> assetDbIndex = db.getDbFolderIndex(meta.uid);

        if (assetDbIndex.isError())
        {
            assetDbIndex = utils::getAssetSubDir(dbPath, fs);
            
            LOG_INFO("Asset {} not found in database, its new asset, adding to folder {}", meta.assetPath, *assetDbIndex);
        }

        if (!isDirtyAsset(meta, db))
        {
            LOG_INFO("Asset {} is not dirty, skipping...", meta.assetPath);
            auto dbMeta = db.get(meta.uid);
            return *dbMeta;
        }

        LOG_INFO("Compiling asset {}", meta.assetPath);

        try
        {
            auto compilationResult = compileAsset(stage, meta, dbPath.string(), projectRootPath, *assetDbIndex);

            if (compilationResult.isError())
            {
                const auto& error = *compilationResult.getError();
                LOG_ERROR("Asset {} cannot be compiled, \nreason:{}!", meta.assetPath, error.getMessage().c_str());
            }
            else
            {
                AssetMetaInfo info = *compilationResult;

                auto lastModified = std::filesystem::last_write_time(meta.assetPath).time_since_epoch().count();

                // Import asset into asset database only if compilation was successful
                db.addOrReplace(info);

                LOG_INFO("Asset {}:{} compiled!", meta.assetPath, nau::toString(info.uid));

                return info;
            }
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("Asset {} cannot be compiled, see log for details!", meta.assetPath);
        }

        return NauMakeError("Asset cannot be compiled");
    };

    nau::Result<std::vector<AssetMetaInfo>> NauImportAssetsJob::updateAssetInDatabase(PXR_NS::UsdStageRefPtr stage, std::vector<nau::UsdMetaInfo>& metaArray, const std::filesystem::path& dbPath, const std::string& projectRootPath, AssetDatabaseManager& db, FileSystem& fs)
    {
        std::vector<AssetMetaInfo> result;

        iterators::iterateMeta(metaArray, [&](nau::UsdMetaInfo& meta)
        {
            auto compilationResult = updateAsset(stage, meta, dbPath, projectRootPath, db, fs);

            if (!compilationResult.isError())
            {
                result.push_back(*compilationResult);
            }
        });

        if (result.empty())
        {
            return NauMakeError("Asset cannot be compiled, see log for details!");
        }

        return result;
    }

    int NauImportAssetsJob::importAssets(const ImportAssetsArguments* args, FileSystem& fs, AssetDatabaseManager& db)
    {
        if (!fs.exist(args->projectPath) || fs.isEmpty(args->projectPath))
        {
            return result(std::format("Project not found at path {}", args->projectPath), ErrorCode::invalidPathError);
        }

        const std::filesystem::path assets = std::filesystem::path(args->projectPath) / getAssetsSubfolderDefaultName();
        const std::filesystem::path assetsDb = std::filesystem::path(args->projectPath) / getAssetsDBfolderName();

        if (!fs.exist(assets) || fs.isEmpty(assets))
        {
            return result(std::format("Assets not found at path {}", assets.string()), ErrorCode::invalidPathError);
        }

        static thread_local std::vector<FileInfo> files;

        FileSearchOptions options;
        options.excludedExtensions = {".meta", ".json", ".nausd"};

        args->filesExtensions.size() > 0 ? options.allowedExtensions = args->filesExtensions : options.allowedExtensions = {};

        LOG_FASSERT(!fs.findAllFiles(assets, files, options), std::format("Project {} cannot be scanned!", args->projectPath));
        LOG_INFO("Project {} scanned, {} assets found", args->projectPath, files.size());

        NAU_VERIFY(db.isLoaded(), "Asset database is not loaded!");

        LOG_INFO("Project {} cache loaded, {} compiled assets registered", args->projectPath, db.size());

        for (const auto& file : files)
        {
            importSingleAsset(file, assetsDb, db, fs);
        }

        return 0;
    }

    int NauImportAssetsJob::importSingleAsset(const FileInfo& file, const std::filesystem::path& dbPath, AssetDatabaseManager& db, FileSystem& fs)
    {
        const std::string ext = file.extension.substr(file.extension.find('.') + 1);

        const std::filesystem::path fileFullPath = std::filesystem::path(file.path + file.extension);
        const std::filesystem::path metafilePath = std::filesystem::path(fileFullPath.string() + ".nausd");

        UsdMetaGenerator& metaGenerator = UsdMetaGenerator::instance();
        UsdMetaManager& metaManager = UsdMetaManager::instance();

        if (!metaGenerator.canGenerate(fileFullPath))
        {
            LOG_ERROR("Asset {} has unsupported extension: {}, cannot generate meta, skipping...", file.path, ext);
            return 0;
        }

        if (!fs.exist(metafilePath))
        {
            LOG_WARN("Asset {} has no meta file, generating...", file.path);

            auto stage = metaGenerator.generate(fileFullPath.string());

            if (!stage)
            {
                LOG_ERROR("Asset {} cannot be generated, skipping...", file.path);
                return 0;
            }

            iterators::iteratePrims(stage, [&](PXR_NS::UsdPrim prim, const std::string& typeName)
            {
                std::string uidStr;

                auto uid = prim.GetAttribute("uid"_tftoken);

                if (uid)
                {
                    uid.Set(toString(nau::Uid::generate()));
                    uid.Get(&uidStr);

                    LOG_INFO("Asset {} uid generated: {}", typeName, uidStr);
                }
                else
                {
                    LOG_INFO("Asset {} uid not found", typeName);
                }
            });

            [[maybe_unused]] auto result = metaGenerator.write(fileFullPath.string(), stage);

            LOG_INFO("Asset {} meta file generated", file.path);
        }

        return 1;
    }

    int NauImportAssetsJob::compileAssets(const ImportAssetsArguments* args, FileSystem& fs, AssetDatabaseManager& db, std::vector<AssetMetaInfo>& assetsList)
    {
        if (!fs.exist(args->projectPath) || fs.isEmpty(args->projectPath))
        {
            return result(std::format("Project not found at path {}", args->projectPath), ErrorCode::invalidPathError);
        }

        const std::filesystem::path assets = std::filesystem::path(args->projectPath) / getAssetsSubfolderDefaultName();
        const std::filesystem::path assetsDb = std::filesystem::path(args->projectPath) / getAssetsDBfolderName();

        if (!fs.exist(assets) || fs.isEmpty(assets))
        {
            return result(std::format("Assets not found at path {}", assets.string()), ErrorCode::invalidPathError);
        }

        UsdMetaManager& metaManager = UsdMetaManager::instance();

        static thread_local std::vector<FileInfo> metaFiles;

        FileSearchOptions options;
        options.allowedExtensions = {".nausd"};

        LOG_FASSERT(!fs.findAllFiles(assets, metaFiles, options), std::format("Project {} cannot be scanned!", args->projectPath));
        LOG_INFO("Project {} scanned, {} assets found!", args->projectPath, metaFiles.size());

        // Adding priority to assets
        // auto getPriority = [](const FileInfo& f) -> int
        // {
        //     const std::string& sp = f.subpath;
        //     if (sp.find("textures") != std::string::npos) return 0;
        //     if (sp.find("materials") != std::string::npos) return 1;
        //     if (sp.find("scenes") != std::string::npos) return 3;
        //     return 2;
        // };
        //
        // auto compareByPriority = [&getPriority](const FileInfo& a, const FileInfo& b)
        // {
        //     return getPriority(a) < getPriority(b);
        // };
        //
        // std::sort(metaFiles.begin(), metaFiles.end(), compareByPriority);

        for (const auto& file : metaFiles)
        {
            auto metafilePath = file.path + file.extension;
            auto stage = PXR_NS::UsdStage::Open(metafilePath);

            LOG_INFO("Loading USD stage at path {}", metafilePath);

            if (stage)
            {
                stage->GetPseudoRoot().Load();
            }
            else
            {
                LOG_WARN("Failed to load stage {}!", metafilePath);
            }

            auto meta = metaManager.getInfo(metafilePath);

            updateMetaPath(meta, std::filesystem::path(file.path).parent_path(), metafilePath);

            auto compilationResultMeta = updateAssetInDatabase(stage, meta, assetsDb, args->projectPath, db, fs);

            if (!compilationResultMeta.isError())
            {
                for (auto& asset : *compilationResultMeta)
                {
                    assetsList.push_back(asset);
                }
            }
        }

        return 0;
    }

    int NauImportAssetsJob::compileSingleAsset(const FileInfo& file, const std::filesystem::path& dbPath, const std::string& projectRootPath, AssetDatabaseManager& db, FileSystem& fs, std::vector<AssetMetaInfo>& assetsList)
    {
        UsdMetaManager& metaManager = UsdMetaManager::instance();

        auto metafilePath = file.path + file.extension;
        auto stage = PXR_NS::UsdStage::Open(metafilePath);

        NAU_VERIFY(stage, "Failed to load stage");
        LOG_INFO("Loading USD stage at path {}", metafilePath);

        stage->GetPseudoRoot().Load();

        auto meta = metaManager.getInfo(metafilePath);

        updateMetaPath(meta, std::filesystem::path(file.path).parent_path(), metafilePath);

        auto compilationResultMeta = updateAssetInDatabase(stage, meta, dbPath, projectRootPath, db, fs);

        if (!compilationResultMeta.isError())
        {
            for (auto& asset : *compilationResultMeta)
            {
                assetsList.push_back(asset);
            }
        }

        return 1;
    }
}  // namespace nau
