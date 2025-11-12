// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#pragma once

#include "nau/string/string.h"

namespace nau
{
    /**
        @brief Handles Engine version
    */
    class NAU_KERNEL_EXPORT EngineVersion
    {
    public:
        /**
            @brief Fill passed EngineVersion& out from string
            @returns true and fill out, if parsed, false and out not touched, if not
        */
        static bool parse(const std::string& engineVersionString, EngineVersion& out);

        /**
            @brief Current Engine version set in versio.h and vcs_version.h
            @returns Ref to static instance of EngineVersion
        */
        static const EngineVersion& current();

    public:
        EngineVersion() :
            m_major(0),
            m_minor(0),
            m_patch(0)
        {
        }

        EngineVersion(uint16_t major, uint16_t minor, uint16_t patch, const nau::string& commit = "", const nau::string& branch = "");

        uint16_t getMajor() const
        {
            return m_major;
        }

        uint16_t getMinor() const
        {
            return m_minor;
        }

        uint16_t getPatch() const
        {
            return m_patch;
        }

        const nau::string& getCommit() const
        {
            return m_commit;
        }

        const nau::string& getBranch() const
        {
            return m_branch;
        }

        /**
            @brief Exact version match, only for Major.Minor.Patch components
        */
        bool matchVersion(const EngineVersion& other) const;

        /**
            @brief Exact version and VCS info match
        */
        bool matchVersionAndBuild(const EngineVersion& other) const;

        /**
            @brief Check, if Major.Minor.Patch components >= other. For compatibility checking.

        */
        bool greaterOrEqualVersion(const EngineVersion& other) const;

        /**
            @brief To string conversion
            @returns string in "1.2.3" format or "1.2.3-commit+branch" when VCS info is set
        */
        nau::string toString() const;

    private:
        uint16_t m_major;
        uint16_t m_minor;
        uint16_t m_patch;
        nau::string m_commit;
        nau::string m_branch;
    };
}  // namespace nau
