# Input research: motion, touch and Create Mode

Notes from 4 November 2026, for the design spec's PC-01, PC-02 and PC-03 (`LBP3_PC_Port_Implementation_Spec.md`
sections 3.2 and 5). Each finding says how it was established. The tracker is `docs/feature-status.md`.

## Where input enters the game

The game reads the DualShock 4 only through libScePad, and only through seven functions: `scePadInit`, `scePadOpen`,
`scePadClose`, `scePadReadState`, `scePadGetControllerInformation`, `scePadSetVibration` and `scePadSetLightBar`
(import table of the 1.28 eboot, `out/analysis.json`, named with `src/import_names.inc`). It never calls
`scePadSetMotionSensorState`, `scePadResetOrientation` or the tilt-correction calls, so motion is always on.

All of it arrives in one structure per sample, `ScePadData` (`src/runtime_pad.c`, `PadData`): buttons, sticks,
triggers, the motion fields (orientation, acceleration, angular velocity), up to two touches. Everything this port
does for input ends up as fields of that structure:

| Host input | Becomes | Where |
|---|---|---|
| Keyboard, controller buttons and sticks, mouse buttons | buttons, sticks, triggers, through `input.ini` | `runtime_input_config.c`, `runtime_pad.c` |
| Mouse motion (`mouse_to_joystick = left/right`) | a stick | `runtime_pad.c` `mouse_to_axis` |
| Mouse (`mouse_to_joystick = touchpad`) | touches and the touchpad press | `runtime_mouse_touch.c` |
| Controller touchpad | touches | `runtime_pad.c` |
| Controller gyro and accelerometer | the motion fields | `runtime_motion.c` |
| `tilt_*` bindings (numpad 4/6/8/2 by default) | the motion fields (virtual tilt) | `runtime_motion.c` |

The game also imports libSceMove and libSceMoveTracker (PlayStation Move) and libSceCamera; we answer "none
connected" (`src/runtime_extra.c`). It has no keyboard or mouse library imports, but its scripts still carry
`MouseConnected`, `MousePosDelta`, `MouseWorldPos` and `KeyboardConnected` fields (strings in the eboot), likely
from the engine's PC development builds. Nothing on PS4 fills them, as far as we know.

## Motion (PC-03)

**Sackboy's head follows the acceleration field, not the orientation.** This was tested in game (prologue level,
frame dumps of Sackboy):

1. Tilting the virtual pad (`BB_PAD_FILE` token `tilt=roll,pitch`): the head leaned for roll right and roll left, and
   looked up or down for pitch. It went back to normal when the tilt was released.
2. The same, changing only the orientation (acceleration held level): no visible change.
3. The same, changing only the acceleration (orientation held at identity): the head moved as in 1.

Gravity in the acceleration is absolute, so no reference posture is needed; the PS3's Sixaxis worked the same way.
The port fills all three fields consistently anyway, in case other code reads the orientation or angular velocity.

Implementation (`src/runtime_motion.c`, tests `tests/test_motion.c`):

- **A controller with sensors** (DualShock 4, DualSense, Switch Pro, anything SDL3 reports a gyro and accelerometer
  for): the raw accelerometer in G, the gyro with its offset removed, and an orientation from a Mahony filter
  (gyro integration, leaning toward the measured gravity).
- **Idle:** the gyro offset is learned while the pad lies still, and rates below 0.008 rad/s are reported as zero.
  On a real DualSense left on a desk for 30 seconds, the reported rotation was exactly zero throughout and the
  orientation did not change (`idle_real.c`, a scratch test outside the repo). In the unit test, a minute idle with
  a typical offset stays level within 1 degree; yaw (which gravity cannot correct) stays within about 2 degrees.
- **Without sensors** (Xbox and other XInput pads, keyboard): the `tilt_left`, `tilt_right`, `tilt_forward` and
  `tilt_back` outputs. Keys tilt fully (35 degrees), half sticks proportionally, eased at 4 rad/s. While the virtual tilt
  is away from level it replaces the sensors' tilt.

Still open:

- **Direction on a real pad.** The virtual tilt and SDL's sensor axes agree with each other, and SDL's axes are meant
  to match the DualShock 4's. A person tilting a real pad should check that the head leans the same way as on a PS4:
  tilt right, head right; tilt the front up, Sackboy looks up.
- **Other motion uses.** The LBP2 Move Pack levels and Sixaxis-style logic (the Controlinator's tilt outputs, the
  `Tilt__b` script function) were not tried.
