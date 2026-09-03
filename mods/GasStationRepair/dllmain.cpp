#include <NFSMW/SDK.hpp>

#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <vector>

namespace
{
    constexpr std::uintptr_t DrawGuiCall = 0x006E75A7u;
    constexpr std::uintptr_t D3DDevicePointer = 0x00982BDCu;
    constexpr std::uintptr_t ViewMatrix = 0x009842D0u;
    constexpr std::uintptr_t ViewProjectionMatrix = 0x00984350u;
    constexpr std::array<std::uint8_t, 5> ExpectedDrawGuiCall{0xE8, 0x94, 0xF8, 0xFF, 0xFF};

    struct RepairZone
    {
        NFSMW::Vector3 position{};
        float radius{12.0f};
        bool playerInside{};
    };

    struct MarkerVertex
    {
        float x{}, y{}, z{};
        D3DCOLOR color{};
        float u{}, v{};
    };

    using DrawGui = void(__cdecl*)(bool);

    HMODULE g_module{};
    wchar_t g_iniPath[MAX_PATH]{};
    wchar_t g_texturePath[MAX_PATH]{};
    std::vector<RepairZone> g_zones;
    DrawGui g_originalDrawGui{};
    IDirect3DTexture9* g_wrenchTexture{};
    float g_markerHeight{4.0f};
    float g_markerSize{3.0f};
    float g_animationSpeed{1.0f};
    float g_bobHeight{0.35f};
    float g_pulseAmount{0.06f};
    DWORD g_repairCooldownMs{1000u};
    bool g_textureLoadAttempted{};

    int ClampInt(int value, int minimum, int maximum) noexcept
    {
        if (value < minimum)
            return minimum;
        if (value > maximum)
            return maximum;
        return value;
    }

    float AtLeast(float value, float minimum) noexcept
    {
        return value < minimum ? minimum : value;
    }

    float ReadFloat(const wchar_t* section, const wchar_t* key, float fallback) noexcept
    {
        wchar_t fallbackText[32]{};
        wchar_t value[64]{};
        swprintf_s(fallbackText, L"%.3f", fallback);
        GetPrivateProfileStringW(section, key, fallbackText, value,
            static_cast<DWORD>(std::size(value)), g_iniPath);
        wchar_t* end{};
        const float parsed = std::wcstof(value, &end);
        return end != value && std::isfinite(parsed) ? parsed : fallback;
    }

    bool BuildPaths() noexcept
    {
        if (!GetModuleFileNameW(g_module, g_iniPath, MAX_PATH))
            return false;
        wchar_t* const fileName = std::wcsrchr(g_iniPath, L'\\');
        if (!fileName)
            return false;

        const std::size_t prefixLength = static_cast<std::size_t>(fileName + 1 - g_iniPath);
        const std::size_t remaining = MAX_PATH - prefixLength;
        if (wcscpy_s(fileName + 1, remaining, L"GasStationRepair.ini") != 0)
            return false;

        if (wmemcpy_s(g_texturePath, MAX_PATH, g_iniPath, prefixLength) != 0)
            return false;
        g_texturePath[prefixLength] = L'\0';
        return wcscpy_s(g_texturePath + prefixLength, MAX_PATH - prefixLength,
            L"repair_wrench.png") == 0;
    }

    void LoadZones()
    {
        const int count = ClampInt(static_cast<int>(
            GetPrivateProfileIntW(L"Settings", L"ZoneCount", 0, g_iniPath)), 0, 64);
        const float defaultRadius = AtLeast(
            ReadFloat(L"Settings", L"DefaultRadius", 12.0f), 1.0f);
        g_markerHeight = ReadFloat(L"Settings", L"MarkerHeight", 4.0f);
        g_markerSize = AtLeast(ReadFloat(L"Settings", L"MarkerSize", 3.0f), 0.25f);
        g_animationSpeed = AtLeast(
            ReadFloat(L"Animation", L"Speed", 1.0f), 0.05f);
        g_bobHeight = AtLeast(
            ReadFloat(L"Animation", L"BobHeight", 0.35f), 0.0f);
        g_pulseAmount = AtLeast(
            ReadFloat(L"Animation", L"PulseAmount", 0.06f), 0.0f);
        g_repairCooldownMs = static_cast<DWORD>(AtLeast(ReadFloat(
            L"Settings", L"RepairCooldownSeconds", 1.0f), 0.1f) * 1000.0f);

        g_zones.clear();
        g_zones.reserve(static_cast<std::size_t>(count));
        for (int index = 1; index <= count; ++index)
        {
            wchar_t section[32]{};
            swprintf_s(section, L"Zone%d", index);
            if (!GetPrivateProfileIntW(section, L"Enabled", 1, g_iniPath))
                continue;

            RepairZone zone{};
            zone.position.x = ReadFloat(section, L"X", 0.0f);
            zone.position.y = -ReadFloat(section, L"Y", 0.0f); // Hot Position stores -Y.
            zone.position.z = ReadFloat(section, L"Z", 0.0f);
            zone.radius = AtLeast(ReadFloat(section, L"Radius", defaultRadius), 1.0f);
            g_zones.push_back(zone);
        }
    }

