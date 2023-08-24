#pragma once

#include <sstream>

/* Perfect hash values for selected strings */
enum class Tag {
    NewProc = 98,
    SchedFork = 74,
    SysClone = 93,
    SysCloneFailed = 99,
    Close = 105,
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
    Symlink = 97,
    Exit = 19,

    ProgramInterpreter = 147,               // PI
    ProgramPath = 77,                       // PP
    CurrentWorkingDirectory = 107,          // CW
    AbsolutePath = 132,                     // FN
    OriginalPath = 92,                      // FO
    RenameFromPath = 2,                     // RF
    RenameToPath = 22,                      // RT
    LinkFromPath = 7,                       // LF
    LinkToPath = 27,                        // LT
    SymlinkTargetName = 52,                 // ST
    SymlinkTargetPath = 32,                 // SR
    SymlinkPath = 42,                       // SL

    ArrayedArguments = 47,                  // A[
    ProgramInterpreterExtended = 88,        // PI[
    ProgramPathExtended = 53,               // PP[
    CurrentWorkingDirectoryExtended = 83,   // CW[
    AbsolutePathExtended = 68,              // FN[
    OriginalPathExtended = 48,              // FO[
    RenameFromPathExtended = 3,             // RF[
    RenameToPathExtended = 13,              // RT[
    LinkFromPathExtended = 8,               // LF[
    LinkToPathExtended = 18,                // LT[
    SymlinkTargetNameExtended = 43,         // ST[
    SymlinkTargetPathExtended = 33,         // SR[
    SymlinkPathExtended = 38,               // SL[

    ProgramInterpreterEnd = 91,             // PI_end
    ProgramPathEnd = 56,                    // PP_end
    CurrentWorkingDirectoryEnd = 86,        // CW_end
    AbsolutePathEnd = 71,                   // FN_end
    OriginalPathEnd = 51,                   // FO_end
    RenameFromPathEnd = 6,                  // RF_end
    RenameToPathEnd = 16,                   // RT_end
    LinkFromPathEnd = 11,                   // LF_end
    LinkToPathEnd = 21,                     // LT_end
    SymlinkTargetNameEnd = 46,              // ST_end
    SymlinkTargetPathEnd = 36,              // SR_end
    SymlinkPathEnd = 41,                    // SL_end

    EndOfArgs = 96,
    Cont = 79,
    ContEnd = 78,
    None = -1,
};

enum class ShortArguments {
    Pid = 58,
    ArgSize = 37,
    Prognameisize = 73,
    Prognamepsize = 128,
    CwdSize = 67,
    Fd = 17,
    Fd1 = 118,
    Fd2 = 113,
    Oldfd = 65,
    Newfd = 50,
    Flags = 90,
    Mode = 34,
    Fnamesize = 69,
    Forigsize = 39,
    Targetnamesize = 59,
    Linknamesize = 82,
    Resolvednamesize = 66,
    Status = 61,
};


// std::ostream& operator<<(std::ostream &o, const Tag &tag);
