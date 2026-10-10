// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/asset_utils.h"

#include "nau/asset_tools/asset_compiler.h"
#include "nau/asset_tools/asset_info.h"
#include "nau/shared/file_system.h"

namespace nau
{
    namespace utils
    {
        constexpr int DEFAULT_INDEX = 0;

        int findIndex(const std::filesystem::path& path, FileSystem& fs, const std::vector<int>& indexes)
        {
            if (indexes.empty())
            {
                return DEFAULT_INDEX;
            }

            int expectedIndex = 0;

            for (int index : indexes)
            {
                if (index != expectedIndex)
                {
                    return expectedIndex;
                }

                if (MAX_FILES_COUNT > fs.countFiles(path / std::to_string(index)))
                {
                    return expectedIndex;
                }

                expectedIndex++;
            }

            expectedIndex = indexes.back();

            if (MAX_FILES_COUNT > fs.countFiles(path / std::to_string(expectedIndex)))
            {
                return expectedIndex;
            }

            return ++expectedIndex;
        }

        int getAssetSubDir(const std::filesystem::path& path, FileSystem& fs)
        {
            auto iterator = std::filesystem::directory_iterator(path);
            std::vector<int> entriesIndexes;

            [[maybe_unused]] const int count = std::count_if(
                begin(iterator),
                end(iterator),
                [&entriesIndexes](const std::filesystem::directory_entry& entry)
            {
                const bool dir = entry.is_directory() && !entry.is_symlink();

                if (dir)
                {
                    const std::string name = entry.path().filename().string();
                    entriesIndexes.push_back(std::atoi(name.c_str()));
                }

                return dir;
            });

            std::sort(begin(entriesIndexes), end(entriesIndexes));

            int dir = findIndex(path, fs, entriesIndexes);

            return dir;
        }

        void* getUsdPlugin(const std::string& pluginName)
        {
            static std::map<std::string, void*> plugins;

            if (plugins.find(pluginName) != plugins.end())
            {
                return plugins[pluginName];
            }
#ifdef WIN32
            HMODULE module = LoadLibraryW(std::filesystem::u8path(pluginName).c_str());
            if (!module)
            {
                NAU_LOG_ERROR("Failed to load module {}, error {}", pluginName, GetLastError());
            }
            plugins[pluginName] = module;
#else
            plugins[pluginName] = dlopen(pluginName.c_str(), RTLD_NOW);
#endif
            return plugins[pluginName];
        }

        namespace compilers
        {
            std::filesystem::path ensureOutputPath(const std::string& outputPath, const AssetMetaInfo& metaInfo, std::string ext)
            {
                std::filesystem::path dbPath = { metaInfo.dbPath.c_str() };

                const std::filesystem::path out = std::filesystem::u8path(outputPath) / dbPath;

                if (!std::filesystem::exists(out.parent_path()))
                {
                    std::filesystem::create_directories(out.parent_path());
                }

                if (ext.empty() && dbPath.has_extension())
                {
                    return out;
                }

                if (ext.empty())
                {
                    ext = metaInfo.sourceType.c_str();
                }

                return out.parent_path() / (out.stem().string() + "." + ext);
            }
            int copyAsset(const std::string& path, const std::string& outputPath, const nau::UsdMetaInfo& metaInfo, int folderIndex, std::string ext)
            {
                FileSystem fs;

                const std::filesystem::path out = std::filesystem::u8path(outputPath) / std::to_string(folderIndex) / (toString(metaInfo.uid) + ext);

                if (!std::filesystem::exists(out.parent_path()))
                {
                    std::filesystem::create_directories(out.parent_path());
                }

                const std::filesystem::path resultPath = out.parent_path() / (out.stem().string() + ext);

                return fs.copyFile(std::filesystem::u8path(path), resultPath) ? static_cast<int>(CompilerResult::Success) : static_cast<int>(CompilerResult::InternalError);
            }

            int copyFileToExportDirectory(const std::string& from, const std::string& projectRoot)
            {
                FileSystem fs;

                const std::filesystem::path resourcesContentPath = std::filesystem::u8path(projectRoot) / "resources";
                std::filesystem::path exprotPath = resourcesContentPath / std::filesystem::relative(std::filesystem::u8path(from), std::filesystem::u8path(projectRoot) / getAssetsSubfolderDefaultName());

                if (!std::filesystem::exists(std::filesystem::path(exprotPath).parent_path()))
                {
                    std::filesystem::create_directories(std::filesystem::path(exprotPath).parent_path());
                }

                return fs.copyFile(std::filesystem::u8path(from), exprotPath) ? static_cast<int>(CompilerResult::Success) : static_cast<int>(CompilerResult::InternalError);
            }
        }  // namespace compilers
    }  // namespace utils
}  // namespace nau
