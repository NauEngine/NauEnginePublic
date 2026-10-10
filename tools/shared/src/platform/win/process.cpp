// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.


#if defined(_WIN32) || defined(_WIN64)
    #include "nau/shared/platform/win/process.h"

    #include <windows.h>

    #include <cstdio>
    #include <iostream>
    #include <stdexcept>

    #include "nau/shared/error_codes.h"
    #include "nau/shared/logger.h"
    #include "nau/string/string_conv.h"

namespace nau
{
    int WindowsProcess::runProcess(const std::string& args, [[maybe_unused]] class Logger* logger)
    {
        STARTUPINFOW startUpInfo;
        ZeroMemory(&startUpInfo, sizeof(startUpInfo));
        startUpInfo.cb = sizeof(startUpInfo);
        startUpInfo.dwFlags |= STARTF_USESTDHANDLES;
        startUpInfo.dwFlags |= STARTF_USESHOWWINDOW;
        startUpInfo.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION processInfo;
        ZeroMemory(&processInfo, sizeof(processInfo));

        SECURITY_ATTRIBUTES securityAttributes;
        ZeroMemory(&securityAttributes, sizeof(securityAttributes));
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.bInheritHandle = true;

        // Create pipes for the child's STDOUT and STDIN.
        HANDLE hStdOutRead, hStdOutWrite;
        if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &securityAttributes, 0))
        {
            LOG_ERROR("Failed to create pipe for process {}", args);
            return ErrorCode::internalError;
        }

        // Ensure the read handle to the pipe for STDOUT is not inherited.
        if (!SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0))
        {
            LOG_ERROR("Failed to setup read handle for process {}", args);
            return ErrorCode::internalError;
        }

        startUpInfo.hStdOutput = hStdOutWrite;
        startUpInfo.hStdError = hStdOutWrite;

        // Start the child process.
        // CreateProcessW requires writable UTF-16.
        auto command = strings::utf8ToWString(eastl::string_view{args.data(), args.size()});
        if (!CreateProcessW(nullptr,
            command.data(),
            nullptr,
            nullptr,
            true,
            0,
            nullptr,
            nullptr,
            &startUpInfo,
            &processInfo))
        {
            LOG_ERROR("CreateProcess with args {} failed. Error {}", args, GetLastError());
            CloseHandle(hStdOutWrite);
            CloseHandle(hStdOutRead);
            return ErrorCode::internalError;
        }

        // Close handles to the child's STDOUT and stdin.
        CloseHandle(hStdOutWrite);

        char buffer[256];
        DWORD readBytes;
        std::string output;
        while (ReadFile(hStdOutRead, buffer, sizeof(buffer) - 1, &readBytes, NULL) && readBytes > 0)
        {
            buffer[readBytes] = '\0';
            output += buffer;
            size_t pos;
            while ((pos = output.find_first_of("\r\n")) != std::string::npos)
            {
                std::string line = output.substr(0, pos);
                output.erase(0, pos + 1);
                LOG_INFO("{}", line);
                processOutput << line;
            }
        }

        CloseHandle(hStdOutRead);
        DWORD exit_code;
        if (!GetExitCodeProcess(processInfo.hProcess, &exit_code))
        {
            LOG_ERROR("The process ended with an error {}", GetLastError());
            CloseHandle(processInfo.hThread);
            CloseHandle(processInfo.hProcess);
            return ErrorCode::internalError;
        }

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return exit_code;
    }
}  // namespace nau
#endif