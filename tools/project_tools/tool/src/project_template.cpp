// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.



#include <functional>
#include <map>
#include <sstream>
#include <unordered_set>

#include "nau/project_tools/project_template.h"
#include "nlohmann/json.hpp"

#include "nau/shared/file_system.h"
#include "nau/shared/util.h"

namespace nau
{
    std::map<std::string, std::function<bool(const NauProjectParams&)>> g_condArgs = {
        {     "Always", [](const NauProjectParams& params)
 {
 return true;
 }},
        {"ContentOnly", [](const NauProjectParams& params)
 {
 return params.count("ContentOnly") > 0;
 }}
    };

    static bool makeFilesMap(const nlohmann::json& json, std::unordered_map<std::string, NauFileInfo>& output)
    {
        for(auto it = json.begin(); it != json.end(); ++it)
        {
            const auto& value = it.value();

            NauFileInfo fileInfo;

            fileInfo.name = value["Name"].get<std::string>();
            fileInfo.updateContent = value.contains("UpdateContent") ? value["UpdateContent"].get<bool>() : false;
            fileInfo.rename = value.contains("Rename") ? value["Rename"].get<bool>() : false;
            fileInfo.lowercase = value.contains("Lowercase") ? value["Lowercase"].get<bool>() : false;

            output[fileInfo.name] = fileInfo;
        }

        return output.size() > 0;
    }

    NauProjectTemplate::NauProjectTemplate(const std::string& name, const std::filesystem::path& path, const std::map<std::string, std::string>& projectParams, FileSystem* fs) noexcept :
        m_templateName(name),
        m_templatePath(path)
    {
        std::vector<std::string> files;
        std::stringstream projectConfig;
        std::unordered_map<std::string, NauFileInfo> projectFiles;

        fs->findAllFiles(m_templatePath, files);
        m_isValid = fs->readFile(m_templatePath / "template.json", projectConfig);

        if(files.size() == 0 || !m_isValid)
            return;

        nlohmann::json jsonProjectData = nlohmann::json::parse(projectConfig.str());

        m_version.fromString(jsonProjectData["FileVersion"].get<std::string>());
        m_files.reserve(files.size());

        const auto excludeCondition = jsonProjectData.contains("Exclude") ? jsonProjectData["Exclude"]["Condition"].get<std::string>() : "";
        const bool excludeAny = g_condArgs.contains(excludeCondition) && g_condArgs[excludeCondition](projectParams);
        const auto excludeList = excludeAny ? jsonProjectData["Exclude"]["List"].get<std::unordered_set<std::string>>() : std::unordered_set<std::string>();

        makeFilesMap(jsonProjectData["ProjectFiles"], projectFiles);

        for(auto it = files.begin(); it != files.end();)
        {
            std::string filepath = *it;
            std::string filename = FileSystemExtensions::nameFromPath(filepath);

            if(excludeAny)
            {
                if(excludeAny && excludeList.contains(filename))
                {
                    it = files.erase(it);

                    continue;
                }
            }

            NauFileInfo info;
            info.name = filename;
            info.path = filepath;
            info.extension = FileSystemExtensions::getExtension(filename);
            info.subpath = FileSystemExtensions::getSubPath(pathToUtf8(path), filepath, true);
            info.pureName = filename.replace(filename.find_last_of('.'), std::string::npos, "");

            if(projectFiles.contains(info.name))
            {
                NauFileInfo& projectFileInfo = projectFiles[info.name];

                info.updateContent = projectFileInfo.updateContent;
                info.rename = projectFileInfo.rename;
                info.lowercase = projectFileInfo.lowercase;
            }

            m_files.push_back(info);

            ++it;
        }
    }
}  // namespace nau