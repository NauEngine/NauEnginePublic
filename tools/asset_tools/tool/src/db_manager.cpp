// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/asset_tools/db_manager.h"

#include <regex>
#include <sstream>

#include "nau/asset_tools/asset_utils.h"
#include "nau/serialization/json_utils.h"
#include "nau/serialization/serialization.h"
#include "nau/shared/error_codes.h"
#include "nau/shared/logger.h"

namespace nau
{
    AssetDatabaseManager& AssetDatabaseManager::instance()
    {
        static AssetDatabaseManager inst;
        return inst;
    }

    bool AssetDatabaseManager::isLoaded() const
    {
        return m_isLoaded;
    }
    bool AssetDatabaseManager::load(const std::string_view& cachePath)
    {
        m_cachePath = cachePath.data();
        m_dbFile = std::filesystem::u8path(m_cachePath) / getAssetsDbName();

        if (!m_fs.exist(std::filesystem::u8path(m_cachePath)))
        {
            std::error_code ec;

            if (!m_fs.createDirectoryRecursive(std::filesystem::u8path(m_cachePath), ec))
            {
                exit(ErrorCode::invalidPathError);
            }
        }

        if (!m_fs.exist(m_dbFile))
		{
            m_fs.createFile(m_dbFile);
		}

        std::stringstream ss;

        m_isLoaded = m_fs.readFile(m_dbFile, ss);

        if (m_isLoaded)
        {
            if (!ss.rdbuf()->in_avail())
            {
                LOG_WARN("Database does not exist, it will be created!");
                m_cache = AssetCache();
            }
            else
            {
                m_cache = *serialization::JsonUtils::parse<AssetCache>(ss.str());
            }

            LOG_INFO("Database loaded, {} entries", m_cache.content.size());
        }

        return m_isLoaded;
    }

    bool AssetDatabaseManager::save()
    {
        eastl::u8string serializedResult = serialization::JsonUtils::stringify(m_cache);
        return m_fs.writeFile(m_dbFile, reinterpret_cast<const char*>(serializedResult.data()), serializedResult.length());
    }

    bool AssetDatabaseManager::addOrReplace(const AssetMetaInfo& metaInfo)
    {
        std::vector<AssetMetaInfo>& db = assets();

        auto it = std::find_if(db.begin(), db.end(), [&](const AssetMetaInfo& info)
        {
            return info.uid == metaInfo.uid;
        });

        if (it != db.end())
        {
            *it = metaInfo;
            return true;
        }
        else
        {
            db.push_back(metaInfo);
            return true;
        }
    }

    int AssetDatabaseManager::update(std::vector<AssetMetaInfo>& list)
    {
        int count = 0;

        FileSystem fs;

        std::filesystem::path cache = std::filesystem::u8path(m_cachePath);

        std::vector<AssetMetaInfo>& db = assets();

        std::erase_if(db, [&](const AssetMetaInfo& info)
        {
            auto it = std::find_if(list.begin(), list.end(), [&](const AssetMetaInfo& asset)
            {
                return asset.uid == info.uid;
            });
            if (it == list.end())
            {
                // TODO: Are to be replaced via some USD reference mechanism!
                // TODO: Check if parent asset contains inside this asset via USD!
                if (const std::string sourcePath = info.sourcePath.c_str(); std::regex_match(sourcePath, std::regex("(.*?)\\+\\[(.*?)\\]")))
                {
                    std::smatch matches;

                    if (std::regex_search(sourcePath, matches, std::regex("(.*?)\\+.*")))
                    {
                        const std::string parentAssetPath = matches[1].str();
                        const bool parentAssetExists = std::ranges::find_if(list, [&](const AssetMetaInfo& asset)
                        {
                            return asset.sourcePath == parentAssetPath.c_str();
                        }) != list.end();

                        if (parentAssetExists)
                        {
                            return false;
                        }
                    }
                }

                const std::string compiledName = toString(info.uid);
                const std::filesystem::path parentPath = std::filesystem::path(cache / std::string(info.dbPath.c_str())).parent_path();

                fs.removeAllFilesByName(parentPath.parent_path(), compiledName);

                LOG_INFO("Removed asset {}.{} id [{}]", info.sourcePath.c_str(), info.sourceType.c_str(), compiledName);

                count++;

                return true;
            }
            return false;
        });

        if (count > 0)
        {
            save();
        }

        return count;
    }

    bool AssetDatabaseManager::exist(const Uid& uid)
    {
        std::vector<AssetMetaInfo>& db = assets();
        return std::find_if(db.begin(), db.end(), [&](const AssetMetaInfo& info)
        {
            return info.uid == uid;
        }) != db.end();
    }

    bool AssetDatabaseManager::compiled(const Uid& uid)
    {
        FileSystem fs;
        nau::Result<AssetMetaInfo> metaInfo = get(uid);
        if (metaInfo.isError())
        {
			return false;
        }
        std::filesystem::path path = std::filesystem::u8path(m_cachePath) / std::string(metaInfo->dbPath.c_str());
        return fs.exist(path);
    }

    bool AssetDatabaseManager::compiled(const std::string_view& sourcePath)
    {
        std::vector<AssetMetaInfo>& db = assets();

        const eastl::string path = sourcePath.data();

        const auto it = std::find_if(db.begin(), db.end(), [&](const AssetMetaInfo& info)
        {
            return info.sourcePath == path;
        });

        if (it != db.end())
        {
            return compiled(it->uid);
        }

        return false;
    }

    nau::Result<Uid> AssetDatabaseManager::findIf(const std::string_view& sourcePath)
    {
        std::vector<AssetMetaInfo>& db = assets();

        const eastl::string path = sourcePath.data();

        const auto it = std::find_if(db.begin(), db.end(), [&](const AssetMetaInfo& info)
        {
            return info.sourcePath == path;
        });

        if (it != db.end())
        {
            return it->uid;
        }

        return NauMakeError("Could not find asset!");
    }

    nau::Result<int> AssetDatabaseManager::getDbFolderIndex(const Uid& uid)
    {
        auto info = get(uid);

        if (info.isError())
        {
			return NauMakeError("Asset not found!");
        }

        auto path = std::filesystem::path(info->dbPath.c_str()).parent_path().string();
        return std::atoi(path.substr(path.find_last_of('/') + 1, path.length()).c_str());
    }

    nau::Result<AssetMetaInfo> AssetDatabaseManager::get(const Uid& uid)
    {
        std::vector<AssetMetaInfo>& db = assets();

        const auto it = std::find_if(db.begin(), db.end(), [&](const AssetMetaInfo& info)
        {
            return info.uid == uid;
        });

        if (it != db.end())
        {
            return *it;
        }

        return NauMakeError("Asset not found!");
    }
}  // namespace nau