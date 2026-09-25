#include "fb.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>

#ifdef HAVE_SDL
#include <SDL.h>
static SDL_Window   *g_win = NULL;
static SDL_Renderer *g_ren = NULL;
static SDL_Texture  *g_tex = NULL;
#endif

static Fb *g_fb = NULL;

static int compute_scale(int w, int h) {
    int m = w < h ? w : h;
    int s = m / 240;
    if (s < 1) s = 1;
    if (s > 8) s = 8;
    return s;
}

static int alloc_canvas(Fb *fb, int w, int h) {
    if (w < 160) w = 160;
    if (h < 120) h = 120;
    if (w > 7680) w = 7680;
    if (h > 4320) h = 4320;
    uint16_t *p = realloc(fb->pix16, (size_t)w * (size_t)h * sizeof(uint16_t));
    if (!p) return -1;
    fb->pix16 = p;
    fb->w = w;
    fb->h = h;
    fb->scale = compute_scale(w, h);
    fb->portrait = h > w;
    memset(p, 0, (size_t)w * (size_t)h * sizeof(uint16_t));
    return 0;
}

#ifdef HAVE_SDL
static int sdl_make_texture(Fb *fb) {
    if (g_tex) { SDL_DestroyTexture(g_tex); g_tex = NULL; }
    g_tex = SDL_CreateTexture(g_ren, SDL_PIXELFORMAT_RGB565,
                              SDL_TEXTUREACCESS_STREAMING, fb->w, fb->h);
    return g_tex ? 0 : -1;
}

