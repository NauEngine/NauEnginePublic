// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "./win_native_file_system.h"

#include "./win_file.h"

namespace nau::io
{
    namespace
    {
        struct DirIteratorData
        {
            HANDLE hFind;
            FsPath basePath;
        };

        FsEntry win32FindDataToFsEntry(const FsPath& basePath, const WIN32_FIND_DATAW& findData)
        {
            const std::wstring_view fileName{findData.cFileName, ::wcslen(findData.cFileName)};

            const auto kind = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? FsEntryKind::Directory : FsEntryKind::File;
            const uint64_t size = (kind == FsEntryKind::File) ? static_cast<uint64_t>(findData.nFileSizeHigh) << 32 | static_cast<uint64_t>(findData.nFileSizeLow) : 0;

            return FsEntry{
                .path = basePath / fileName,
                .kind = kind,
                .size = static_cast<size_t>(size),
                .lastWriteTime = 0};
        }

        bool skipNotNeededEntries(HANDLE hFind, WIN32_FIND_DATAW& findData)
        {
            const std::array namesToSkip = {
                std::wstring_view{L"."},
                std::wstring_view{L".."}
            };


            do
            {
                const std::wstring_view fileName {findData.cFileName, ::wcslen(findData.cFileName)};
                if (std::find(namesToSkip.begin(), namesToSkip.end(), fileName) == namesToSkip.end())
                {
                    break;
                }

                if (::FindNextFileW(hFind, &findData) == FALSE)
                {
                    return false;
                }
            }
            while (true);


            return true;
        }

    }  // namespace

    WinNativeFileSystem::WinNativeFileSystem(std::string basePath, bool isReadOnly) :
        m_basePath(std::move(basePath)),
        m_isReadOnly(isReadOnly)
    {
    }

    bool WinNativeFileSystem::isReadOnly() const
    {
        return m_isReadOnly;
    }

    bool WinNativeFileSystem::exists(const FsPath& path, std::optional<FsEntryKind> kind)
    {
        auto fullPath = resolveToNativePathNoCheck(path);
        if(fullPath.empty())
        {
            return false;
        }

        const DWORD attributes = GetFileAttributesW(fullPath.c_str());
        if(attributes == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        if(!kind)
        {
            return true;
        }

        const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        return isDirectory == (*kind == FsEntryKind::Directory);
    }

    size_t WinNativeFileSystem::getLastWriteTime(const FsPath&)
    {
        return 0;
    }

    IFile::Ptr WinNativeFileSystem::openFile(const FsPath& vfsPath, AccessModeFlag accessMode, OpenFileMode openMode)
    {
        auto fullPath = resolveToNativePathNoCheck(vfsPath);
        if(fullPath.empty())
        {
            return nullptr;
        }

        if(!accessMode.has(AccessMode::Write) || openMode == OpenFileMode::OpenExisting)
        {
            const DWORD attributes = GetFileAttributesW(fullPath.c_str());
            if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                return nullptr;
            }
        }

        return rtti::createInstance<WinFile>(fullPath.c_str(), accessMode, openMode);
    }

    IFileSystem::OpenDirResult WinNativeFileSystem::openDirIterator(const FsPath& path)
    {
        auto searchPath = resolveToNativePathNoCheck(path);
        if(searchPath.empty())
        {
            return {};
        }

        const DWORD attributes = ::GetFileAttributesW(searchPath.c_str());
        if((attributes == INVALID_FILE_ATTRIBUTES) || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return {};
        }

        WIN32_FIND_DATAW findData;
        searchPath.append("\\*");

        const HANDLE hFind = ::FindFirstFileW(searchPath.c_str(), &findData);
        if(hFind == INVALID_HANDLE_VALUE)
        {
            return {};
        }

        if (!skipNotNeededEntries(hFind, findData))
        {
            ::FindClose(hFind);
            return {};
        }

        return {new DirIteratorData {hFind, path}, win32FindDataToFsEntry(path, findData)};
    }

    void WinNativeFileSystem::closeDirIterator(void* ptr)
    {
        if(!ptr)
        {
            return;
        }
        auto* const data = reinterpret_cast<DirIteratorData*>(ptr);
        if (data->hFind && data->hFind != INVALID_HANDLE_VALUE)
        {
            ::FindClose(data->hFind);
        }

        delete data;
    }

    FsEntry WinNativeFileSystem::incrementDirIterator(void* ptr)
    {
        if(!ptr)
        {
            return {};
        }

        auto* const data = reinterpret_cast<DirIteratorData*>(ptr);
        WIN32_FIND_DATAW findData;
        if (::FindNextFileW(data->hFind, &findData) == FALSE)
        {
            return {};
        }

        return skipNotNeededEntries(data->hFind, findData) ? win32FindDataToFsEntry(data->basePath, findData) : FsEntry{};
    }

    Result<> WinNativeFileSystem::createDirectory(const FsPath&)
    {
        return {};
    }

    Result<> WinNativeFileSystem::remove(const FsPath&, bool recursive)
    {
        return {};
    }

    std::wstring WinNativeFileSystem::resolveToNativePath(const FsPath& path)
    {
        namespace fs = std::filesystem;

        fs::path fullPath = fs::u8path(m_basePath);

        if(!path.isEmpty())
        {
            fullPath /= fs::u8path(path.getString());
        }

        fullPath.make_preferred();
        if(!fs::exists(fullPath))
        {
            return {};
        }

        return fullPath.wstring();
    }

    std::filesystem::path WinNativeFileSystem::resolveToNativePathNoCheck(const FsPath& path)
    {
        namespace fs = std::filesystem;

        fs::path fullPath = fs::u8path(m_basePath);

        if(!path.isEmpty())
        {
            fullPath /= fs::u8path(path.getString());
        }

        return fullPath.make_preferred();
    }

    IFileSystem::Ptr createNativeFileSystem(std::string basePath, bool readOnly)
    {
        using namespace nau::strings;

        NAU_ASSERT(!basePath.empty());
        if(basePath.empty())
        {
            return nullptr;
        }

        auto wcsBasePath = utf8ToWString(toStringView(basePath));


        const DWORD attributes = GetFileAttributesW(wcsBasePath.c_str());
        NAU_ASSERT(attributes != INVALID_FILE_ATTRIBUTES, "Path ({}) does not exists", basePath);
        NAU_ASSERT((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0, "Path ({}) expected to be directory", basePath);

        if(attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
        {
            return nullptr;
        }

        return rtti::createInstance<WinNativeFileSystem>(std::move(basePath), readOnly);
    }
}  // namespace nau::io
