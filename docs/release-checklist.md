# Release checklist

Before tagging a release.

## Temporary diagnostics to switch off

| What | Where | Why it's there |
|---|---|---|
| `LBPPC_PROFILE` on by default ("Profile:" and "Threads:" lines) | `run.py`, `env.setdefault('LBPPC_PROFILE', '1')` | Performance work. Measured as free, but makes logs larger. |
| `BB_TOGGLE_FILE` (`user/toggles.txt`) | `run.py` | Climbing teleport hunt: optimizations switched off at run time. |
| Friends log ("Friends (menu):" lines every 5 s, online only) | `src/probe.c`, `friend_log_install()` | Friends menu investigation. Remove once the menu lists friends. |

Keep:

- The "Sync:" lines (`src/probe.c`). `docs/troubleshooting.md` tells players to send them when the game diverges.
- `BB_FRAME_STATS`, the 5-second performance lines `docs/troubleshooting.md` explains.

## Tests

- [ ] A clean folder: unzip the release zip somewhere new, first start, prepare, play to the Pod.
- [ ] Quit Game from the pause menu, twice, including right after a save.
- [ ] Online co-op at 16:9 and with one player at ultrawide; a level start right after leaving another.
- [ ] The in-game friends list with a friend online.
- [ ] Make a report: the zip has the logs and no password or user name.

## Publish

- [ ] `VERSION` in `launcher/lbppc_launcher.py` and the status line in `README.md`.
- [ ] Release notes in `docs/releases/<version>.md`.
- [ ] `bash packaging/windows/package.sh`, then the GitHub release with the zip and the notes.
- [ ] The website: download link and version (`lbppc-site`), and deploy.
- [ ] Announce on the Discord.
