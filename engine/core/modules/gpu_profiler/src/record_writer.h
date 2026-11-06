// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// CSV output of the profiler. The render thread passes fixed-size records; names are replaced by ids of a string
// table owned by the writer.
#pragma once

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <cstdint>
#include <cstdio>

namespace nau::gpu_profiler
{
    enum class RecordKind : uint8_t
    {
        Pass, // GPU time of a scope
        Cpu, // CPU time of a scope on the render thread
        CpuFrame, // CPU time between the ends of two frames
        TransferSrc,
        TransferDst,
        TransferSum,
        TransferSpan
    };

    // Record::gpu of CPU records.
    inline constexpr uint8_t CpuGpu = 0xff;
    // Record::age when not applicable.
    inline constexpr uint16_t NoAge = 0xffff;

    struct Record
    {
        uint32_t frame = 0;
        uint16_t name = 0; // string table id
        uint8_t gpu = 0; // GPU index; transfers: (src << 4) | dst; CpuGpu
        uint8_t queue = 0; // queue table id
        RecordKind kind = RecordKind::Pass;
        uint8_t mode = 0; // bit 7: two GPUs used; bits 0..6: features table id
        uint16_t age = NoAge; // frames between the start of a transfer and the use of its result
        float ms = 0;
        uint32_t bytes = 0; // transfers: payload size, 0 = unknown or not a transfer
    };
    static_assert(sizeof(Record) == 20);

    // Writes "gpu" column text of a record.
    void formatGpu(char* out, size_t capacity, RecordKind kind, uint8_t gpu);

    class RecordWriter
    {
    public:
        RecordWriter() = default;
        ~RecordWriter();

        RecordWriter(const RecordWriter&) = delete;
        RecordWriter& operator=(const RecordWriter&) = delete;

        bool open(const char* path);
        void close();

        // Render thread. False when the record had to be dropped.
        bool push(const Record& record);
        uint64_t dropped() const
        {
            return m_dropped;
        }

        // Render thread; ids are stable for the run.
        uint16_t addString(const char* text);
        uint8_t addQueue(const char* name);
        uint8_t addFeatures(const char* text);

        eastl::string string(uint16_t id, RecordKind kind = RecordKind::Pass) const;
        eastl::string queue(uint8_t id) const;

    private:
        void write(const Record& record);

        FILE* m_file = nullptr;
        uint64_t m_dropped = 0;
        eastl::vector<eastl::string> m_strings;
        eastl::vector<eastl::string> m_queues;
        eastl::vector<eastl::string> m_features;
    };
}  // namespace nau::gpu_profiler
