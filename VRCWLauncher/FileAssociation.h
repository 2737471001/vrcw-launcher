#pragma once

namespace FileAssociation
{
    //
    // True when .vrcw files currently open with this
    // executable.
    //
    bool IsRegistered();

    //
    // Registers or removes the per-user file association
    // for .vrcw files.
    //
    bool SetRegistered(
        bool registered);
}
