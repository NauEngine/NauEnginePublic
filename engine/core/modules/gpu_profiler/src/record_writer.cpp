// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "record_writer.h"

namespace nau::gpu_profiler
{
    namespace
    {
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
        void writeField(FILE* file, const eastl::string& text)
        {
            if (text.find_first_of(",\"\n") == eastl::string::npos)
            {
                std::fputs(text.c_str(), file);
                return;
            }
            std::fputc('"', file);
            for (const char c : text)
            {
                if (c == '"')
                {
                    std::fputc('"', file);
                }
                std::fputc(c, file);
            }
            std::fputc('"', file);
        }

        uint8_t addTo(eastl::vector<eastl::string>& table, const char* text, const size_t limit)
        {
            for (size_t i = 0; i < table.size(); ++i)
            {
                if (table[i] == text)
                {
                    return static_cast<uint8_t>(i);
                }
            }
            if (table.size() >= limit)
            {
                return static_cast<uint8_t>(limit - 1);
            }
            table.emplace_back(text);
            return static_cast<uint8_t>(table.size() - 1);
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
        std::fputs("frame,pass,gpu,queue,ms,mode,features,age\n", m_file);
        // Features id 0: nothing placed.
        m_features.clear();
        m_features.emplace_back("");
        return true;
    }

    void RecordWriter::close()
    {
        if (m_file != nullptr)
        {
            std::fclose(m_file);
            m_file = nullptr;
        }
    }

    bool RecordWriter::push(const Record& record)
    {
        if (m_file == nullptr)
        {
            ++m_dropped;
            return false;
        }
        write(record);
        return true;
    }

    uint16_t RecordWriter::addString(const char* text)
    {
        if (m_strings.size() >= 0xffff)
        {
            return 0xffff;
        }
        m_strings.emplace_back(text);
        return static_cast<uint16_t>(m_strings.size() - 1);
    }

    uint8_t RecordWriter::addQueue(const char* name)
    {
        return addTo(m_queues, name != nullptr ? name : "", 255);
    }

    uint8_t RecordWriter::addFeatures(const char* text)
    {
        return addTo(m_features, text != nullptr ? text : "", 128);
    }

    eastl::string RecordWriter::string(const uint16_t id, const RecordKind kind) const
    {
        eastl::string text = id < m_strings.size() ? m_strings[id] : eastl::string("<overflow>");
        text += kindSuffix(kind);
        return text;
    }

    eastl::string RecordWriter::queue(const uint8_t id) const
    {
        return id < m_queues.size() ? m_queues[id] : eastl::string();
    }

    void RecordWriter::write(const Record& record)
    {
        char gpu[16];
        formatGpu(gpu, sizeof(gpu), record.kind, record.gpu);
        const uint8_t features = record.mode & 0x7f;

        std::fprintf(m_file, "%u,", record.frame);
        writeField(m_file, string(record.name, record.kind));
        std::fprintf(m_file, ",%s,", gpu);
        writeField(m_file, isTransferPair(record.kind) ? eastl::string("-") : queue(record.queue));
        std::fprintf(m_file, ",%.4f,%s,", record.ms, (record.mode & 0x80) != 0 ? "2gpu" : "1gpu");
        writeField(m_file, features < m_features.size() ? m_features[features] : eastl::string());
        if (record.age != NoAge)
        {
            std::fprintf(m_file, ",%u\n", record.age);
        }
        else
        {
            std::fputs(",\n", m_file);
        }
    }
}  // namespace nau::gpu_profiler