    bool EnsureTexture(IDirect3DDevice9* device) noexcept
    {
        if (g_wrenchTexture)
            return true;
        if (g_textureLoadAttempted || !device)
            return false;

        g_textureLoadAttempted = true;
        const HRESULT result = D3DXCreateTextureFromFileExW(device, g_texturePath,
            D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT, 0,
            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_FILTER_LINEAR,
            D3DX_FILTER_LINEAR, 0, nullptr, nullptr, &g_wrenchTexture);
        if (SUCCEEDED(result) && g_wrenchTexture)
        {
            NFSMW::Log("Custom blue wrench texture loaded");
            return true;
        }

        NFSMW::Log("Failed to load repair_wrench.png (HRESULT=%08X)",
            static_cast<unsigned int>(result));
        return false;
    }

    void DrawCustomMarkers() noexcept
    {
        IDirect3DDevice9* device{};
        if (!NFSMW::Memory::Read(NFSMW::Memory::FromIda(D3DDevicePointer), device) ||
            !device || !EnsureTexture(device))
            return;

        auto* const view = reinterpret_cast<const D3DXMATRIX*>(NFSMW::Memory::FromIda(ViewMatrix));
        auto* const viewProjection = reinterpret_cast<const D3DXMATRIX*>(
            NFSMW::Memory::FromIda(ViewProjectionMatrix));
        if (!NFSMW::Memory::IsReadable(view, sizeof(D3DXMATRIX)) ||
            !NFSMW::Memory::IsReadable(viewProjection, sizeof(D3DXMATRIX)))
            return;

        D3DXMATRIX inverseView{};
        D3DXMATRIX projection{};
        if (!D3DXMatrixInverse(&inverseView, nullptr, view))
            return;
        D3DXMatrixMultiply(&projection, &inverseView, viewProjection);

        IDirect3DStateBlock9* stateBlock{};
        if (FAILED(device->CreateStateBlock(D3DSBT_ALL, &stateBlock)) || !stateBlock)
            return;
        stateBlock->Capture();

        D3DXMATRIX identity{};
        D3DXMatrixIdentity(&identity);
        device->SetTransform(D3DTS_WORLD, &identity);
        device->SetTransform(D3DTS_VIEW, &projection);
        device->SetTransform(D3DTS_PROJECTION, &identity);
        device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1);
        device->SetVertexShader(nullptr);
        device->SetPixelShader(nullptr);
        device->SetTexture(0, g_wrenchTexture);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_ZENABLE, TRUE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);

        // Underground 2-style marker movement: the card continuously spins
        // around its vertical axis, gently floats, and subtly breathes.
        const float time = static_cast<float>(GetTickCount64() % 600000u) *
            0.001f * g_animationSpeed;
        const float bob = std::sin(time * 2.0f) * g_bobHeight;
        const float pulse = 1.0f + std::sin(time * 4.0f) * g_pulseAmount;
        const float halfHeight = g_markerSize * 0.5f * pulse;
        // Keeping the cosine signed mirrors the card after it turns edge-on,
        // which creates a real two-sided vertical-axis spin.
        const float halfWidth = halfHeight * std::cos(time * 1.7f);
        for (const RepairZone& zone : g_zones)
        {
            // Convert the SDK position to MWHealthbars' proven render coordinate system.
            const D3DXVECTOR3 world(zone.position.x, -zone.position.y,
                zone.position.z + g_markerHeight + bob);
            D3DXVECTOR4 center{};
            D3DXVec3Transform(&center, &world, view);
            if (center.z <= 0.1f)
                continue;

            const MarkerVertex vertices[6]{
                {center.x - halfWidth, center.y - halfHeight, center.z, 0xFFFFFFFFu, 0.0f, 1.0f},
                {center.x + halfWidth, center.y - halfHeight, center.z, 0xFFFFFFFFu, 1.0f, 1.0f},
                {center.x + halfWidth, center.y + halfHeight, center.z, 0xFFFFFFFFu, 1.0f, 0.0f},
                {center.x + halfWidth, center.y + halfHeight, center.z, 0xFFFFFFFFu, 1.0f, 0.0f},
                {center.x - halfWidth, center.y + halfHeight, center.z, 0xFFFFFFFFu, 0.0f, 0.0f},
                {center.x - halfWidth, center.y - halfHeight, center.z, 0xFFFFFFFFu, 0.0f, 1.0f}
            };
            device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, vertices, sizeof(MarkerVertex));
        }

        stateBlock->Apply();
        stateBlock->Release();
    }

    void __cdecl DrawGuiHook(bool alternatePass)
    {
        if (!alternatePass)
            DrawCustomMarkers();
        if (g_originalDrawGui)
            g_originalDrawGui(alternatePass);
    }

    bool InstallRenderHook() noexcept
    {
        const std::uintptr_t callAddress = NFSMW::Memory::FromIda(DrawGuiCall);
        std::array<std::uint8_t, 5> current{};
        if (!NFSMW::Memory::Read(callAddress, current) || current != ExpectedDrawGuiCall)
            return false;

        std::int32_t originalDisplacement{};
        std::memcpy(&originalDisplacement, current.data() + 1, sizeof(originalDisplacement));
        g_originalDrawGui = reinterpret_cast<DrawGui>(callAddress + 5u + originalDisplacement);

        std::array<std::uint8_t, 5> patch{0xE8, 0, 0, 0, 0};
        const auto destination = reinterpret_cast<std::uintptr_t>(&DrawGuiHook);
        const auto displacement = static_cast<std::int32_t>(destination - (callAddress + 5u));
        std::memcpy(patch.data() + 1, &displacement, sizeof(displacement));
        return NFSMW::Memory::WriteBytes(reinterpret_cast<void*>(callAddress),
            patch.data(), patch.size());
    }

    bool IsInside(const NFSMW::Vector3& player, const RepairZone& zone) noexcept
    {
        const float dx = player.x - zone.position.x;
        const float dy = player.y - zone.position.y;
        const float dz = player.z - zone.position.z;
        return dx * dx + dy * dy + dz * dz <= zone.radius * zone.radius;
    }

    DWORD WINAPI ModMain(void*)
    {
        const NFSMW::Result result = NFSMW::Initialize(L"GasStationRepair.log");
        if (result != NFSMW::Result::Ok && result != NFSMW::Result::AlreadyInitialized)
            return static_cast<DWORD>(result);
        if (!BuildPaths())
            return ERROR_BAD_PATHNAME;

        LoadZones();
        if (!InstallRenderHook())
        {
            NFSMW::Log("Failed to install the custom marker render hook");
            return ERROR_INVALID_ADDRESS;
        }
        NFSMW::Log("Custom wrench marker renderer installed");

        ULONGLONG nextRepairAttempt{};
        for (;;)
        {
            NFSMW::VehicleSnapshot snapshot{};
            if (!NFSMW::Vehicle::GetLocalSnapshot(snapshot))
            {
                for (RepairZone& zone : g_zones)
                    zone.playerInside = false;
                Sleep(200);
                continue;
            }

            for (RepairZone& zone : g_zones)
            {
                const bool inside = IsInside(snapshot.position, zone);
                if (inside && !zone.playerInside)
                {
                    NFSMW::Log("You are inside a repair zone");
                    if (snapshot.vehicle && NFSMW::Vehicle::Repair(snapshot.vehicle))
                        NFSMW::Log("Repair zone: vehicle repaired");
                    nextRepairAttempt = GetTickCount64() + g_repairCooldownMs;
                }
                else if (inside && snapshot.vehicle &&
                    GetTickCount64() >= nextRepairAttempt)
                {
                    if (NFSMW::Vehicle::Repair(snapshot.vehicle))
                        NFSMW::Log("Repair zone: vehicle repaired");
                    nextRepairAttempt = GetTickCount64() + g_repairCooldownMs;
                }
                zone.playerInside = inside;
            }
            Sleep(100);
        }
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = module;
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, ModMain, nullptr, 0, nullptr))
            CloseHandle(thread);
    }
    return TRUE;
}