- **Pads without sensors** have no default controller binding for tilt (every button is taken). Players can bind
  one in `input.ini`, for example `tilt_left = axis_right_x_minus`.

## Create Mode with a mouse (PC-02A feasibility)

**LBP3 has a touch Create Mode, and it is the narrowest working route for the mouse.** The game's own tip text,
extracted from `base_001.farc` (UTF-16 strings), describes it:

> Did you know you can use the touchpad on your controller to manipulate things in Create mode? It's easy! Push in
> your touchpad to open up the Popit Menu, slide your finger up to highlight the Goodies Bag, and push in the touchpad
> again. Now browse the Goodies Bag with the power of touch until you highlight the object or material you want. Push
> in the touchpad to make your choice. Now you can use one finger to move the object around your create space. You
> can also use two fingers: spread them apart to increase the object's size, pinch them closer together to shrink
> it, or twist them to spin it.

Sticker painting uses the touchpad too ("Move your finger across the touch pad to move your paint cursor across the
canvas. Push down on the touch pad to start painting"). The eboot has `GetTouchCreateEnabled`,
`GetTouchCreateCursorMode` and `SetTouchCreateCursorMode` script functions.

**Tested in game:** pushing the touchpad in the prologue level opened the Popit, and a finger sliding right moved the
highlight from Customize Character to Stickers & Decorations (`BB_PAD_FILE` token `finger=x,y`, frame dumps). The
game navigates from the touch position we report.

**Implemented** (`src/runtime_mouse_touch.c`, tests `tests/test_mouse_touch.c`), with `mouse_to_joystick =
touchpad` in `input.ini` and F7 to capture the mouse:

- **Move:** one finger slides. It goes down at the pad's centre and lifts after 0.3 seconds still; near an edge it
  lifts and goes down at the centre again. Touch input is relative, so nothing jumps.
- **Left button:** the touchpad pushed in.
- **Wheel:** two fingers spread (up) or pinch (down), the size gesture.
- **Ctrl + wheel:** two fingers twist (rotate).

The left button and the wheel are taken from their usual bindings while this mode is on. Every operation is still
the game's own (selection, transforms, undo), so saved levels cannot differ from controller editing. That's the
design spec's requirement for PC-02.

Not yet tested in a real Create session: picking up and moving an object, resizing and rotating with the wheel,
sticker painting, and how fast the cursor feels (`mouse_movement_params`' speed scales it).

### Other routes considered

- **Emulating a PlayStation Move controller.** LBP3 on PS4 carries the LBP2 Move Pack: Move levels, Move
  tutorials (calibrate, navigate), a Move cursor that points at things and picks them up (tip text: "A CURSOR ...
  will let you use your MOTION CONTROLLER to point at things, pick them up, fling them round"), and the script
  functions `GetMoveControllerScreenPos`, `IsUsingMoveController` and `GetMoveCursorStep`. A mouse posing as a
  Move pointer would give absolute point-and-drag. It needs a working Move pad, the Move Tracker's state and
  probably a camera that looks attached, all faked consistently, plus the layouts of `SceMoveData` and the
  tracker state. That's a much bigger and riskier job. It is worth a prototype only if touch Create turns out too
  limited (for example, no direct object picking).
- **Writing the cursor position into game memory.** Invasive and fragile across patches; the design spec says to
  avoid it while an existing interaction surface exists, and one does.

## Keyboard and mouse gameplay (PC-01): where it stands

Already there before this work: remappable keyboard, controller and mouse bindings (`input.ini`, shadPS4 syntax,
F8 reloads), the mouse as a stick, combos, SOCD handling, controller and keyboard mixed (MIX-001). Missing for the
spec's PC-01:

- **Keyboard button prompts in the game's UI** (it draws DualShock 4 icons). That needs the game's icon or
  texture lookups found, which is reverse engineering not started yet.
- **Alt-tab and focus:** the window thread already drops captured input when focus is lost. A check for stuck keys
  after alt-tab should be added to the regression list.

## How these were tested

- `out/tiltplay.sh` and `out/touchplay.sh` (scratch scripts, not in the repo) drive `run.py` with `BB_PAD_FILE` into
  the prologue level and take frame dumps (`BB_FRAME_DUMP_TRIGGER`).
- Unit tests: `out/motion-test`, `out/mouse-touch-test`; `build.sh --test` builds them on Linux. On Windows (MSYS2),
  build.sh has no test step. The pad and input-config tests' unit sections pass there, but their end-to-end
  parts use `/tmp` and `setenv`, which are Linux-only and were already not runnable on Windows.
