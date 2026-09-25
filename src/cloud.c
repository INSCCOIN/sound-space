#include "cloud.h"

#include <stdlib.h>
#include <string.h>

void cloud_init(Cloud *c) {
    memset(c, 0, sizeof(*c));
    c->yaw = 0.35f;
    c->pitch = 0.22f;
    c->zoom = 1.0f;
    c->emit_gain = 1.0f;
    c->life_scale = 1.0f;
}

void cloud_clear(Cloud *c) {
    c->count = 0;
    memset(c->p, 0, sizeof(c->p));
}

static int alloc_slot(Cloud *c) {
    for (int i = 0; i < SS_MAX_PARTICLES; i++) {
        if (!c->p[i].alive) {
            if (i >= c->count) c->count = i + 1;
            return i;
        }
    }
    /* steal oldest */
    int best = 0;
    float lo = 2.0f;
    for (int i = 0; i < SS_MAX_PARTICLES; i++) {
        if (c->p[i].life < lo) { lo = c->p[i].life; best = i; }
    }
    return best;
}

void cloud_emit(Cloud *c, const Features *f) {
    if (c->frozen) return;

    float amp = f->rms * c->emit_gain;
    /* always drip a faint particle; extra burst on energy / flux */
    int n = 1;
    if (amp > 0.04f) n++;
    if (amp > 0.10f) n++;
    if (f->flux > 0.8f) n += 2;

    /* map features into a unit cube, Arese-style: not a spectrogram slab */
    float cx = logf(f->centroid_hz + 80.0f);
    float px = logf(f->peak_hz + 80.0f);
    float x = (cx - logf(200.0f)) / (logf(6000.0f) - logf(200.0f));
    float y = (px - logf(200.0f)) / (logf(7000.0f) - logf(200.0f));
    x = clampf(x, 0, 1) * 2.0f - 1.0f;
    y = clampf(y, 0, 1) * 2.0f - 1.0f;
    float z = clampf(f->slope, -1, 1);          /* brightness tilt */
    z += clampf(f->flatness * 0.6f, 0, 0.6f);   /* noise pushes out */

    float hue = clampf((logf(f->peak_hz + 80.0f) - logf(200.0f)) /
                       (logf(8000.0f) - logf(200.0f)), 0, 1);
    /* wrap so low=blue-ish, mid=green, high=red-magenta */
    uint8_t r, g, b;
    hsv_rgb(0.66f - hue * 0.66f, 0.85f, 0.55f + clampf(amp * 3.0f, 0, 0.45f), &r, &g, &b);
    uint16_t col = rgb565(r, g, b);

    for (int k = 0; k < n; k++) {
        int i = alloc_slot(c);
        Particle *p = &c->p[i];
        float jitter = 0.04f + amp * 0.12f;
        float jx = ((float)(rand() % 1000) / 1000.0f - 0.5f) * jitter;
        float jy = ((float)(rand() % 1000) / 1000.0f - 0.5f) * jitter;
        float jz = ((float)(rand() % 1000) / 1000.0f - 0.5f) * jitter;
        p->x = x + jx;
        p->y = y + jy;
        p->z = z + jz;
        /* slow drift so the cloud breathes instead of stacking */
        p->vx = jx * 0.4f + f->flux * 0.02f;
        p->vy = (f->centroid_hz - f->peak_hz) * 0.00002f;
        p->vz = -0.08f - amp * 0.15f; /* recede in z = time fold */
        p->amp = amp;
        p->hz = f->peak_hz;
        p->flux = f->flux;
        p->life = clampf(0.55f + amp * 1.8f, 0.35f, 1.0f) * c->life_scale;
        p->color = col;
        p->alive = 1;
    }
}

void cloud_tick(Cloud *c, float dt) {
    if (c->frozen) return;
    int max_alive = 0;
    for (int i = 0; i < SS_MAX_PARTICLES; i++) {
        Particle *p = &c->p[i];
        if (!p->alive) continue;
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->z += p->vz * dt;
        p->life -= dt * 0.35f;
        if (p->life <= 0) {
            p->alive = 0;
            continue;
        }
        max_alive = i + 1;
    }
    c->count = max_alive;
}
