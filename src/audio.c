#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <strings.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "minimp3.h"

#ifdef HAVE_ALSA
/*
 * WalnutOS / older alsa-lib: asoundlib.h pulls time.h (glibc timespec)
 * then global.h defines struct timespec again unless a POSIX source
 * macro is visible. Define it before the ALSA header.
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <time.h>
#define __timespec_defined 1
#include <alsa/asoundlib.h>
#endif

struct Audio {
    AudioKind kind;
    char label[64];
    /* wav */
    FILE *fp;
    int channels;
    int src_rate;
    int bytes_per;
    long data_off;
    long data_end;
    /* resample leftover */
    float hold;
    double pos_frac;
    double step;
    /* synth */
    double t;
    int phrase;
    /* mp3 */
    uint8_t *mp3;
    size_t mp3_size;
    size_t mp3_off;
    size_t mp3_start;
    mp3dec_t mp3dec;
    int16_t mp3_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int mp3_frame_len;
    int mp3_frame_pos;
    /* alsa */
#ifdef HAVE_ALSA
    snd_pcm_t *pcm;
#endif
};

static uint16_t r16(FILE *f) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return (uint16_t)(b[0] | (b[1] << 8));
}
static uint32_t r32(FILE *f) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return (uint32_t)(b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24));
}

static int wav_open(Audio *a, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    char riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) || memcmp(riff + 8, "WAVE", 4)) {
        fclose(f);
        return -1;
    }
    int fmt_ok = 0;
    long data_off = 0, data_end = 0;
    int ch = 1, rate = SS_SR, bps = 16;
    for (;;) {
        char id[4];
        if (fread(id, 1, 4, f) != 4) break;
        uint32_t sz = r32(f);
        long next = ftell(f) + (long)sz + (sz & 1);
        if (!memcmp(id, "fmt ", 4)) {
            uint16_t format = r16(f);
            ch = r16(f);
            rate = (int)r32(f);
            r32(f); /* byterate */
            r16(f); /* block */
            bps = r16(f);
            if (format != 1 || (bps != 16 && bps != 8)) {
                fclose(f);
                return -1;
            }
            fmt_ok = 1;
        } else if (!memcmp(id, "data", 4)) {
            data_off = ftell(f);
            data_end = data_off + (long)sz;
            break;
        }
        fseek(f, next, SEEK_SET);
    }
    if (!fmt_ok || data_off == 0) {
        fclose(f);
        return -1;
    }
    a->kind = AUDIO_WAV;
    a->fp = f;
    a->channels = ch < 1 ? 1 : ch;
    a->src_rate = rate > 0 ? rate : SS_SR;
    a->bytes_per = (bps / 8) * a->channels;
    a->data_off = data_off;
    a->data_end = data_end;
    a->step = (double)a->src_rate / (double)SS_SR;
    a->pos_frac = 0;
    fseek(f, data_off, SEEK_SET);
    snprintf(a->label, sizeof(a->label), "wav %dHz %s", a->src_rate, path);
    return 0;
}

static float wav_next_src(Audio *a) {
    if (!a->fp) return 0;
    if (ftell(a->fp) >= a->data_end) {
        fseek(a->fp, a->data_off, SEEK_SET);
    }
    float acc = 0;
    int ch = a->channels;
    for (int c = 0; c < ch; c++) {
        if (a->bytes_per / ch == 1) {
            int v = fgetc(a->fp);
            if (v == EOF) { fseek(a->fp, a->data_off, SEEK_SET); v = fgetc(a->fp); }
            acc += ((float)v - 128.0f) / 128.0f;
        } else {
            int lo = fgetc(a->fp);
            int hi = fgetc(a->fp);
            if (lo == EOF || hi == EOF) {
                fseek(a->fp, a->data_off, SEEK_SET);
                lo = fgetc(a->fp); hi = fgetc(a->fp);
            }
            int16_t s = (int16_t)(lo | (hi << 8));
            acc += (float)s / 32768.0f;
        }
    }
    return acc / (float)ch;
}

static int wav_read(Audio *a, float *dst, int n) {
    for (int i = 0; i < n; i++) {
        while (a->pos_frac >= 1.0) {
            a->hold = wav_next_src(a);
            a->pos_frac -= 1.0;
        }
        dst[i] = a->hold;
        a->pos_frac += a->step;
    }
    return n;
}

static size_t skip_id3(const uint8_t *b, size_t n) {
    if (n < 10) return 0;
    if (b[0] != 'I' || b[1] != 'D' || b[2] != '3') return 0;
    size_t size = ((size_t)(b[6] & 0x7f) << 21) |
                  ((size_t)(b[7] & 0x7f) << 14) |
                  ((size_t)(b[8] & 0x7f) << 7) |
                  ((size_t)(b[9] & 0x7f));
    size += 10;
    if (b[5] & 0x10) size += 10; /* footer */
    if (size > n) size = n;
    return size;
}

