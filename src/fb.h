#ifndef SOUNDSPACE_FB_H
#define SOUNDSPACE_FB_H

#include "common.h"

typedef enum {
    FB_BACKEND_SOFT = 0,
    FB_BACKEND_FBDEV,
    FB_BACKEND_SDL
} FbBackend;

typedef struct {
    int fd;
    int w, h;
    int bpp;
    int stride;          /* bytes per row (fbdev) */
    int scale;           /* UI scale, 1 on ~320p, 2 on ~640p, ... */
    uint8_t *map;
    size_t map_size;
    uint16_t *pix16;     /* software canvas, always RGB565, w*h */
    int portrait;
    FbBackend backend;
    int resized;         /* set when window size changes */
    int want_quit;
} Fb;

/* dev: NULL/"auto" = fb0 then SDL then software
 *      "/dev/fb0"   = framebuffer only
 *      "sdl"        = window only
 * win_w/win_h: 0 = sensible default / native */
int  fb_open(Fb *fb, const char *dev, int win_w, int win_h);
void fb_close(Fb *fb);
int  fb_poll_key(void);          /* SSKEY_* or 0; also handles resize/quit */
void fb_clear(Fb *fb, uint16_t c);
void fb_pixel(Fb *fb, int x, int y, uint16_t c);
void fb_blend(Fb *fb, int x, int y, uint16_t c, int alpha);
void fb_hline(Fb *fb, int x, int y, int n, uint16_t c);
void fb_vline(Fb *fb, int x, int y, int n, uint16_t c);
void fb_rect(Fb *fb, int x, int y, int w, int h, uint16_t c);
void fb_fill(Fb *fb, int x, int y, int w, int h, uint16_t c);
void fb_circle(Fb *fb, int cx, int cy, int r, uint16_t c);
void fb_disc(Fb *fb, int cx, int cy, int r, uint16_t c);
void fb_line(Fb *fb, int x0, int y0, int x1, int y1, uint16_t c);
void fb_flip(Fb *fb);
void fb_dim(Fb *fb, int keep);

static inline int ui(const Fb *fb, int v) {
    return v * (fb->scale > 0 ? fb->scale : 1);
}

#endif
