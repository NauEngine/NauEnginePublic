#include "nau/mgpu/Utils/pch.h"
#include "nau/mgpu/Utils/d3dUtil.h"
#include <comdef.h>
#include <fstream>
#include <string>

namespace nau::PEPEngine::Utils
{
    using Microsoft::WRL::ComPtr;

    DxException::DxException(const HRESULT hr, const std::wstring& functionName, const std::wstring& filename,
                             const int lineNumber, const std::wstring& message) :
        ErrorCode(hr),
        FunctionName(functionName),
        Filename(filename),
        LineNumber(lineNumber), message(message)
    {
    }

    DxException::DxException(const std::wstring& message) : message(message)
    {
    }

    bool d3dUtil::IsKeyDown(const int vkeyCode)
    {
        return (GetAsyncKeyState(vkeyCode) & 0x8000) != 0;
    }


    std::wstring DxException::ToString() const
    {
        // Get the string description of the error code.
        _com_error err(ErrorCode);
        std::wstring msg = err.ErrorMessage();

        return FunctionName + L" failed in " + Filename + L"; line " + std::to_wstring(LineNumber) + L"; error: " +
            msg;
    }
}
