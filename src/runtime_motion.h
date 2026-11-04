/* The DualShock 4's motion fields of OrbisPadData (orientation, acceleration, angular velocity),
 * made from a host controller's gyro and accelerometer (SDL3 sensors) or, without one, from a
 * "virtual tilt" that keys or sticks drive (input.ini's tilt_* outputs). LBP3 reads only these
 * fields: it imports no scePadSetMotionSensorState, scePadResetOrientation or tilt-correction
 * calls, so motion is always on. Sackboy's head follows the acceleration (gravity), as with the
 * PS3's Sixaxis, not the orientation (tested: changing only one of them, only acceleration moved
 * it); the orientation is filled in as well, its reference the pad held level.
 *
 * Axes (SDL's sensor axes, the same as the DualShock 4's): +X right, +Y up out of the pad's
 * face, +Z toward the player. Acceleration is in G and is the pad's reaction to gravity: held
 * level and still it reads (0, +1, 0). Angular velocity is in rad/s, positive counterclockwise
 * about each axis. The orientation (x, y, z, w) turns pad axes into the world's, world +Y up;
 * held level it is the identity. */
#ifndef BB_RUNTIME_MOTION_H
#define BB_RUNTIME_MOTION_H
#include <stdint.h>

typedef struct {
    float q[4];         /* orientation x, y, z, w */
    float bias[3];      /* learned gyro offset, rad/s */
    float still_s;      /* how long the pad has been still (bias learning) */
    int primed;         /* q set from a first accelerometer sample */
    float tilt[2];      /* virtual tilt now: roll (+ right side down), pitch (+ front up), radians */
    float accel[3], gyro[3]; /* last reported */
} MotionState;

#define MOTION_MAX_TILT 0.61f      /* ~35 degrees: a full stick or key push of virtual tilt */
#define MOTION_TILT_RATE 4.0f      /* rad/s the virtual tilt moves toward its target */
#define MOTION_GYRO_DEADBAND 0.008f /* rad/s: below this (after bias), no rotation is reported */

void motion_reset(MotionState *m);
/* One host sensor sample: accelerometer in m/s^2, gyro in rad/s, dt seconds since the last one. */
void motion_sensor_sample(MotionState *m, const float accel_ms2[3], const float gyro_rads[3], float dt);
/* Virtual tilt toward `roll`, `pitch` (each -1..1 of MOTION_MAX_TILT), dt seconds since the last call.
 * Returns whether the virtual tilt is away from level (it then replaces the sensors' roll and pitch). */
int motion_virtual_tilt(MotionState *m, float roll, float pitch, float dt);
/* The pad fields: orientation (x, y, z, w), acceleration in G, angular velocity in rad/s. */
void motion_fill(const MotionState *m, float orientation[4], float acceleration[3], float angular_velocity[3]);

#endif
