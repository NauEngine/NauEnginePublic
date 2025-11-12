// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/shared/file_system.h"

#include <fstream>

#include "nau/shared/logger.h"
#include "nau/service/service_provider.h"
#include "nau/io/virtual_file_system.h"

namespace nau
{
    bool FileSystem::isEmpty(const std::filesystem::path& path)
    {
        return std::filesystem::is_empty(path);
    }

    bool FileSystem::exist(const std::filesystem::path& path)
    {
        return std::filesystem::exists(path);
    }

    bool FileSystem::existInFolder(const std::filesystem::path& path, const std::string& name, bool withExtension)
    {
        if (!std::filesystem::exists(path))
        {
            return false;
        }

        if (withExtension)
        {
            return std::filesystem::exists(path / std::filesystem::path(name));
        }

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (!std::filesystem::is_directory(entry.path()))
            {
                if (entry.path().filename().stem() == name)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool FileSystem::createDirectoryRecursive(const std::filesystem::path& path, std::error_code& error)
    {
        error.clear();

        if (!std::filesystem::create_directories(path, error))
        {
            if (std::filesystem::exists(path))
            {
                error.clear();
                return true;
            }
            return false;
        }
        return true;
    }

    bool FileSystem::copyFile(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        return std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
    }

    bool FileSystem::copyAll(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        if (!std::filesystem::exists(to))
        {
            std::filesystem::create_directory(to);
        }
        std::filesystem::copy(from, to, std::filesystem::copy_options::overwrite_existing | std::filesystem::copy_options::recursive);
        return true;
    }

    bool FileSystem::moveFile(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        std::filesystem::rename(from, to);
        return true;
    }

    bool FileSystem::moveFilesInFolder(const std::filesystem::path& from, const std::filesystem::path& to)
    {
        for (const auto& entry : std::filesystem::directory_iterator(from))
        {
            if (!std::filesystem::is_directory(entry.path()))
            {
                std::filesystem::rename(entry.path(), to / entry.path().filename());
            }
        }

        return true;
    }

    bool FileSystem::deleteFile(const std::filesystem::path& path)
    {
        return std::filesystem::remove(path);
    }

    bool FileSystem::deleteDirectory(const std::filesystem::path& path)
    {
        return std::filesystem::remove_all(path);
    }

    bool FileSystem::readFile(const std::filesystem::path& path, std::stringstream& content)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);

        if (!file)
            return false;

        std::ifstream::pos_type size = file.tellg();

        file.seekg(0, std::ios::beg);

        const std::streamsize chunkSize = 4096;

        char chunk[chunkSize];

        while (size > 0)
        {
            std::streamsize toRead = std::min(size, static_cast<std::ifstream::pos_type>(chunkSize));
            file.read(chunk, toRead);
            content.write(chunk, toRead);
            size -= toRead;
        }

        return true;
    }

    bool FileSystem::readBinaryFile(const std::filesystem::path& path, std::vector<std::byte>& content)
    {
        std::basic_ifstream<std::byte> stream{path, std::ios::binary};

        content = {std::istreambuf_iterator<std::byte>(stream), {}};

        return true;
    }

    bool FileSystem::writeFile(const std::filesystem::path& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        file << content;
        file.close();
        return true;
    }

    bool FileSystem::writeFile(const std::filesystem::path& path, const char* content, size_t size)
    {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(content, size);
        file.close();
        return true;
    }

