// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
#include "nau/asset_pack/asset_pack_builder.h"

#include "nau/io/asset_pack.h"
#include <array>
#include "nau/io/file_system.h"
#include "nau/io/nau_container.h"
#include "nau/io/special_paths.h"
#include "nau/io/stream.h"
#include "nau/memory/bytes_buffer.h"
#include "nau/serialization/runtime_value_builder.h"
#include "nau/string/string_conv.h"

namespace nau
{
    Result<io::AssetPackIndexData> writeAssetPackIndexDataToStream(const eastl::vector<PackInputFileData>& content, PackBuildOptions buildOptions, const std::string& tempFilePath, const PackBuildProgressCallback& progress)
    {
        using namespace io;
        AssetPackIndexData packData;
        packData.version = buildOptions.version;
        packData.description = buildOptions.description;

        IStreamWriter::Ptr tempStream = createNativeFileStream(tempFilePath.data(), AccessMode::Write, OpenFileMode::CreateAlways);

        uint64_t completedFiles = 0;
        const uint64_t totalFiles = content.size();
        if (progress) progress({PackBuildStage::ReadingFiles, 0, totalFiles, {}});
        for (const PackInputFileData& content : content)
        {
            IStreamReader::Ptr srcStream = content.stream();
            NAU_ASSERT(srcStream, "Invalid stream:({})", content.filePathInPack);
            if (!srcStream)
            {
                return NauMakeError("Invalid input stream for asset package");
            }

            AssetPackFileEntry& packEntry = packData.content.emplace_back();
            packEntry.blobData.offset = tempStream->getPosition();
            const auto copied = copyStream(*tempStream, *srcStream);
            NauCheckResult(copied);

            packEntry.filePath = content.filePathInPack;
            packEntry.contentCompression = packEntry.clientSize;
            packEntry.clientSize = srcStream->getPosition();
            packEntry.blobData.size = srcStream->getPosition();
            if (progress) progress({PackBuildStage::ReadingFiles, ++completedFiles, totalFiles,
                std::string(content.filePathInPack.c_str())});
        }

        tempStream->flush();
        return packData;
    }

    Result<> buildAssetPackage(const eastl::vector<PackInputFileData>& content, PackBuildOptions buildOptions, io::IStreamWriter::Ptr outputStream)
    {
        return buildAssetPackageWithProgress(content, std::move(buildOptions), std::move(outputStream), {});
    }

    Result<> buildAssetPackageWithProgress(const eastl::vector<PackInputFileData>& content,
        PackBuildOptions buildOptions, io::IStreamWriter::Ptr outputStream,
        const PackBuildProgressCallback& progress)
    {
        using namespace io;
        const eastl::u8string u8tempFilePath = getNativeTempFilePath();
        const std::string tempFilePath(u8tempFilePath.cbegin(), u8tempFilePath.cend());

        auto indexResult = writeAssetPackIndexDataToStream(content, buildOptions, tempFilePath, progress);
        NauCheckResult(indexResult);
        AssetPackIndexData packIndexData = *indexResult;
        writeContainerHeader(outputStream, "nau-vfs-pack", nau::makeValueRef(packIndexData));

        IStreamReader::Ptr temp = createNativeFileStream(tempFilePath.data(), AccessMode::Read, OpenFileMode::OpenExisting);
        const uint64_t total = temp->setPosition(OffsetOrigin::End, 0);
        temp->setPosition(OffsetOrigin::Begin, 0);
        uint64_t completed = 0;
        if (progress) progress({PackBuildStage::WritingPackage, 0, total, {}});
        std::array<std::byte, 65536> buffer;
        for (;;)
        {
            auto read = temp->read(buffer.data(), buffer.size());
            NauCheckResult(read);
            if (*read == 0) break;
            size_t offset = 0;
            while (offset < *read)
            {
                auto written = outputStream->write(buffer.data() + offset, *read - offset);
                NauCheckResult(written);
                if (*written == 0) return NauMakeError("Unable to write asset package");
                offset += *written;
            }
            completed += *read;
            if (progress) progress({PackBuildStage::WritingPackage, completed, total, {}});
        }
        outputStream->flush();

        return {};
    }

    Result<io::AssetPackIndexData> readAssetPackage(io::IStreamReader::Ptr packageStream)
    {
        RuntimeValue::Ptr packData;
        size_t headerDataOffset;
        eastl::tie(packData, headerDataOffset) = *readContainerHeader(packageStream);

        io::AssetPackIndexData packIndexData;
        auto value = nau::makeValueRef(packIndexData);
        auto res = RuntimeValue::assign(value, packData);

        for (io::AssetPackFileEntry& content : packIndexData.content)
        {
            content.blobData.offset += headerDataOffset;
        }

        return {packIndexData};
    }
}  // namespace nau