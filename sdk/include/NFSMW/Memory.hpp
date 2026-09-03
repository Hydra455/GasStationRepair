#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace NFSMW::Memory
{
    std::uintptr_t ModuleBase() noexcept;
    std::uintptr_t FromIda(std::uintptr_t idaVirtualAddress) noexcept;
    bool IsReadable(const void* address, std::size_t bytes = 1) noexcept;
    bool IsExecutable(const void* address) noexcept;
    bool WriteBytes(void* destination, const void* source, std::size_t bytes) noexcept;

    template <typename T>
    bool Read(std::uintptr_t address, T& output) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (!IsReadable(reinterpret_cast<const void*>(address), sizeof(T)))
            return false;
        std::memcpy(&output, reinterpret_cast<const void*>(address), sizeof(T));
        return true;
    }

    template <typename T>
    bool Write(std::uintptr_t address, const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        return WriteBytes(reinterpret_cast<void*>(address), &value, sizeof(T));
    }
}
