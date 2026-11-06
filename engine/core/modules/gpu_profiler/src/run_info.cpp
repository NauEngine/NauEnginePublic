// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "run_info.h"

#include <cstdio>
#include <cstring>
#include <ctime>

#include "gpu_profiler_build_info.h" // generated at build time (CMakeLists.txt)
#include "profiler_clock.h"

#if defined(_WIN32)
    #include <dxgi.h>
    #pragma comment(lib, "dxgi.lib")
#endif

#ifndef NAU_GPU_PROFILER_CONFIG
    #define NAU_GPU_PROFILER_CONFIG "unknown"
#endif

namespace nau::gpu_profiler
{
    namespace
    {
        eastl::string jsonString(const char* text)
        {
            eastl::string out = "\"";
            for (const char* c = text; c != nullptr && *c != '\0'; ++c)
            {
                switch (*c)
                {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (static_cast<unsigned char>(*c) < 0x20)
                        {
                            char escaped[8];
                            std::snprintf(escaped, sizeof(escaped), "\\u%04x", *c);
                            out += escaped;
                        }
                        else
                        {
                            out += *c;
                        }
                }
            }
            out += "\"";
            return out;
        }

        eastl::string jsonPathFor(const char* csvPath)
        {
            eastl::string path = csvPath;
            const size_t slash = path.find_last_of("/\\");
            const size_t dot = path.find_last_of('.');
            if (dot != eastl::string::npos && (slash == eastl::string::npos || dot > slash))
            {
                path.resize(dot);
            }
            return path + ".json";
        }

        eastl::string localTime()
        {
            const std::time_t now = std::time(nullptr);
            std::tm local = {};
#if defined(_WIN32)
            localtime_s(&local, &now);
#else
            localtime_r(&now, &local);
#endif
            char text[32];
            std::strftime(text, sizeof(text), "%Y-%m-%dT%H:%M:%S", &local);
            return text;
        }

        eastl::string powerSource()
        {
#if defined(_WIN32)
            SYSTEM_POWER_STATUS status = {};
            if (GetSystemPowerStatus(&status))
            {
                char text[48];
                const char* line = status.ACLineStatus == 1 ? "ac" : status.ACLineStatus == 0 ? "battery" : "unknown";
                if (status.BatteryLifePercent <= 100)
                {
                    std::snprintf(text, sizeof(text), "%s, battery %u%%", line, status.BatteryLifePercent);
                }
                else
                {
                    std::snprintf(text, sizeof(text), "%s", line);
                }
                return text;
            }
#endif
            return "unknown";
        }

        eastl::string executable()
        {
#if defined(_WIN32)
            char path[MAX_PATH] = {};
            if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0)
            {
                const char* name = std::strrchr(path, '\\');
                return name != nullptr ? name + 1 : path;
            }
#endif
            return "";
        }

        // Driver (user-mode driver) version of the adapter with this name, "a.b.c.d"; empty when not found.
        eastl::string driverVersion(const eastl::string& adapterName, eastl::string& ids)
        {
#if defined(_WIN32)
            if (adapterName.empty())
            {
                return "";
            }
            IDXGIFactory1* factory = nullptr;
            if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))))
            {
                return "";
            }
            eastl::string version;
            IDXGIAdapter1* adapter = nullptr;
            for (UINT i = 0; version.empty() && factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                DXGI_ADAPTER_DESC1 desc = {};
                char name[256] = {};
                if (SUCCEEDED(adapter->GetDesc1(&desc)) &&
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), nullptr, nullptr) > 0 &&
                    adapterName.find(name) != eastl::string::npos)
                {
                    LARGE_INTEGER umd = {};
                    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd)))
                    {
                        char text[48];
                        std::snprintf(text, sizeof(text), "%u.%u.%u.%u", HIWORD(umd.HighPart), LOWORD(umd.HighPart),
                                      HIWORD(umd.LowPart), LOWORD(umd.LowPart));
                        version = text;
                    }
                    char idText[32];
                    std::snprintf(idText, sizeof(idText), "%04X:%04X", desc.VendorId, desc.DeviceId);
                    ids = idText;
                }
                adapter->Release();
            }
            factory->Release();
            return version;
#else
            return "";
