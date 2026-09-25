#ifndef SOUNDSPACE_PICKER_H
#define SOUNDSPACE_PICKER_H

#include "fb.h"

#define PICK_MAX  96
#define PICK_PATH 256

typedef enum {
    PICK_MIC = 0,
    PICK_DIR,
    PICK_WAV
} PickKind;

typedef struct {
    PickKind kind;
    char name[80];
    char path[PICK_PATH];
} PickItem;

typedef struct {
    char dir[PICK_PATH];
    PickItem items[PICK_MAX];
    int count;
    int sel;
    int scroll;
    char status[96];
} Picker;

void picker_init(Picker *p, const char *start_dir);
void picker_rescan(Picker *p);
int  picker_up(Picker *p);
int  picker_down(Picker *p);
/* 1 = open this (fills out_path). 0 = entered a dir. -1 = nothing */
int  picker_activate(Picker *p, char *out_path, int out_sz, int *want_mic);
void picker_parent(Picker *p);
void draw_picker(Fb *fb, const Picker *p);

#endif