static int sdl_open(Fb *fb, int w, int h) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "soundspace: SDL_Init: %s\n", SDL_GetError());
        return -1;
    }
    if (w <= 0 || h <= 0) {
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w > 0 && dm.h > 0) {
            /* window ~ 2/3 of the desktop, min 640x400 */
            w = dm.w * 2 / 3;
            h = dm.h * 2 / 3;
        } else {
            w = 960;
            h = 640;
        }
    }
    if (w < 320) w = 320;
    if (h < 240) h = 240;
    g_win = SDL_CreateWindow("SoundSpace",
                             SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             w, h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);
    if (!g_win) {
        fprintf(stderr, "soundspace: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    g_ren = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_ren)
        g_ren = SDL_CreateRenderer(g_win, -1, 0);
    if (!g_ren) {
        fprintf(stderr, "soundspace: SDL_CreateRenderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(g_win);
        SDL_Quit();
        return -1;
    }
    int cw, ch;
    SDL_GetRendererOutputSize(g_ren, &cw, &ch);
    if (cw <= 0) cw = w;
    if (ch <= 0) ch = h;
    if (alloc_canvas(fb, cw, ch) != 0) return -1;
    if (sdl_make_texture(fb) != 0) return -1;
    fb->backend = FB_BACKEND_SDL;
    fb->bpp = 16;
    fb->stride = fb->w * 2;
    fprintf(stderr, "soundspace: SDL window %dx%d scale=%d\n", fb->w, fb->h, fb->scale);
    return 0;
}

static void sdl_close(void) {
    if (g_tex) SDL_DestroyTexture(g_tex);
    if (g_ren) SDL_DestroyRenderer(g_ren);
    if (g_win) SDL_DestroyWindow(g_win);
    g_tex = NULL; g_ren = NULL; g_win = NULL;
    SDL_Quit();
}

static int sdl_key(SDL_Keycode k) {
    switch (k) {
        case SDLK_ESCAPE:
        case SDLK_q:          return SSKEY_QUIT;
        case SDLK_TAB:
        case SDLK_m:
        case SDLK_SPACE:
        case SDLK_F1:         return SSKEY_MODE;
        case SDLK_p:
        case SDLK_F2:         return SSKEY_PAUSE;
        case SDLK_h:
        case SDLK_F3:         return SSKEY_FREEZE;
        case SDLK_c:
        case SDLK_F4:         return SSKEY_CLEAR;
        case SDLK_LEFT:       return SSKEY_LEFT;
        case SDLK_RIGHT:      return SSKEY_RIGHT;
        case SDLK_UP:         return SSKEY_UP;
        case SDLK_DOWN:       return SSKEY_DOWN;
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS:    return SSKEY_ZOOM_IN;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:   return SSKEY_ZOOM_OUT;
        case SDLK_RIGHTBRACKET:
        case SDLK_F5:         return SSKEY_GAIN_UP;
        case SDLK_LEFTBRACKET:
        case SDLK_F6:         return SSKEY_GAIN_DN;
        case SDLK_PERIOD:     return SSKEY_LIFE_UP;
        case SDLK_COMMA:      return SSKEY_LIFE_DN;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:   return SSKEY_ENTER;
        case SDLK_o:          return SSKEY_OPEN;
        default:              return 0;
    }
}
#endif

static int fbdev_open(Fb *fb, const char *path) {
    fb->fd = open(path, O_RDWR);
    if (fb->fd < 0) return -1;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    if (ioctl(fb->fd, FBIOGET_VSCREENINFO, &vinfo) < 0 ||
        ioctl(fb->fd, FBIOGET_FSCREENINFO, &finfo) < 0) {
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }
    int w = (int)vinfo.xres;
    int h = (int)vinfo.yres;
    if (w <= 0 || h <= 0) { close(fb->fd); fb->fd = -1; return -1; }
    fb->bpp = (int)vinfo.bits_per_pixel;
    fb->stride = (int)finfo.line_length;
    fb->map_size = (size_t)fb->stride * (size_t)h;
    if (fb->map_size < (size_t)w * (size_t)h * 2)
        fb->map_size = (size_t)w * (size_t)h * 4;
    fb->map = mmap(NULL, fb->map_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb->fd, 0);
    if (fb->map == MAP_FAILED) {
        fb->map = NULL;
        close(fb->fd);
        fb->fd = -1;
        return -1;
    }
    if (alloc_canvas(fb, w, h) != 0) return -1;
    fb->backend = FB_BACKEND_FBDEV;
    fprintf(stderr, "soundspace: fbdev %s %dx%d bpp=%d scale=%d\n",
            path, fb->w, fb->h, fb->bpp, fb->scale);
    return 0;
}

int fb_open(Fb *fb, const char *dev, int win_w, int win_h) {
    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;
    g_fb = fb;

    int force_sdl = 0, force_fb = 0;
    const char *fbpath = "/dev/fb0";
    if (dev && *dev) {
        if (!strcmp(dev, "sdl") || !strcmp(dev, "window") || !strcmp(dev, "x11"))
            force_sdl = 1;
        else if (!strcmp(dev, "auto") || !strcmp(dev, "default"))
            ;
        else {
            force_fb = 1;
            fbpath = dev;
        }
    }

    if (!force_sdl) {
        if (fbdev_open(fb, fbpath) == 0)
            return 0;
        if (force_fb)
            fprintf(stderr, "soundspace: cannot open %s\n", fbpath);
    }

#ifdef HAVE_SDL
    if (!force_fb) {
        if (sdl_open(fb, win_w, win_h) == 0)
            return 0;
    }
#else
    (void)win_w; (void)win_h;
#endif

    int w = win_w > 0 ? win_w : 960;
    int h = win_h > 0 ? win_h : 640;
    if (alloc_canvas(fb, w, h) != 0) return -1;
    fb->backend = FB_BACKEND_SOFT;
    fb->bpp = 16;
    fb->stride = fb->w * 2;
    fprintf(stderr, "soundspace: software canvas %dx%d scale=%d (no fb/SDL)\n",
            fb->w, fb->h, fb->scale);
    return 0;
}

void fb_close(Fb *fb) {
#ifdef HAVE_SDL
    if (fb->backend == FB_BACKEND_SDL)
        sdl_close();
#endif
    if (fb->pix16) free(fb->pix16);
    if (fb->map && fb->map != MAP_FAILED) munmap(fb->map, fb->map_size);
    if (fb->fd >= 0) close(fb->fd);
    memset(fb, 0, sizeof(*fb));
    fb->fd = -1;
    g_fb = NULL;
}

int fb_poll_key(void) {
#ifdef HAVE_SDL
    if (!g_fb || g_fb->backend != FB_BACKEND_SDL) return 0;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g_fb->want_quit = 1;
            return SSKEY_QUIT;
        }
        if (e.type == SDL_WINDOWEVENT &&
            (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
             e.window.event == SDL_WINDOWEVENT_RESIZED)) {
            int w = e.window.data1, h = e.window.data2;
            if (w > 0 && h > 0 && (w != g_fb->w || h != g_fb->h)) {
                if (alloc_canvas(g_fb, w, h) == 0) {
                    sdl_make_texture(g_fb);
                    g_fb->resized = 1;
                    fprintf(stderr, "soundspace: resize %dx%d scale=%d\n",
                            g_fb->w, g_fb->h, g_fb->scale);
                }
            }
        }
        if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            int k = sdl_key(e.key.keysym.sym);
            if (k) return k;
        }
    }
