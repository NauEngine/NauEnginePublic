// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#include "nau/string/string_conv.h"

namespace nau::strings
{

    eastl::wstring utf8ToWString(eastl::u8string_view text)
    {
        if(text.empty())
        {
            return {};
        }

        const char* const inTextPtr = reinterpret_cast<const char*>(text.data());
        const int inTextLen = static_cast<int>(text.size());

        int len = ::MultiByteToWideChar(CP_UTF8, 0, inTextPtr, inTextLen, nullptr, 0);
        if(len == 0)
        {
            return L"INVALID_TEXT\n";
        }

        eastl::wstring result;
        result.resize(static_cast<size_t>(len));

        len = ::MultiByteToWideChar(CP_UTF8, 0, inTextPtr, inTextLen, result.data(), static_cast<int>(result.size()));

        return len > 0 ? result : eastl::wstring{L""};
    }


    eastl::u8string wstringToUtf8(eastl::wstring_view text)
    {
        if(text.empty())
        {
            return {};
        }

        const wchar_t* const inTextPtr = text.data();
        const int inTextLen = static_cast<int>(text.size());

        int len = ::WideCharToMultiByte(CP_UTF8, 0, inTextPtr, inTextLen, nullptr, 0, nullptr, nullptr);
        if(len == 0)
        {
            return u8"INVALID_TEXT\n";
        }

        eastl::u8string result;
        result.resize(static_cast<size_t>(len));
        
        len = ::WideCharToMultiByte(CP_UTF8, 0, inTextPtr, inTextLen, reinterpret_cast<char*>(result.data()), static_cast<int>(result.size()), nullptr, nullptr);
        return len > 0 ? result : eastl::u8string{u8""};
    }

}  // namespace nau::strings