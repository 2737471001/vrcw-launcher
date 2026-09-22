#pragma once

#include <string>

namespace VRChatLauncher
{
    enum class LaunchError
    {
        None,
        VrchatExecutableMissing,
        WorldFileMissing,
        WorldFileNotFound,
        StartFailed
    };

    //
    // Builds a room id with the same shape as the
    // original launch.bat: two values between
    // 10000 and 42767 joined together.
    //
    std::wstring CreateRoomId();

    //
    // Builds the --url value that tells VRChat to
    // open the given world.
    //
    std::wstring BuildUrl(
        const std::wstring& vrcwPath,
        const std::wstring& roomId);

    //
    // Builds the full command line for VRChat.exe.
    //
    std::wstring BuildCommandLine(
        const std::wstring& vrchatExePath,
        const std::wstring& vrcwPath,
        const std::wstring& roomId);

    //
    // Starts VRChat with the given world. The launcher
    // does not wait for VRChat to exit.
    //
    // Returns an error code; the interface turns it into a
    // message in the current language.
    //
    LaunchError Launch(
        const std::wstring& vrchatExePath,
        const std::wstring& vrcwPath);
}
