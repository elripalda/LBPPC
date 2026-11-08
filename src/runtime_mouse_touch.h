/* PC-02A: the mouse as the DualShock 4's touchpad (input.ini: mouse_to_joystick = touchpad, F7
 * to capture). LBP3 has its own touch Create Mode: one finger moves the Popit cursor or the
 * picked-up object, pushing the touchpad in selects (and opens the Popit), two fingers spread or
 * pinch to resize and twist to rotate; sticker painting is drawn with a finger. The mouse drives
 * a virtual finger, so all of that is the game's own code, nothing fabricated:
 *   move     one finger slides (lifted after a moment still; put down again at the opposite edge
 *            when it would leave the pad, so a long drag gets the pad's whole width or height
 *            between the pauses a new touch makes: the game's touch input is relative, so
 *            nothing jumps)
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
#define MOUSE_TOUCH_RELIFT_US 50000   /* a finger lifted at the edge stays up this long: longer than a game
                                         frame, so a game reading the pad more often than it draws sees it */

typedef struct {
    float x, y;            /* the finger, while down */
    int down;
    uint8_t id;            /* the touch's number: a new one for each touch, as the DualShock 4 counts them */
    int relift;            /* lifted at the edge: down at relift_x/y once relift_us has passed */
    float relift_x, relift_y;
    uint64_t relift_us;
    uint64_t last_us, moved_us;
    int gesture;           /* 0 none, 1 pinch, 2 twist */
    float spacing, spacing_target, angle, angle_target;
    uint64_t gesture_us;   /* last wheel step or gesture movement */
} MouseTouch;

typedef struct {
    int count;             /* fingers down: 0..2 */
    uint16_t x[2], y[2];
    uint8_t id[2];         /* each finger's touch number (0..127) */
    int click;             /* the touchpad pushed in */
} MouseTouchOut;

void mouse_touch_reset(MouseTouch *t);
/* One pad sample: mouse motion since the last (counts), whether the left button is held, wheel
 * steps since the last (+ up, - down) and whether they twist (ctrl held). */
void mouse_touch_update(MouseTouch *t, float dx, float dy, float scale, int click, int wheel_steps, int twist,
                        uint64_t now_us, MouseTouchOut *out);

#endif
