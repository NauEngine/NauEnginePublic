// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#pragma once

#include <nau/dag_ioSys/dag_io.h>
namespace nau::iosys
{
    //! unpacks data through intermediate buffer to increase performance when reading to uncached memory
    static inline void NAU_KERNEL_EXPORT read_to_uncached_mem(IGenLoad& crd, void* ptr, int size)
    {
        static constexpr int OUTBUF_SZ = 8192;
        char outBuf[OUTBUF_SZ];
        char* p = (char*)ptr;

        while(size > OUTBUF_SZ)
        {
            crd.read(outBuf, OUTBUF_SZ);
            memcpy(p, outBuf, OUTBUF_SZ);
            p += OUTBUF_SZ;
            size -= OUTBUF_SZ;
        }

        crd.read(outBuf, size);
        memcpy(p, outBuf, size);
    }

    //! helper to reduce reading to single line
    static inline void NAU_KERNEL_EXPORT read_to_device_mem(IGenLoad& crd, void* ptr, int size, bool cached_mem)
    {
        if(cached_mem)
            crd.read(ptr, size);
        else
            read_to_uncached_mem(crd, ptr, size);
    }
}  // namespace nau::iosys