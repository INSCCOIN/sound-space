#ifndef SOUNDSPACE_INPUT_H
#define SOUNDSPACE_INPUT_H

#include "common.h"

enum {
    SSKEY_NONE = 0,
    SSKEY_QUIT,
    SSKEY_MODE,
    SSKEY_PAUSE,
    SSKEY_FREEZE,
    SSKEY_CLEAR,
    SSKEY_LEFT,
    SSKEY_RIGHT,
    SSKEY_UP,
    SSKEY_DOWN,
    SSKEY_ZOOM_IN,
    SSKEY_ZOOM_OUT,
    SSKEY_GAIN_UP,
    SSKEY_GAIN_DN,
    SSKEY_LIFE_UP,
    SSKEY_LIFE_DN,
    SSKEY_ENTER,
    SSKEY_OPEN
};

int  input_open(void);
void input_close(void);
int  input_poll(void);   /* one mapped key, or KEY_NONE */

#endif
