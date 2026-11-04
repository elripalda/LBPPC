/* PC-02A: the mouse as the DualShock 4's touchpad (input.ini: mouse_to_joystick = touchpad, F7
 * to capture). LBP3 has its own touch Create Mode: one finger moves the Popit cursor or the
 * picked-up object, pushing the touchpad in selects (and opens the Popit), two fingers spread or
 * pinch to resize and twist to rotate; sticker painting is drawn with a finger. The mouse drives
 * a virtual finger, so all of that is the game's own code, nothing fabricated:
 *   move     one finger slides (lifted after a moment still, put down again at the centre when
 *            it would leave the pad: the game's touch input is relative, so nothing jumps)
 *   left     the touchpad pushed in, while held
 *   wheel    a two-finger spread (up) or pinch (down)
 *   ctrl+wheel  a two-finger twist, counterclockwise (up) or clockwise (down)
 * Touchpad units: x 0..1919, y 0..942 (scePadGetControllerInformation's resolution). */
#ifndef BB_RUNTIME_MOUSE_TOUCH_H
#define BB_RUNTIME_MOUSE_TOUCH_H
#include <stdint.h>

#define MOUSE_TOUCH_W 1920
#define MOUSE_TOUCH_H 943
#define MOUSE_TOUCH_SCALE 1.5f      /* touchpad units per mouse count, times mouse_movement_params' speed */
#define MOUSE_TOUCH_IDLE_US 300000  /* a still finger is lifted after this */
#define MOUSE_TOUCH_PINCH_STEP 90.0f /* finger spacing per wheel step */
#define MOUSE_TOUCH_TWIST_STEP 0.2618f /* 15 degrees per wheel step */
#define MOUSE_TOUCH_GESTURE_US 200000 /* a gesture ends this long after its last step is done */

typedef struct {
    float x, y;            /* the finger, while down */
    int down;
    int relift;            /* lifted at the edge this sample: down at the centre next time */
    uint64_t last_us, moved_us;
    int gesture;           /* 0 none, 1 pinch, 2 twist */
    float spacing, spacing_target, angle, angle_target;
    uint64_t gesture_us;   /* last wheel step or gesture movement */
} MouseTouch;

typedef struct {
    int count;             /* fingers down: 0..2 */
    uint16_t x[2], y[2];
    int click;             /* the touchpad pushed in */
} MouseTouchOut;

void mouse_touch_reset(MouseTouch *t);
/* One pad sample: mouse motion since the last (counts), whether the left button is held, wheel
 * steps since the last (+ up, - down) and whether they twist (ctrl held). */
void mouse_touch_update(MouseTouch *t, float dx, float dy, float scale, int click, int wheel_steps, int twist,
                        uint64_t now_us, MouseTouchOut *out);

#endif
