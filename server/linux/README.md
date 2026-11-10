# Hosting an LBPPC server on Linux

Two programs make an LBPPC server:

- **Refresh**: LittleBigPlanet's levels, profiles and matchmaking. The game talks to it over HTTPS, through a
  reverse proxy (Nginx Proxy Manager or plain nginx) that holds the certificate.
- **shadNet**: sign-in, friends and the signaling that connects players for multiplayer. Its own TCP and UDP
  ports, not through the proxy.

Gameplay itself is peer to peer between the players; it does not pass through the server.

Tested target: Ubuntu 24.04 / Linux Mint 22 (glibc 2.38 or newer). Check with `ldd --version`.

Copy this folder (`server/linux`) to the server first; the commands below use it as `$KIT`:

```bash
KIT=~/lbppc-server        # where you copied server/linux
```

## 1. Packages and a user

```bash
sudo apt update
sudo apt install -y postgresql libicu74 unzip curl python3-venv
sudo useradd --system --create-home --home-dir /opt/lbppc --shell /usr/sbin/nologin lbppc
```

## 2. Database

```bash
sudo -u postgres psql -c "CREATE USER refresh WITH PASSWORD 'pick-a-password';"
sudo -u postgres psql -c "CREATE DATABASE refresh OWNER refresh;"
```

Put the same password in `$KIT/refresh/db.json` in place of `CHANGE_ME`.

Check the database's port with `pg_lsclusters`: it is 5432 unless something else (another PostgreSQL, a Supabase
stack in Docker) already used that port at install time, and then usually 5433. Put that port in `db.json` too;
on the wrong one Refresh reaches the other database and stops with `password authentication failed`.

Write `db.json` with a password of letters and digits only (no `;`, `"` or `\`).

## 3. Refresh

```bash
cd /tmp
curl -LO https://github.com/LittleBigRefresh/Refresh/releases/download/v3.7.2/Refresh_for_Linux_x64.zip
sudo -u lbppc mkdir -p /opt/lbppc/refresh
sudo unzip -o Refresh_for_Linux_x64.zip -d /opt/lbppc/refresh
sudo cp $KIT/refresh/*.json /opt/lbppc/refresh/
sudo chmod +x /opt/lbppc/refresh/Refresh.GameServer
sudo chown -R lbppc:lbppc /opt/lbppc/refresh
```

play.lbppc.com runs Refresh built from source instead (commit `4060ec4`, in `/opt/refresh-src` on the VPS) with
two changes: `refresh/lbppc-refresh.patch` (archived levels' files fetched from the archive's mirrors, and level
and user lists that stay fast with the archive's millions of levels) and `refresh/lbppc-refresh-asset-types.patch` (those files are served only when
they are a type the game itself shares, the rule `BlockedAssetFlags` applies to uploads). Apply them in that order.

The configuration here keeps the digest key LBPPC's game is patched to (`CustomServerDigest`), and uses
`https://play.lbppc.com` as the public address: change `ExternalUrl`, `WebExternalUrl` and `GameConfigStorageUrl`
for another domain. Refresh listens on port 10061 on every interface so the proxy can reach it; the router does
not forward that port.

## 4. shadNet

The shadNet build needs Qt 6.10, newer than the distribution's, so Qt is installed beside it:

```bash
python3 -m venv /tmp/aqt && /tmp/aqt/bin/pip install aqtinstall
sudo /tmp/aqt/bin/aqt install-qt linux desktop 6.10.0 linux_gcc_64 -m qthttpserver qtwebsockets -O /opt/lbppc/qt
cd /tmp
curl -LO https://github.com/shadps4-emu/shadNet/releases/download/pre-release-shadnet-2026-11-03-1c64b8c8d17c488ddc98308d968e3fa77a9aa65c/shadnet-linux64-2026-11-03-1c64b8c.zip
unzip -o shadnet-linux64-2026-11-03-1c64b8c.zip && tar xzf shadnet-linux64.tar.gz
sudo mkdir -p /opt/lbppc/shadnet
sudo cp shadnet /opt/lbppc/shadnet/ && sudo chmod +x /opt/lbppc/shadnet/shadnet
sudo cp $KIT/shadnet/*.cfg /opt/lbppc/shadnet/
sudo chown -R lbppc:lbppc /opt/lbppc/shadnet /opt/lbppc/qt
```

`shadnet.cfg` listens on every interface (`Host=0.0.0.0`); its admin API stays on 127.0.0.1.

The release build above lacks LBPPC's two changes (`shadnet/patches/lbppc-shadnet.patch`): a relay for players
who can't reach each other directly (two players behind one router, or a router without UPnP), and LBP3 players
counting as being in the same game, which the in-game friends list needs. Without them co-op works only between
players who can connect directly, and the friends list stays empty. To have them, build shadNet from source at
commit 75edbf0 with the patch applied (`git apply`), and use that `shadnet` binary instead. UDP 31314 must be open
for the relay.

## 5. Services

```bash
sudo cp $KIT/systemd/*.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now refresh shadnet
systemctl status refresh shadnet          # both "active (running)"
journalctl -u refresh -f                  # the logs (Ctrl+C to stop)
curl http://localhost:10061/api/v3/instance
```

## 6. Network

| Port | Protocol | To | What |
|---|---|---|---|
| 80, 443 | TCP | the proxy | Refresh over HTTPS, and the certificate's renewal |
| 31313 | TCP | this machine | shadNet |
| 31314 | UDP | this machine | shadNet matching |
| 31315 | TCP | this machine | shadNet web API |

Forward them on the router, and open them in the firewall if it is on (`sudo ufw allow 31313/tcp`,
`31314/udp`, `31315/tcp`).

In Nginx Proxy Manager, the proxy host for `play.lbppc.com` forwards to `http`, this machine's **LAN address**
(not `localhost` when the proxy runs in Docker) and port `10061`, with Websockets Support on.

## 7. Accounts

From a PC with LBPPC (the same online name on both servers):

```bash
python tools/shadnet_account.py <name> --server play.lbppc.com:31313
python tools/refresh_account.py <name> --server https://play.lbppc.com
```

In the launcher: Play online, server `https://play.lbppc.com`, shadNet server `play.lbppc.com:31313`.

## Backups

The data is the PostgreSQL database and the folders under `/opt/lbppc` (uploaded levels and assets in
`refresh/`, shadNet's accounts in `shadnet/db/`):

```bash
sudo -u postgres pg_dump refresh > refresh-$(date +%F).sql
sudo tar czf lbppc-data-$(date +%F).tar.gz /opt/lbppc/refresh /opt/lbppc/shadnet
```
