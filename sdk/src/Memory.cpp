#include <NFSMW/Build.hpp>
#include <NFSMW/Memory.hpp>

namespace NFSMW::Memory
{
    std::uintptr_t ModuleBase() noexcept
    {
        return reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    }

    std::uintptr_t FromIda(std::uintptr_t idaVirtualAddress) noexcept
    {
        return ModuleBase() + (idaVirtualAddress - PreferredImageBase);
    }

    bool IsReadable(const void* address, std::size_t bytes) noexcept
    {
        if (!address || bytes == 0)
            return false;
        MEMORY_BASIC_INFORMATION info{};
        if (!VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT)
            return false;
        if ((info.Protect & PAGE_GUARD) || info.Protect == PAGE_NOACCESS)
            return false;
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        const auto end = start + bytes;
        const auto regionEnd = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
        return end >= start && end <= regionEnd;
    }

    bool IsExecutable(const void* address) noexcept
    {
        MEMORY_BASIC_INFORMATION info{};
        if (!address || !VirtualQuery(address, &info, sizeof(info)) || info.State != MEM_COMMIT)
            return false;
        const DWORD protection = info.Protect & 0xFFu;
        return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
               protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    }

    bool WriteBytes(void* destination, const void* source, std::size_t bytes) noexcept
    {
        if (!destination || !source || bytes == 0)
            return false;
        DWORD oldProtection{};
        if (!VirtualProtect(destination, bytes, PAGE_EXECUTE_READWRITE, &oldProtection))
            return false;
        std::memcpy(destination, source, bytes);
        FlushInstructionCache(GetCurrentProcess(), destination, bytes);
        DWORD ignored{};
        return VirtualProtect(destination, bytes, oldProtection, &ignored) != FALSE;
    }
}
