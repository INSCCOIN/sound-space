#include "draw.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 5x7 font, bits top-to-bottom in each column-ish: row major 5 cols */
static const uint8_t FONT[96][5] = {
    {0,0,0,0,0}, /* space */
    {0x00,0x00,0x5F,0x00,0x00},
    {0x00,0x07,0x00,0x07,0x00},
    {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},
    {0x36,0x49,0x55,0x22,0x50},
    {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},
    {0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14},
    {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},
    {0x08,0x08,0x08,0x08,0x08},
    {0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},
    {0x00,0x42,0x7F,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},
    {0x00,0x36,0x36,0x00,0x00},
    {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},
    {0x14,0x14,0x14,0x14,0x14},
    {0x00,0x41,0x22,0x14,0x08},
    {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},
    {0x7E,0x11,0x11,0x11,0x7E},
    {0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},
    {0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},
    {0x20,0x40,0x41,0x3F,0x01},
    {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},
    {0x7F,0x04,0x08,0x10,0x7F},
    {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43},
    {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},
    {0x00,0x41,0x41,0x7F,0x00},
    {0x04,0x02,0x01,0x02,0x04},
    {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},
    {0x20,0x54,0x54,0x54,0x78},
    {0x7F,0x48,0x44,0x44,0x38},
    {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},
    {0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},
    {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78},
    {0x00,0x44,0x7D,0x40,0x00},
    {0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00},
    {0x7C,0x04,0x18,0x04,0x78},
    {0x7C,0x08,0x04,0x04,0x78},
    {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},
    {0x08,0x14,0x14,0x18,0x7C},
    {0x7C,0x08,0x04,0x04,0x08},
    {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},
    {0x1C,0x20,0x40,0x20,0x1C},
    {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},
    {0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},
    {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x77,0x00,0x00},
    {0x00,0x41,0x36,0x08,0x00},
    {0x08,0x04,0x08,0x10,0x08},
    {0x00,0x00,0x00,0x00,0x00},
};

void draw_text(Fb *fb, int x, int y, const char *s, uint16_t c) {
    int scli = fb->scale > 0 ? fb->scale : 1;
    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t *g = FONT[ch - 32];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1u << row)) {
                    if (scli == 1)
                        fb_pixel(fb, x + col, y + row, c);
                    else
                        fb_fill(fb, x + col * scli, y + row * scli, scli, scli, c);
                }
            }
        }
        x += 6 * scli;
    }
}

int draw_text_width(const Fb *fb, const char *s) {
    int scli = fb && fb->scale > 0 ? fb->scale : 1;
    return (int)strlen(s) * 6 * scli;
}

static uint16_t C_BG    = 0;
static uint16_t C_DIM   = 0;
static uint16_t C_GRID  = 0;
static uint16_t C_TEXT  = 0;
static uint16_t C_ACC   = 0;
static uint16_t C_MUTED = 0;

static void colors_init(void) {
    static int once;
    if (once) return;
    once = 1;
    C_BG    = rgb565(6, 8, 14);
    C_DIM   = rgb565(14, 18, 28);
    C_GRID  = rgb565(28, 36, 52);
    C_TEXT  = rgb565(210, 220, 230);
    C_ACC   = rgb565(80, 220, 180);
    C_MUTED = rgb565(110, 130, 150);
}

static const char *mode_name(int m) {
    switch (m) {
        case MODE_SPACE: return "SPACE";
        case MODE_SPEC:  return "SPEC";
        case MODE_WAVE:  return "WAVE";
        case MODE_FIELD: return "FIELD";
        default:         return "?";
    }
}

