#pragma once

#include <windows.h>

#include <string>

namespace VrcwDropTarget
{
    using PathCallback =
        void (*)(const std::wstring& path);

    using HoverCallback =
        void (*)(bool hovering);

    //
    // Registers an OLE drop target that accepts
    // .vrcw files dropped onto the window.
    //
    // onDrop  is called with the first dropped .vrcw file.
    // onHover is called when a valid drag enters or leaves.
    //
    bool Register(
        HWND window,
        PathCallback onDrop,
        HoverCallback onHover);

    void Unregister(
        HWND window);
}
