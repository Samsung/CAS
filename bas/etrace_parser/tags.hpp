#pragma once

#include <sstream>

/* Perfect hash values for selected strings */
enum class Tag {
    NewProc = 123,
    SchedFork = 54,
    SysClone = 78,
    SysCloneFailed = 104,
    Close = 100,
    Pipe = 14,
    Dup = 133,
    Open = 114,
    RenameFrom = 50,
    RenameTo = 38,
    Rename2From = 41,
    RenameFailed = 32,
    LinkFailed = 40,
    LinkTo = 56,
    LinkatFrom = 70,
    LinkFrom = 48,
    Symlink = 77,
    Exit = 24,
    Mount = 85,
    Umount = 76,

    UPID = 119,                             // UPID
    ProgramInterpreter = 212,               // PI
    ProgramPath = 2,                        // PP
    CurrentWorkingDirectory = 122,          // CW
    AbsolutePath = 172,                     // FN
    OriginalPath = 112,                     // FO
    RenameFromPath = 52,                    // RF
    RenameToPath = 12,                      // RT
    LinkFromPath = 62,                      // LF
    LinkToPath = 22,                        // LT
    SymlinkTargetName = 7,                  // ST
    SymlinkTargetPath = 27,                 // SR
    SymlinkPath = 47,                       // SL
    MountSource = 67,                       // MS
    MountTarget = 57,                       // MT
    MountType = 107,                        // MX

    ArrayedEnvs = 39,                       // Env[
    ArrayedArguments = 37,                  // A[
    ProgramInterpreterExtended = 108,       // PI[
    ProgramPathExtended = 3,                // PP[
    CurrentWorkingDirectoryExtended = 93,   // CW[
    AbsolutePathExtended = 98,              // FN[
    OriginalPathExtended = 68,              // FO[
    RenameFromPathExtended = 33,            // RF[
    RenameToPathExtended = 13,              // RT[
    LinkFromPathExtended = 43,              // LF[
    LinkToPathExtended = 23,                // LT[
    SymlinkTargetNameExtended = 8,          // ST[
    SymlinkTargetPathExtended = 18,         // SR[
    SymlinkPathExtended = 28,               // SL[
    MountSourceExtended = 63,               // MS[
    MountTargetExtended = 58,               // MT[
    MountTypeExtended = 83,                 // MX[

    ProgramInterpreterEnd = 111,            // PI_end
    ProgramPathEnd = 6,                     // PP_end
    CurrentWorkingDirectoryEnd = 96,        // CW_end
    AbsolutePathEnd = 101,                  // FN_end
    OriginalPathEnd = 71,                   // FO_end
    RenameFromPathEnd = 36,                 // RF_end
    RenameToPathEnd = 16,                   // RT_end
    LinkFromPathEnd = 46,                   // LF_end
    LinkToPathEnd = 26,                     // LT_end
    SymlinkTargetNameEnd = 11,              // ST_end
    SymlinkTargetPathEnd = 21,              // SR_end
    SymlinkPathEnd = 31,                    // SL_end
    MountSourceEnd = 66,                    // MS_end
    MountTargetEnd = 61,                    // MT_end
    MountTypeEnd = 86,                      // MX_end

    EndOfArgs = 91,
    Cont = 89,
    ContEnd = 88,
    None = -1,
};

enum class ShortArguments {
    Pid = 53,
    ArgSize = 82,
    Prognameisize = 73,
    Prognamepsize = 103,
    CwdSize = 132,
    Fd = 17,
    Fd1 = 138,
    Fd2 = 118,
    Oldfd = 60,
    Newfd = 30,
    Flags = 90,
    Mode = 34,
    Fnamesize = 49,
    Forigsize = 44,
    Sourcenamesize = 79,
    Typenamesize = 117,
    Targetnamesize = 94,
    Linknamesize = 92,
    Resolvednamesize = 51,
    Status = 81,
};


// std::ostream& operator<<(std::ostream &o, const Tag &tag);
