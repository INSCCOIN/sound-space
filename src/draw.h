#ifndef SOUNDSPACE_DRAW_H
#define SOUNDSPACE_DRAW_H

#include "fb.h"
#include "analyze.h"
#include "cloud.h"

void draw_text(Fb *fb, int x, int y, const char *s, uint16_t c);
int  draw_text_width(const Fb *fb, const char *s);
void draw_hud(Fb *fb, int mode, const Features *f, const char *src, int paused, int frozen);
void draw_mode_space(Fb *fb, Cloud *c, const Features *f);
void draw_mode_spec(Fb *fb, const Features *f);
void draw_mode_wave(Fb *fb, const Features *f);
void draw_mode_field(Fb *fb, Cloud *c, const Features *f);
void spec_init(int w);

#endif