void draw_hud(Fb *fb, int mode, const Features *f, const char *src, int paused, int frozen) {
    colors_init();
    int pad = ui(fb, 4);
    int y = ui(fb, 3);
    char buf[96];
    draw_text(fb, pad, y, "SOUNDSPACE", C_ACC);
    snprintf(buf, sizeof(buf), "%s", mode_name(mode));
    draw_text(fb, fb->w - draw_text_width(fb, buf) - pad, y, buf, C_TEXT);
    y = ui(fb, 13);
    snprintf(buf, sizeof(buf), "pk %.0fHz  cen %.0f  rms %.3f", f->peak_hz, f->centroid_hz, f->rms);
    draw_text(fb, pad, y, buf, C_MUTED);
    y = fb->h - ui(fb, 10);
    snprintf(buf, sizeof(buf), "%s%s%s", src ? src : "", paused ? "  PAUSE" : "", frozen ? "  HOLD" : "");
    int maxc = fb->w / (6 * (fb->scale > 0 ? fb->scale : 1));
    if (maxc < 8) maxc = 8;
    if ((int)strlen(buf) > maxc) buf[maxc] = 0;
    draw_text(fb, pad, y, buf, C_MUTED);
}

/* ---- 3D project ---- */
static void project(const Cloud *c, float x, float y, float z, int cx, int cy, int *sx, int *sy, float *scale) {
    float cyw = cosf(c->yaw), syw = sinf(c->yaw);
    float cpi = cosf(c->pitch), spi = sinf(c->pitch);
    float x1 = x * cyw - z * syw;
    float z1 = x * syw + z * cyw;
    float y1 = y * cpi - z1 * spi;
    float z2 = y * spi + z1 * cpi;
    float persp = 2.2f / (2.6f + z2);
    persp *= c->zoom;
    *scale = persp;
    *sx = cx + (int)(x1 * persp * (float)(cx - 8));
    *sy = cy - (int)(y1 * persp * (float)(cy - 18));
}

static void draw_box(Fb *fb, Cloud *c, int cx, int cy) {
    const float e = 1.05f;
    float corners[8][3] = {
        {-e,-e,-e},{e,-e,-e},{e,e,-e},{-e,e,-e},
        {-e,-e, e},{e,-e, e},{e,e, e},{-e,e, e}
    };
    int p[8][2];
    for (int i = 0; i < 8; i++) {
        float s;
        project(c, corners[i][0], corners[i][1], corners[i][2], cx, cy, &p[i][0], &p[i][1], &s);
    }
    static const int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (int i = 0; i < 12; i++)
        fb_line(fb, p[edges[i][0]][0], p[edges[i][0]][1],
                    p[edges[i][1]][0], p[edges[i][1]][1], C_GRID);
}

