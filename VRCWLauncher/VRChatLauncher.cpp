#include "VRChatLauncher.h"

#include "PathUtils.h"

#include <windows.h>

#include <random>
#include <string>
#include <vector>

namespace
{
    std::wstring Quote(
        const std::wstring& value)
    {
        std::wstring result;

        result.reserve(value.size() + 2);

        result.push_back(L'"');
        result += value;
        result.push_back(L'"');

        return result;
    }
}

namespace VRChatLauncher
{
    std::wstring CreateRoomId()
    {
        std::random_device device;

        std::mt19937 generator(
            static_cast<unsigned int>(
                device() ^
                static_cast<unsigned int>(
                    GetTickCount64())));

        std::uniform_int_distribution<int> distribution(
            10000,
            42767);

        std::wstring roomId;

        roomId += std::to_wstring(
            distribution(generator));

        roomId += std::to_wstring(
            distribution(generator));

        return roomId;
    }

    std::wstring BuildUrl(
        const std::wstring& vrcwPath,
        const std::wstring& roomId)
    {
        return L"create?roomId=" + roomId +
            L"&hidden=true&name=BuildAndRun&url=file:///" +
            vrcwPath;
    }

    std::wstring BuildCommandLine(
        const std::wstring& vrchatExePath,
        const std::wstring& vrcwPath,
        const std::wstring& roomId)
    {
        std::wstring commandLine =
            Quote(vrchatExePath);

        commandLine += L" ";

        commandLine += Quote(
            L"--url=" +
            BuildUrl(vrcwPath, roomId));

        commandLine += L" --watch-worlds";
        commandLine += L" --watch-avatars";

        return commandLine;
    }

    LaunchError Launch(
        const std::wstring& vrchatExePath,
        const std::wstring& vrcwPath)
    {
        if (vrchatExePath.empty() ||
            !PathUtils::FileExists(vrchatExePath))
        {
            return LaunchError::VrchatExecutableMissing;
        }

        if (vrcwPath.empty())
        {
            return LaunchError::WorldFileMissing;
        }

        if (!PathUtils::FileExists(vrcwPath))
        {
            return LaunchError::WorldFileNotFound;
        }

        const std::wstring roomId =
            CreateRoomId();

        std::wstring commandLine =
            BuildCommandLine(
                vrchatExePath,
                vrcwPath,
                roomId);

        //
        // CreateProcessW may modify the command line,
        // so it needs a writable buffer.
        //
        std::vector<wchar_t> buffer(
            commandLine.begin(),
            commandLine.end());

        buffer.push_back(L'\0');

        const std::wstring workingDirectory =
            PathUtils::GetFolderPath(vrchatExePath);

        STARTUPINFOW startupInfo{};

        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};

        const BOOL created =
            CreateProcessW(
                vrchatExePath.c_str(),
                buffer.data(),
                nullptr,
                nullptr,
                FALSE,
                0,
                nullptr,
                workingDirectory.empty()
                    ? nullptr
                    : workingDirectory.c_str(),
                &startupInfo,
                &processInfo);

        if (created == FALSE)
        {
            return LaunchError::StartFailed;
        }

        //
        // The launcher does not wait for VRChat.
        //
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        return LaunchError::None;
    }
}
