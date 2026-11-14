// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "record_writer.h"

#include <chrono>

namespace nau::gpu_profiler
{
    namespace
    {
        // The writer thread wakes up this often; the ring holds far more than one period of records.
        constexpr std::chrono::milliseconds WakePeriod{20};
        constexpr size_t FileBufferSize = 1 << 20;

        const char* kindSuffix(const RecordKind kind)
        {
            switch (kind)
            {
                case RecordKind::TransferSrc:
                    return "/src";
                case RecordKind::TransferDst:
                    return "/dst";
                case RecordKind::TransferSum:
                    return "/sum";
                case RecordKind::TransferSpan:
                    return "/span";
                default:
                    return "";
            }
        }

        bool isTransferPair(const RecordKind kind)
        {
            return kind == RecordKind::TransferSum || kind == RecordKind::TransferSpan;
        }

        // Quotes a CSV field when needed.
        void writeField(FILE* file, const char* text)
        {
            bool quote = false;
            for (const char* c = text; *c != '\0'; ++c)
            {
                quote = quote || *c == ',' || *c == '"' || *c == '\n';
            }
            if (!quote)
            {
                std::fputs(text, file);
                return;
            }
            std::fputc('"', file);
            for (const char* c = text; *c != '\0'; ++c)
            {
                if (*c == '"')
                {
                    std::fputc('"', file);
                }
                std::fputc(*c, file);
            }
            std::fputc('"', file);
        }

        const char* nameOf(const eastl::vector<eastl::string>& table, const size_t id)
        {
            return id < table.size() ? table[id].c_str() : "<unknown>";
        }
    }  // namespace

    void formatGpu(char* out, const size_t capacity, const RecordKind kind, const uint8_t gpu)
    {
        if (gpu == CpuGpu)
        {
            std::snprintf(out, capacity, "cpu");
        }
        else if (isTransferPair(kind))
        {
            std::snprintf(out, capacity, "%u>%u", gpu >> 4, gpu & 0xf);
        }
        else
        {
            std::snprintf(out, capacity, "%u", gpu);
        }
    }

    eastl::vector<eastl::string>& RecordWriter::Names::table(const Table which)
    {
        switch (which)
        {
            case Table::Queues:
                return queues;
            case Table::Features:
                return features;
            default:
                return strings;
        }
    }

    RecordWriter::~RecordWriter()
    {
        close();
    }

    bool RecordWriter::open(const char* path)
    {
        m_file = std::fopen(path, "w");
        if (m_file == nullptr)
        {
            return false;
        }
        m_fileBuffer.reset(new char[FileBufferSize]);
        std::setvbuf(m_file, m_fileBuffer.get(), _IOFBF, FileBufferSize);
        std::fputs("frame,pass,gpu,queue,ms,mode,features,age,bytes\n", m_file);

        m_ring.reset(new Record[Capacity]);
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
        m_dropped.store(0, std::memory_order_relaxed);
        m_stop.store(false, std::memory_order_relaxed);

        // Features id 0: nothing placed.
        addFeatures("");
        m_thread = std::thread([this]
        {
            run();
        });
        return true;
    }

    void RecordWriter::close()
    {
        if (m_file == nullptr)
        {
            return;
        }
        {
            std::lock_guard lock(m_wakeMutex);
            m_stop.store(true, std::memory_order_release);
        }
        m_wake.notify_one();
        if (m_thread.joinable())
        {
            m_thread.join();
        }
        std::fclose(m_file);
        m_file = nullptr;
        m_fileBuffer.reset();
        m_ring.reset();
    }

