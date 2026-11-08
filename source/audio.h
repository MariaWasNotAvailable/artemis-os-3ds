#ifndef AUDIO_H
#define AUDIO_H

#include <3ds.h>

typedef enum {
    SFX_STARTUP = 0,
    SFX_CLICK,
    SFX_DING,
    SFX_ERROR,
    SFX_FLAG,
    SFX_REVEAL,
    SFX_SHUTDOWN,
    SFX_COUNT
} SfxId;

void sfx_init(void);
void sfx_exit(void);
void sfx_play(SfxId id);
const char *sfx_status(void);

#endif /* AUDIO_H */
