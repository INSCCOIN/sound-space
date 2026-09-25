#ifndef SOUNDSPACE_ANALYZE_H
#define SOUNDSPACE_ANALYZE_H

#include "common.h"

typedef struct {
    float rms;
    float peak;
    float centroid_hz;
    float peak_hz;
    float flux;
    float flatness;
    float slope;
    float mag[SS_BINS];
    float wave[SS_FFT_N];
} Features;

void analyze_init(void);
void analyze_free(void);
void analyze_frame(const float *samples, int n, Features *out);

#endif
