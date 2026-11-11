// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "nau/3d/dag_drv3d.h"

namespace camera_preview_test
{
    // Coordinates start at the top left. The right and bottom edges are exclusive.
    struct Rect
    {
        int x, y, width, height;
    };

    struct Frame
    {
        int width = 0, height = 0;
        std::vector<std::uint32_t> pixels;  // 0x00RRGGBB

        Rect bounds() const { return {0, 0, width, height}; }

        void checkArea(Rect area) const
        {
            if (area.x < 0 || area.y < 0 || area.width <= 0 || area.height <= 0 ||
                area.x > width - area.width || area.y > height - area.height)
            {
                throw std::runtime_error("Pixel comparison area is outside the captured frame");
            }
        }
    };

    inline Frame captureFrame()
    {
        Frame frame;
        int pitch = 0, format = -1;
        auto* bytes = static_cast<const std::uint8_t*>(d3d::fast_capture_screen(frame.width, frame.height, pitch, format));
        if (!bytes)
        {
            throw std::runtime_error("DX12 back buffer readback failed");
        }
        struct Unlock
        {
            ~Unlock() { d3d::end_fast_capture_screen(); }
        } unlock;

        if (format != d3d::CAPFMT_X8R8G8B8 || frame.width <= 0 || frame.height <= 0 || pitch / 4 < frame.width)
        {
            throw std::runtime_error("Unexpected screen capture format, dimensions or pitch");
        }
        frame.pixels.resize(static_cast<std::size_t>(frame.width) * frame.height);
        for (int y = 0; y < frame.height; ++y)
        {
            auto* row = frame.pixels.data() + static_cast<std::size_t>(y) * frame.width;
            std::memcpy(row, bytes + static_cast<std::size_t>(y) * pitch, sizeof(*row) * frame.width);
            for (int x = 0; x < frame.width; ++x)
            {
                row[x] &= 0x00ffffffu;  // X in X8R8G8B8 is not a color channel.
            }
        }
        return frame;
    }

    inline bool differs(std::uint32_t first, std::uint32_t second, int rgbTolerance)
    {
        if (rgbTolerance == 0)
        {
            return first != second;
        }
        for (int shift = 0; shift < 24; shift += 8)
        {
            const int a = (first >> shift) & 255;
            const int b = (second >> shift) & 255;
            if (std::abs(a - b) > rgbTolerance)
            {
                return true;
            }
        }
        return false;
    }

    inline std::size_t changedPixels(const Frame& a, const Frame& b, Rect area, int rgbTolerance = 0)
    {
        if (a.width != b.width || a.height != b.height)
        {
            throw std::runtime_error("Expected identical back buffer dimensions for pixel comparison");
        }
        a.checkArea(area);
        std::size_t changed = 0;
        for (int y = area.y; y < area.y + area.height; ++y)
        {
            for (int x = area.x; x < area.x + area.width; ++x)
            {
                const auto i = static_cast<std::size_t>(y) * a.width + x;
                changed += differs(a.pixels[i], b.pixels[i], rgbTolerance);
            }
        }
        return changed;
    }

    inline std::size_t changedOutside(const Frame& a, const Frame& b, Rect inset, int rgbTolerance = 0)
    {
        return changedPixels(a, b, a.bounds(), rgbTolerance) - changedPixels(a, b, inset, rgbTolerance);
    }
}
