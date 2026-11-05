#include <string.h>
#include "volume_state.h"

void pv_volume_init(PvVolume *state, unsigned long value)
{
    memset(state, 0, sizeof(*state));
    state->present = value != PIDOS_MIXER_MISSING;
    state->original = state->applied = value;
    state->muted = value == 0;
    state->level = value ? value : PIDOS_MIXER_UNITY;
}

unsigned long pv_volume_value(const PvVolume *state)
{
    unsigned long left = state->level & 0xffffUL, right = state->level >> 16;
    if (left > 10000UL) left = 10000UL;
    if (right > 10000UL) right = 10000UL;
    return state->muted ? 0 : left | (right << 16);
}

void pv_volume_level(PvVolume *state, unsigned percent)
{
    unsigned long gain = (percent > 100 ? 100 : percent) * 100UL;
    state->level = gain | (gain << 16);
    state->changed = 1;
}

void pv_volume_mute(PvVolume *state, int mute)
{
    state->muted = mute != 0;
    state->changed = 1;
}

unsigned pv_volume_percent(const PvVolume *state)
{
    unsigned percent = (unsigned)(((state->level & 0xffffUL) + (state->level >> 16) + 100UL) / 200UL);
    return percent > 100 ? 100 : percent;
}
