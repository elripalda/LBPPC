/* Motion for libScePad's pad data (runtime_motion.h): a Mahony filter over the host controller's
 * gyro and accelerometer, gyro bias learned while the pad lies still, and a virtual tilt. */
#include "runtime_motion.h"
#include <math.h>
#include <string.h>

#define STANDARD_GRAVITY 9.80665f
#define CORRECTION_GAIN 1.0f   /* how fast the orientation leans to the measured gravity (1/s) */
#define STILL_GYRO 0.06f       /* rad/s: slower than this ... */
#define STILL_ACCEL 0.1f       /* ... and within this many G of 1 G (sensors read a few % off), still */
#define STILL_BEFORE_BIAS 0.5f /* seconds still before the gyro's offset is learned */
#define BIAS_RATE 0.5f         /* of the remaining offset per second, while still */

static void quat_mul(const float a[4], const float b[4], float out[4]) {
    const float x=a[3]*b[0]+a[0]*b[3]+a[1]*b[2]-a[2]*b[1];
    const float y=a[3]*b[1]-a[0]*b[2]+a[1]*b[3]+a[2]*b[0];
    const float z=a[3]*b[2]+a[0]*b[1]-a[1]*b[0]+a[2]*b[3];
    const float w=a[3]*b[3]-a[0]*b[0]-a[1]*b[1]-a[2]*b[2];
    out[0]=x; out[1]=y; out[2]=z; out[3]=w;
}
static void quat_normalize(float q[4]) {
    const float n=sqrtf(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    if (n<1e-12f) { q[0]=q[1]=q[2]=0.0f; q[3]=1.0f; return; }
    for (int i=0;i<4;++i) q[i]/=n;
}
/* The rotation by `angle` radians about the unit `axis`. */
static void quat_axis_angle(const float axis[3], float angle, float out[4]) {
    const float s=sinf(angle*0.5f);
    out[0]=axis[0]*s; out[1]=axis[1]*s; out[2]=axis[2]*s; out[3]=cosf(angle*0.5f);
}
/* World up (0, 1, 0) seen in the pad's axes: the conjugate of q turning it. */
static void up_in_pad(const float q[4], float out[3]) {
    const float x=q[0], y=q[1], z=q[2], w=q[3];
    out[0]=2.0f*(x*y+w*z);
    out[1]=1.0f-2.0f*(x*x+z*z);
    out[2]=2.0f*(y*z-w*x);
}
static float length3(const float v[3]) { return sqrtf(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); }

void motion_reset(MotionState *m) {
    memset(m,0,sizeof(*m));
    m->q[3]=1.0f;
    m->accel[1]=1.0f;
}

/* Turns q by the pad-axes angular velocity w for dt seconds. */
static void integrate(float q[4], const float w[3], float dt) {
    const float rate=length3(w);
    if (rate<=0.0f || dt<=0.0f) return;
    const float axis[3]={w[0]/rate,w[1]/rate,w[2]/rate};
    float step[4], out[4];
    quat_axis_angle(axis,rate*dt,step);
    quat_mul(q,step,out);
    memcpy(q,out,sizeof(out));
    quat_normalize(q);
}

void motion_sensor_sample(MotionState *m, const float accel_ms2[3], const float gyro_rads[3], float dt) {
    if (!(dt>0.0f) || dt>0.25f) dt=0.0f; /* the first sample, or after a pause: no integration */
    float a[3];
    for (int i=0;i<3;++i) a[i]=accel_ms2[i]/STANDARD_GRAVITY;
    const float g_len=length3(a);
    float w[3];
    for (int i=0;i<3;++i) w[i]=gyro_rads[i]-m->bias[i];

    /* The gyro reads a small offset at rest, which would turn the orientation forever: learned
     * while the pad is still (near 1 G, barely turning) and taken off. */
    if (length3(w)<STILL_GYRO && fabsf(g_len-1.0f)<STILL_ACCEL) {
        m->still_s+=dt;
        if (m->still_s>STILL_BEFORE_BIAS) {
            const float k=fminf(1.0f,BIAS_RATE*dt);
            for (int i=0;i<3;++i) m->bias[i]+=(gyro_rads[i]-m->bias[i])*k;
        }
    } else {
        m->still_s=0.0f;
    }
    for (int i=0;i<3;++i) w[i]=gyro_rads[i]-m->bias[i];
    if (length3(w)<MOTION_GYRO_DEADBAND) w[0]=w[1]=w[2]=0.0f;

    if (g_len>0.5f) {
        const float up[3]={a[0]/g_len,a[1]/g_len,a[2]/g_len};
        if (!m->primed) {
            /* The first sample sets the tilt at once: the rotation taking the measured up to world up. */
            const float world[3]={0.0f,1.0f,0.0f};
            float q[4]={up[1]*world[2]-up[2]*world[1],up[2]*world[0]-up[0]*world[2],
                        up[0]*world[1]-up[1]*world[0],1.0f+up[1]};
            if (q[3]<1e-6f) { q[0]=0.0f; q[1]=0.0f; q[2]=1.0f; q[3]=0.0f; } /* upside down */
            quat_normalize(q);
            memcpy(m->q,q,sizeof(q));
            m->primed=1;
        } else if (g_len>0.7f && g_len<1.3f) {
            /* Lean toward the measured gravity (not while the pad is being shaken). */
            float estimate[3];
            up_in_pad(m->q,estimate);
            const float e[3]={up[1]*estimate[2]-up[2]*estimate[1],up[2]*estimate[0]-up[0]*estimate[2],
                              up[0]*estimate[1]-up[1]*estimate[0]};
            float corrected[3];
            for (int i=0;i<3;++i) corrected[i]=w[i]+CORRECTION_GAIN*e[i];
            integrate(m->q,corrected,dt);
        } else {
            integrate(m->q,w,dt);
        }
    } else {
        integrate(m->q,w,dt);
    }
    memcpy(m->accel,a,sizeof(a));
    memcpy(m->gyro,w,sizeof(w));
}

int motion_virtual_tilt(MotionState *m, float roll, float pitch, float dt) {
    const float target[2]={fmaxf(-1.0f,fminf(1.0f,roll))*MOTION_MAX_TILT,
                           fmaxf(-1.0f,fminf(1.0f,pitch))*MOTION_MAX_TILT};
    const float step=MOTION_TILT_RATE*fmaxf(0.0f,fminf(dt,0.25f));
    for (int i=0;i<2;++i) {
        const float d=target[i]-m->tilt[i];
        m->tilt[i]+=fabsf(d)<=step ? d : (d>0.0f ? step : -step);
    }
    return fabsf(m->tilt[0])>1e-4f || fabsf(m->tilt[1])>1e-4f || target[0]!=0.0f || target[1]!=0.0f;
}

/* The virtual tilt's orientation: pitch about +X (front up), then roll about -Z (right side down). */
static void virtual_orientation(const MotionState *m, float q[4]) {
    const float x_axis[3]={1.0f,0.0f,0.0f}, z_axis[3]={0.0f,0.0f,1.0f};
    float pitch[4], roll[4];
    quat_axis_angle(x_axis,m->tilt[1],pitch);
    quat_axis_angle(z_axis,-m->tilt[0],roll);
    quat_mul(pitch,roll,q);
}

void motion_fill(const MotionState *m, float orientation[4], float acceleration[3], float angular_velocity[3]) {
    if (m->tilt[0]!=0.0f || m->tilt[1]!=0.0f) {
        virtual_orientation(m,orientation);
        up_in_pad(orientation,acceleration);
        angular_velocity[0]=angular_velocity[1]=angular_velocity[2]=0.0f;
        return;
    }
    memcpy(orientation,m->q,4*sizeof(float));
    memcpy(acceleration,m->accel,3*sizeof(float));
    memcpy(angular_velocity,m->gyro,3*sizeof(float));
}
