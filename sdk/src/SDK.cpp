#include <NFSMW/SDK.hpp>

#include <cstdarg>
#include <cstdio>

namespace
{
    NFSMW::RuntimeInfo g_runtime{};
    HANDLE g_log = INVALID_HANDLE_VALUE;
    SRWLOCK g_logLock = SRWLOCK_INIT;
    bool g_initialized = false;

    bool ReadFingerprint(HMODULE module, NFSMW::ExecutableFingerprint& value) noexcept
    {
        if (!module)
            return false;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
            return false;

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
            reinterpret_cast<const std::byte*>(module) + dos->e_lfanew);

        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
            return false;

        value.peTimestamp = nt->FileHeader.TimeDateStamp;
        value.imageSize = nt->OptionalHeader.SizeOfImage;
        value.entryPointRva = nt->OptionalHeader.AddressOfEntryPoint;
        value.checksum = nt->OptionalHeader.CheckSum;

        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(module, path, MAX_PATH))
            return false;

        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (!GetFileAttributesExW(path, GetFileExInfoStandard, &attributes))
            return false;
        if (attributes.nFileSizeHigh != 0)
            return false;
        value.fileSize = attributes.nFileSizeLow;
        return true;
    }

    bool Equal(const NFSMW::ExecutableFingerprint& a,
               const NFSMW::ExecutableFingerprint& b) noexcept
    {
        return a.peTimestamp == b.peTimestamp &&
               a.imageSize == b.imageSize &&
               a.entryPointRva == b.entryPointRva &&
               a.checksum == b.checksum &&
               a.fileSize == b.fileSize;
    }
}

namespace NFSMW
{
    Result Initialize(const wchar_t* logFileName) noexcept
    {
        if (g_initialized)
            return Result::AlreadyInitialized;

        if (logFileName)
        {
            g_log = CreateFileW(logFileName, FILE_APPEND_DATA,
                FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL, nullptr);
            if (g_log == INVALID_HANDLE_VALUE)
                return Result::LogInitializationFailed;
        }

        Log("Initialize: entered deferred plugin worker");
        HMODULE module = GetModuleHandleW(nullptr);
        if (!module)
            return Result::GameModuleNotFound;

        Log("Initialize: main executable module=%p", module);

        ExecutableFingerprint actual{};
        if (!ReadFingerprint(module, actual))
            return Result::InvalidPeImage;

        Log("Initialize: PE fingerprint read successfully");

        g_runtime.gameModule = module;
        g_runtime.moduleBase = reinterpret_cast<std::uintptr_t>(module);
        g_runtime.actual = actual;
        g_runtime.exactBuildMatch = Equal(actual, SupportedBuild);

        if (!g_runtime.exactBuildMatch)
        {
            Log("Unsupported speed.exe: timestamp=%08X image=%08X entry=%08X checksum=%08X size=%u",
                actual.peTimestamp, actual.imageSize, actual.entryPointRva,
                actual.checksum, actual.fileSize);
            if (g_log != INVALID_HANDLE_VALUE) { CloseHandle(g_log); g_log = INVALID_HANDLE_VALUE; }
            g_runtime = {};
            return Result::UnsupportedExecutable;
        }

        g_initialized = true;
        Log("NFSMW SDK initialized; module base=%p", module);
        return Result::Ok;
    }

    void Shutdown() noexcept
    {
        if (!g_initialized)
            return;
        Log("NFSMW SDK shutdown");
        g_initialized = false;
        g_runtime = {};
        AcquireSRWLockExclusive(&g_logLock);
        if (g_log != INVALID_HANDLE_VALUE) { CloseHandle(g_log); g_log = INVALID_HANDLE_VALUE; }
        ReleaseSRWLockExclusive(&g_logLock);
    }

    bool IsInitialized() noexcept { return g_initialized; }
    const RuntimeInfo& Runtime() noexcept { return g_runtime; }

    void Log(const char* format, ...) noexcept
    {
        if (g_log == INVALID_HANDLE_VALUE || !format)
            return;

        char message[2048]{};
        va_list args;
        va_start(args, format);
        _vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
        va_end(args);

        SYSTEMTIME time{};
        GetLocalTime(&time);
        char line[2200]{};
        _snprintf_s(line, sizeof(line), _TRUNCATE,
            "[%02u:%02u:%02u.%03u] %s\r\n", time.wHour, time.wMinute,
            time.wSecond, time.wMilliseconds, message);

        AcquireSRWLockExclusive(&g_logLock);
        if (g_log != INVALID_HANDLE_VALUE)
        {
            DWORD written{};
            WriteFile(g_log, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
            FlushFileBuffers(g_log);
        }
        ReleaseSRWLockExclusive(&g_logLock);
    }

    const char* ToString(Result result) noexcept
    {
        switch (result)
        {
        case Result::Ok: return "Ok";
        case Result::AlreadyInitialized: return "AlreadyInitialized";
        case Result::GameModuleNotFound: return "GameModuleNotFound";
        case Result::InvalidPeImage: return "InvalidPeImage";
        case Result::UnsupportedExecutable: return "UnsupportedExecutable";
        case Result::LogInitializationFailed: return "LogInitializationFailed";
        default: return "Unknown";
        }
    }
}
