/* Included by mixer.cpp, beside the existing MIXER command's volume state. */
#include "pidos_mixer_api.h"

/* Retain CD gains while media are unmounted, including boot-time MIXER commands. */
static const char *pidos_cd_names[] = {"CDAUDIO", "USBCD"};
static float pidos_cd_volume[2][2] = {{1, 1}, {1, 1}};

static int PiDosMixerCdSlot(const char *name)
{
    for (int i = 0; i < 2; ++i) if (!strcmp(name, pidos_cd_names[i])) return i;
    return -1;
}

static void PiDosMixerRememberCd(const char *name, float left, float right)
{
    const int slot = PiDosMixerCdSlot(name);
    if (slot >= 0) { pidos_cd_volume[slot][0] = left; pidos_cd_volume[slot][1] = right; }
}

static MixerChannel *PiDosMixerChannel(unsigned channel)
{
    static const char *names[] = {"MASTER", "SB", "FM", "GUS", "CMS", "SPKR",
                                  "TANDY", "TANDYDAC", "DISNEY", "SYNTH", "CDAUDIO", "USBCD"};
    if (!channel || channel >= PIDOS_MIXER_CHANNELS) return nullptr;
    MixerChannel *found = MIXER_FindChannel(names[channel]);
    if (!found && channel == 9) found = MIXER_FindChannel("MT32");
    return found;
}

static uint32_t PiDosMixerPack(float left, float right)
{
    const auto gain = [](float value) -> uint32_t {
        if (!(value > 0)) return 0;
        if (value >= 6.5534f) return 65534;
        return static_cast<uint32_t>(value * 10000.0f + 0.5f);
    };
    return gain(left) | (gain(right) << 16);
}

uint32_t PiDosMixerRead(uint32_t reg)
{
    if (reg == PIDOS_MIXER_INFO) return PIDOS_MIXER_VERSION;
    const unsigned channel = reg - PIDOS_MIXER_CHANNEL;
    if (channel >= PIDOS_MIXER_CHANNELS) return PIDOS_MIXER_MISSING;
    if (!channel) return PiDosMixerPack(mixer.mastervol[0], mixer.mastervol[1]);
    MixerChannel *found = PiDosMixerChannel(channel);
    return found ? PiDosMixerPack(found->volmain[0], found->volmain[1]) : PIDOS_MIXER_MISSING;
}

bool PiDosMixerWrite(uint32_t reg, uint32_t value)
{
    const unsigned channel = reg - PIDOS_MIXER_CHANNEL;
    const unsigned left = value & 0xffff, right = value >> 16;
    if (channel >= PIDOS_MIXER_CHANNELS || left == 65535 || right == 65535) return false;
    if (!channel) {
        MIXER_SetMaster(left / 10000.0f, right / 10000.0f);
        return true;
    }
    MixerChannel *found = PiDosMixerChannel(channel);
    if (!found) return false;
    found->SetVolume(left / 10000.0f, right / 10000.0f);
    return true;
}
