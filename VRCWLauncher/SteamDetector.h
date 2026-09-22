#pragma once

#include <string>

namespace SteamDetector
{
    //
    // Returns the full path to the Steam version of
    // VRChat.exe, or an empty string when it cannot
    // be found.
    //
    std::wstring FindVRChatExecutable();

    //
    // Returns the VRChat.exe path inside the given
    // folder, or an empty string when that folder
    // does not contain VRChat.exe.
    //
    std::wstring FindVRChatExecutableInFolder(
        const std::wstring& folder);
}