#else
    (void)g_fb;
#endif
    return 0;
}

void fb_clear(Fb *fb, uint16_t c) {
    uint16_t *p = fb->pix16;
    int n = fb->w * fb->h;
    for (int i = 0; i < n; i++) p[i] = c;
}

void fb_pixel(Fb *fb, int x, int y, uint16_t c) {
    if ((unsigned)x >= (unsigned)fb->w || (unsigned)y >= (unsigned)fb->h) return;
    fb->pix16[y * fb->w + x] = c;
}

void fb_blend(Fb *fb, int x, int y, uint16_t c, int a) {
    if ((unsigned)x >= (unsigned)fb->w || (unsigned)y >= (unsigned)fb->h) return;
    if (a >= 256) { fb->pix16[y * fb->w + x] = c; return; }
    if (a <= 0) return;
    uint16_t d = fb->pix16[y * fb->w + x];
    int dr = (d >> 11) & 31, dg = (d >> 5) & 63, db = d & 31;
    int sr = (c >> 11) & 31, sg = (c >> 5) & 63, sb = c & 31;
    int nr = (dr * (256 - a) + sr * a) >> 8;
    int ng = (dg * (256 - a) + sg * a) >> 8;
    int nb = (db * (256 - a) + sb * a) >> 8;
    fb->pix16[y * fb->w + x] = (uint16_t)((nr << 11) | (ng << 5) | nb);
}

void fb_hline(Fb *fb, int x, int y, int n, uint16_t c) {
    if ((unsigned)y >= (unsigned)fb->h) return;
    if (x < 0) { n += x; x = 0; }
    if (x + n > fb->w) n = fb->w - x;
    if (n <= 0) return;
    uint16_t *p = fb->pix16 + y * fb->w + x;
    for (int i = 0; i < n; i++) p[i] = c;
}

void fb_vline(Fb *fb, int x, int y, int n, uint16_t c) {
    if ((unsigned)x >= (unsigned)fb->w) return;
    if (y < 0) { n += y; y = 0; }
    if (y + n > fb->h) n = fb->h - y;
    if (n <= 0) return;
    uint16_t *p = fb->pix16 + y * fb->w + x;
    for (int i = 0; i < n; i++) { *p = c; p += fb->w; }
}

void fb_rect(Fb *fb, int x, int y, int w, int h, uint16_t c) {
    fb_hline(fb, x, y, w, c);
    fb_hline(fb, x, y + h - 1, w, c);
    fb_vline(fb, x, y, h, c);
    fb_vline(fb, x + w - 1, y, h, c);
}

void fb_fill(Fb *fb, int x, int y, int w, int h, uint16_t c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fb->w) w = fb->w - x;
    if (y + h > fb->h) h = fb->h - y;
    if (w <= 0 || h <= 0) return;
    for (int row = 0; row < h; row++)
        fb_hline(fb, x, y + row, w, c);
}

