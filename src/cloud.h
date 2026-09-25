#ifndef SOUNDSPACE_CLOUD_H
#define SOUNDSPACE_CLOUD_H

#include "common.h"
#include "analyze.h"

typedef struct {
    float x, y, z;
    float vx, vy, vz;
    float life;      /* 0..1 */
    float amp;
    float hz;
    float flux;
    uint16_t color;
    uint8_t alive;
} Particle;

typedef struct {
    Particle p[SS_MAX_PARTICLES];
    int count;
    float yaw, pitch;
    float zoom;
    float emit_gain;
    float life_scale;
    int frozen;
} Cloud;

void cloud_init(Cloud *c);
void cloud_clear(Cloud *c);
void cloud_emit(Cloud *c, const Features *f);
void cloud_tick(Cloud *c, float dt);

#endif