    bool FileSystem::writeBinaryFile(const std::filesystem::path& path, const std::vector<uint8_t>& content)
    {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(content.data()), content.size());
        file.close();
        return true;
    }

    void FileSystem::removeAllFilesByName(const std::filesystem::path& path, const std::string& name)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (!std::filesystem::is_directory(entry.path()))
            {
                if (entry.path().filename().stem() == name)
                {
                    std::filesystem::remove(entry.path());

                    LOG_INFO("Removed file: {}", entry.path().string());
                }
            }
        }

        if (FileSystem::isEmpty(path))
        {
            std::filesystem::remove(path);
        }
    }

    void FileSystem::findAllFilesByName(const std::filesystem::path& path, const std::string& fileName, std::vector<FileInfo>& out, const std::string& excludedExtension)
    {
        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            std::string extension = entry.path().extension().string();

            if (!std::filesystem::is_directory(entry.path()))
            {
                if (excludedExtension != extension && entry.path().filename().stem() == fileName)
                {
                    std::string fileName = entry.path().string();

                    std::replace(fileName.begin(), fileName.end(), '\\', '/');

                    FileInfo info;

                    std::string nameWithoutExtension = fileName.substr(0, fileName.length() - extension.length());
                    std::replace(nameWithoutExtension.begin(), nameWithoutExtension.end(), '\\', '/');

                    info.path = fileName.substr(0, fileName.length() - entry.path().extension().string().length());
                    info.subpath = FileSystemExtensions::getSubPath(path.string(), entry.path().string(), true);
                    info.extension = entry.path().extension().string();
                    info.size = std::filesystem::file_size(entry.path());
                    info.name = FileSystemExtensions::nameFromPath(nameWithoutExtension);
                    info.lastModified = std::filesystem::last_write_time(entry.path()).time_since_epoch().count();

                    out.push_back(info);
                }
            }
        }
    }

    bool FileSystem::findAllFiles(const std::filesystem::path& path, std::vector<std::string>& out)
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            if (!std::filesystem::is_directory(entry.path()))
            {
                out.push_back(entry.path().string());
            }
        }

        return out.size() > 0;
    }

    bool FileSystem::findAllFiles(const std::filesystem::path& path, std::vector<FileInfo>& out, const FileSearchOptions& options)
    {
        const bool checkAllowedExtensions = options.allowedExtensions.size() > 0;
        const bool checkExcludedExtensions = options.excludedExtensions.size() > 0;

        for (const auto& entry : std::filesystem::recursive_directory_iterator(path))
        {
            std::string extension = entry.path().extension().string();

            if (!std::filesystem::is_directory(entry.path()))
            {
                if (!checkAllowedExtensions || std::find(options.allowedExtensions.begin(), options.allowedExtensions.end(), extension) != options.allowedExtensions.end())
                {
                    if (!checkExcludedExtensions || std::find(options.excludedExtensions.begin(), options.excludedExtensions.end(), extension) == options.excludedExtensions.end())
                    {
                        std::string fileName = entry.path().string();

                        std::replace(fileName.begin(), fileName.end(), '\\', '/');

                        FileInfo info;

                        std::string nameWithoutExtension = fileName.substr(0, fileName.length() - extension.length());
                        std::replace(nameWithoutExtension.begin(), nameWithoutExtension.end(), '\\', '/');

                        info.path = fileName.substr(0, fileName.length() - entry.path().extension().string().length());
                        info.subpath = FileSystemExtensions::getSubPath(path.string(), entry.path().string(), true);
                        info.extension = entry.path().extension().string();
                        info.size = std::filesystem::file_size(entry.path());
                        info.name = FileSystemExtensions::nameFromPath(nameWithoutExtension);
                        info.lastModified = std::filesystem::last_write_time(entry.path()).time_since_epoch().count();

                        out.push_back(info);
                    }
                }
            }
        }

        return out.size() > 0;
    }

    FileInfo FileSystem::getFileInfo(const std::filesystem::path& path)
    {
        std::string fileName = path.string();
        std::string extension = path.extension().string();

        std::replace(fileName.begin(), fileName.end(), '\\', '/');

        FileInfo info;

        std::string nameWithoutExtension = fileName.substr(0, fileName.length() - extension.length());
        std::replace(nameWithoutExtension.begin(), nameWithoutExtension.end(), '\\', '/');

        info.path = fileName.substr(0, fileName.length() - path.extension().string().length());
        info.subpath = FileSystemExtensions::getSubPath(path.string(), path.string(), true);
        info.extension = path.extension().string();
        info.size = std::filesystem::file_size(path);
        info.name = FileSystemExtensions::nameFromPath(nameWithoutExtension);
        info.lastModified = std::filesystem::last_write_time(path).time_since_epoch().count();

        return info;
    }

    std::string FileSystem::findFirst(const std::filesystem::path& directory, const std::string& extension)
    {
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            if (entry.path().extension() == extension)
            {
                return entry.path().string();
            }
        }
        return "";
    }

    void FileSystem::createFile(const std::filesystem::path& path)
    {
        std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
        file.close();
    }

    std::string FileSystem::findExtension(const std::filesystem::path& path, const std::string& fileName)
    {
        std::string ext;

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (entry.path().stem().filename() == fileName)
            {
                ext = entry.path().extension().string();
                break;
            }
        }

        return ext;
    }

    int FileSystem::countFiles(const std::filesystem::path& path)
    {
        int count = 0;

        if (!std::filesystem::exists(path))
        {
            return 0;
        }

        for (const auto& entry : std::filesystem::directory_iterator(path))
        {
            if (!std::filesystem::is_directory(entry.path()))
            {
                count++;
            }
        }

        return count;
    }

    std::string FileSystemExtensions::findDirectoryInUpperRecursive(const std::string& basePath, const std::string& directoryName)
    {
        std::string path = basePath;
        while (path != std::filesystem::path(path).parent_path().string())
        {
            if (std::filesystem::exists(path + "/" + directoryName))
            {
                return path;
            }
            path = std::filesystem::path(path).parent_path().string();
        }
        return "";
    }

    std::filesystem::path nau::FileSystemExtensions::getAssetsDirectory(const std::filesystem::path& currPath)
    {
        const bool pathContainsAssets = currPath.string().find(getAssetsSubfolderDefaultName()) != std::string::npos;
        LOG_FASSERT(!pathContainsAssets, "Current path does not contain assets directory");
        const std::filesystem::path assetsDirectory = currPath.string().substr(0, currPath.string().find(getAssetsSubfolderDefaultName()));
        return assetsDirectory;
    }

    bool nau::FileSystemExtensions::isGeometryAsset(const std::filesystem::path& path)
    {
        std::string extension = path.extension().string();
        static std::vector<std::string> geomExtensions = {".obj", ".fbx", ".gltf", ".glb", ".dae", ".usda", ".usdc", ".usd"};

        return std::find(geomExtensions.begin(), geomExtensions.end(), extension) != geomExtensions.end();
    }

    std::filesystem::path FileSystemExtensions::getRelativeAssetPath(const std::filesystem::path& currPath, bool stripExtension)
    {
        auto& paths = Paths::instance();
        std::string assetsPath = paths.getAssetsPath();
        LOG_FASSERT(assetsPath.empty(), "Assets path is empty!");

        std::string path = currPath.string();

        // replace \ with // to make uniform path
        std::replace(path.begin(), path.end(), '\\', '/');
        std::replace(assetsPath.begin(), assetsPath.end(), '\\', '/');

        if (stripExtension)
        {
            path = path.substr(0, path.length() - currPath.extension().string().length());
        }

        return std::filesystem::path(path.substr(assetsPath.length() + 1));
    }

    std::string FileSystemExtensions::resolveToNativePathContentFolder(const std::string& relativePath)
    {
        auto& vfs = nau::getServiceProvider().get<nau::io::IVirtualFileSystem>();
        auto fs_path = vfs.resolveToNativePath("/content/" + relativePath);
        return std::filesystem::path(fs_path).string();
    }

    std::string FileSystemExtensions::replaceExtension(const std::string& path, const std::string& newExtension)
    {
        return std::filesystem::path(path).replace_extension(newExtension).string();
    }

    std::string Paths::getExecutablePath() const
    {
        return std::filesystem::current_path().string();
    }

    std::string Paths::getAssetsPath() const
    {
        if (m_paths.find("assets") == m_paths.end())
        {
            return "";
        }

        return m_paths.find("assets")->second;
    }

    std::string Paths::getEnginePath() const
    {
        if (m_paths.find("engine") == m_paths.end())
        {
            return "";
        }

        return m_paths.find("engine")->second;
    }

    void Paths::setPath(const std::string& name, const std::string& path)
    {
        m_paths[name] = path;
    }

    Paths& Paths::instance()
    {
        static Paths instance;
        return instance;
    }

    SHARED_API std::string getShadersIncludeDir(const std::filesystem::path& shadersIn)
    {
        std::stringstream ss;

        auto sourceEngineEnvDir = std::getenv("NAU_ENGINE_SOURCE_DIR");
        auto sdkEngineEnvDir = std::getenv("NAU_ENGINE_SDK_DIR");

        ss << "\"" << (shadersIn / "include").string() << "\"";

        if (sourceEngineEnvDir)
        {
            const std::filesystem::path shadersIncludeDir = std::filesystem::path(sourceEngineEnvDir) / "engine/core/modules/render/include/nau/shaders";

            if (std::filesystem::is_directory(shadersIncludeDir))
            {
                ss << " \"" << shadersIncludeDir.string() << "\"";
            }
        }

        if (sdkEngineEnvDir)
        {
            const std::filesystem::path shadersIncludeDir = std::filesystem::path(sdkEngineEnvDir) / "include/core/modules/render/include/nau/shaders";

            if (std::filesystem::is_directory(shadersIncludeDir))
            {
                ss << " \"" << shadersIncludeDir.string() << "\"";
            }
        }

        LOG_INFO("Shaders include dir: {}", ss.str());

        return ss.str();
    }
}  // namespace nau