#ifndef SOUNDSPACE_AUDIO_H
#define SOUNDSPACE_AUDIO_H

#include "common.h"

typedef enum {
    AUDIO_NONE = 0,
    AUDIO_SYNTH,
    AUDIO_WAV,
    AUDIO_ALSA
} AudioKind;

typedef struct Audio Audio;

/* path: WAV file. want_alsa: open ALSA device (path or "default").
 * No silent synth fallback — returns NULL on failure. */
Audio *audio_open(const char *path_or_device, int want_alsa);
void   audio_close(Audio *a);
int    audio_read(Audio *a, float *dst, int n);
AudioKind audio_kind(const Audio *a);
const char *audio_label(const Audio *a);
const char *audio_last_error(void);

#endif
