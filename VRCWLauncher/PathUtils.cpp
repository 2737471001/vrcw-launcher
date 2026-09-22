#include "PathUtils.h"

#include <windows.h>

namespace PathUtils
{
    std::wstring GetFileName(
        const std::wstring& path)
    {
        const size_t position =
            path.find_last_of(L"\\/");

        if (position == std::wstring::npos)
        {
            return path;
        }

        return path.substr(position + 1);
    }

    std::wstring GetFolderPath(
        const std::wstring& path)
    {
        const size_t position =
            path.find_last_of(L"\\/");

        if (position == std::wstring::npos)
        {
            return std::wstring();
        }

        return path.substr(0, position);
    }

    std::wstring CombinePaths(
        const std::wstring& folder,
        const std::wstring& name)
    {
        if (folder.empty())
        {
            return name;
        }

        if (name.empty())
        {
            return folder;
        }

        const wchar_t last =
            folder[folder.size() - 1];

        if (last == L'\\' || last == L'/')
        {
            return folder + name;
        }

        return folder + L"\\" + name;
    }

    bool FileExists(
        const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        const DWORD attributes =
            GetFileAttributesW(path.c_str());

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        return (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }

    bool FolderExists(
        const std::wstring& path)
    {
        if (path.empty())
        {
            return false;
        }

        const DWORD attributes =
            GetFileAttributesW(path.c_str());

        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            return false;
        }

        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    bool IsVrcwFile(
        const std::wstring& path)
    {
        const size_t position =
            path.find_last_of(L'.');

        if (position == std::wstring::npos)
        {
            return false;
        }

        std::wstring extension =
            path.substr(position + 1);

        for (wchar_t& character : extension)
        {
            if (character >= L'A' &&
                character <= L'Z')
            {
                character =
                    character - L'A' + L'a';
            }
        }

        return extension == L"vrcw";
    }

    std::wstring NormalizeSlashes(
        const std::wstring& path)
    {
        std::wstring result = path;

        for (wchar_t& character : result)
        {
            if (character == L'/')
            {
                character = L'\\';
            }
        }

        return result;
    }
}
