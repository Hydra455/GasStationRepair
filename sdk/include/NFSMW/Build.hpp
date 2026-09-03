#pragma once

#include <cstdint>

namespace NFSMW
{
    struct ExecutableFingerprint
    {
        std::uint32_t peTimestamp;
        std::uint32_t imageSize;
        std::uint32_t entryPointRva;
        std::uint32_t checksum;
        std::uint32_t fileSize;
    };

    inline constexpr std::uintptr_t PreferredImageBase = 0x00400000u;

    // speed.exe supplied with NFSMostWanted-v1.3.i64.
    inline constexpr ExecutableFingerprint SupportedBuild{
        0x438E4C8Cu, // Raw TimeDateStamp stored in the supplied PE header.
        0x00678E4Eu,
        0x003C4040u,
        0x005C4D62u,
        6029312u
    };

    inline constexpr char SupportedSha256[] =
        "b248271bf8eac8c9b283b8c95e3add672b713bf529b05f1780e58268493b9d06";
}
