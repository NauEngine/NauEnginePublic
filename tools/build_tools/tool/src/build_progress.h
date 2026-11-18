// Optional, process-local wire protocol. No change to the public BuildTool ABI.
#pragma once

#include <chrono>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <nlohmann/json.hpp>

namespace nau::build_progress
{
    inline std::string utf8(const std::filesystem::path& path)
    {
        const auto text = path.generic_u8string();
        return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

    class Reporter
    {
    public:
        Reporter()
        {
#ifdef _WIN32
            const char* name = std::getenv("NAU_BUILD_EVENTS_PIPE");
            if (name && std::string_view(name).starts_with(R"(\\.\pipe\NauBuildProgress-)"))
            {
                m_pipe = CreateFileA(name, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                if (m_pipe != INVALID_HANDLE_VALUE)
                {
                    DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
                    if (!SetNamedPipeHandleState(m_pipe, &mode, nullptr, nullptr)) disable();
                }
            }
#endif
        }
        ~Reporter() { disable(); }
        Reporter(const Reporter&) = delete;
        Reporter& operator=(const Reporter&) = delete;

        void report(const char* stage, uint64_t completed = 0, int64_t total = -1,
                    const char* unit = "files", const std::string& file = {}) noexcept
        {
            #ifdef _WIN32
            if (m_pipe == INVALID_HANDLE_VALUE) return;
#else
            return;
#endif
            try
            {
                const auto now = std::chrono::steady_clock::now();
                const bool changed = m_stage != stage;
                const bool finished = total >= 0 && completed == static_cast<uint64_t>(total);
                if (!changed && !finished && now - m_last < std::chrono::milliseconds(100)) return;
                nlohmann::json event = {{"v", 1}, {"stage", stage}, {"completed", completed},
                    {"total", total < 0 ? nlohmann::json(nullptr) : nlohmann::json(total)},
                    {"unit", unit}, {"file", file}};
                const auto line = "NAU_BUILD_EVENT " + event.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) + "\n";
#ifdef _WIN32
                DWORD written = 0;
                if (line.size() > 65536 || !WriteFile(m_pipe, line.data(), static_cast<DWORD>(line.size()), &written, nullptr)
                    || written != line.size())
                {
                    disable();
                    return;
                }
#endif
                m_stage = stage;
                m_last = now;
            }
            catch (...) { /* Telemetry must not fail a build. */ }
        }

    private:
        void disable() noexcept
        {
#ifdef _WIN32
            if (m_pipe != INVALID_HANDLE_VALUE) CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
#endif
        }
#ifdef _WIN32
        HANDLE m_pipe = INVALID_HANDLE_VALUE;
#endif
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
