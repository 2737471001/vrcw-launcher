#include "FileAssociation.h"

#include <windows.h>
#include <shlobj.h>

#include <string>
#include <vector>

#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Shell32.lib")

namespace
{
    constexpr wchar_t PROG_ID[] =
        L"VRCWLauncher.VRCWWorld";

    constexpr wchar_t FILE_DESCRIPTION[] =
        L"VRCW World";

    constexpr wchar_t EXTENSION_KEY[] =
        L"Software\\Classes\\.vrcw";

    constexpr wchar_t PROG_ID_KEY[] =
        L"Software\\Classes\\VRCWLauncher.VRCWWorld";

    constexpr wchar_t DEFAULT_ICON_KEY[] =
        L"Software\\Classes\\VRCWLauncher.VRCWWorld\\DefaultIcon";

    constexpr wchar_t OPEN_COMMAND_KEY[] =
        L"Software\\Classes\\VRCWLauncher.VRCWWorld\\shell\\open\\command";

    std::wstring GetExecutablePath()
    {
        std::vector<wchar_t> buffer(32768);

        const DWORD length =
            GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(buffer.size()));

        if (length == 0)
        {
            return std::wstring();
        }

        return std::wstring(
            buffer.data(),
            length);
    }

    std::wstring BuildOpenCommand()
    {
        return L"\"" +
            GetExecutablePath() +
            L"\" \"%1\"";
    }

    bool WriteString(
        HKEY rootKey,
        const wchar_t* subKey,
        const wchar_t* valueName,
        const std::wstring& value)
    {
        HKEY key = nullptr;

        const LSTATUS opened =
            RegCreateKeyExW(
                rootKey,
                subKey,
                0,
                nullptr,
                0,
                KEY_SET_VALUE,
                nullptr,
                &key,
                nullptr);

        if (opened != ERROR_SUCCESS)
        {
            return false;
        }

        const DWORD size =
            static_cast<DWORD>(
                (value.size() + 1) *
                sizeof(wchar_t));

        const LSTATUS written =
            RegSetValueExW(
                key,
                valueName,
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(
                    value.c_str()),
                size);

        RegCloseKey(key);

        return written == ERROR_SUCCESS;
    }

    bool ReadString(
        HKEY rootKey,
        const wchar_t* subKey,
        const wchar_t* valueName,
        std::wstring& value)
    {
        HKEY key = nullptr;

        const LSTATUS opened =
            RegOpenKeyExW(
                rootKey,
                subKey,
                0,
                KEY_QUERY_VALUE,
                &key);

        if (opened != ERROR_SUCCESS)
        {
            return false;
        }

        wchar_t buffer[2048] = {};

        DWORD size = sizeof(buffer);
        DWORD type = 0;

        const LSTATUS read =
            RegQueryValueExW(
                key,
                valueName,
                nullptr,
                &type,
                reinterpret_cast<LPBYTE>(buffer),
                &size);

        RegCloseKey(key);

        if (read != ERROR_SUCCESS ||
            type != REG_SZ)
        {
            return false;
        }

        value.assign(buffer);

        return true;
    }
}

namespace FileAssociation
{
    bool IsRegistered()
    {
        std::wstring extension;

        if (!ReadString(
            HKEY_CURRENT_USER,
            EXTENSION_KEY,
            nullptr,
            extension))
        {
            return false;
        }

        if (_wcsicmp(
            extension.c_str(),
            PROG_ID) != 0)
        {
            return false;
        }

        std::wstring command;

        if (!ReadString(
            HKEY_CURRENT_USER,
            OPEN_COMMAND_KEY,
            nullptr,
            command))
        {
            return false;
        }

        //
        // The association is out of date when the
        // executable has been moved.
        //
        return _wcsicmp(
            command.c_str(),
            BuildOpenCommand().c_str()) == 0;
    }

    bool SetRegistered(
        bool registered)
    {
        if (!registered)
        {
            std::wstring extension;

            const bool extensionIsOurs =
                ReadString(
                    HKEY_CURRENT_USER,
                    EXTENSION_KEY,
                    nullptr,
                    extension) &&
                _wcsicmp(
                    extension.c_str(),
                    PROG_ID) == 0;

            RegDeleteTreeW(
                HKEY_CURRENT_USER,
                PROG_ID_KEY);

            //
            // Leave another application's association
            // alone.
            //
            if (extensionIsOurs)
            {
                RegDeleteTreeW(
                    HKEY_CURRENT_USER,
                    EXTENSION_KEY);
            }

            SHChangeNotify(
                SHCNE_ASSOCCHANGED,
                SHCNF_IDLIST,
                nullptr,
                nullptr);

            return true;
        }

        if (GetExecutablePath().empty())
        {
            return false;
        }

        if (!WriteString(
            HKEY_CURRENT_USER,
            EXTENSION_KEY,
            nullptr,
            PROG_ID))
        {
            return false;
        }

        if (!WriteString(
            HKEY_CURRENT_USER,
            PROG_ID_KEY,
            nullptr,
            FILE_DESCRIPTION))
        {
            return false;
        }

        if (!WriteString(
            HKEY_CURRENT_USER,
            DEFAULT_ICON_KEY,
            nullptr,
            GetExecutablePath() + L",0"))
        {
            return false;
        }

        if (!WriteString(
            HKEY_CURRENT_USER,
            OPEN_COMMAND_KEY,
            nullptr,
            BuildOpenCommand()))
        {
            return false;
        }

        SHChangeNotify(
            SHCNE_ASSOCCHANGED,
            SHCNF_IDLIST,
            nullptr,
            nullptr);

        return true;
    }
}
