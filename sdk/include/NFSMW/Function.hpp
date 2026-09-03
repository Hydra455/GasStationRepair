#pragma once

#include "Memory.hpp"
#include <cstdint>
#include <type_traits>

namespace NFSMW
{
    template <typename Signature>
    class Function;

    template <typename Return, typename... Args>
    class Function<Return(__cdecl)(Args...)>
    {
    public:
        explicit constexpr Function(std::uintptr_t idaAddress) : idaAddress_(idaAddress) {}

        bool IsAvailable() const noexcept
        {
            return Memory::IsExecutable(reinterpret_cast<const void*>(Address()));
        }

        std::uintptr_t Address() const noexcept { return Memory::FromIda(idaAddress_); }

        Return operator()(Args... args) const
        {
            using Pointer = Return(__cdecl*)(Args...);
            return reinterpret_cast<Pointer>(Address())(args...);
        }

    private:
        std::uintptr_t idaAddress_;
    };

    template <typename Return, typename This, typename... Args>
    Return CallThis(std::uintptr_t idaAddress, This* self, Args... args)
    {
        using Pointer = Return(__thiscall*)(This*, Args...);
        return reinterpret_cast<Pointer>(Memory::FromIda(idaAddress))(self, args...);
    }
}
