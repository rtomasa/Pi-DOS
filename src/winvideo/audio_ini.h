#ifndef PI_DOS_AUDIO_INI_H
#define PI_DOS_AUDIO_INI_H
/* Replace only legacy sound VxD lines; preserve repeated unrelated devices. */
int pa_sound_vxd(const char *input, const char *output, const char *device);
/* Select an existing setup in a staged Windows 3.1 MIDIMAP.CFG. */
int pa_midi_default(const char *path, const char *name);
#endif
