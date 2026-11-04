/* runtime_motion.c: the pad's orientation, acceleration and angular velocity from host sensors
 * and the virtual tilt. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "../src/runtime_motion.h"

#define G 9.80665f

static float angle_from_level(const float q[4]) { return 2.0f*acosf(fminf(1.0f,fabsf(q[3]))); }

/* Idle on a desk with a real gyro's offset: no drift, no rotation reported once the offset is learned. */
static void test_idle_no_drift(void) {
    MotionState m; motion_reset(&m);
    const float accel[3]={0.0f,G,0.0f}, gyro[3]={0.012f,-0.02f,0.015f};
    float q[4], a[3], w[3];
    for (int i=0;i<250*60;++i) motion_sensor_sample(&m,accel,gyro,1.0f/250.0f);
    motion_fill(&m,q,a,w);
    printf("idle: %.3f degrees from level, reported rate %.4f %.4f %.4f\n",angle_from_level(q)*57.2958f,w[0],w[1],w[2]);
    /* Yaw is not corrected by gravity: the offset turns it until learned (half a second still, then
     * half of it per second), a few degrees in all. Tilt stays level. */
    float up[3]={2.0f*(q[0]*q[1]+q[3]*q[2]),1.0f-2.0f*(q[0]*q[0]+q[2]*q[2]),2.0f*(q[1]*q[2]-q[3]*q[0])};
    assert(up[1]>0.9998f); /* tilt within ~1 degree */
    assert(angle_from_level(q)<0.2f); /* yaw bounded */
    assert(w[0]==0.0f && w[1]==0.0f && w[2]==0.0f);
    assert(fabsf(a[1]-1.0f)<1e-4f);
    /* Learned: another minute adds (almost) nothing. */
    const float before=angle_from_level(q);
    for (int i=0;i<250*60;++i) motion_sensor_sample(&m,accel,gyro,1.0f/250.0f);
    motion_fill(&m,q,a,w);
    assert(fabsf(angle_from_level(q)-before)<0.01f);
}

/* The first sample tilts the orientation at once: rolled 30 degrees, right side down. */
static void test_first_sample_sets_tilt(void) {
    MotionState m; motion_reset(&m);
    const float r=0.5236f;
    const float accel[3]={-sinf(r)*G,cosf(r)*G,0.0f}, gyro[3]={0.0f,0.0f,0.0f};
    motion_sensor_sample(&m,accel,gyro,0.0f);
    float q[4], a[3], w[3];
    motion_fill(&m,q,a,w);
    assert(fabsf(angle_from_level(q)-r)<1e-3f);
    /* Pad right (+X) in world: below the horizon. */
    const float right_y=2.0f*(q[0]*q[1]+q[3]*q[2]);
    assert(right_y<-0.45f);
}

/* Turning at 1 rad/s about +Z for half a second, accelerometer reading nothing (free fall: no
 * correction): half a radian. */
static void test_gyro_integrates(void) {
    MotionState m; motion_reset(&m);
    const float level[3]={0.0f,G,0.0f}, none[3]={0.0f,0.0f,0.0f};
    motion_sensor_sample(&m,level,none,0.0f);
    const float free_fall[3]={0.0f,0.0f,0.0f}, turn[3]={0.0f,0.0f,1.0f};
    for (int i=0;i<125;++i) motion_sensor_sample(&m,free_fall,turn,1.0f/250.0f);
    float q[4], a[3], w[3];
    motion_fill(&m,q,a,w);
    assert(fabsf(angle_from_level(q)-0.5f)<1e-3f);
    assert(q[2]>0.0f); /* counterclockwise about +Z */
    assert(fabsf(w[2]-1.0f)<1e-6f);
}

/* Tilted, then put level again: gravity brings the orientation back level. */
static void test_gravity_corrects(void) {
    MotionState m; motion_reset(&m);
    const float level[3]={0.0f,G,0.0f}, none[3]={0.0f,0.0f,0.0f};
    motion_sensor_sample(&m,level,none,0.0f);
    const float free_fall[3]={0.0f,0.0f,0.0f}, turn[3]={1.0f,0.0f,0.0f};
    for (int i=0;i<50;++i) motion_sensor_sample(&m,free_fall,turn,1.0f/250.0f); /* 0.2 rad wrong */
    float q[4], a[3], w[3];
    for (int i=0;i<250*5;++i) motion_sensor_sample(&m,level,none,1.0f/250.0f);
    motion_fill(&m,q,a,w);
    assert(angle_from_level(q)<0.002f);
}

/* Virtual tilt: a full push reaches MOTION_MAX_TILT smoothly, gravity reads accordingly, and
 * releasing it returns to the sensors' values. */
static void test_virtual_tilt(void) {
    MotionState m; motion_reset(&m);
    float q[4], a[3], w[3];
    assert(!motion_virtual_tilt(&m,0.0f,0.0f,1.0f/60.0f));
    assert(motion_virtual_tilt(&m,1.0f,0.0f,1.0f/60.0f));
    assert(m.tilt[0]>0.0f && m.tilt[0]<MOTION_MAX_TILT); /* not at once */
    for (int i=0;i<60;++i) motion_virtual_tilt(&m,1.0f,0.0f,1.0f/60.0f);
    assert(fabsf(m.tilt[0]-MOTION_MAX_TILT)<1e-6f);
    motion_fill(&m,q,a,w);
    assert(a[0]<-0.5f && a[1]>0.7f); /* right side down: up leans to the pad's left */
    for (int i=0;i<60;++i) motion_virtual_tilt(&m,0.0f,0.0f,1.0f/60.0f);
    assert(!motion_virtual_tilt(&m,0.0f,0.0f,1.0f/60.0f));
    motion_fill(&m,q,a,w);
    assert(q[3]==1.0f && a[1]==1.0f);
    /* Pitch: front up; the face turns toward the player, so up (in the pad's axes) leans to its front, -Z. */
    for (int i=0;i<60;++i) motion_virtual_tilt(&m,0.0f,1.0f,1.0f/60.0f);
    motion_fill(&m,q,a,w);
    assert(a[2]<-0.5f);
}

int main(void) {
    test_idle_no_drift();
    test_first_sample_sets_tilt();
    test_gyro_integrates();
    test_gravity_corrects();
    test_virtual_tilt();
    puts("motion tests passed");
    return 0;
}