static int mp3_decode_one(Audio *a) {
    for (;;) {
        if (a->mp3_off >= a->mp3_size) {
            a->mp3_off = a->mp3_start;
            mp3dec_init(&a->mp3dec);
            if (a->mp3_off >= a->mp3_size) return -1;
        }
        mp3dec_frame_info_t info;
        memset(&info, 0, sizeof(info));
        int left = (int)(a->mp3_size - a->mp3_off);
        int samples = mp3dec_decode_frame(&a->mp3dec,
                                          a->mp3 + a->mp3_off, left,
                                          a->mp3_pcm, &info);
        if (info.frame_bytes > 0)
            a->mp3_off += (size_t)info.frame_bytes;
        else
            a->mp3_off += 1; /* hunt sync */
        if (samples > 0) {
            if (info.channels > 0) a->channels = info.channels;
            if (info.hz > 0) {
                a->src_rate = info.hz;
                a->step = (double)a->src_rate / (double)SS_SR;
            }
            a->mp3_frame_len = samples; /* per channel */
            a->mp3_frame_pos = 0;
            return 0;
        }
        if (a->mp3_off >= a->mp3_size) {
            a->mp3_off = a->mp3_start;
            mp3dec_init(&a->mp3dec);
            return -1;
        }
    }
}

static float mp3_next_src(Audio *a) {
    if (a->mp3_frame_pos >= a->mp3_frame_len) {
        if (mp3_decode_one(a) != 0) return 0;
    }
    int ch = a->channels > 0 ? a->channels : 1;
    int idx = a->mp3_frame_pos * ch;
    float acc = 0;
    for (int c = 0; c < ch; c++)
        acc += (float)a->mp3_pcm[idx + c] / 32768.0f;
    a->mp3_frame_pos++;
    return acc / (float)ch;
}

static int mp3_read(Audio *a, float *dst, int n) {
    for (int i = 0; i < n; i++) {
        while (a->pos_frac >= 1.0) {
            a->hold = mp3_next_src(a);
            a->pos_frac -= 1.0;
        }
        dst[i] = a->hold;
        a->pos_frac += a->step;
    }
    return n;
}

static int mp3_open(Audio *a, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz <= 0 || sz > 48L * 1024 * 1024) { fclose(f); return -1; }
    rewind(f);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    a->mp3 = buf;
    a->mp3_size = (size_t)sz;
    a->mp3_start = skip_id3(buf, (size_t)sz);
    a->mp3_off = a->mp3_start;
    mp3dec_init(&a->mp3dec);
    a->channels = 2;
    a->src_rate = 44100;
    a->step = (double)a->src_rate / (double)SS_SR;
    a->mp3_frame_len = 0;
    a->mp3_frame_pos = 0;

    if (mp3_decode_one(a) != 0) {
        free(a->mp3);
        a->mp3 = NULL;
        return -1;
    }
    a->kind = AUDIO_MP3;
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    snprintf(a->label, sizeof(a->label), "mp3 %dHz %s", a->src_rate, base);
    return 0;
}

/* Built-in demo: sweeping chirps + trills so the mapper has something to eat. */
static int synth_read(Audio *a, float *dst, int n) {
    for (int i = 0; i < n; i++) {
        a->t += 1.0 / (double)SS_SR;
        double t = a->t;
        int bar = (int)(t * 2.0) % 8;
        float s = 0;
        if (bar == 0 || bar == 1) {
            double f = 800.0 + 2400.0 * fmod(t * 0.7, 1.0);
            s = 0.35f * sinf((float)(2.0 * M_PI * f * t));
        } else if (bar == 2) {
            double f = 1800.0 + 400.0 * sin(2.0 * M_PI * 18.0 * t);
            s = 0.4f * sinf((float)(2.0 * M_PI * f * t));
        } else if (bar == 3) {
            double f = ((int)(t * 40.0) % 2) ? 3200.0 : 1400.0;
            s = 0.3f * sinf((float)(2.0 * M_PI * f * t));
        } else if (bar == 4 || bar == 5) {
            double f0 = 500.0 + 150.0 * sin(2.0 * M_PI * 2.5 * t);
            s = 0.28f * sinf((float)(2.0 * M_PI * f0 * t));
            s += 0.12f * sinf((float)(2.0 * M_PI * f0 * 2.0 * t));
        } else {
            double f = 900.0 + 3500.0 * fmod(t * 1.4, 1.0);
            s = 0.22f * sinf((float)(2.0 * M_PI * f * t));
            if (fmod(t * 12.0, 1.0) < 0.15) s += 0.2f * ((float)rand() / RAND_MAX * 2.0f - 1.0f);
        }
        dst[i] = s;
    }
    return n;
}