void draw_mode_space(Fb *fb, Cloud *c, const Features *f) {
    colors_init();
    int top = ui(fb, 24);
    int bot = fb->h - ui(fb, 14);
    int cx = fb->w / 2;
    int cy = (top + bot) / 2;
    draw_box(fb, c, cx, cy);

    /* faint links between nearby live particles */
    int shown = 0;
    int idx[SS_MAX_PARTICLES];
    for (int i = 0; i < SS_MAX_PARTICLES; i++)
        if (c->p[i].alive) idx[shown++] = i;

    int links = shown < 80 ? shown : 80;
    for (int a = 0; a < links; a++) {
        Particle *pa = &c->p[idx[a]];
        float best = 0.18f;
        int bj = -1;
        for (int b = a + 1; b < links; b++) {
            Particle *pb = &c->p[idx[b]];
            float dx = pa->x - pb->x, dy = pa->y - pb->y, dz = pa->z - pb->z;
            float d = dx*dx + dy*dy + dz*dz;
            if (d < best) { best = d; bj = b; }
        }
        if (bj >= 0) {
            int x0,y0,x1,y1; float s0,s1;
            project(c, pa->x, pa->y, pa->z, cx, cy, &x0, &y0, &s0);
            Particle *pb = &c->p[idx[bj]];
            project(c, pb->x, pb->y, pb->z, cx, cy, &x1, &y1, &s1);
            fb_line(fb, x0, y0, x1, y1, rgb565(30, 50, 70));
        }
    }

    for (int i = 0; i < SS_MAX_PARTICLES; i++) {
        Particle *p = &c->p[i];
        if (!p->alive) continue;
        int sx, sy; float sc;
        project(c, p->x, p->y, p->z, cx, cy, &sx, &sy, &sc);
        int r = (int)((1.0f + p->amp * 14.0f * sc + p->life * 1.5f) * (float)fb->scale);
        if (r < 1) r = 1;
        if (r > 7 * fb->scale) r = 7 * fb->scale;
        int a = (int)(p->life * 256.0f);
        if (r <= 1) fb_blend(fb, sx, sy, p->color, a);
        else {
            fb_disc(fb, sx, sy, r, p->color);
            fb_pixel(fb, sx, sy, rgb565(255, 255, 255));
        }
    }

    /* live cursor at current feature position */
    float cxv = logf(f->centroid_hz + 80.0f);
    float pxv = logf(f->peak_hz + 80.0f);
    float x = clampf((cxv - logf(200.0f)) / (logf(6000.0f) - logf(200.0f)), 0, 1) * 2 - 1;
    float y = clampf((pxv - logf(200.0f)) / (logf(7000.0f) - logf(200.0f)), 0, 1) * 2 - 1;
    float z = clampf(f->slope, -1, 1);
    int sx, sy; float sc;
    project(c, x, y, z, cx, cy, &sx, &sy, &sc);
    fb_circle(fb, sx, sy, ui(fb, 4), C_ACC);
}

/* scrolling spectrogram: columns of log-frequency bins */
#define SPEC_COLS 160
static uint8_t spec_col[SPEC_COLS][64];
static int spec_head;
static int spec_ready;

void spec_init(int w) {
    (void)w;
    memset(spec_col, 0, sizeof(spec_col));
    spec_head = 0;
    spec_ready = 1;
}

void draw_mode_spec(Fb *fb, const Features *f) {
    colors_init();
    if (!spec_ready) spec_init(fb->w);
    int bins = 64;
    uint8_t col[64];
    for (int i = 0; i < bins; i++) {
        /* log-ish bin grouping */
        float t0 = (float)i / (float)bins;
        float t1 = (float)(i + 1) / (float)bins;
        int k0 = 1 + (int)(powf(t0, 1.6f) * (SS_BINS - 2));
        int k1 = 1 + (int)(powf(t1, 1.6f) * (SS_BINS - 2));
        if (k1 <= k0) k1 = k0 + 1;
        float m = 0;
        for (int k = k0; k < k1 && k < SS_BINS; k++) m += f->mag[k];
        m /= (float)(k1 - k0);
        float db = logf(m + 1e-5f);
        int v = (int)clampf((db + 4.0f) * 40.0f, 0, 255);
        col[i] = (uint8_t)v;
    }
    memcpy(spec_col[spec_head], col, (size_t)bins);
    spec_head = (spec_head + 1) % SPEC_COLS;

    int top = ui(fb, 24), bot = fb->h - ui(fb, 14);
    int left = ui(fb, 4), right = fb->w - ui(fb, 4);
    int cw = right - left;
    int ch = bot - top;
    int step = fb->scale > 1 ? fb->scale : 1;
    for (int x = 0; x < cw; x += step) {
        int src = spec_head - 1 - ((cw - 1 - x) * SPEC_COLS / (cw > 1 ? cw : 1));
        src %= SPEC_COLS;
        if (src < 0) src += SPEC_COLS;
        for (int y = 0; y < ch; y += step) {
            int b = y * bins / (ch > 0 ? ch : 1);
            if (b >= bins) b = bins - 1;
            b = bins - 1 - b;
            int v = spec_col[src][b];
            uint8_t r, g, bl;
            hsv_rgb(0.70f - (v / 255.0f) * 0.70f, 0.85f, 0.15f + v / 320.0f, &r, &g, &bl);
            fb_fill(fb, left + x, top + y, step, step, rgb565(r, g, bl));
        }
    }
}

