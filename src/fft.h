#ifndef SOUNDSPACE_FFT_H
#define SOUNDSPACE_FFT_H

#include "common.h"

/* In-place radix-2 real-friendly complex FFT. n must be power of two. */
void fft_init(int n);
void fft_forward(float *re, float *im, int n);
void fft_window_hann(float *re, int n);
void fft_free(void);

#endif
