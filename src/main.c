#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "common.h"
#include "fb.h"
#include "audio.h"
#include "analyze.h"
#include "cloud.h"
#include "draw.h"
#include "input.h"
#include "picker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

static volatile int g_run = 1;
static void on_sig(int s) { (void)s; g_run = 0; }

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static void usage(const char *argv0) {
    fprintf(stderr,
        "SoundSpace — 3D sound mapper (Linux framebuffer or SDL window)\n"
        "Usage: %s [options] [file.wav]\n"
        "  no args     open file picker (WAV + MIC)\n"
        "  file.wav    play that file (loops)\n"
        "  -m          live mic (ALSA default)\n"
        "  -d DEV      live ALSA device\n"
        "  --fb DEV    auto | /dev/fb0 | sdl\n"
        "  --geom WxH  window / software canvas size\n"
        "  -h          help\n"
        "\n"
        "Picker: arrows  enter/space open  left=up dir  o=picker again\n"
        "Viz: F1 mode  F2 pause  F3 hold  F4 clear  q quit\n",
        argv0);
}

int main(int argc, char **argv) {
    const char *fbdev = "auto";
    const char *source = NULL;
    int want_alsa = 0;
    int geom_w = 0, geom_h = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]);
            return 0;
        } else if (!strcmp(argv[i], "-m")) {
            want_alsa = 1;
        } else if (!strcmp(argv[i], "-d") && i + 1 < argc) {
            source = argv[++i];
            want_alsa = 1;
        } else if (!strcmp(argv[i], "--fb") && i + 1 < argc) {
            fbdev = argv[++i];
        } else if (!strcmp(argv[i], "--geom") && i + 1 < argc) {
            sscanf(argv[++i], "%dx%d", &geom_w, &geom_h);
        } else if (argv[i][0] != '-') {
            source = argv[i];
        }
    }

    signal(SIGINT, on_sig);
    signal(SIGTERM, on_sig);
    srand((unsigned)time(NULL));

    Fb fb;
    if (fb_open(&fb, fbdev, geom_w, geom_h) != 0) {
        fprintf(stderr, "soundspace: cannot allocate canvas\n");
        return 1;
    }

    analyze_init();
    spec_init(fb.w);
    Cloud cloud;
    cloud_init(&cloud);
    input_open();

    Picker pick;
    picker_init(&pick, ".");
    int picking = 1;
    Audio *au = NULL;

    if (source || want_alsa) {
        au = audio_open(source, want_alsa);
        if (au) {
            picking = 0;
        } else {
            snprintf(pick.status, sizeof(pick.status), "%s", audio_last_error());
            fprintf(stderr, "soundspace: %s\n", audio_last_error());
        }
    }

    int mode = MODE_SPACE;
    int paused = 0;
    float ring[SS_FFT_N];
    int ring_fill = 0;
    Features feat;
    memset(&feat, 0, sizeof(feat));

    double t_prev = now_sec();

    fprintf(stderr, "soundspace %dx%d scale=%d backend=%d\n",
            fb.w, fb.h, fb.scale, (int)fb.backend);

    while (g_run) {
        if (fb.want_quit) g_run = 0;
        if (fb.resized) {
            spec_init(fb.w);
            fb.resized = 0;
        }
        int k;
        while ((k = input_poll()) != SSKEY_NONE) {
            if (picking) {
                switch (k) {
                    case SSKEY_QUIT: g_run = 0; break;
                    case SSKEY_UP:   picker_up(&pick); break;
                    case SSKEY_DOWN: picker_down(&pick); break;
                    case SSKEY_LEFT: picker_parent(&pick); break;
                    case SSKEY_RIGHT:
                    case SSKEY_ENTER:
                    case SSKEY_MODE: {
                        char path[PICK_PATH];
                        int mic = 0;
                        int act = picker_activate(&pick, path, sizeof(path), &mic);
                        if (act == 1) {
                            Audio *next = audio_open(mic ? NULL : path, mic);
                            if (next) {
                                audio_close(au);
                                au = next;
                                picking = 0;
                                ring_fill = 0;
                                memset(&feat, 0, sizeof(feat));
                                cloud_clear(&cloud);
                                fprintf(stderr, "soundspace: %s\n", audio_label(au));
                            } else {
                                snprintf(pick.status, sizeof(pick.status), "%s", audio_last_error());
                            }
                        }
                        break;
                    }
                    default: break;
                }
                continue;
            }

            switch (k) {
                case SSKEY_QUIT:   g_run = 0; break;
                case SSKEY_OPEN:   picking = 1; picker_rescan(&pick); break;
                case SSKEY_MODE:   mode = (mode + 1) % MODE_COUNT; break;
                case SSKEY_PAUSE:  paused = !paused; break;
                case SSKEY_FREEZE: cloud.frozen = !cloud.frozen; break;
                case SSKEY_CLEAR:  cloud_clear(&cloud); break;
                case SSKEY_LEFT:   cloud.yaw   -= 0.12f; break;
                case SSKEY_RIGHT:  cloud.yaw   += 0.12f; break;
                case SSKEY_UP:     cloud.pitch += 0.08f; break;
                case SSKEY_DOWN:   cloud.pitch -= 0.08f; break;
                case SSKEY_ZOOM_IN:  cloud.zoom = clampf(cloud.zoom * 1.12f, 0.4f, 3.0f); break;
                case SSKEY_ZOOM_OUT: cloud.zoom = clampf(cloud.zoom / 1.12f, 0.4f, 3.0f); break;
                case SSKEY_GAIN_UP:  cloud.emit_gain = clampf(cloud.emit_gain * 1.15f, 0.2f, 6.0f); break;
                case SSKEY_GAIN_DN:  cloud.emit_gain = clampf(cloud.emit_gain / 1.15f, 0.2f, 6.0f); break;
                case SSKEY_LIFE_UP:  cloud.life_scale = clampf(cloud.life_scale * 1.15f, 0.3f, 4.0f); break;
                case SSKEY_LIFE_DN:  cloud.life_scale = clampf(cloud.life_scale / 1.15f, 0.3f, 4.0f); break;
            }
        }

        double t = now_sec();
        float dt = (float)(t - t_prev);
        if (dt < 0) dt = 0;
        if (dt > 0.1f) dt = 0.1f;
        t_prev = t;

        if (!paused && !picking && au) {
            float chunk[SS_HOP];
            int got = audio_read(au, chunk, SS_HOP);
            if (got > 0) {
                int room = SS_FFT_N - ring_fill;
                if (got > room) {
                    memmove(ring, ring + got - room, (size_t)room * sizeof(float));
                    memcpy(ring + room, chunk, (size_t)(got - (got > room ? got - room : 0)) );
                    /* simpler: shift and append */
                }
                if (ring_fill + got <= SS_FFT_N) {
                    memcpy(ring + ring_fill, chunk, (size_t)got * sizeof(float));
                    ring_fill += got;
                } else {
                    int keep = SS_FFT_N - got;
                    if (keep < 0) keep = 0;
                    if (keep) memmove(ring, ring + ring_fill - keep, (size_t)keep * sizeof(float));
                    memcpy(ring + keep, chunk, (size_t)(got < SS_FFT_N ? got : SS_FFT_N) * sizeof(float));
                    ring_fill = SS_FFT_N;
                }
                if (ring_fill >= SS_FFT_N) {
                    analyze_frame(ring, SS_FFT_N, &feat);
                    cloud_emit(&cloud, &feat);
                }
            }
        }

        cloud_tick(&cloud, dt);
        if (mode == MODE_SPACE && !paused && !cloud.frozen)
            cloud.yaw += dt * 0.12f;

        fb_clear(&fb, rgb565(6, 8, 14));
        if (picking) {
            draw_picker(&fb, &pick);
        } else {
            switch (mode) {
                case MODE_SPACE: draw_mode_space(&fb, &cloud, &feat); break;
                case MODE_SPEC:  draw_mode_spec(&fb, &feat); break;
                case MODE_WAVE:  draw_mode_wave(&fb, &feat); break;
                case MODE_FIELD: draw_mode_field(&fb, &cloud, &feat); break;
            }
            draw_hud(&fb, mode, &feat, au ? audio_label(au) : "none", paused, cloud.frozen);
        }
        fb_flip(&fb);

        /* cap around 30 fps — SPI LCD on 3.5" cannot do much more */
        double frame = now_sec() - t;
        if (frame < 1.0 / 30.0)
            usleep((useconds_t)((1.0 / 30.0 - frame) * 1e6));
    }

    input_close();
    analyze_free();
    audio_close(au);
    fb_close(&fb);
    return 0;
}
