#pragma once

#include <sstream>

/* Perfect hash values for selected strings */
enum class Tag {
    NewProc = 93,
    SchedFork = 59,
    SysClone = 88,
    SysCloneFailed = 94,
    Close = 60,
    Pipe = 29,
    Dup = 108,
    Open = 149,
    RenameFrom = 40,
    RenameTo = 23,
    Rename2From = 31,
    RenameFailed = 12,
    LinkFailed = 15,
    LinkTo = 26,
    LinkatFrom = 45,
    LinkFrom = 28,
    Symlink = 87,
    Exit = 14,

    ProgramInterpreter = 137,               // PI
    ProgramPath = 77,                       // PP
    CurrentWorkingDirectory = 112,          // CW
    AbsolutePath = 132,                     // FN
    OriginalPath = 92,                      // FO
    RenameFromPath = 2,                     // RF
    RenameToPath = 22,                      // RT
    LinkFromPath = 7,                       // LF
    LinkToPath = 27,                        // LT
    SymlinkTargetName = 52,                 // ST
    SymlinkTargetPath = 32,                 // SR
    SymlinkPath = 42,                       // SL

    ArrayedArguments = 37,                  // A[
    ProgramInterpreterExtended = 83,        // PI[
    ProgramPathExtended = 53,               // PP[
    CurrentWorkingDirectoryExtended = 78,   // CW[
    AbsolutePathExtended = 68,              // FN[
    OriginalPathExtended = 48,              // FO[
    RenameFromPathExtended = 3,             // RF[
    RenameToPathExtended = 13,              // RT[
    LinkFromPathExtended = 8,               // LF[
    LinkToPathExtended = 18,                // LT[
    SymlinkTargetNameExtended = 43,         // ST[
    SymlinkTargetPathExtended = 33,         // SR[
    SymlinkPathExtended = 38,               // SL[

    ProgramInterpreterEnd = 86,             // PI_end
    ProgramPathEnd = 56,                    // PP_end
    CurrentWorkingDirectoryEnd = 81,        // CW_end
    AbsolutePathEnd = 71,                   // FN_end
    OriginalPathEnd = 51,                   // FO_end
    RenameFromPathEnd = 6,                  // RF_end
    RenameToPathEnd = 16,                   // RT_end
    LinkFromPathEnd = 11,                   // LF_end
    LinkToPathEnd = 21,                     // LT_end
    SymlinkTargetNameEnd = 46,              // ST_end
    SymlinkTargetPathEnd = 36,              // SR_end
    SymlinkPathEnd = 41,                    // SL_end

    EndOfArgs = 91,
    Cont = 64,
    ContEnd = 63,
    None = -1,
};

enum class ShortArguments {
    Pid = 58,
    ArgSize = 62,
    Prognameisize = 73,
    Prognamepsize = 128,
    CwdSize = 102,
    Fd = 17,
    Fd1 = 118,
    Fd2 = 113,
    Oldfd = 35,
    Newfd = 50,
    Flags = 65,
    Mode = 34,
    Fnamesize = 69,
    Forigsize = 39,
    Targetnamesize = 84,
    Linknamesize = 57,
    Resolvednamesize = 66,
};


// std::ostream& operator<<(std::ostream &o, const Tag &tag);