#ifdef HAVE_ALSA
static int alsa_open(Audio *a, const char *dev) {
    snd_pcm_t *pcm = NULL;
    const char *name = (dev && *dev) ? dev : "default";
    int err = snd_pcm_open(&pcm, name, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) return -1;
    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hw, 1);
    unsigned int rate = SS_SR;
    snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
    snd_pcm_uframes_t per = SS_HOP;
    snd_pcm_hw_params_set_period_size_near(pcm, hw, &per, 0);
    err = snd_pcm_hw_params(pcm, hw);
    if (err < 0) {
        snd_pcm_close(pcm);
        return -1;
    }
    snd_pcm_prepare(pcm);
    a->pcm = pcm;
    a->kind = AUDIO_ALSA;
    a->src_rate = (int)rate;
    snprintf(a->label, sizeof(a->label), "alsa %s %uHz", name, rate);
    return 0;
}

static int alsa_read(Audio *a, float *dst, int n) {
    int16_t tmp[1024];
    int got = 0;
    while (got < n) {
        int chunk = n - got;
        if (chunk > 1024) chunk = 1024;
        snd_pcm_sframes_t r = snd_pcm_readi(a->pcm, tmp, (snd_pcm_uframes_t)chunk);
        if (r == -EPIPE) {
            snd_pcm_prepare(a->pcm);
            continue;
        }
        if (r < 0) return got;
        for (int i = 0; i < (int)r; i++)
            dst[got + i] = (float)tmp[i] / 32768.0f;
        got += (int)r;
    }
    return got;
}
#endif

static char g_err[160];

const char *audio_last_error(void) {
    return g_err[0] ? g_err : "no source";
}

static int looks_like_wav(const char *p) {
    if (!p) return 0;
    const char *dot = strrchr(p, '.');
    if (!dot) return 0;
    return !strcasecmp(dot, ".wav") || !strcasecmp(dot, ".wave");
}

static int looks_like_mp3(const char *p) {
    if (!p) return 0;
    const char *dot = strrchr(p, '.');
    if (!dot) return 0;
    return !strcasecmp(dot, ".mp3");
}

Audio *audio_open(const char *path_or_device, int want_alsa) {
    Audio *a = calloc(1, sizeof(*a));
    if (!a) {
        snprintf(g_err, sizeof(g_err), "out of memory");
        return NULL;
    }
    a->hold = 0;
    a->step = 1.0;
    g_err[0] = 0;

    if (path_or_device && *path_or_device && !want_alsa) {
        if (looks_like_mp3(path_or_device)) {
            if (mp3_open(a, path_or_device) == 0)
                return a;
            snprintf(g_err, sizeof(g_err), "cannot decode MP3: %s", path_or_device);
            free(a);
            return NULL;
        }
        if (wav_open(a, path_or_device) == 0)
            return a;
        if (mp3_open(a, path_or_device) == 0)
            return a;
        snprintf(g_err, sizeof(g_err), "not a WAV/MP3: %s", path_or_device);
        free(a);
        return NULL;
    }

    if (want_alsa) {
#ifdef HAVE_ALSA
        const char *dev = (path_or_device && *path_or_device) ? path_or_device : "default";
        if (alsa_open(a, dev) == 0)
            return a;
        snprintf(g_err, sizeof(g_err), "ALSA open failed: %s", dev);
#else
        snprintf(g_err, sizeof(g_err), "built without ALSA");
        (void)path_or_device;
#endif
        free(a);
        return NULL;
    }

    if (path_or_device && *path_or_device) {
        if (wav_open(a, path_or_device) == 0)
            return a;
        if (mp3_open(a, path_or_device) == 0)
            return a;
#ifdef HAVE_ALSA
        if (alsa_open(a, path_or_device) == 0)
            return a;
#endif
        snprintf(g_err, sizeof(g_err), "cannot open %s", path_or_device);
        free(a);
        return NULL;
    }

    snprintf(g_err, sizeof(g_err), "no file or mic selected");
    free(a);
    return NULL;
}

void audio_close(Audio *a) {
    if (!a) return;
    if (a->fp) fclose(a->fp);
    if (a->mp3) free(a->mp3);
#ifdef HAVE_ALSA
    if (a->pcm) snd_pcm_close(a->pcm);
#endif
    free(a);
}

int audio_read(Audio *a, float *dst, int n) {
    if (!a) return 0;
    switch (a->kind) {
        case AUDIO_WAV:   return wav_read(a, dst, n);
        case AUDIO_MP3:   return mp3_read(a, dst, n);
        case AUDIO_SYNTH: return synth_read(a, dst, n);
#ifdef HAVE_ALSA
        case AUDIO_ALSA:  return alsa_read(a, dst, n);
#endif
        default:          memset(dst, 0, (size_t)n * sizeof(float)); return n;
    }
}

AudioKind audio_kind(const Audio *a) { return a ? a->kind : AUDIO_NONE; }
const char *audio_label(const Audio *a) { return a ? a->label : "none"; }
