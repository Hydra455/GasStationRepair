#pragma once

#include <cstdint>

namespace NFSMW::Addresses
{
    namespace Patches
    {
        // Immediate byte in: BD 01 00 00 00  (mov ebp, 1), HUD render path.
        inline constexpr std::uintptr_t DrawHudImmediate = 0x0057CAA8u;
    }

    // Add only addresses confirmed against the supplied IDA database and speed.exe.
    // Keep absolute IDA virtual addresses here (image base 0x00400000).
    namespace Functions
    {
        // inline constexpr std::uintptr_t StringToHash = 0x00501230u; // Example only.
    }

    namespace Globals
    {
        // inline constexpr std::uintptr_t LocalPlayer = 0x00901230u; // Example only.
    }
}
