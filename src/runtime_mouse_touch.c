/* The mouse as a virtual finger on the touchpad (runtime_mouse_touch.h). */
#include "runtime_mouse_touch.h"
#include <math.h>
#include <string.h>

#define EDGE 40.0f            /* a finger this close to the pad's edge is lifted and put down again */
#define GESTURE_SPACING 320.0f /* two fingers' starting distance */
#define SPACING_MIN 80.0f
#define SPACING_MAX 900.0f
#define SPACING_RATE 3000.0f  /* units/s the fingers spread or close toward the target */
#define TWIST_RATE 6.0f       /* rad/s */
#define CENTRE_X 960.0f       /* where a finger goes down: the touch clicks' centre (runtime_pad.c) */
#define CENTRE_Y 471.0f

void mouse_touch_reset(MouseTouch *t) { memset(t,0,sizeof(*t)); }

static float clampf(float v, float lo, float hi) { return v<lo ? lo : v>hi ? hi : v; }
static float toward(float v, float target, float step) {
    const float d=target-v;
    return fabsf(d)<=step ? target : v+(d>0.0f ? step : -step);
}

void mouse_touch_update(MouseTouch *t, float dx, float dy, float scale, int click, int wheel_steps, int twist,
                        uint64_t now_us, MouseTouchOut *out) {
    memset(out,0,sizeof(*out));
    const float dt=t->last_us && now_us>t->last_us ? (float)(now_us-t->last_us)*1e-6f : 0.0f;
    t->last_us=now_us;
    const int moved=dx!=0.0f || dy!=0.0f;

    /* Wheel steps start a gesture (or add to the one going on). */
    if (wheel_steps) {
        const int kind=twist ? 2 : 1;
        if (t->gesture!=kind) {
            t->gesture=kind;
            t->spacing=t->spacing_target=GESTURE_SPACING;
            t->angle=t->angle_target=0.0f;
        }
        if (kind==1) t->spacing_target=clampf(t->spacing_target+wheel_steps*MOUSE_TOUCH_PINCH_STEP,SPACING_MIN,SPACING_MAX);
        else t->angle_target+=wheel_steps*MOUSE_TOUCH_TWIST_STEP;
        t->gesture_us=now_us;
        if (!t->down) { t->down=1; t->x=CENTRE_X; t->y=CENTRE_Y; }
    }

    if (t->relift) { /* lifted at the edge last sample: down again at the centre */
        t->relift=0;
        t->down=1; t->x=CENTRE_X; t->y=CENTRE_Y;
    }
    if ((moved || click) && !t->down) { t->down=1; t->x=CENTRE_X; t->y=CENTRE_Y; }
    if (moved) {
        t->x+=dx*scale; t->y+=dy*scale;
        t->moved_us=now_us;
        if (!t->gesture && !click &&
            (t->x<EDGE || t->x>MOUSE_TOUCH_W-1-EDGE || t->y<EDGE || t->y>MOUSE_TOUCH_H-1-EDGE)) {
            t->down=0; t->relift=1; /* this sample: no finger */
            return;
        }
        t->x=clampf(t->x,0.0f,MOUSE_TOUCH_W-1.0f); t->y=clampf(t->y,0.0f,MOUSE_TOUCH_H-1.0f);
    }
    if (click) t->moved_us=now_us;

    if (t->gesture) {
        t->spacing=toward(t->spacing,t->spacing_target,SPACING_RATE*dt);
        t->angle=toward(t->angle,t->angle_target,TWIST_RATE*dt);
        const int done=t->spacing==t->spacing_target && t->angle==t->angle_target;
        if (!done) t->gesture_us=now_us;
        if (done && now_us-t->gesture_us>MOUSE_TOUCH_GESTURE_US) {
            t->gesture=0; t->down=0; /* both fingers lifted */
            return;
        }
        /* Two fingers about the finger's point, kept on the pad. */
        const float r=t->spacing*0.5f, cx=cosf(t->angle)*r, cy=-sinf(t->angle)*r;
        const float mx=clampf(t->x,fabsf(cx),MOUSE_TOUCH_W-1-fabsf(cx)), my=clampf(t->y,fabsf(cy),MOUSE_TOUCH_H-1-fabsf(cy));
        out->count=2;
        out->x[0]=(uint16_t)lroundf(mx-cx); out->y[0]=(uint16_t)lroundf(my-cy);
        out->x[1]=(uint16_t)lroundf(mx+cx); out->y[1]=(uint16_t)lroundf(my+cy);
        out->click=click;
        return;
    }

    if (t->down && !click && now_us-t->moved_us>MOUSE_TOUCH_IDLE_US) t->down=0;
    if (t->down) {
        out->count=1;
        out->x[0]=(uint16_t)lroundf(t->x); out->y[0]=(uint16_t)lroundf(t->y);
    }
    out->click=click;
}
