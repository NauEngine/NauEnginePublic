// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "./win_file.h"

#include "nau/platform/windows/diag/win_error.h"

namespace nau::io
{
    namespace
    {
        HANDLE createFile(const wchar_t* path, AccessModeFlag accessMode, OpenFileMode openMode, DWORD attributes = FILE_ATTRIBUTE_NORMAL)
        {
            DWORD accessFlags = 0;
            if (accessMode && AccessMode::Read)
            {
                accessFlags |= GENERIC_READ;
            }
            else if (accessMode && AccessMode::Write)
            {
                accessFlags |= GENERIC_WRITE;
            }

            const DWORD shareFlags = FILE_SHARE_READ;
            const DWORD createFlag = EXPR_Block
            {
                if (openMode == OpenFileMode::CreateAlways)
                {
                    return CREATE_ALWAYS;
                }
                else if (openMode == OpenFileMode::CreateNew)
                {
                    return CREATE_NEW;
                }
                else if (openMode == OpenFileMode::OpenAlways)
                {
                    return OPEN_ALWAYS;
                }
                else if (openMode == OpenFileMode::OpenExisting)
                {
                    return OPEN_EXISTING;
                }

                NAU_FAILURE("Unknown openMode");
                return 0;
            };

            const HANDLE fileHandle = ::CreateFileW(path, accessFlags, shareFlags, nullptr, createFlag, attributes, nullptr);
            // NAU_ASSERT(m_fileHandle != INVALID_HANDLE_VALUE, "Fail to open file: ({})", path);

            return fileHandle;
        }
    }  // namespace
    /*
    class WinFileMapping final : public virtual IMemoryMappedObject
    {
        NAU_CLASS_(nau::io::WinFileMapping, IMemoryMappedObject)

    public:
        WinFileMapping(nau::Ptr<WinFile> file) :
            m_file(std::move(file))
        {
            NAU_ASSERT(m_file);
            NAU_ASSERT(m_file->isOpened());
            if(!m_file || !m_file->isOpened())
            {
                return;
            }

            const DWORD pageProtectFlag = m_file->getAccessMode() && AccessMode::Write ? PAGE_READWRITE : PAGE_READONLY;
            m_fileMappingHandle = ::CreateFileMappingA(m_file->getFileHandle(), nullptr, pageProtectFlag, 0, 0, nullptr);

            NAU_ASSERT(m_fileMappingHandle != nullptr);
        }

        ~WinFileMapping()
        {
            if(m_fileMappingHandle != nullptr)
            {
                CloseHandle(m_fileMappingHandle);
            }
        }

        bool isValid() const
        {
            return m_fileMappingHandle != nullptr;
        }

        void* memMap(size_t offset, std::optional<size_t> count) override
        {
            NAU_ASSERT(m_fileMappingHandle != nullptr);
            NAU_ASSERT(m_file);
            NAU_ASSERT(m_file->getAccessMode().hasAny(AccessMode::Read, AccessMode::Write));
            NAU_ASSERT(offset < m_file->getSize());

            const DWORD access = (m_file->getAccessMode() && AccessMode::Write) ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

            const DWORD offsetHigh = static_cast<DWORD>((offset & 0xFFFFFFFF00000000LL) >> 32);
            const DWORD offsetLow = static_cast<DWORD>(offset & 0xFFFFFFFFLL);
            const SIZE_T mapSize = static_cast<SIZE_T>(count.value_or(0));

            void* const ptr = ::MapViewOfFile(m_fileMappingHandle, access, offsetHigh, offsetLow, mapSize);
            NAU_ASSERT(ptr, "MapViewOfFile returns nullptr");

            return ptr;
        }

        void memUnmap(const void* ptr) override
        {
            NAU_ASSERT(m_fileMappingHandle != nullptr);

            if(ptr != nullptr)
            {
                [[maybe_unused]]
                const BOOL unmapSuccess = ::UnmapViewOfFile(ptr);
                NAU_ASSERT(unmapSuccess);
            }
        }

    private:
        const nau::Ptr<WinFile> m_file;
        HANDLE m_fileMappingHandle = nullptr;
    };
*/
    WinFile::WinFile(const wchar_t* path, AccessModeFlag accessMode, OpenFileMode openMode, DWORD attributes) :
        m_accessMode(accessMode),
        m_fileHandle(createFile(path, accessMode, openMode, attributes))
    {
        // NAU_ASSERT(m_fileHandle != INVALID_HANDLE_VALUE, "Fail to open file: ({})", path);
    }

