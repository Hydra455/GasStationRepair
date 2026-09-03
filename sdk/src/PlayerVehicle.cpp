#include <NFSMW/SDK.hpp>

#pragma warning(push)
#pragma warning(disable : 4099) // Upstream bNode/bTNode use struct forward declarations.
#include <NFSPluginSDK/Game.MW05/Types/IDamageable.h>
#include <NFSPluginSDK/Game.MW05/Types/IEngine.h>
#include <NFSPluginSDK/Game.MW05/Types/PVehicle.h>
#pragma warning(pop)

#include <cstddef>
#include <cmath>

namespace
{
    using namespace NFSPluginSDK::MW05;

    PVehicle* AsVehicle(NFSMW::VehicleHandle handle) noexcept
    {
        auto* candidate = static_cast<PVehicle*>(handle);
        if (!candidate || !NFSMW::Memory::IsReadable(candidate, sizeof(void*)))
            return nullptr;

        void* vtable{};
        if (!NFSMW::Memory::Read(reinterpret_cast<std::uintptr_t>(candidate), vtable) ||
            !NFSMW::Memory::IsReadable(vtable, sizeof(void*)))
            return nullptr;

        void* firstFunction{};
        if (!NFSMW::Memory::Read(reinterpret_cast<std::uintptr_t>(vtable), firstFunction) ||
            !NFSMW::Memory::IsExecutable(firstFunction))
            return nullptr;

        if (candidate->mObjType == NFSPluginSDK::SimableType::Invalid ||
            candidate->mDirty || !candidate->mRigidBody)
            return nullptr;
        return candidate;
    }

    PVehicle* FindPlayerVehicle() noexcept
    {
        const auto instances = reinterpret_cast<std::uintptr_t>(PVehicle::g_mInstances);
        for (std::size_t index = 0; index < 256; ++index)
        {
            PVehicle* candidate{};
            const auto slot = instances + index * sizeof(PVehicle::_InstanceLayout);
            if (!NFSMW::Memory::Read(slot, candidate) || !candidate)
                break;

            auto* vehicle = AsVehicle(candidate);
            if (vehicle && vehicle->IsPlayer() && vehicle->IsOwnedByPlayer())
                return vehicle;
        }
        return nullptr;
    }
}

namespace NFSMW::Player
{
    VehicleHandle GetVehicle() noexcept
    {
        if (!IsInitialized())
            return nullptr;
        return FindPlayerVehicle();
    }

    PlayerHandle GetLocalPlayer() noexcept
    {
        if (!IsInitialized())
            return nullptr;
        auto* vehicle = FindPlayerVehicle();
        return vehicle ? vehicle->mPlayer : nullptr;
    }
}

namespace NFSMW::Vehicle
{
    bool IsValid(VehicleHandle vehicle) noexcept
    {
        return IsInitialized() && AsVehicle(vehicle) != nullptr;
    }

    bool GetSnapshot(VehicleHandle handle, VehicleSnapshot& snapshot) noexcept
    {
        auto* vehicle = AsVehicle(handle);
        if (!vehicle)
            return false;

        snapshot = {};
        snapshot.vehicle = vehicle;
        snapshot.player = vehicle->mPlayer;
        snapshot.vehicleName = vehicle->GetVehicleName();
        snapshot.speedMetresPerSecond = vehicle->GetSpeed();
        snapshot.speedometer = vehicle->GetSpeedometer();

        const auto& position = vehicle->GetPosition();
        snapshot.position = {position.x, position.y, position.z};

        if (vehicle->mDamage)
            snapshot.health = vehicle->mDamage->GetHealth();

        if (vehicle->mEngine)
        {
            snapshot.hasNitrous = vehicle->mEngine->HasNOS();
            snapshot.nitrousEngaged = vehicle->mEngine->IsNOSEngaged();
            snapshot.nitrousCapacity = vehicle->mEngine->GetNOSCapacity();
        }

        return std::isfinite(snapshot.speedMetresPerSecond) &&
               std::isfinite(snapshot.position.x) &&
               std::isfinite(snapshot.position.y) &&
               std::isfinite(snapshot.position.z);
    }

    bool GetLocalSnapshot(VehicleSnapshot& snapshot) noexcept
    {
        return GetSnapshot(Player::GetVehicle(), snapshot);
    }

    bool Repair(VehicleHandle handle) noexcept
    {
        auto* vehicle = AsVehicle(handle);
        if (!vehicle || !vehicle->mDamage)
            return false;
        vehicle->mDamage->ResetDamage();
        return true;
    }

    bool ChargeNitrous(VehicleHandle handle, float amount) noexcept
    {
        auto* vehicle = AsVehicle(handle);
        if (!vehicle || !vehicle->mEngine || !std::isfinite(amount) || amount <= 0.0f)
            return false;
        vehicle->mEngine->ChargeNOS(amount);
        return true;
    }

    bool SetInfiniteNitrous(bool enabled) noexcept
    {
        if (!IsInitialized())
            return false;
        return Memory::Write<bool>(0x00937804u, enabled);
    }

    bool IsInfiniteNitrousEnabled() noexcept
    {
        if (!IsInitialized())
            return false;
        bool enabled{};
        return Memory::Read<bool>(0x00937804u, enabled) && enabled;
    }
}
