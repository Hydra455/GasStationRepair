#include <NFSMW/SDK.hpp>

#include <cstdint>

namespace
{
    bool HasExpectedHudInstruction() noexcept
    {
        // Address 0x0057CAA7 must contain: BD xx 00 00 00 (mov ebp, imm32).
        std::uint8_t bytes[5]{};
        constexpr std::uintptr_t instruction = NFSMW::Addresses::Patches::DrawHudImmediate - 1u;
        if (!NFSMW::Memory::IsReadable(reinterpret_cast<const void*>(instruction), sizeof(bytes)))
            return false;

        std::memcpy(bytes, reinterpret_cast<const void*>(instruction), sizeof(bytes));
        return bytes[0] == 0xBDu && bytes[2] == 0x00u &&
               bytes[3] == 0x00u && bytes[4] == 0x00u && bytes[1] <= 0x01u;
    }
}

namespace NFSMW::Interface
{
    bool IsVisible() noexcept
    {
        if (!IsInitialized() || !HasExpectedHudInstruction())
            return false;

        std::uint8_t value{};
        return Memory::Read(Addresses::Patches::DrawHudImmediate, value) && value == 1u;
    }

    bool SetVisible(bool visible) noexcept
    {
        if (!IsInitialized() || !HasExpectedHudInstruction())
            return false;

        const std::uint8_t value = visible ? 1u : 0u;
        return Memory::Write(Addresses::Patches::DrawHudImmediate, value);
    }

    bool Toggle() noexcept
    {
        if (!IsInitialized() || !HasExpectedHudInstruction())
            return false;
        return SetVisible(!IsVisible());
    }
}
