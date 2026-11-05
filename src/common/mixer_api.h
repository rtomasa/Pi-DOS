#ifndef PIDOS_MIXER_API_H
#define PIDOS_MIXER_API_H
/* DOSBox integration registers, accessed with four little-endian byte I/Os.
 * Each channel packs left/right gain in hundredths of a percent. */
#define PIDOS_MIXER_INFO 0x504d5800UL
#define PIDOS_MIXER_CHANNEL 0x504d5810UL
#define PIDOS_MIXER_VERSION 0x0001000cUL
#define PIDOS_MIXER_MISSING 0xffffffffUL
#define PIDOS_MIXER_CHANNELS 12
#define PIDOS_MIXER_UNITY 0x27102710UL
#endif
