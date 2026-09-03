#pragma once

namespace NFSMW
{
    enum class Result
    {
        Ok,
        AlreadyInitialized,
        GameModuleNotFound,
        InvalidPeImage,
        UnsupportedExecutable,
        LogInitializationFailed
    };

    const char* ToString(Result result) noexcept;
}
