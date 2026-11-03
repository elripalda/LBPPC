# Playing online

LittleBigPlanet 3's servers are closed, so LBPPC plays online on community servers. Two servers work together:

| Server | What it does | Ports |
|---|---|---|
| [Refresh](https://github.com/LittleBigRefresh/Refresh) | LittleBigPlanet itself: profiles, levels, comments, matchmaking | TCP 10061 |
| [shadNet](https://github.com/shadps4-emu/shadNet) | The PSN stand-in: sign-in, friends, and the signaling that connects players for multiplayer | TCP 31313, UDP 31314, TCP 31315 |

Playing alone online needs only Refresh (sign in locally with an online name). Multiplayer needs both.

## Hosting

Run the servers on a machine your players can reach. A private network such as Tailscale is the safest way to share them with friends: nothing is opened to the internet.

### Refresh (3.7.2 or later)

Refresh needs PostgreSQL. After its first start it writes its settings next to it; change these:

- `bunkum.json`: `ListenHost` `0.0.0.0` (it is `127.0.0.1` only for this machine).
- `refreshGameServer.json`:
  - `UseTicketVerification` `false`: LBPPC's login tickets are not signed by Sony (the PS4 game logs in to PSN differently; LBPPC sends a PS3-style ticket for the player).
  - `EnforcePatchwork` `false`: Patchwork is the PS3 game's security plugin; LBPPC has its own join protection (below).
  - `WebExternalUrl` and `GameConfigStorageUrl`: the address players use, such as `http://<host>:10061` and `http://<host>:10061/lbp`.

Accounts: `python tools/refresh_account.py <online name> --server http://<host>:10061` (once per player; the name must match their shadNet account).

### shadNet

After its first start, in `shadnet.cfg`:

- `Host` `0.0.0.0`.
- `Matching2Enabled` `true`: the UDP signaling that connects players' games (off by default).

Copy `worlds.cfg` and `scoreboards.cfg` from the shadNet repository next to `shadnet.exe`.

Accounts: `python tools/shadnet_account.py <online name> --server <host>:31313` (prints the password).

## Playing

In the launcher, tick **Play online** and fill in:

- **Online name**: the player's account name.
- **Server**: Refresh's address, `http://<host>:10061`.
- **shadNet password** and **shadNet server** (`<host>:31313`) for multiplayer.
- **Who may join you**: see below.

Or from a shell: `LBPPC_SERVER=http://<host>:10061 BB_SHADNET_SERVER=<host>:31313 BB_SHADNET_NPID=<name> BB_SHADNET_PASSWORD=<password> python run.py <game folder>`.

## Join protection

LittleBigPlanet's community servers were closed by a "force join" exploit: a forged invite made the game accept a stranger into the player's session. LBPPC accepts a connection from another player only if:

- they are your shadNet friend,
- they are named in **Who may join you** (`LBPPC_JOIN_ALLOW`, comma separated), or
- your game connected to them first (you joined them).

Anyone else's connection is dropped before it reaches the game, and the log says `Online: refused a connection from <name>`.

## What LBPPC changes for online play

- The game's server address goes to the configured Refresh server.
- The PS4 game's PSN login (an authorization code) becomes a PS3-style ticket for the player, which custom servers read.
- The key the game signs its requests and checks the server's replies with becomes `CustomServerDigest`, the community servers' key; replies are still checked.
- Refresh disables two endpoints the PS3 game does without (`farc_hashes`, `t_conf`); the PS4 game needs them, so LBPPC answers them with empty, signed replies.
- Without shadNet, PSN itself is stood in for locally: sign-in, empty friend and block lists.