    WinFile::~WinFile()
    {
        if (m_fileHandle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_fileHandle);
        }
    }

    bool WinFile::supports(FileFeature feature) const
    {
        if (feature == FileFeature::AsyncStreaming)
        {
            return false;
        }

        if (feature == FileFeature::MemoryMapping)
        {
            return true;
        }

        return false;
    }

    bool WinFile::isOpened() const
    {
        return m_fileHandle != INVALID_HANDLE_VALUE;
    }

    /*
        IMemoryMappedObject::Ptr WinFile::getMemoryMappedObject()
        {
            if(auto mappedObject = rtti::createInstance<WinFileMapping>(rtti::Acquire{this}); mappedObject->isValid())
            {
                return mappedObject;
            }

            return nullptr;
        }*/

    IStreamBase::Ptr WinFile::createStream([[maybe_unused]] std::optional<AccessModeFlag> accessMode)
    {
        NAU_ASSERT(isOpened());
        if (!isOpened())
        {
            return nullptr;
        }

        const std::string path = getNativePath();
        if (path.empty())
        {
            return nullptr;
        }

        return createNativeFileStream(path.c_str(), m_accessMode, OpenFileMode::OpenExisting);
    }

    void* WinFile::memMap([[maybe_unused]] size_t offset, [[maybe_unused]] size_t count)
    {
        NAU_ASSERT(isOpened());
        NAU_ASSERT(getAccessMode().hasAny(AccessMode::Read, AccessMode::Write));
        NAU_ASSERT(offset < getSize());

        lock_(m_mutex);
        if (++m_fileMappingCounter == 1)
        {
            const DWORD access = (getAccessMode() && AccessMode::Write) ? (FILE_MAP_READ | FILE_MAP_WRITE) : FILE_MAP_READ;

            const DWORD offsetHigh = static_cast<DWORD>((offset & 0xFFFFFFFF00000000LL) >> 32);
            const DWORD offsetLow = static_cast<DWORD>(offset & 0xFFFFFFFFLL);
            const SIZE_T mapSize = static_cast<SIZE_T>(count);

            m_mappedPtr = ::MapViewOfFile(m_fileMappingHandle, access, offsetHigh, offsetLow, mapSize);
            NAU_ASSERT(m_mappedPtr, "MapViewOfFile returns nullptr");
        }

        return m_mappedPtr;
    }

    void WinFile::memUnmap(const void* ptr)
    {
        NAU_ASSERT(ptr == nullptr || ptr == m_mappedPtr);

        const void* const ptrToUnmap = EXPR_Block->const void*
        {
            lock_(m_mutex);
            NAU_ASSERT(m_fileMappingCounter > 0);
            if (m_fileMappingCounter == 0 || --m_fileMappingCounter > 0)
            {
                return nullptr;
            }

            return m_mappedPtr;
        };

        if (ptrToUnmap)
        {
            const BOOL unmapSuccess = ::UnmapViewOfFile(ptrToUnmap);
            NAU_ASSERT(unmapSuccess);
        }
    }

    size_t WinFile::getSize() const
    {
        NAU_ASSERT(isOpened());

        if (!isOpened())
        {
            return 0;
        }

        LARGE_INTEGER size{};
        if (::GetFileSizeEx(m_fileHandle, &size) != TRUE)
        {
            return 0;
        }

        return static_cast<size_t>(size.QuadPart);
    }

    FsPath WinFile::getPath() const
    {
        return m_vfsPath;
    }

    void WinFile::setVfsPath(io::FsPath path)
    {
        m_vfsPath = std::move(path);
    }

    std::string WinFile::getNativePath() const
    {
        NAU_ASSERT(isOpened());
        if (!isOpened())
        {
            return {};
        }

        const DWORD required = ::GetFinalPathNameByHandleW(m_fileHandle, nullptr, 0, FILE_NAME_NORMALIZED);
        if (required == 0)
        {
            return {};
        }

        eastl::wstring path(required, L'\0');
        const DWORD pathLen = ::GetFinalPathNameByHandleW(m_fileHandle, path.data(), required, FILE_NAME_NORMALIZED);
        if (pathLen == 0 || pathLen >= required)
        {
            return {};
        }

        const auto utf8 = strings::wstringToUtf8(eastl::wstring_view{path.data(), pathLen});
        return std::string{reinterpret_cast<const char*>(utf8.data()), utf8.size()};
    }

    AccessModeFlag WinFile::getAccessMode() const
    {
        return m_accessMode;
    }

    WinFileStreamBase::WinFileStreamBase(HANDLE fileHandle) :
        m_fileHandle(fileHandle)
    {
        NAU_ASSERT(m_fileHandle != INVALID_HANDLE_VALUE);
    }

    WinFileStreamBase::~WinFileStreamBase()
    {
        if (m_fileHandle != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(m_fileHandle);
        }
    }

    size_t WinFileStreamBase::getPositionInternal() const
    {
        NAU_ASSERT(m_fileHandle != INVALID_HANDLE_VALUE);
        if (m_fileHandle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        LARGE_INTEGER offset{.QuadPart = 0};
        LARGE_INTEGER currentOffset{.QuadPart = 0};

        [[maybe_unused]]
        const auto success = ::SetFilePointerEx(getFileHandle(), offset, &currentOffset, FILE_CURRENT);
        NAU_ASSERT(success);

        return static_cast<size_t>(currentOffset.QuadPart);
    }

    size_t WinFileStreamBase::setPositionInternal(OffsetOrigin origin, int64_t value)
    {
        NAU_ASSERT(m_fileHandle != INVALID_HANDLE_VALUE);
        if (m_fileHandle == INVALID_HANDLE_VALUE)
        {
            return 0;
        }

        const LARGE_INTEGER offset{.QuadPart = static_cast<LONGLONG>(value)};
        LARGE_INTEGER newOffset{.QuadPart = 0};

        const DWORD offsetMethod = EXPR_Block->DWORD
        {
            if (origin == OffsetOrigin::Begin)
            {
                return FILE_BEGIN;
            }
            else if (origin == OffsetOrigin::End)
            {
                return FILE_END;
            }
            NAU_ASSERT(origin == OffsetOrigin::Current);

            return FILE_CURRENT;
        };

        [[maybe_unused]]
        const auto success = ::SetFilePointerEx(getFileHandle(), offset, &newOffset, offsetMethod);
        NAU_ASSERT(success);

        return static_cast<size_t>(newOffset.QuadPart);
    }

    WinFileStreamReader::WinFileStreamReader(HANDLE fileHandle) :
        WinFileStreamBase(fileHandle)
    {
    }

    WinFileStreamReader::WinFileStreamReader(const wchar_t* path, AccessModeFlag accessMode, OpenFileMode openMode) :
        WinFileStreamBase(createFile(path, accessMode, openMode))
    {
    }

    size_t WinFileStreamReader::getPosition() const
    {
        return getPositionInternal();
    }

    size_t WinFileStreamReader::setPosition(OffsetOrigin origin, int64_t value)
    {
        return setPositionInternal(origin, value);
    }

    Result<size_t> WinFileStreamReader::read(std::byte* ptr, size_t count)
    {
        NAU_ASSERT(isOpened());
        if (!isOpened())
        {
            return NauMakeError("File is not opened");
        }

        DWORD actualReadCount = 0;

        const bool readOk = ::ReadFile(getFileHandle(), reinterpret_cast<void*>(ptr), static_cast<DWORD>(count), &actualReadCount, nullptr);
        if (!readOk)
        {
            return NauMakeErrorT(diag::WinCodeError)("Fail to read file");
        }

        return static_cast<size_t>(actualReadCount);
    }

    WinFileStreamWriter::WinFileStreamWriter(HANDLE fileHandle) :
        WinFileStreamBase(fileHandle)
    {
    }

    WinFileStreamWriter::WinFileStreamWriter(const wchar_t* path, AccessModeFlag accessMode, OpenFileMode openMode) :
        WinFileStreamBase(createFile(path, accessMode, openMode))
    {
    }

    size_t WinFileStreamWriter::getPosition() const
    {
        return getPositionInternal();
    }

    size_t WinFileStreamWriter::setPosition(OffsetOrigin origin, int64_t value)
    {
        return setPositionInternal(origin, value);
    }

    Result<size_t> WinFileStreamWriter::write(const std::byte* ptr, size_t count)
    {
        NAU_ASSERT(isOpened());
        if (!isOpened())
        {
            return NauMakeError("File is not opened");
        }

        DWORD actualWriteCount = 0;

        const bool writeOk = ::WriteFile(getFileHandle(), reinterpret_cast<const void*>(ptr), static_cast<DWORD>(count), &actualWriteCount, nullptr);
        if (!writeOk)
        {
            return NauMakeErrorT(diag::WinCodeError)("Fail to write file");
        }

        return static_cast<size_t>(actualWriteCount);
    }

    void WinFileStreamWriter::flush()
    {
        FlushFileBuffers(getFileHandle());
    }

    IStreamBase::Ptr createNativeFileStream(const char* path, AccessModeFlag accessMode, OpenFileMode openMode)
    {
        accessMode -= AccessMode::Async;

        // NAU_ASSERT(!(accessMode && AccessMode::Async), "Async IO not supported yet");

        const eastl::wstring wcsPath = strings::utf8ToWString(path);

        if (accessMode == AccessMode::Read)
        {
            if (auto stream = rtti::createInstance<WinFileStreamReader>(wcsPath.c_str(), accessMode, openMode); stream->isOpened())
            {
                return stream;
            }
        }
        else if (accessMode == AccessMode::Write)
        {
            if (auto stream = rtti::createInstance<WinFileStreamWriter>(wcsPath.c_str(), accessMode, openMode); stream->isOpened())
            {
                return stream;
            }
        }

        // const auto attributes = GetFileAttributesA(fullPath.c_str());
        // if (attributes == INVALID_FILE_ATTRIBUTES)
        // {
        //     return nullptr;
        // }

        // auto file = rtti::createInstance<WinFile>(fullPath.c_str(), accessMode, openMode);
        // return file->isOpened() ? file : nullptr;

        return nullptr;
    }

}  // namespace nau::io
