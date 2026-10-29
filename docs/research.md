# Research notes

First notes, 29 October 2026. Each claim links to the page it came from.

## Where LittleBigPlanet 3 stands on shadPS4

It reaches the menus for the first time, then crashes. On shadPS4 0.20.0 (19 October 2026) the EU version got to the menus and then died with a Vulkan device lost during submit (`vk_scheduler.cpp:241`, "SubmitExecution: Assertion Failed!"); the issue was relabeled "menus" two days later ([#264](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/264)). Before that, every version stopped earlier:

| Report | Game version | shadPS4 | Where it stopped |
|---|---|---|---|
| [#264](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/264) EU CUSA00063 | 1.00 | 0.20.0 | Menus, then device lost (Oct 2026); on 0.9.0 it hung connecting to the official servers |
| [#2202](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/2202) CUSA00693 | 1.28 | 0.13.0 | Crashed after the brightness screen, assertion in `net.cpp` |
| [#285](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/285) US CUSA00473 | 1.28 | 0.7.0 | Hung seconds after boot |
| [#642](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/642) US CUSA00473 | 1.00 | 0.8.0 | Froze on the brightness screen |
| [#86](https://github.com/shadps4-compatibility/shadps4-game-compatibility/issues/86) GB CUSA00762 | 1.00 | 0.6.0 | Froze on the brightness screen, no input or audio |

What this tells us:

- **The game polls the PlayStation Camera constantly.** Older logs repeat the `sceCameraIsAttached` stub, and every report carries the camera and Move labels. Our runtime should answer "no camera, no Move" cleanly from the start.
- **The network library matters early.** One build hung connecting to the official servers, and 1.28 hit an assertion in `net.cpp`. The game should be told "offline" cleanly until we have a server.
- **The GPU crash is the current wall.** A device lost on submit usually means a bad command buffer or resource, the same kind of problem the Bloodborne port worked through.
- **Use version 1.28,** the last patch, as the target: it is what most players have and what any server must speak.

## Official servers

The PS4 servers are offline for good. Sony shut the PS3 and Vita servers in September 2021 and kept LBP3 on PS4 running ([TheSixthAxis](https://www.thesixthaxis.com/2021/09/13/littlebigplanet-3-servers-come-back-online-for-ps4-but-ps3-and-ps-vita-game-servers-to-be-shut-down/)); in April 2024 it said the PS4 servers would stay down indefinitely, leaving levels playable on the console but not shareable ([Checkpoint Gaming](https://checkpointgaming.net/news/2024/04/littlebigplanet-3-servers-on-ps4-will-stay-shut-down-indefinitely-sony-reveals)).

## Community servers

Both existing projects target the PS3 and Vita games, played on modded consoles or RPCS3. Neither page says anything about the PS4 version.

- **Project Lighthouse:** a custom server by the LBP Union's research team, for the PS3 and Vita titles; players need a modded PS3 or RPCS3 and their own copy of the game ([LBP Union wiki](https://wiki.lbpunion.com/index.php/Project_Lighthouse); the wiki was down when checked, details from search summaries).
- **Refresh:** "a second-generation custom server for LittleBigPlanet that focuses on ease of use and reliability", AGPL-3.0, actively developed ([GitHub](https://github.com/LittleBigRefresh/Refresh)). Its docs site now redirects to "Bonsai" ([docs](https://docs.lbpbonsai.com/index_md.html)), which seems to be the umbrella name for Refresh and its libraries.

## Open questions

- Does the PS4 version speak the same server protocol as LBP3 on PS3? If it is close, Refresh could serve us and we inherit the community's archive of levels.
- Would the Refresh or Lighthouse developers support a PS4 client? Ask on their Discord before writing a server of our own.
- Refresh is AGPL-3.0: fine to run and contribute to; any server code we take from it must stay AGPL.
- Which PS4 system libraries does the game import, beyond camera, Move and network? A first look at its import list (from our own dump) answers this.
- What engine does it use, and does anything carry over from the Bloodborne port's patches and code caves?

## Next steps

1. Get the import list from a 1.28 dump and compare it with what the Bloodborne runtime already provides.
2. Bring up the runtime and GPU layer from the Bloodborne port and aim for the title screen, with camera, Move and network answering "absent".
3. Contact the Refresh/Bonsai team about the PS4 protocol.
