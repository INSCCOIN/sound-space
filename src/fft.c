#include "fft.h"

#include <stdlib.h>

static int   g_n = 0;
static int  *g_rev = NULL;
static float *g_wr = NULL;
static float *g_wi = NULL;

void fft_free(void) {
    free(g_rev); g_rev = NULL;
    free(g_wr);  g_wr = NULL;
    free(g_wi);  g_wi = NULL;
    g_n = 0;
}

void fft_init(int n) {
    if (g_n == n && g_rev) return;
    fft_free();
    g_n = n;
    g_rev = calloc((size_t)n, sizeof(int));
    g_wr  = calloc((size_t)n, sizeof(float));
    g_wi  = calloc((size_t)n, sizeof(float));
    int bits = 0;
    while ((1 << bits) < n) bits++;
    for (int i = 0; i < n; i++) {
        int x = i, y = 0;
        for (int b = 0; b < bits; b++) {
            y = (y << 1) | (x & 1);
            x >>= 1;
        }
        g_rev[i] = y;
    }
    for (int i = 0; i < n; i++) {
        float a = -2.0f * (float)M_PI * (float)i / (float)n;
        g_wr[i] = cosf(a);
        g_wi[i] = sinf(a);
    }
}

void fft_window_hann(float *re, int n) {
    for (int i = 0; i < n; i++) {
        float w = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1));
        re[i] *= w;
    }
}

void fft_forward(float *re, float *im, int n) {
    if (g_n != n) fft_init(n);
    for (int i = 0; i < n; i++) {
        int j = g_rev[i];
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        int half = len >> 1;
        int step = n / len;
        for (int i = 0; i < n; i += len) {
            for (int j = 0; j < half; j++) {
                int k = j * step;
                float wr = g_wr[k], wi = g_wi[k];
                float ur = re[i + j], ui = im[i + j];
                float vr = re[i + j + half], vi = im[i + j + half];
                float tr = wr * vr - wi * vi;
                float ti = wr * vi + wi * vr;
                re[i + j] = ur + tr;
                im[i + j] = ui + ti;
                re[i + j + half] = ur - tr;
                im[i + j + half] = ui - ti;
            }
        }
    }
}
