// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Asynchronous CSV output of the profiler (producer - consumer).
//
// The render thread (producer) puts fixed-size records into a lock-free single-producer single-consumer ring buffer and
// never waits: when the ring is full the record is dropped and counted. Names (passes, queues, feature placements) are
// replaced by ids; a new name is handed to the writer thread once, through a small mutex-protected list (names are
// rare: the mutex is not taken for ordinary records). The writer thread (consumer) wakes up periodically, takes the
// records, formats them with the names and writes them to the file. close() stops the thread after it has written
// everything.
#pragma once

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>

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
        // Records in the ring: ~1.3 MB, several seconds of records at a few hundred frames per second.
        static constexpr uint32_t Capacity = 1u << 16;

        RecordWriter() = default;
        ~RecordWriter();

        RecordWriter(const RecordWriter&) = delete;
        RecordWriter& operator=(const RecordWriter&) = delete;

        // Opens the file, writes the header and starts the writer thread.
        bool open(const char* path);
        // Writes everything still in the ring, stops the thread and closes the file.
        void close();

        // Render thread, never waits. False when the ring is full and the record was dropped.
        bool push(const Record& record);
        // Records dropped because the ring was full.
        uint64_t dropped() const
        {
            return m_dropped.load(std::memory_order_relaxed);
        }

        // Render thread; ids are stable for the run.
        uint16_t addString(const char* text);
        uint8_t addQueue(const char* name);
        uint8_t addFeatures(const char* text);

        // Render thread: the names as the producer knows them (for the log).
        eastl::string string(uint16_t id, RecordKind kind = RecordKind::Pass) const;
        eastl::string queue(uint8_t id) const;

    private:
        enum class Table : uint8_t
        {
            Strings,
            Queues,
            Features
        };

        struct NewName
        {
            Table table = Table::Strings;
            eastl::string text;
        };

        // Names on one side (producer or consumer).
        struct Names
        {
            eastl::vector<eastl::string> strings;
            eastl::vector<eastl::string> queues;
            eastl::vector<eastl::string> features;

            eastl::vector<eastl::string>& table(Table which);
        };

        uint32_t addName(Table which, const char* text, size_t limit);
        void run();
        // Consumer: takes the new names, then writes the records up to `end`.
        void drain(uint32_t end);
        void write(const Record& record);

        FILE* m_file = nullptr;
        eastl::unique_ptr<char[]> m_fileBuffer;

        // Ring: the producer writes m_head, the consumer m_tail. Indices grow and wrap at 2^32.
        eastl::unique_ptr<Record[]> m_ring;
        alignas(64) std::atomic<uint32_t> m_head = 0;
        alignas(64) std::atomic<uint32_t> m_tail = 0;
        std::atomic<uint64_t> m_dropped = 0;

        Names m_producerNames; // render thread
        Names m_consumerNames; // writer thread
        std::mutex m_newNamesMutex;
        eastl::vector<NewName> m_newNames; // guarded by m_newNamesMutex

        std::thread m_thread;
        std::mutex m_wakeMutex;
        std::condition_variable m_wake;
        std::atomic<bool> m_stop = false;
    };
}  // namespace nau::gpu_profiler
