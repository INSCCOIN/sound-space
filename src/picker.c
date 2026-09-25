#define _DEFAULT_SOURCE

#include "picker.h"
#include "draw.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

static int is_wav_name(const char *n) {
    const char *dot = strrchr(n, '.');
    if (!dot) return 0;
    return !strcasecmp(dot, ".wav") || !strcasecmp(dot, ".wave");
}

static int cmp_item(const void *a, const void *b) {
    const PickItem *x = a, *y = b;
    if (x->kind != y->kind) return (int)x->kind - (int)y->kind;
    return strcasecmp(x->name, y->name);
}

static void add_item(Picker *p, PickKind k, const char *name, const char *path) {
    if (p->count >= PICK_MAX) return;
    PickItem *it = &p->items[p->count++];
    it->kind = k;
    snprintf(it->name, sizeof(it->name), "%s", name);
    snprintf(it->path, sizeof(it->path), "%s", path);
}

void picker_rescan(Picker *p) {
    p->count = 0;
    p->sel = 0;
    p->scroll = 0;

    add_item(p, PICK_MIC, "MIC  (ALSA default)", "default");

    DIR *d = opendir(p->dir[0] ? p->dir : ".");
    if (!d) {
        snprintf(p->status, sizeof(p->status), "cannot read %s", p->dir);
        return;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;
        char full[PICK_PATH];
        snprintf(full, sizeof(full), "%s/%s", p->dir[0] ? p->dir : ".", de->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode))
            add_item(p, PICK_DIR, de->d_name, full);
        else if (S_ISREG(st.st_mode) && is_wav_name(de->d_name))
            add_item(p, PICK_WAV, de->d_name, full);
    }
    closedir(d);

    if (p->count > 1)
        qsort(p->items + 1, (size_t)(p->count - 1), sizeof(PickItem), cmp_item);

    int wavs = 0, dirs = 0;
    for (int i = 0; i < p->count; i++) {
        if (p->items[i].kind == PICK_WAV) wavs++;
        if (p->items[i].kind == PICK_DIR) dirs++;
    }
    snprintf(p->status, sizeof(p->status), "%d wav  %d dir   enter=open  o=this screen", wavs, dirs);
}

void picker_init(Picker *p, const char *start_dir) {
    memset(p, 0, sizeof(*p));
    if (start_dir && *start_dir)
        snprintf(p->dir, sizeof(p->dir), "%s", start_dir);
    else {
        if (!getcwd(p->dir, sizeof(p->dir)))
            snprintf(p->dir, sizeof(p->dir), ".");
    }
    picker_rescan(p);
}

int picker_up(Picker *p) {
    if (p->count <= 0) return 0;
    if (p->sel > 0) p->sel--;
    if (p->sel < p->scroll) p->scroll = p->sel;
    return 1;
}

int picker_down(Picker *p) {
    if (p->count <= 0) return 0;
    if (p->sel + 1 < p->count) p->sel++;
    return 1;
}

void picker_parent(Picker *p) {
    char *slash = strrchr(p->dir, '/');
    if (!slash || slash == p->dir) {
        snprintf(p->dir, sizeof(p->dir), "/");
    } else {
        *slash = 0;
        if (!p->dir[0]) snprintf(p->dir, sizeof(p->dir), "/");
    }
    picker_rescan(p);
}

int picker_activate(Picker *p, char *out_path, int out_sz, int *want_mic) {
    if (want_mic) *want_mic = 0;
    if (p->sel < 0 || p->sel >= p->count) return -1;
    PickItem *it = &p->items[p->sel];
    if (it->kind == PICK_MIC) {
        if (want_mic) *want_mic = 1;
        snprintf(out_path, out_sz, "default");
        return 1;
    }
    if (it->kind == PICK_DIR) {
        snprintf(p->dir, sizeof(p->dir), "%s", it->path);
        picker_rescan(p);
        return 0;
    }
    snprintf(out_path, out_sz, "%s", it->path);
    return 1;
}

void draw_picker(Fb *fb, const Picker *p) {
    uint16_t bg    = rgb565(6, 8, 14);
    uint16_t acc   = rgb565(80, 220, 180);
    uint16_t text  = rgb565(210, 220, 230);
    uint16_t muted = rgb565(110, 130, 150);
    uint16_t wavc  = rgb565(230, 200, 90);
    uint16_t dirc  = rgb565(120, 170, 230);
    uint16_t bar   = rgb565(16, 28, 36);
    (void)bg;

    int pad = ui(fb, 4);
    draw_text(fb, pad, ui(fb, 4), "SOUNDSPACE", acc);
    draw_text(fb, pad, ui(fb, 14), "open a sound", muted);

    char dirline[128];
    snprintf(dirline, sizeof(dirline), "%s", p->dir);
    int cw = 6 * (fb->scale > 0 ? fb->scale : 1);
    int maxc = (fb->w - ui(fb, 8)) / (cw > 0 ? cw : 6);
    if (maxc < 8) maxc = 8;
    if (maxc > 120) maxc = 120;
    if ((int)strlen(dirline) > maxc) {
        const char *s = dirline + strlen(dirline) - maxc + 1;
        draw_text(fb, pad, ui(fb, 24), s, muted);
    } else {
        draw_text(fb, pad, ui(fb, 24), dirline, muted);
    }

    int top = ui(fb, 36);
    int row_h = ui(fb, 12);
    int visible = (fb->h - top - 14) / row_h;
    if (visible < 3) visible = 3;

    int scroll = p->scroll;
    if (p->sel >= scroll + visible) scroll = p->sel - visible + 1;
    if (p->sel < scroll) scroll = p->sel;
    if (scroll < 0) scroll = 0;

    for (int i = 0; i < visible; i++) {
        int idx = scroll + i;
        if (idx >= p->count) break;
        int y = top + i * row_h;
        const PickItem *it = &p->items[idx];
        int selected = (idx == p->sel);
        if (selected)
            fb_fill(fb, ui(fb, 2), y - ui(fb, 2), fb->w - ui(fb, 4), row_h, bar);

        uint16_t col = text;
        const char *tag = "   ";
        if (it->kind == PICK_MIC) { col = acc;  tag = "mic"; }
        if (it->kind == PICK_DIR) { col = dirc; tag = "dir"; }
        if (it->kind == PICK_WAV) { col = wavc; tag = "wav"; }
        if (selected) col = rgb565(255, 255, 255);

        char line[88];
        snprintf(line, sizeof(line), "%s %-3s %s", selected ? ">" : " ", tag, it->name);
        if ((int)strlen(line) > maxc) line[maxc] = 0;
        draw_text(fb, pad, y, line, col);
    }

    if (p->count == 1)
        draw_text(fb, pad, top + row_h, "no .wav in this folder", muted);

    draw_text(fb, pad, fb->h - ui(fb, 10), p->status, muted);
}
