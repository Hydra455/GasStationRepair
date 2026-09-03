# Repair Zone Wrench Marker v0.3.2

This version detects the local player's vehicle entering configured repair
zones, repairs it automatically, and draws a blue wrench with a black outline
at every enabled zone.

The wrench uses an Underground 2-inspired world-marker animation: it spins
around its vertical axis, gently floats up and down, and subtly pulses.

The marker is a custom world-space billboard. NFSMW's native `GIcon` table only
accepts built-in game assets and provides no verified runtime color setter, so
the supplied `repair_wrench.png` is already colored blue.

## Build and install

1. Open `GasStationRepair.sln` in Visual Studio 2026.
2. Select `Release` and `x86`, then build `GasStationRepair`.
3. Copy these files beside `speed.exe`:
   - `bin\Release\GasStationRepair.asi`
   - `bin\Release\repair_wrench.png`
   - `mods\GasStationRepair\GasStationRepair.ini`
4. Remove the previous `GasStationRepair.asi` before installing this version.

The texture is copied into `bin\Release` automatically during the build. Keep
it in the same folder as the ASI when installing.

## Configuration

Copy coordinates directly from Extra Options' `HotPositionL2RA.hot` file:

```ini
[Settings]
ZoneCount=1
DefaultRadius=12.0
MarkerHeight=4.0
MarkerSize=3.0
RepairCooldownSeconds=1.0

[Animation]
Speed=1.0
BobHeight=0.35
PulseAmount=0.06

[Zone1]
Enabled=1
X=123.0
Y=456.0
Z=7.0
Radius=12.0
```

Hot Position stores coordinates as `X, -Y, Z`; the plugin performs that
conversion automatically. `MarkerHeight` raises the wrench above the saved
position and `MarkerSize` controls its world-space size. The `Animation`
section controls the spin/hover rate, vertical travel and pulse strength.
`RepairCooldownSeconds` controls how often damage is reset while the vehicle
remains inside a zone.

When the vehicle enters a zone, `GasStationRepair.log` receives:

```text
You are inside a repair zone
Repair zone: vehicle repaired
```

The message is written once per entry. Leave the zone and enter it again to
write another message.

The renderer uses the same verified GUI render-call location used by
MWHealthbars, restores the game's Direct3D state after drawing, and does not
hook or modify mouse/camera input.