void draw_mode_wave(Fb *fb, const Features *f) {
    colors_init();
    int top = ui(fb, 24), bot = fb->h / 2 + ui(fb, 4);
    int left = ui(fb, 4), right = fb->w - ui(fb, 4);
    int mid = (top + bot) / 2;
    fb_hline(fb, left, mid, right - left, C_GRID);
    int n = SS_FFT_N;
    int prevx = left, prevy = mid;
    for (int i = 0; i < n; i++) {
        int x = left + i * (right - left - 1) / (n - 1);
        int y = mid - (int)(f->wave[i] * (float)(bot - top) * 0.45f);
        fb_line(fb, prevx, prevy, x, y, C_ACC);
        prevx = x; prevy = y;
    }

    /* spectrum bars */
    int btop = bot + ui(fb, 8), bbot = fb->h - ui(fb, 16);
    int bars = fb->w >= 800 ? 72 : 48;
    int gap = (right - left) / bars;
    if (gap < 2) gap = 2;
    float maxm = 1e-6f;
    for (int k = 1; k < SS_BINS; k++) if (f->mag[k] > maxm) maxm = f->mag[k];
    for (int i = 0; i < bars; i++) {
        float t0 = (float)i / (float)bars;
        float t1 = (float)(i + 1) / (float)bars;
        int k0 = 1 + (int)(powf(t0, 1.5f) * (SS_BINS - 2));
        int k1 = 1 + (int)(powf(t1, 1.5f) * (SS_BINS - 2));
        if (k1 <= k0) k1 = k0 + 1;
        float m = 0;
        for (int k = k0; k < k1 && k < SS_BINS; k++) m += f->mag[k];
        m /= (float)(k1 - k0);
        int h = (int)((m / maxm) * (float)(bbot - btop));
        if (h < 1 && m > 0) h = 1;
        uint8_t r, g, b;
        hsv_rgb(0.66f - t0 * 0.66f, 0.8f, 0.8f, &r, &g, &b);
        fb_fill(fb, left + i * gap, bbot - h, gap - 1, h, rgb565(r, g, b));
    }
}

void draw_mode_field(Fb *fb, Cloud *c, const Features *f) {
    colors_init();
    int left = ui(fb, 28), right = fb->w - ui(fb, 8);
    int top = ui(fb, 26), bot = fb->h - ui(fb, 18);
    fb_rect(fb, left, top, right - left, bot - top, C_GRID);
    draw_text(fb, ui(fb, 4), (top + bot) / 2 - ui(fb, 3), "amp", C_MUTED);
    draw_text(fb, left, bot + ui(fb, 2), "centroid", C_MUTED);

    for (int i = 0; i < SS_MAX_PARTICLES; i++) {
        Particle *p = &c->p[i];
        if (!p->alive) continue;
        /* reuse x as centroid, amp as y */
        float nx = (p->x + 1.0f) * 0.5f;
        float ny = clampf(p->amp * 4.0f, 0, 1);
        int sx = left + (int)(nx * (right - left - 2));
        int sy = bot - 2 - (int)(ny * (bot - top - 4));
        fb_blend(fb, sx, sy, p->color, (int)(p->life * 220));
        if (p->amp > 0.08f) fb_pixel(fb, sx + 1, sy, p->color);
    }

    float nx = clampf((logf(f->centroid_hz + 80.0f) - logf(200.0f)) /
                      (logf(6000.0f) - logf(200.0f)), 0, 1);
    float ny = clampf(f->rms * 4.0f, 0, 1);
    int sx = left + (int)(nx * (right - left - 2));
    int sy = bot - 2 - (int)(ny * (bot - top - 4));
    fb_circle(fb, sx, sy, 3, C_ACC);
}
