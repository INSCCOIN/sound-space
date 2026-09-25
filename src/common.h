#ifndef SOUNDSPACE_COMMON_H
#define SOUNDSPACE_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SS_SR            16000
#define SS_FFT_N         512
#define SS_HOP           256
#define SS_BINS          (SS_FFT_N / 2)
#define SS_MAX_PARTICLES 420
#define SS_MAX_TRAIL     96

enum {
    MODE_SPACE = 0,
    MODE_SPEC,
    MODE_WAVE,
    MODE_FIELD,
    MODE_COUNT
};

static inline float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* hue 0..1 -> RGB */
static inline void hsv_rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b) {
    float c = v * s;
    float hp = fmodf(h, 1.0f) * 6.0f;
    if (hp < 0) hp += 6.0f;
    float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
    float m = v - c;
    float rf = 0, gf = 0, bf = 0;
    if      (hp < 1) { rf = c; gf = x; bf = 0; }
    else if (hp < 2) { rf = x; gf = c; bf = 0; }
    else if (hp < 3) { rf = 0; gf = c; bf = x; }
    else if (hp < 4) { rf = 0; gf = x; bf = c; }
    else if (hp < 5) { rf = x; gf = 0; bf = c; }
    else             { rf = c; gf = 0; bf = x; }
    *r = (uint8_t)clampf((rf + m) * 255.0f, 0, 255);
    *g = (uint8_t)clampf((gf + m) * 255.0f, 0, 255);
    *b = (uint8_t)clampf((bf + m) * 255.0f, 0, 255);
}

#endif
