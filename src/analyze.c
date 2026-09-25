#include "analyze.h"
#include "fft.h"

#include <string.h>

static float re[SS_FFT_N];
static float im[SS_FFT_N];
static float prev[SS_BINS];
static int   have_prev;

void analyze_init(void) {
    fft_init(SS_FFT_N);
    memset(prev, 0, sizeof(prev));
    have_prev = 0;
}

void analyze_free(void) {
    fft_free();
}

void analyze_frame(const float *samples, int n, Features *out) {
    memset(out, 0, sizeof(*out));
    int use = n < SS_FFT_N ? n : SS_FFT_N;

    float sum2 = 0, pk = 0;
    for (int i = 0; i < use; i++) {
        float s = samples[i];
        out->wave[i] = s;
        sum2 += s * s;
        float a = fabsf(s);
        if (a > pk) pk = a;
    }
    for (int i = use; i < SS_FFT_N; i++) out->wave[i] = 0;
    out->rms = sqrtf(sum2 / (float)(use > 0 ? use : 1));
    out->peak = pk;

    for (int i = 0; i < SS_FFT_N; i++) {
        re[i] = (i < use) ? samples[i] : 0;
        im[i] = 0;
    }
    fft_window_hann(re, SS_FFT_N);
    fft_forward(re, im, SS_FFT_N);

    float mag_sum = 0, weighted = 0, log_sum = 0;
    float peak_m = 0;
    int peak_k = 1;
    const float hz_per = (float)SS_SR / (float)SS_FFT_N;
    float flux = 0;

    /* skip DC bin */
    for (int k = 1; k < SS_BINS; k++) {
        float m = sqrtf(re[k] * re[k] + im[k] * im[k]);
        out->mag[k] = m;
        mag_sum += m;
        weighted += m * (float)k;
        log_sum += logf(m + 1e-8f);
        if (m > peak_m) { peak_m = m; peak_k = k; }
        float d = m - prev[k];
        if (d > 0) flux += d;
        prev[k] = m;
    }
    out->mag[0] = 0;
    out->flux = have_prev ? flux / (float)SS_BINS : 0;
    have_prev = 1;

    if (mag_sum > 1e-8f) {
        out->centroid_hz = (weighted / mag_sum) * hz_per;
        float geo = expf(log_sum / (float)(SS_BINS - 1));
        float arith = mag_sum / (float)(SS_BINS - 1);
        out->flatness = clampf(geo / (arith + 1e-12f), 0, 1);
    } else {
        out->centroid_hz = 0;
        out->flatness = 0;
    }
    out->peak_hz = (float)peak_k * hz_per;

    /* crude spectral slope: low vs high energy */
    float lo = 0, hi = 0;
    int mid = SS_BINS / 3;
    for (int k = 1; k < mid; k++) lo += out->mag[k];
    for (int k = mid; k < SS_BINS; k++) hi += out->mag[k];
    out->slope = (hi - lo) / (hi + lo + 1e-8f);
}
