#include "SteamDetector.h"

#include "PathUtils.h"

#include <windows.h>

#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")

namespace
{
    //
    // VRChat's Steam application id.
    //
    constexpr wchar_t VRCHAT_APP_MANIFEST[] =
        L"appmanifest_438100.acf";

    std::wstring ReadRegistryString(
        HKEY rootKey,
        const wchar_t* subKey,
        const wchar_t* valueName)
    {
        wchar_t buffer[1024] = {};

        DWORD size = sizeof(buffer);

        const LSTATUS status =
            RegGetValueW(
                rootKey,
                subKey,
                valueName,
                RRF_RT_REG_SZ,
                nullptr,
                buffer,
                &size);

        if (status != ERROR_SUCCESS)
        {
            return std::wstring();
        }

        return std::wstring(buffer);
    }

    std::wstring ToWide(
        const std::string& text)
    {
        if (text.empty())
        {
            return std::wstring();
        }

        const int size =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(text.size()),
                nullptr,
                0);

        if (size <= 0)
        {
            return std::wstring();
        }

        std::wstring wide(
            static_cast<size_t>(size),
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(text.size()),
            wide.data(),
            size);

        return wide;
    }

    //
    // Pulls the first two quoted values out of a line
    // that looks like:  "key"    "value"
    //
    bool ParseQuotedValues(
        const std::wstring& line,
        std::wstring& key,
        std::wstring& value)
    {
        std::vector<std::wstring> values;

        size_t position = 0;

        while (position < line.size())
        {
            const size_t open =
                line.find(L'"', position);

            if (open == std::wstring::npos)
            {
                break;
            }

            const size_t close =
                line.find(L'"', open + 1);

            if (close == std::wstring::npos)
            {
                break;
            }

            values.push_back(
                line.substr(
                    open + 1,
                    close - open - 1));

            position = close + 1;
        }

        if (values.size() < 2)
        {
            return false;
        }

        key = values[0];
        value = values[1];

        return true;
    }

    //
    // Steam escapes backslashes inside its
    // configuration files.
    //
    std::wstring Unescape(
        const std::wstring& text)
    {
        std::wstring result;

        result.reserve(text.size());

        for (size_t index = 0; index < text.size(); ++index)
        {
            if (text[index] == L'\\' &&
                index + 1 < text.size())
            {
                const wchar_t next =
                    text[index + 1];

                if (next == L'\\' || next == L'/')
                {
                    result.push_back(L'\\');

                    ++index;

                    continue;
                }
            }

            result.push_back(text[index]);
        }

        return result;
    }

    bool IsSamePath(
        const std::wstring& left,
        const std::wstring& right)
    {
        return _wcsicmp(
            left.c_str(),
            right.c_str()) == 0;
    }

    void AddLibrary(
        std::vector<std::wstring>& libraries,
        const std::wstring& library)
    {
        if (library.empty())
        {
            return;
        }

        for (const std::wstring& existing : libraries)
        {
            if (IsSamePath(existing, library))
            {
                return;
            }
        }

        libraries.push_back(library);
    }

    void ParseLibraryFolders(
        const std::wstring& vdfPath,
        std::vector<std::wstring>& libraries)
    {
        std::ifstream file(vdfPath.c_str());

        if (!file.is_open())
        {
            return;
        }

        std::string line;

        while (std::getline(file, line))
        {
            std::wstring key;
            std::wstring value;

            if (!ParseQuotedValues(
                ToWide(line),
                key,
                value))
            {
                continue;
            }

            //
            // Current Steam uses "path".
            // Older Steam used numeric keys.
            //
            bool isLibraryKey =
                _wcsicmp(
                    key.c_str(),
                    L"path") == 0;

            if (!isLibraryKey)
            {
                isLibraryKey = !key.empty();

                for (const wchar_t character : key)
                {
                    if (character < L'0' ||
                        character > L'9')
                    {
                        isLibraryKey = false;

                        break;
                    }
                }
            }

            if (!isLibraryKey)
            {
                continue;
            }

            AddLibrary(
                libraries,
                PathUtils::NormalizeSlashes(
                    Unescape(value)));
        }
    }

    std::wstring GetSteamInstallPath()
    {
        //
        // Steam stores this value with forward slashes.
        //
        std::wstring path =
            ReadRegistryString(
                HKEY_CURRENT_USER,
                L"Software\\Valve\\Steam",
                L"SteamPath");

        if (path.empty())
        {
            path = ReadRegistryString(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\WOW6432Node\\Valve\\Steam",
                L"InstallPath");
        }

        if (path.empty())
        {
            path = ReadRegistryString(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Valve\\Steam",
                L"InstallPath");
        }

        return PathUtils::NormalizeSlashes(path);
    }

    std::wstring GetVrchatInstallFolderName(
        const std::wstring& library)
    {
        const std::wstring manifestPath =
            PathUtils::CombinePaths(
                PathUtils::CombinePaths(
                    library,
                    L"steamapps"),
                VRCHAT_APP_MANIFEST);

        std::ifstream file(manifestPath.c_str());

        if (!file.is_open())
        {
            return L"VRChat";
        }

        std::string line;

        while (std::getline(file, line))
        {
            std::wstring key;
            std::wstring value;

            if (!ParseQuotedValues(
                ToWide(line),
                key,
                value))
            {
                continue;
            }

            if (_wcsicmp(
                key.c_str(),
                L"installdir") == 0)
            {
                return Unescape(value);
            }
        }

        return L"VRChat";
    }
}

namespace SteamDetector
{
    std::wstring FindVRChatExecutable()
    {
        std::vector<std::wstring> libraries;

        const std::wstring steamPath =
            GetSteamInstallPath();

        if (!steamPath.empty())
        {
            AddLibrary(libraries, steamPath);

            ParseLibraryFolders(
                PathUtils::CombinePaths(
                    PathUtils::CombinePaths(
                        steamPath,
                        L"steamapps"),
                    L"libraryfolders.vdf"),
                libraries);
        }

        //
        // A library can list further libraries in its
        // own libraryfolders.vdf file.
        //
        const std::vector<std::wstring> discovered =
            libraries;

        for (const std::wstring& library : discovered)
        {
            ParseLibraryFolders(
                PathUtils::CombinePaths(
                    PathUtils::CombinePaths(
                        library,
                        L"steamapps"),
                    L"libraryfolders.vdf"),
                libraries);
        }

        for (const std::wstring& library : libraries)
        {
            const std::wstring folder =
                PathUtils::CombinePaths(
                    PathUtils::CombinePaths(
                        PathUtils::CombinePaths(
                            library,
                            L"steamapps"),
                        L"common"),
                    GetVrchatInstallFolderName(library));

            const std::wstring executable =
                PathUtils::CombinePaths(
                    folder,
                    L"VRChat.exe");

            if (PathUtils::FileExists(executable))
            {
                return executable;
            }
        }

        return std::wstring();
    }

    std::wstring FindVRChatExecutableInFolder(
        const std::wstring& folder)
    {
        if (folder.empty())
        {
            return std::wstring();
        }

        const std::wstring executable =
            PathUtils::CombinePaths(
                folder,
                L"VRChat.exe");

        if (PathUtils::FileExists(executable))
        {
            return executable;
        }

        return std::wstring();
    }
}
