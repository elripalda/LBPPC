/* runtime_mouse_touch.c: the mouse as a virtual finger on the touchpad. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/runtime_mouse_touch.h"

#define MS 1000ull

static float spacing(const MouseTouchOut *o) {
    return hypotf((float)o->x[1]-o->x[0],(float)o->y[1]-o->y[0]);
}

/* Moving puts a finger down at the centre and slides it; still for a moment, it is lifted. */
static void test_slide_and_lift(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    mouse_touch_update(&t,0,0,1.5f,0,0,0,1000*MS,&o);
    assert(o.count==0 && !o.click);
    mouse_touch_update(&t,100,-20,1.5f,0,0,0,1012*MS,&o);
    assert(o.count==1 && o.x[0]==960+150 && o.y[0]==471-30);
    mouse_touch_update(&t,0,0,1.5f,0,0,0,1200*MS,&o);
    assert(o.count==1); /* still down a moment */
    mouse_touch_update(&t,0,0,1.5f,0,0,0,1400*MS,&o);
    assert(o.count==0); /* lifted */
}

/* At the pad's edge the finger is lifted for MOUSE_TOUCH_RELIFT_US (longer than a game frame) and
 * put down just inside the opposite edge, the other axis where it was, as a new touch (a new id):
 * the drag goes on with the whole pad ahead of it. */
static void test_edge_relift(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    mouse_touch_update(&t,400,0,1.5f,0,0,0,1000*MS,&o);
    assert(o.count==1 && o.x[0]==1560);
    const uint8_t first=o.id[0];
    mouse_touch_update(&t,250,0,1.5f,0,0,0,1012*MS,&o); /* 1560 + 375: past the edge margin */
    assert(o.count==0); /* would pass the edge: lifted */
    mouse_touch_update(&t,10,0,1.5f,0,0,0,1030*MS,&o);
    assert(o.count==0); /* still up: a game reading more often than it draws sees the lift */
    mouse_touch_update(&t,10,0,1.5f,0,0,0,1070*MS,&o);
    assert(o.count==1 && o.x[0]==80+15 && o.y[0]==471 && o.id[0]!=first);

    /* Upward: down again near the bottom. */
    mouse_touch_reset(&t);
    mouse_touch_update(&t,0,-200,1.5f,0,0,0,2000*MS,&o);
    assert(o.count==1 && o.y[0]==471-300);
    mouse_touch_update(&t,0,-100,1.5f,0,0,0,2012*MS,&o); /* 171 - 150: past the top margin */
    assert(o.count==0);
    mouse_touch_update(&t,0,-10,1.5f,0,0,0,2070*MS,&o);
    assert(o.count==1 && o.x[0]==960 && o.y[0]==862-15);
}

/* Each new touch has its own number, as on the DualShock 4; one touch keeps it while it moves. */
static void test_touch_ids(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    mouse_touch_update(&t,10,0,1.5f,0,0,0,1000*MS,&o);
    const uint8_t first=o.id[0];
    mouse_touch_update(&t,10,0,1.5f,0,0,0,1012*MS,&o);
    assert(o.count==1 && o.id[0]==first);
    mouse_touch_update(&t,0,0,1.5f,0,0,0,1400*MS,&o); /* still: lifted */
    assert(o.count==0);
    mouse_touch_update(&t,10,0,1.5f,0,0,0,1412*MS,&o);
    assert(o.count==1 && o.id[0]!=first && o.id[0]<128);
}

/* The left button pushes the touchpad in, with a finger down, for as long as it is held. */
static void test_click(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    mouse_touch_update(&t,0,0,1.5f,1,0,0,1000*MS,&o);
    assert(o.click && o.count==1);
    mouse_touch_update(&t,0,0,1.5f,1,0,0,2000*MS,&o);
    assert(o.click && o.count==1); /* held: not lifted */
    mouse_touch_update(&t,0,0,1.5f,0,0,0,2012*MS,&o);
    assert(!o.click);
}

/* Wheel up spreads two fingers apart, smoothly, then lifts them; wheel down pinches. */
static void test_pinch(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    uint64_t now=1000*MS;
    mouse_touch_update(&t,0,0,1.5f,0,2,0,now,&o);
    assert(o.count==2);
    const float start=spacing(&o);
    assert(fabsf(start-320.0f)<2.0f);
    float last=start;
    for (int i=0;i<10;++i) {
        now+=12*MS; mouse_touch_update(&t,0,0,1.5f,0,0,0,now,&o);
        assert(o.count==2 && spacing(&o)>=last-0.5f);
        last=spacing(&o);
    }
    assert(fabsf(last-(320.0f+2*90.0f))<2.0f);
    now+=300*MS; mouse_touch_update(&t,0,0,1.5f,0,0,0,now,&o);
    assert(o.count==0);
    now+=12*MS; mouse_touch_update(&t,0,0,1.5f,0,-1,0,now,&o);
    for (int i=0;i<10;++i) { now+=12*MS; mouse_touch_update(&t,0,0,1.5f,0,0,0,now,&o); }
    assert(o.count==2 && fabsf(spacing(&o)-(320.0f-90.0f))<2.0f);
}

/* Ctrl + wheel up twists two fingers counterclockwise (on the pad, y down), keeping their spacing. */
static void test_twist(void) {
    MouseTouch t; mouse_touch_reset(&t); MouseTouchOut o;
    uint64_t now=1000*MS;
    mouse_touch_update(&t,0,0,1.5f,0,1,1,now,&o);
    for (int i=0;i<10;++i) { now+=12*MS; mouse_touch_update(&t,0,0,1.5f,0,0,1,now,&o); }
    assert(o.count==2 && fabsf(spacing(&o)-320.0f)<2.0f);
    /* The second finger (right of the first at the start) has turned upward: smaller y. */
    assert(o.y[1]<o.y[0] && o.x[1]>o.x[0]);
    const float angle=atan2f((float)o.y[0]-o.y[1],(float)o.x[1]-o.x[0]);
    assert(fabsf(angle-0.2618f)<0.01f);
}

int main(void) {
    test_slide_and_lift();
    test_edge_relift();
    test_touch_ids();
    test_click();
    test_pinch();
    test_twist();
    puts("mouse touch tests passed");
    return 0;
}
