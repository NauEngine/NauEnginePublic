// Optional, process-local wire protocol. No change to the public BuildTool ABI.
#pragma once

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace nau::build_progress
{
    inline std::string utf8(const std::filesystem::path& path)
    {
        const auto text = path.generic_u8string();
        return {text.begin(), text.end()};
    }

    class Reporter
    {
    public:
        Reporter()
        {
            const char* value = std::getenv("NAU_BUILD_EVENTS");
            m_enabled = value && std::strcmp(value, "1") == 0;
        }

        void report(const char* stage, uint64_t completed = 0, int64_t total = -1,
                    const char* unit = "files", const std::string& file = {}) noexcept
        {
            if (!m_enabled) return;
            try
            {
                const auto now = std::chrono::steady_clock::now();
                const bool changed = m_stage != stage;
                const bool finished = total >= 0 && completed == static_cast<uint64_t>(total);
                if (!changed && !finished && now - m_last < std::chrono::milliseconds(100)) return;
                nlohmann::json event = {{"v", 1}, {"stage", stage}, {"completed", completed},
                    {"total", total < 0 ? nlohmann::json(nullptr) : nlohmann::json(total)},
                    {"unit", unit}, {"file", file}};
                const auto line = "NAU_BUILD_EVENT " + event.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
                std::fprintf(stdout, "%s\n", line.c_str());
                std::fflush(stdout);
                m_stage = stage;
                m_last = now;
            }
            catch (...) { /* Telemetry must not fail a build. */ }
        }

    private:
        bool m_enabled = false;
        std::string m_stage;
        std::chrono::steady_clock::time_point m_last{};
    };

    inline Reporter*& current()
    {
        static thread_local Reporter* reporter = nullptr;
        return reporter;
    }

    class Scope
    {
    public:
        Scope() : m_previous(current()) { current() = &m_reporter; }
        ~Scope() { current() = m_previous; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        Reporter m_reporter;
        Reporter* m_previous;
    };

    inline void report(const char* stage, uint64_t completed = 0, int64_t total = -1,
                       const char* unit = "files", const std::string& file = {})
    {
        if (current()) current()->report(stage, completed, total, unit, file);
    }
}
