# Feature status

The design spec's features (`LBP3_PC_Port_Implementation_Spec.md`) and where each stands. States: Not Started,
Discovery, Prototype, In Progress, Testing, Released, Blocked, Deferred. Nothing is Released until its acceptance
criteria and the release gates pass.

## Client and Create Mode

| ID | Feature | State | Notes |
|---|---|---|---|
| PC-01 | Keyboard and mouse integration | In Progress | Remappable keyboard/controller/mouse (`input.ini`) works in play. The launcher's Controls page edits keys, mouse buttons, controller buttons, mouse mode and sensitivity (`launcher/input_settings.py`, checked against the game's parser by `tests/test_launcher_input.py`). Missing: keyboard prompts in the game's UI, an alt-tab stuck-key check. `docs/input-research.md` |
| PC-02 | Mouse cursor and shortcuts in Create Mode | Prototype | PC-02A: the mouse as the touchpad (`mouse_to_joystick = touchpad`) drives the game's own touch Create Mode. Popit navigation tested in game; object moving, resizing and rotating not yet tested in a Create session. No keyboard shortcuts yet. |
| PC-03 | Motion fixes and substitutes | Testing | Real gyro and accelerometer (SDL3) fill the motion fields, with no idle drift on a real DualSense; `tilt_*` bindings for pads without sensors (numpad by default). Head tilt tested in game. To do: tilt direction on a real pad checked by a person; Move Pack and tilt-logic levels. |
| PC-04 | Desktop display settings | In Progress | Resolution, display mode, frame generation and low latency in the launcher. Known issue: some UI (bottom button prompts) stretched about 1.34x at ultrawide. |
| PC-05 | Mixed local multiplayer and desktop focus | Not Started | One pad (user 1) only. |
| CR-01 to CR-08 | Editor improvements | Not Started | Depend on PC-02. |

## Online and website

| ID | Feature | State | Notes |
|---|---|---|---|
| NET-01 | Community account and identity | In Progress | One LBPPC account (shadNet and Refresh) created from the launcher; server at lbppc.com. |
| NET-02 | Secure multiplayer sessions | In Progress | Invitations arrive and are accepted; joining still fails with "failed to join". Join protection: friends only. LBP3's own Triangle prompt for an invitation (the game custom data dialog, mode 2) now joins it like Cross on ours; untested with two players. The server ping's lookup of Refresh's `CDNHostName` (a URL) now resolves the URL's host. Open: Refresh `/match` 500; sending game custom data (mode 1) still reports success without sending. |
| NET-03 | Level publishing and discovery | Not Started | Refresh supports it; not yet tried from the game. |
| NET-04 | Reporting, blocking, moderation | Not Started | |
| NET-05 | Stats, events, leaderboards | Not Started | |
| NET-06 | Social graph and notifications | Prototype | Friends list in the launcher: requests (sent and received), friends with online status. |
| NET-07 to NET-09 | Play queue, preservation, offline cache | Not Started | |
| WEB-01 to WEB-06 | Website | Not Started | refresh-web not deployed yet. |

## Decision records

**2026-11-04: Create Mode mouse through the touchpad, not Move or memory.** LBP3 ships a touch Create Mode (its own
tip text describes one-finger moving, pushing to select, pinch/spread to resize, twist to rotate). Feeding it a
virtual finger from the mouse uses the game's own interaction code, so selection, undo and saved data cannot differ
from controller editing; Popit navigation was verified in game. Emulating a PlayStation Move pointer (also present
in LBP3) stays the fallback if touch turns out too limited; it would need the Move pad, the Move Tracker and the camera faked
together. Writing to the game's cursor in memory was ruled out as invasive.

**2026-11-04: Motion from the acceleration field, gravity-referenced.** Tested in game: Sackboy's head follows only
the acceleration field (changing only the orientation had no effect). The port reports the raw accelerometer,
learns the gyro offset while still and reports sub-0.008 rad/s rates as zero, so an idle pad gives no motion. All
three motion fields are filled consistently.
