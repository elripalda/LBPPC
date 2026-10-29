# LBPPC

LittleBigPlanet 3 running natively on PC, with community servers so creating and sharing levels can live on.

**Status:** early research. Nothing runs yet.

## How it will work

The plan follows the Bloodborne PC port: the game's own PS4 code runs natively on the PC's CPU, a runtime stands in for the PS4 system libraries, and a GPU layer derived from shadPS4 translates its graphics to Vulkan. Online play goes to community servers instead of the original ones, which are gone.

## You need your own copy of the game

This repository contains no game files and never will. To play, you dump LittleBigPlanet 3 from your own PS4. Please don't open issues or pull requests containing game files, and don't ask for them.

## Layout

| Folder | What goes there |
|---|---|
| `runtime/` | Loads and runs the game's code; stands in for the PS4 system libraries |
| `gpu/` | The graphics layer: PS4 GPU commands and shaders to Vulkan |
| `launcher/` | The Windows launcher: setup, settings, updates, online sign-in |
| `tools/` | Development tools: dumping helpers, log and file inspectors |
| `docs/` | Research notes and design documents |

## Licence

GPL-3.0-or-later; see [LICENSE](LICENSE). Code taken from shadPS4 and the Bloodborne PC port keeps its own notices.

This is an unofficial fan project, not affiliated with or endorsed by Sony Interactive Entertainment or Sumo Digital. LittleBigPlanet is a trademark of its owners.
