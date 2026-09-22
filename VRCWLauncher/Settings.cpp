#include "Settings.h"

#include <windows.h>
#include <shlobj.h>

#include <string>
#include <vector>

#pragma comment(lib, "Shell32.lib")

namespace
{
    constexpr wchar_t APP_FOLDER_NAME[] =
        L"VRCW Launcher";

    constexpr wchar_t SETTINGS_FILE_NAME[] =
        L"settings.ini";

    constexpr wchar_t SECTION_GENERAL[] =
        L"General";

    constexpr wchar_t SECTION_VRCHAT[] =
        L"VRChat";

    constexpr wchar_t SECTION_RECENT[] =
        L"Recent";

    //
    // How many entries are remembered. The window only
    // shows the first few.
    //
    constexpr int MAX_RECENT_FILES = 8;

    std::wstring GetAppDataFolder()
    {
        wchar_t buffer[MAX_PATH] = {};

        const HRESULT result =
            SHGetFolderPathW(
                nullptr,
                CSIDL_APPDATA,
                nullptr,
                SHGFP_TYPE_CURRENT,
                buffer);

        if (FAILED(result))
        {
            return std::wstring();
        }

        return std::wstring(buffer);
    }

    std::wstring ReadString(
        const wchar_t* section,
        const wchar_t* key,
        const std::wstring& filePath)
    {
        std::vector<wchar_t> buffer(32768);

        const DWORD length =
            GetPrivateProfileStringW(
                section,
                key,
                L"",
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                filePath.c_str());

        return std::wstring(
            buffer.data(),
            length);
    }

    //
    // Builds "prefix" followed by a decimal number, so no
    // formatting runtime is needed.
    //
    std::wstring MakeNumberedKey(
        const wchar_t* prefix,
        int value)
    {
        std::wstring result(prefix);

        wchar_t digits[16] = {};

        int length = 0;

        int remaining = value;

        if (remaining <= 0)
        {
            digits[length++] = L'0';
        }

        while (remaining > 0 && length < 15)
        {
            digits[length++] =
                static_cast<wchar_t>(
                    L'0' + (remaining % 10));

            remaining /= 10;
        }

        while (length > 0)
        {
            result.push_back(digits[--length]);
        }

        return result;
    }

    std::wstring MakeRecentKey(
        int index)
    {
        return MakeNumberedKey(
            L"File",
            index);
    }
}

namespace Settings
{
    std::wstring GetFolderPath()
    {
        const std::wstring appData =
            GetAppDataFolder();

        if (appData.empty())
        {
            return std::wstring();
        }

        const std::wstring folder =
            appData + L"\\" + APP_FOLDER_NAME;

        //
        // The folder may already exist.
        //
        CreateDirectoryW(
            folder.c_str(),
            nullptr);

        return folder;
    }

    Data Load()
    {
        Data data;

        const std::wstring folder =
            GetFolderPath();

        if (folder.empty())
        {
            return data;
        }

        const std::wstring filePath =
            folder + L"\\" + SETTINGS_FILE_NAME;

        data.keepLauncherOpen =
            GetPrivateProfileIntW(
                SECTION_GENERAL,
                L"KeepLauncherOpen",
                0,
                filePath.c_str()) != 0;

        data.chineseUi =
            GetPrivateProfileIntW(
                SECTION_GENERAL,
                L"ChineseUi",
                -1,
                filePath.c_str());

        data.vrchatExecutablePath =
            ReadString(
                SECTION_VRCHAT,
                L"ExecutablePath",
                filePath);

        const int count =
            GetPrivateProfileIntW(
                SECTION_RECENT,
                L"Count",
                0,
                filePath.c_str());

        for (int index = 0;
            index < count &&
            index < MAX_RECENT_FILES;
            ++index)
        {
            const std::wstring path =
                ReadString(
                    SECTION_RECENT,
                    MakeRecentKey(index).c_str(),
                    filePath);

            if (!path.empty())
            {
                data.recentVrcwFiles.push_back(path);
            }
        }

        return data;
    }

    void Save(
        const Data& data)
    {
        const std::wstring folder =
            GetFolderPath();

        if (folder.empty())
        {
            return;
        }

        const std::wstring filePath =
            folder + L"\\" + SETTINGS_FILE_NAME;

        WritePrivateProfileStringW(
            SECTION_GENERAL,
            L"KeepLauncherOpen",
            data.keepLauncherOpen
                ? L"1"
                : L"0",
            filePath.c_str());

        //
        // Left out while the language still follows the
        // Windows language, so the key only exists once the
        // user picked one.
        //
        if (data.chineseUi >= 0)
        {
            WritePrivateProfileStringW(
                SECTION_GENERAL,
                L"ChineseUi",
                data.chineseUi != 0
                    ? L"1"
                    : L"0",
                filePath.c_str());
        }

        WritePrivateProfileStringW(
            SECTION_VRCHAT,
            L"ExecutablePath",
            data.vrchatExecutablePath.c_str(),
            filePath.c_str());

        //
        // Rewrite the whole recent list so that removed
        // entries do not stay behind.
        //
        WritePrivateProfileStringW(
            SECTION_RECENT,
            nullptr,
            nullptr,
            filePath.c_str());

        const std::wstring countText =
            MakeNumberedKey(
                L"",
                static_cast<int>(
                    data.recentVrcwFiles.size()));

        WritePrivateProfileStringW(
            SECTION_RECENT,
            L"Count",
            countText.c_str(),
            filePath.c_str());

        for (size_t index = 0;
            index < data.recentVrcwFiles.size();
            ++index)
        {
            const std::wstring key =
                MakeRecentKey(
                    static_cast<int>(index));

            WritePrivateProfileStringW(
                SECTION_RECENT,
                key.c_str(),
                data.recentVrcwFiles[index].c_str(),
                filePath.c_str());
        }
    }

    void AddRecentFile(
        Data& data,
        const std::wstring& path)
    {
        if (path.empty())
        {
            return;
        }

        for (size_t index = 0;
            index < data.recentVrcwFiles.size();
            ++index)
        {
            if (_wcsicmp(
                data.recentVrcwFiles[index].c_str(),
                path.c_str()) == 0)
            {
                data.recentVrcwFiles.erase(
                    data.recentVrcwFiles.begin() +
                    static_cast<ptrdiff_t>(index));

                break;
            }
        }

        data.recentVrcwFiles.insert(
            data.recentVrcwFiles.begin(),
            path);

        if (static_cast<int>(
            data.recentVrcwFiles.size()) > MAX_RECENT_FILES)
        {
            data.recentVrcwFiles.resize(
                static_cast<size_t>(MAX_RECENT_FILES));
        }
    }

    void ClearRecentFiles(
        Data& data)
    {
        data.recentVrcwFiles.clear();
    }
}
