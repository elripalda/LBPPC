# What the game imports, against the Bloodborne runtime

Checked 29 October 2026 on CUSA00473 (US), version 1.28, against the Bloodborne port's runtime at 1.9.5 (`bb-195`).

About 81% of the game's system imports are already answered. The eboot imports 646 functions from 52 modules. The Bloodborne runtime answers 373 of them, and the game's own `libc.prx` and `libSceFios2.prx` answer 149 more: the Bloodborne linker (`scripts/link_modules.py`) already links exactly those two modules. That leaves 124 functions, most of which only need to say "not available".

## Already covered

Graphics (GnmDriver 50/50, VideoOut 9/9), video playback (AvPlayer 15/15), audio output, HTTP and SSL, the network and NP libraries (Net, NetCtl, Auth, Commerce, Signaling, Trophy, Utility, WebApi), user service, IME and message dialogs, and nearly all of the kernel (67/75).

## Missing, by how much work each needs

| Work | Library | Missing | What to do |
|---|---|---|---|
| Real code | libSceFiber | 7 | Fibers (cooperative threads); the game's job system likely runs on them |
| Real code | libkernel | 8 | Event flags (create, set, wait), `sceKernelSleep`, `sceKernelLoadStartModule`, 3 unnamed |
| Real code | libScePosix | 11 | POSIX semaphores and `pthread_setschedparam`; sockets (`socket`, `bind`, `sendto`, `recv`, `recvfrom`) for online |
| Real code | libSceJpegDec / JpegEnc | 5 + 4 | Decode and encode JPEGs: level thumbnails and photos |
| Real code | Game's own modules | 5 | Load `gamedata_orbis/spu/*.prx` (four FMOD audio plugins and `spu.prx`) when the game asks through `sceKernelLoadStartModule`; Bloodborne links its modules ahead of time instead |
| Small | libSceAppContent | 4 | Temporary-data mount, format and free space; entitlement key |
| Small | libSceSaveData / SaveDataDialog | 3 + 4 | Backup-data checks; dialog result and progress |
| Small | libScePlayGo | 4 | Report everything installed |
| Small | libSceRtc, Pad, SystemService, Voice | 2, 1, 1, 1 | Tick arithmetic and validity, light bar, safe area, voice port attributes |
| Report absent | libSceCamera, Move, MoveTracker | 9, 5, 6 | No camera, no Move controllers: answer cleanly, as the shadPS4 reports show the game polling these constantly |
| Report absent | GameLiveStreaming, CompanionHttpd, ContentSearch, Coredump, ErrorDialog, GameCustomDataDialog, Ime | 10, 6, 5, 4, 4, 6, 3 | Streaming, the companion app, content search, crash dumps: report "not available" |
| Later | libSceNpManager | 6 (unnamed) | Online sign-in pieces; matter when we have a server |

Bloodborne never needed the Fiber, JPEG, camera, Move or event flag libraries, so those are new code. Most other items exist in upstream shadPS4 and can be ported from there.

## Notes

- 64 of the 646 imports have no public name yet (shown as NIDs in the full list).
- Three Bloodborne exports (`sceKernelGetTscFrequency`, `sceKernelMapNamedDirectMemory`, `sceRtcGetCurrentTick`) exist in the runtime but are missing from its NID-to-name table (`src/import_names.inc`), which only lists what Bloodborne imports. Before LBP3 can use any name-matched export, that table needs every NID this game imports.
- Method: the import tables were read from the eboot with the Bloodborne port's `scripts/prepare.py` parser; names come from upstream shadPS4's library sources and the port's own table.
