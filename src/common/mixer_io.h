#ifndef PIDOS_MIXER_IO_H
#define PIDOS_MIXER_IO_H
/* Include <conio.h> first. Shared by the Win16 panel and DOS guest check. */
#include "mixer_api.h"

static void pidos_mixer_select(unsigned long reg)
{
    unsigned i;
    outp(0x2a, 0x20); /* Preserve other integration-device clients' state. */
    outp(0x2a, 0);
    for (i = 0; i < 4; ++i) { outp(0x28, (unsigned)(reg & 255)); reg >>= 8; }
}

static unsigned long pidos_mixer_read(unsigned long reg)
{
    unsigned i;
    unsigned long value = 0;
    pidos_mixer_select(reg);
    for (i = 0; i < 4; ++i) value |= (unsigned long)inp(0x29) << (i * 8);
    outp(0x2a, 0x21);
    return value;
}

static int pidos_mixer_write(unsigned channel, unsigned long value)
{
    unsigned i;
    unsigned long data = value;
    pidos_mixer_select(PIDOS_MIXER_CHANNEL + channel);
    for (i = 0; i < 4; ++i) { outp(0x29, (unsigned)(data & 255)); data >>= 8; }
    i = inp(0x2a) & 2;
    outp(0x2a, 0x21);
    return !i && pidos_mixer_read(PIDOS_MIXER_CHANNEL + channel) == value;
}
#endif
