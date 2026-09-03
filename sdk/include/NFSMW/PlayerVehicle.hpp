#pragma once

#include <cstdint>

namespace NFSMW
{
    using PlayerHandle = void*;
    using VehicleHandle = void*;

    struct Vector3
    {
        float x{};
        float y{};
        float z{};
    };

    struct VehicleSnapshot
    {
        PlayerHandle player{};
        VehicleHandle vehicle{};
        const char* vehicleName{};
        Vector3 position{};
        float speedMetresPerSecond{};
        float speedometer{};
        float health{};
        float nitrousCapacity{};
        bool hasNitrous{};
        bool nitrousEngaged{};
    };

    namespace Player
    {
        PlayerHandle GetLocalPlayer() noexcept;
        VehicleHandle GetVehicle() noexcept;
    }

    namespace Vehicle
    {
        bool IsValid(VehicleHandle vehicle) noexcept;
        bool GetSnapshot(VehicleHandle vehicle, VehicleSnapshot& snapshot) noexcept;
        bool GetLocalSnapshot(VehicleSnapshot& snapshot) noexcept;
        bool Repair(VehicleHandle vehicle) noexcept;
        bool ChargeNitrous(VehicleHandle vehicle, float amount) noexcept;
        bool SetInfiniteNitrous(bool enabled) noexcept;
        bool IsInfiniteNitrousEnabled() noexcept;
    }
}
