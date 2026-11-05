#ifndef PIDOS_VOLUME_STATE_H
#define PIDOS_VOLUME_STATE_H
#include "mixer_api.h"
typedef struct {
    unsigned long original, level, applied;
    int present, muted, changed;
} PvVolume;
void pv_volume_init(PvVolume *state, unsigned long value);
unsigned long pv_volume_value(const PvVolume *state);
void pv_volume_level(PvVolume *state, unsigned percent);
void pv_volume_mute(PvVolume *state, int mute);
unsigned pv_volume_percent(const PvVolume *state);
#endif
