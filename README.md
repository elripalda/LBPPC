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

## How it will work

The plan follows the Bloodborne PC port: the game's own PS4 code runs natively on the PC's CPU, a runtime stands in for the PS4 system libraries, and a GPU layer derived from shadPS4 translates its graphics to Vulkan. Online play goes to community servers instead of the original ones, which are gone.

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

The runtime and graphics layer come from the [Bloodborne PC port](https://github.com/Supermedo/bloodborne_pc) 1.9.5, merged with its history (remote `bloodborne`) so its fixes can be merged in later. Build on Windows with MSYS2 CLANG64: `./build.sh` (packages in `packaging/windows/README.md`).

## Licence

GPL-3.0-or-later; see [LICENSE](LICENSE). Code taken from shadPS4 and the Bloodborne PC port keeps its own notices.

This is an unofficial fan project, not affiliated with or endorsed by Sony Interactive Entertainment or Sumo Digital. LittleBigPlanet is a trademark of its owners.
