# LBPPC

LittleBigPlanet 3 running natively on PC, with community servers so creating and sharing levels can live on.

**Status:** 0.0.1 beta. LittleBigPlanet 3 (US, CUSA00473 version 1.28) boots, plays and goes online on community servers:

- 1080p to 4K and ultrawide (the menus lay themselves out for it; movies and loading screens stay 16:9).
- 60 FPS with frame generation (DLSS or FSR 3.1); the game itself steps 30 times a second, as on PS4.
- NVIDIA Reflex / AMD Anti-Lag 2.
- Online on [Refresh](https://github.com/LittleBigRefresh/Refresh) (levels, profiles) with [shadNet](https://github.com/shadps4-emu/shadNet) for sign-in and multiplayer, with a join protection against the exploit that closed LittleBigPlanet's community servers: see [docs/online.md](docs/online.md).

Multiplayer between players is new in this release and only lightly tested.

## Playing

Unzip a release, start `LBPPC.exe`, choose the folder of your game dump and press Play. For online play, see [docs/online.md](docs/online.md).

If something goes wrong, click **Make a report** in the launcher and attach the zip to your bug report; [docs/troubleshooting.md](docs/troubleshooting.md) explains what the log says.

### Controls

A controller works out of the box. The launcher's **Controls** page sets the keys, mouse buttons and controller buttons, the mouse mode and its sensitivity; it writes `input.ini` (next to `LBPPC.exe`), which F8 reloads in game. Lines the page does not edit (combos, dead zones) are kept.

- **Motion:** a controller with motion sensors (DualShock 4, DualSense, Switch Pro) tilts Sackboy's head as on a PS4. Without one, the number pad tilts it (4 left, 6 right, 8 forward, 2 back), or bind `tilt_left`, `tilt_right`, `tilt_forward` and `tilt_back` to anything.
- **Mouse as the touchpad**, for LBP3's touch Create Mode: choose **Touchpad (Create Mode)** under Controls, Mouse, and press F7 in the game to capture the mouse. Moving is a finger on the pad, the left button pushes the pad in (it opens the Popit and selects), the wheel spreads or pinches (resizes) and Ctrl + wheel twists (rotates).

## How it works

As in the Bloodborne PC port, the game's own PS4 code runs natively on the PC's CPU, a runtime stands in for the PS4 system libraries, and a GPU layer derived from shadPS4 translates its graphics to Vulkan. Online play goes to community servers instead of the original ones, which are gone.

## You need your own copy of the game

This repository contains no game files and never will. To play, you dump LittleBigPlanet 3 from your own PS4. Please don't open issues or pull requests containing game files, and don't ask for them.

## Layout

| Folder | What goes there |
|---|---|
| `src/` | The runtime: loads and runs the game's code; stands in for the PS4 system libraries |
| `gpu/` | The graphics layer: PS4 GPU commands and shaders to Vulkan (from shadPS4) |
| `scripts/` | Reads the game's executable and links it with its bundled modules before a start |
| `tools/` | Development tools: import list, GPU capability check |
| `tests/` | Tests of the GPU layer |
| `docs/` | Research notes and design documents |

The runtime and graphics layer come from the [Bloodborne PC port](https://github.com/Supermedo/bloodborne_pc) 1.9.5, merged with its history (remote `bloodborne`); the Bloodborne-only features (its patches, menus, mouse camera, Discord status and online backend) have been removed. Build on Windows with MSYS2 CLANG64: `./build.sh` (packages in `packaging/windows/README.md`).

## Licence

GPL-3.0-or-later; see [LICENSE](LICENSE). Code taken from shadPS4 and the Bloodborne PC port keeps its own notices.

This is an unofficial fan project, not affiliated with or endorsed by Sony Interactive Entertainment or Sumo Digital. LittleBigPlanet is a trademark of its owners.
