#pragma once

#include <memory>

namespace nau::PEPEngine::Graphics
{
    class GCommandList;

    // Emits a PIX event only in debug builds, pairing it automatically at scope exit.
    class GCommandListMarker final
    {
    public:
        GCommandListMarker(const std::shared_ptr<GCommandList>& commandList, const wchar_t* label);
        ~GCommandListMarker() noexcept;

        GCommandListMarker(const GCommandListMarker&) = delete;
        GCommandListMarker& operator=(const GCommandListMarker&) = delete;
        GCommandListMarker(GCommandListMarker&&) = delete;
        GCommandListMarker& operator=(GCommandListMarker&&) = delete;

    private:
#if defined(DEBUG) || defined(_DEBUG)
        std::shared_ptr<GCommandList> commandList;
#endif
    };
}

#if defined(DEBUG) || defined(_DEBUG)
#include "nau/mgpu/Graphics/GCommandList.h"

namespace nau::PEPEngine::Graphics
{
    inline GCommandListMarker::GCommandListMarker(const std::shared_ptr<GCommandList>& commandListValue,
                                                   const wchar_t* label)
        : commandList(commandListValue)
    {
        if (commandList != nullptr)
            commandList->StartMark(label);
    }

    inline GCommandListMarker::~GCommandListMarker() noexcept
    {
        if (commandList != nullptr)
        {
            try
            {
                commandList->EndMark();
            }
            catch (...)
            {
            }
        }
    }
}
#else
namespace nau::PEPEngine::Graphics
{
    inline GCommandListMarker::GCommandListMarker(const std::shared_ptr<GCommandList>&, const wchar_t*) {}
    inline GCommandListMarker::~GCommandListMarker() noexcept = default;
}
#endif