#endif
        }
    }  // namespace

    void RunInfo::start(const Config& config)
    {
        m_config = config;
        m_path = jsonPathFor(config.csvPath);
        m_startTime = localTime();
        m_powerAtStart = powerSource();
        m_startQpc = qpcNow();
        write(false, 0, nullptr);
    }

    void RunInfo::setGpuName(const GpuIndex gpu, const char* name)
    {
        if (gpu < MaxGpus && name != nullptr && m_gpuNames[gpu] != name)
        {
            m_gpuNames[gpu] = name;
        }
    }

    void RunInfo::set(const char* key, const char* value)
    {
        if (key == nullptr)
        {
            return;
        }
        for (auto& entry : m_values)
        {
            if (entry.first == key)
            {
                entry.second = value != nullptr ? value : "";
                return;
            }
        }
        m_values.emplace_back(eastl::string(key), eastl::string(value != nullptr ? value : ""));
    }

    void RunInfo::setResolution(const uint64_t frame, const uint32_t width, const uint32_t height)
    {
        if (m_resolutions.empty() || m_resolutions.back().width != width || m_resolutions.back().height != height)
        {
            m_resolutions.push_back({frame, width, height});
        }
    }

    void RunInfo::countFrame(const bool twoGpus, const eastl::string& features)
    {
        ++(twoGpus ? m_frames2Gpu : m_frames1Gpu);
        for (FeatureCount& count : m_features)
        {
            if (count.features == features)
            {
                ++count.frames;
                return;
            }
        }
        m_features.push_back({features, 1});
    }

    void RunInfo::finish(const uint64_t frames, const Counters& counters)
    {
        write(true, frames, &counters);
    }

    void RunInfo::write(const bool complete, const uint64_t frames, const Counters* counters) const
    {
        FILE* file = std::fopen(m_path.c_str(), "w");
        if (file == nullptr)
        {
            return;
        }
        const auto field = [file](const char* key, const eastl::string& value, const bool last = false)
        {
            std::fprintf(file, "  %s: %s%s\n", jsonString(key).c_str(), jsonString(value.c_str()).c_str(), last ? "" : ",");
        };

        std::fprintf(file, "{\n");
        std::fprintf(file, "  \"complete\": %s,\n", complete ? "true" : "false");
        field("csv", m_config.csvPath);
        field("executable", executable());
        std::fprintf(file, "  \"build\": {\"commit\": %s, \"branch\": %s, \"local_changes\": %s, \"preset\": %s, \"config\": %s},\n",
                     jsonString(NAU_GPU_PROFILER_GIT_COMMIT).c_str(), jsonString(NAU_GPU_PROFILER_GIT_BRANCH).c_str(),
                     NAU_GPU_PROFILER_GIT_DIRTY ? "true" : "false", jsonString(NAU_GPU_PROFILER_PRESET).c_str(),
                     jsonString(NAU_GPU_PROFILER_CONFIG).c_str());

        std::fprintf(file, "  \"adapters\": [");
        bool first = true;
        for (GpuIndex gpu = 0; gpu < MaxGpus; ++gpu)
        {
            if (m_gpuNames[gpu].empty())
            {
                continue;
            }
            eastl::string ids;
            const eastl::string version = driverVersion(m_gpuNames[gpu], ids);
            std::fprintf(file, "%s\n    {\"gpu\": %u, \"name\": %s, \"pci_id\": %s, \"driver_version\": %s}", first ? "" : ",", gpu,
                         jsonString(m_gpuNames[gpu].c_str()).c_str(), jsonString(ids.c_str()).c_str(),
                         jsonString(version.c_str()).c_str());
            first = false;
        }
        std::fprintf(file, "%s],\n", first ? "" : "\n  ");

        std::fprintf(file, "  \"resolution\": [");
        for (size_t i = 0; i < m_resolutions.size(); ++i)
        {
            std::fprintf(file, "%s{\"frame\": %llu, \"width\": %u, \"height\": %u}", i == 0 ? "" : ", ",
                         static_cast<unsigned long long>(m_resolutions[i].frame), m_resolutions[i].width, m_resolutions[i].height);
        }
        std::fprintf(file, "],\n");

        std::fprintf(file, "  \"settings\": {");
        for (size_t i = 0; i < m_values.size(); ++i)
        {
            std::fprintf(file, "%s%s: %s", i == 0 ? "" : ", ", jsonString(m_values[i].first.c_str()).c_str(),
                         jsonString(m_values[i].second.c_str()).c_str());
        }
        std::fprintf(file, "},\n");

        std::fprintf(file, "  \"profiler\": {\"warmup_frames\": %u, \"max_pending_frames\": %u, \"timeout_frames\": %u, \"timeout_ms\": %u},\n",
                     m_config.warmupFrames, m_config.maxPendingFrames, m_config.timeoutFrames, m_config.timeoutMs);
        field("start_time", m_startTime);
        field("power_at_start", m_powerAtStart);

        if (complete)
        {
            field("end_time", localTime());
            field("power_at_end", powerSource());
            std::fprintf(file, "  \"duration_s\": %.3f,\n", qpcToMs(qpcNow() - m_startQpc) / 1000.0);
            std::fprintf(file, "  \"frames\": %llu,\n", static_cast<unsigned long long>(frames));
            std::fprintf(file, "  \"measured_frames\": {\"1gpu\": %llu, \"2gpu\": %llu},\n", static_cast<unsigned long long>(m_frames1Gpu),
                         static_cast<unsigned long long>(m_frames2Gpu));
            std::fprintf(file, "  \"features\": [");
            for (size_t i = 0; i < m_features.size(); ++i)
            {
                std::fprintf(file, "%s{\"features\": %s, \"frames\": %llu}", i == 0 ? "" : ", ",
                             jsonString(m_features[i].features.c_str()).c_str(), static_cast<unsigned long long>(m_features[i].frames));
            }
            std::fprintf(file, "],\n");
            const Counters& c = *counters;
            std::fprintf(file,
                         "  \"counters\": {\"frames_measured\": %llu, \"frames_skipped\": %llu, \"frames_timed_out\": %llu, "
                         "\"frames_in_flight_at_shutdown\": %llu, \"scopes_unmeasured\": %llu, \"records_dropped\": %llu, "
                         "\"transfers_complete\": %llu, \"transfers_unused\": %llu, \"transfers_incomplete\": %llu, "
                         "\"transfers_in_flight_at_shutdown\": %llu}\n",
                         static_cast<unsigned long long>(c.framesMeasured), static_cast<unsigned long long>(c.framesSkipped),
                         static_cast<unsigned long long>(c.framesTimedOut), static_cast<unsigned long long>(c.framesInFlightAtShutdown),
                         static_cast<unsigned long long>(c.scopesUnmeasured), static_cast<unsigned long long>(c.recordsDropped),
                         static_cast<unsigned long long>(c.transfersComplete), static_cast<unsigned long long>(c.transfersUnused),
                         static_cast<unsigned long long>(c.transfersIncomplete),
                         static_cast<unsigned long long>(c.transfersInFlightAtShutdown));
        }
        else
        {
            std::fprintf(file, "  \"frames\": null\n");
        }
        std::fprintf(file, "}\n");
        std::fclose(file);
    }
}  // namespace nau::gpu_profiler
