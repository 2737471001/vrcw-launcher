#pragma once

#include <string>
#include <vector>

namespace Settings
{
    struct Data
    {
        bool keepLauncherOpen = false;

        //
        // Language of the interface:
        //   -1  not chosen yet, follow the Windows language
        //    0  English
        //    1  Chinese
        //
        int chineseUi = -1;

        //
        // Only set when the user picked a VRChat folder
        // manually. Steam detection stays the default.
        //
        std::wstring vrchatExecutablePath;

        std::vector<std::wstring> recentVrcwFiles;
    };

    //
    // Folder that holds the settings file.
    //
    std::wstring GetFolderPath();

    Data Load();

    void Save(
        const Data& data);

    //
    // Moves a file to the front of the recent list,
    // removes duplicates and shortens the list.
    //
    void AddRecentFile(
        Data& data,
        const std::wstring& path);

    void ClearRecentFiles(
        Data& data);
}
