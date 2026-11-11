// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.
#pragma once
#include <functional>
#include <string>
#include <cstdint>

#include "nau/utils/functor.h"
#include "nau/io/stream.h"
#include "nau/rtti/rtti_object.h"
#include "nau/rtti/ptr.h"
#include "nau/io/asset_pack.h"

/**
 * @brief This file defines structures and functions for building and reading asset packages.
 * Asset packages allow bundling multiple files into a single package for easier management and loading.
 */

namespace nau
{
    /**
     * @struct PackInputFileData
     * @brief Structure representing input file data for asset packages.
     *
     * This structure includes a functor for creating a stream reader and the file path of the input file within the package.
     */
    struct PackInputFileData
    {
        using StreamFactory = Functor<io::IStreamReader::Ptr()>;

        StreamFactory stream; ///< Functor that creates an instance of IStreamReader for the input file.
        eastl::string filePathInPack; ///< The path to the file within the asset package.
    };

    /**
     * @struct PackBuildOptions
     * @brief Structure representing build options for creating an asset package.
     *
     * This structure allows the user to specify the content type, version, and description of the asset package.
     */
    struct PackBuildOptions
    {
        eastl::string contentType = "application/json"; ///< The content type of the asset package.
        eastl::string version = "0.1"; ///< The version of the asset package.
        eastl::string description; ///< A human-readable description of the asset package.
    };

    /**
     * @brief Builds an asset package from the provided input files and options.
     *
     * This function collects input file data and builds an asset package, writing it to the specified output stream.
     *
     * @param content A vector of PackInputFileData structures representing the files to include in the package.
     * @param buildOptions Options for the asset package, including content type, version, and description.
     * @param outputStream A pointer to the output stream where the asset package will be written.
     * @return Result<> Indicates the success or failure of the operation.
     */
    enum class PackBuildStage { ReadingFiles, WritingPackage };
    struct PackBuildProgress
    {
        PackBuildStage stage;
        uint64_t completed;
        uint64_t total;
        std::string file;
    };
    using PackBuildProgressCallback = std::function<void(const PackBuildProgress&)>;

    // Additive API: the existing entry point remains unchanged for older clients.
    NAU_ASSETPACKTOOL_EXPORT
    Result<> buildAssetPackageWithProgress(const eastl::vector<PackInputFileData>& content,
        PackBuildOptions buildOptions, io::IStreamWriter::Ptr outputStream,
        const PackBuildProgressCallback& progress);

    NAU_ASSETPACKTOOL_EXPORT
    Result<> buildAssetPackage(const eastl::vector<PackInputFileData>& content, PackBuildOptions buildOptions, io::IStreamWriter::Ptr outputStream);

    /**
     * @brief Reads an asset package from the given input stream.
     *
     * This function reads an asset package from the specified stream and returns the index data of the package.
     *
     * @param packageStream A pointer to the input stream from which the asset package is read.
     * @return Result<io::AssetPackIndexData> The index data of the asset package, or an error result if the operation fails.
     */
    NAU_ASSETPACKTOOL_EXPORT
    Result<io::AssetPackIndexData> readAssetPackage(io::IStreamReader::Ptr packageStream);
}  // namespace nau