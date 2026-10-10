#pragma once

#include <filesystem>
#include <string>

namespace nau::shader_tools
{
    inline std::string pathToUtf8(const std::filesystem::path& path)
    {
        const auto utf8 = path.u8string();
        return {reinterpret_cast<const char*>(utf8.data()), utf8.size()};
    }
}
