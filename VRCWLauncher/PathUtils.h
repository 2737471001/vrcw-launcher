#pragma once

#include <string>

namespace PathUtils
{
    //
    // Returns the file name portion of a path.
    //
    std::wstring GetFileName(
        const std::wstring& path);

    //
    // Returns the folder portion of a path,
    // without a trailing separator.
    //
    std::wstring GetFolderPath(
        const std::wstring& path);

    //
    // Joins a folder and a name with a single separator.
    //
    std::wstring CombinePaths(
        const std::wstring& folder,
        const std::wstring& name);

    //
    // True when the path points to an existing file.
    //
    bool FileExists(
        const std::wstring& path);

    //
    // True when the path points to an existing folder.
    //
    bool FolderExists(
        const std::wstring& path);

    //
    // True when the path has a .vrcw extension.
    // The comparison is case insensitive.
    //
    bool IsVrcwFile(
        const std::wstring& path);

    //
    // Replaces forward slashes with backslashes.
    //
    std::wstring NormalizeSlashes(
        const std::wstring& path);
}