void fb_circle(Fb *fb, int cx, int cy, int r, uint16_t c) {
    int x = r, y = 0, err = 0;
    while (x >= y) {
        fb_pixel(fb, cx + x, cy + y, c);
        fb_pixel(fb, cx + y, cy + x, c);
        fb_pixel(fb, cx - y, cy + x, c);
        fb_pixel(fb, cx - x, cy + y, c);
        fb_pixel(fb, cx - x, cy - y, c);
        fb_pixel(fb, cx - y, cy - x, c);
        fb_pixel(fb, cx + y, cy - x, c);
        fb_pixel(fb, cx + x, cy - y, c);
        y++;
        if (err <= 0) err += 2 * y + 1;
        if (err > 0) { x--; err -= 2 * x + 1; }
    }
}

void fb_disc(Fb *fb, int cx, int cy, int r, uint16_t c) {
    if (r <= 0) { fb_pixel(fb, cx, cy, c); return; }
    for (int y = -r; y <= r; y++) {
        int xx = (int)sqrtf((float)(r * r - y * y));
        fb_hline(fb, cx - xx, cy + y, xx * 2 + 1, c);
    }
}

void fb_line(Fb *fb, int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb_pixel(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void fb_dim(Fb *fb, int keep) {
    int n = fb->w * fb->h;
    uint16_t *p = fb->pix16;
    for (int i = 0; i < n; i++) {
        uint16_t d = p[i];
        int r = ((d >> 11) & 31) * keep >> 8;
        int g = ((d >> 5) & 63) * keep >> 8;
        int b = (d & 31) * keep >> 8;
        p[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
}

void fb_flip(Fb *fb) {
#ifdef HAVE_SDL
    if (fb->backend == FB_BACKEND_SDL && g_tex && g_ren) {
        SDL_UpdateTexture(g_tex, NULL, fb->pix16, fb->w * (int)sizeof(uint16_t));
        SDL_RenderClear(g_ren);
        SDL_RenderCopy(g_ren, g_tex, NULL, NULL);
        SDL_RenderPresent(g_ren);
        return;
    }
#endif
    if (!fb->map) return;
    if (fb->bpp == 16) {
        if (fb->stride == fb->w * 2) {
            memcpy(fb->map, fb->pix16, (size_t)fb->w * (size_t)fb->h * 2);
        } else {
            for (int y = 0; y < fb->h; y++)
                memcpy(fb->map + (size_t)y * (size_t)fb->stride,
                       fb->pix16 + y * fb->w,
                       (size_t)fb->w * 2);
        }
    } else if (fb->bpp == 32) {
        for (int y = 0; y < fb->h; y++) {
            uint8_t *dst = fb->map + (size_t)y * (size_t)fb->stride;
            uint16_t *src = fb->pix16 + y * fb->w;
            for (int x = 0; x < fb->w; x++) {
                uint16_t c = src[x];
                dst[x * 4 + 0] = (uint8_t)((c & 31) << 3);
                dst[x * 4 + 1] = (uint8_t)(((c >> 5) & 63) << 2);
                dst[x * 4 + 2] = (uint8_t)(((c >> 11) & 31) << 3);
                dst[x * 4 + 3] = 0xFF;
            }
        }
    } else if (fb->bpp == 24) {
        for (int y = 0; y < fb->h; y++) {
            uint8_t *dst = fb->map + (size_t)y * (size_t)fb->stride;
            uint16_t *src = fb->pix16 + y * fb->w;
            for (int x = 0; x < fb->w; x++) {
                uint16_t c = src[x];
                dst[x * 3 + 0] = (uint8_t)((c & 31) << 3);
                dst[x * 3 + 1] = (uint8_t)(((c >> 5) & 63) << 2);
                dst[x * 3 + 2] = (uint8_t)(((c >> 11) & 31) << 3);
            }
        }
    }
}
