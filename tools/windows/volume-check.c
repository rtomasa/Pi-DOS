/* Exercise the actual panel state and byte-I/O transport in the real guest. */
#include <assert.h>
#include <conio.h>
#include <stdio.h>
#include "mixer_io.h"
#include "volume_state.h"
#include "config.h"
#include "forms.h"
#include "live.h"

static unsigned long pack(unsigned left, unsigned right)
{
    return left * 100UL | (right * 100UL << 16);
}

int main(void)
{
    PvVolume master, fm, midi, cd, usb;
    unsigned long original_master, original_fm, original_midi, original_cd, original_usb;
    char value[32];
    Field fields[FORM_MAX_FIELDS];
    unsigned mask = 0;
    int i, count;
    assert(pidos_mixer_read(PIDOS_MIXER_INFO) == PIDOS_MIXER_VERSION);
    original_master = pidos_mixer_read(PIDOS_MIXER_CHANNEL);
    original_fm = pidos_mixer_read(PIDOS_MIXER_CHANNEL + 2);
    original_midi = pidos_mixer_read(PIDOS_MIXER_CHANNEL + 9);
    original_cd = pidos_mixer_read(PIDOS_MIXER_CHANNEL + 10);
    original_usb = pidos_mixer_read(PIDOS_MIXER_CHANNEL + 11);
    assert(original_master == pack(70, 80));
    assert(original_fm == pack(60, 70));
    assert(original_midi == pack(75, 85));
    assert(original_cd == pack(55, 65));
    assert(original_usb == pack(45, 55));
    /* PIDOSCFG must list actual CD sources and omit absent cards. */
    assert(cfg_load("C:\\PIDOS.CFG"));
    live_update(1);
    count = form_fields(PAGE_MIXER, fields);
    for (i = 0; i < count; ++i) mask |= 1U << (fields[i].key - K_VOL_MASTER);
    assert((mask & 0xc01U) == 0xc01U);
    assert(!(mask & (1U << (K_VOL_DISNEY - K_VOL_MASTER))));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 3) == PIDOS_MIXER_MISSING);
    assert(!pidos_mixer_write(3, pack(50, 50)));
    assert(!pidos_mixer_write(0, PIDOS_MIXER_MISSING));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL) == original_master);

    pv_volume_init(&master, original_master);
    pv_volume_init(&fm, original_fm);
    pv_volume_init(&midi, original_midi);
    pv_volume_init(&cd, original_cd);
    pv_volume_init(&usb, original_usb);
    pv_volume_level(&cd, 20);
    pv_volume_level(&usb, 30);
    assert(pidos_mixer_write(10, pv_volume_value(&cd)));
    assert(pidos_mixer_write(11, pv_volume_value(&usb)));
    pv_volume_mute(&cd, 1);
    pv_volume_mute(&usb, 1);
    assert(pidos_mixer_write(10, pv_volume_value(&cd)));
    assert(pidos_mixer_write(11, pv_volume_value(&usb)));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 10) == 0);
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 11) == 0);
    pv_volume_mute(&cd, 0);
    pv_volume_mute(&usb, 0);
    assert(pidos_mixer_write(10, pv_volume_value(&cd)));
    assert(pidos_mixer_write(11, pv_volume_value(&usb)));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 10) == pack(20, 20));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 11) == pack(30, 30));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 2) == original_fm);
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 9) == original_midi);
    assert(pidos_mixer_write(10, cd.original));
    assert(pidos_mixer_write(11, usb.original));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 10) == original_cd);
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 11) == original_usb);
    pv_volume_level(&fm, 35);
    assert(pidos_mixer_write(2, pv_volume_value(&fm)));
    pv_volume_level(&midi, 40);
    assert(pidos_mixer_write(9, pv_volume_value(&midi)));
    pv_volume_mute(&midi, 1);
    assert(pidos_mixer_write(9, pv_volume_value(&midi)));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 9) == 0);
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 2) == pack(35, 35));
    pv_volume_level(&midi, 45); /* Moving a muted slider keeps the source muted. */
    assert(pv_volume_value(&midi) == 0);
    pv_volume_mute(&midi, 0);
    assert(pidos_mixer_write(9, pv_volume_value(&midi)));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 9) == pack(45, 45));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL) == original_master);
    assert(pidos_mixer_write(2, fm.original));
    assert(pidos_mixer_write(9, midi.original));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 2) == original_fm);
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL + 9) == original_midi);

    /* Save to the real shared configuration engine, preserving other edits. */
    assert(cfg_load("C:\\PIDOS.CFG"));
    pv_volume_level(&master, 55);
    assert(pidos_mixer_write(0, pv_volume_value(&master)));
    sprintf(value, "%u:%u", pv_volume_percent(&master), pv_volume_percent(&master));
    assert(cfg_set(K_VOL_MASTER, value));
    assert(cfg_set(K_VOL_MIDI, "45:45"));
    assert(cfg_set(K_VOL_CD, "20:20"));
    assert(cfg_set(K_VOL_USB_CD, "30:30"));
    assert(!cfg_set(K_VOL_CD, "101:100"));
    assert(!cfg_set(K_VOL_USB_CD, "100:101"));
    assert(cfg_save_changes("C:\\PIDOS.CFG"));
    assert(pidos_mixer_read(PIDOS_MIXER_CHANNEL) == pack(55, 55));

    pv_volume_init(&midi, 0);
    pv_volume_mute(&midi, 0);
    assert(pv_volume_value(&midi) == PIDOS_MIXER_UNITY);
    pv_volume_level(&midi, 200);
    assert(pv_volume_value(&midi) == PIDOS_MIXER_UNITY);
    assert(pv_volume_percent(&midi) == 100);
    pv_volume_init(&midi, pack(120, 150));
    assert(pv_volume_value(&midi) == PIDOS_MIXER_UNITY);
    assert(midi.original == pack(120, 150));
    puts("PASS: live master/FM/MIDI, independent mute, Cancel stereo restoration and Save");
    return 0;
}