    bool RecordWriter::push(const Record& record)
    {
        if (!m_ring)
        {
            m_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        const uint32_t head = m_head.load(std::memory_order_relaxed);
        if (head - m_tail.load(std::memory_order_acquire) >= Capacity)
        {
            // Full: the render thread does not wait for the writer.
            m_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        m_ring[head & (Capacity - 1)] = record;
        m_head.store(head + 1, std::memory_order_release);
        return true;
    }

    uint32_t RecordWriter::addName(const Table which, const char* text, const size_t limit)
    {
        if (text == nullptr)
        {
            text = "";
        }
        eastl::vector<eastl::string>& table = m_producerNames.table(which);
        // Queues and feature placements are few: found by text. Strings are already unique (GpuProfiler::nameId).
        if (which != Table::Strings)
        {
            for (size_t i = 0; i < table.size(); ++i)
            {
                if (table[i] == text)
                {
                    return static_cast<uint32_t>(i);
                }
            }
        }
        if (table.size() >= limit)
        {
            return static_cast<uint32_t>(limit - 1);
        }
        table.emplace_back(text);
        {
            // Before any record with this id is pushed, so the writer always knows the names it meets.
            std::lock_guard lock(m_newNamesMutex);
            m_newNames.push_back({which, eastl::string(text)});
        }
        return static_cast<uint32_t>(table.size() - 1);
    }

    uint16_t RecordWriter::addString(const char* text)
    {
        return static_cast<uint16_t>(addName(Table::Strings, text, 0xffff));
    }

    uint8_t RecordWriter::addQueue(const char* name)
    {
        return static_cast<uint8_t>(addName(Table::Queues, name, 255));
    }

    uint8_t RecordWriter::addFeatures(const char* text)
    {
        return static_cast<uint8_t>(addName(Table::Features, text, 128));
    }

    eastl::string RecordWriter::string(const uint16_t id, const RecordKind kind) const
    {
        eastl::string text = nameOf(m_producerNames.strings, id);
        text += kindSuffix(kind);
        return text;
    }

    eastl::string RecordWriter::queue(const uint8_t id) const
    {
        return id < m_producerNames.queues.size() ? m_producerNames.queues[id] : eastl::string();
    }

    void RecordWriter::run()
    {
        for (;;)
        {
            const bool stop = m_stop.load(std::memory_order_acquire);
            drain(m_head.load(std::memory_order_acquire));
            if (stop)
            {
                // Everything pushed before close() has been written.
                std::fflush(m_file);
                return;
            }
            std::unique_lock lock(m_wakeMutex);
            m_wake.wait_for(lock, WakePeriod, [this]
            {
                return m_stop.load(std::memory_order_acquire);
            });
        }
    }

    void RecordWriter::drain(const uint32_t end)
    {
        // The names of the records up to `end` were handed over before those records were pushed.
        {
            std::lock_guard lock(m_newNamesMutex);
            for (NewName& name : m_newNames)
            {
                m_consumerNames.table(name.table).push_back(eastl::move(name.text));
            }
            m_newNames.clear();
        }

        uint32_t tail = m_tail.load(std::memory_order_relaxed);
        while (tail != end)
        {
            write(m_ring[tail & (Capacity - 1)]);
            ++tail;
            // Frees the slot for the producer as soon as it is written.
            m_tail.store(tail, std::memory_order_release);
        }
    }

    void RecordWriter::write(const Record& record)
    {
        char gpu[16];
        formatGpu(gpu, sizeof(gpu), record.kind, record.gpu);
        const uint8_t features = record.mode & 0x7f;

        char name[512];
        std::snprintf(name, sizeof(name), "%s%s", nameOf(m_consumerNames.strings, record.name), kindSuffix(record.kind));

        std::fprintf(m_file, "%u,", record.frame);
        writeField(m_file, name);
        std::fprintf(m_file, ",%s,", gpu);
        writeField(m_file, isTransferPair(record.kind) ? "-" : nameOf(m_consumerNames.queues, record.queue));
        std::fprintf(m_file, ",%.4f,%s,", record.ms, (record.mode & 0x80) != 0 ? "2gpu" : "1gpu");
        writeField(m_file, features < m_consumerNames.features.size() ? m_consumerNames.features[features].c_str() : "");
        if (record.age != NoAge)
        {
            std::fprintf(m_file, ",%u", record.age);
        }
        else
        {
            std::fputc(',', m_file);
        }
        if (record.bytes != 0)
        {
            std::fprintf(m_file, ",%u\n", record.bytes);
        }
        else
        {
            std::fputs(",\n", m_file);
        }
    }
}  // namespace nau::gpu_profiler
