#pragma once

#include "Addresses.hpp"
#include "Build.hpp"
#include "Function.hpp"
#include "Interface.hpp"
#include "Memory.hpp"
#include "PlayerVehicle.hpp"
#include "Result.hpp"
#include <Windows.h>

namespace NFSMW
{
    struct RuntimeInfo
    {
        HMODULE gameModule{};
        std::uintptr_t moduleBase{};
        ExecutableFingerprint actual{};
        bool exactBuildMatch{};
    };

    Result Initialize(const wchar_t* logFileName = L"NFSMWSDK.log") noexcept;
    void Shutdown() noexcept;
    bool IsInitialized() noexcept;
    const RuntimeInfo& Runtime() noexcept;
    void Log(const char* format, ...) noexcept;
}
