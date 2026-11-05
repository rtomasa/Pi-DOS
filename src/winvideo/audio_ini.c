#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "audio_ini.h"

static unsigned midi_word(const unsigned char *data)
{
    return data[0] | ((unsigned)data[1] << 8);
}

int pa_midi_default(const char *path, const char *name)
{
    unsigned char header[14], directory[4], entry[54];
    unsigned offset, capacity, count, i;
    unsigned long data;
    long size;
    int ok = 0;
    FILE *file;
    if (!*name || strlen(name) >= 16) return 0;
    file = fopen(path, "r+b");
    if (!file) return 0;
    if (fseek(file, 0, SEEK_END) || (size = ftell(file)) < 14 ||
            fseek(file, 0, SEEK_SET) || fread(header, 1, 14, file) != 14 || midi_word(header) != 1) goto done;
    offset = midi_word(header + 8);
    if (offset < 14 || (long)offset + 4 > size || fseek(file, offset, SEEK_SET) ||
            fread(directory, 1, 4, file) != 4) goto done;
    capacity = midi_word(directory); count = midi_word(directory + 2);
    if (!capacity || capacity > 100 || count > capacity ||
            offset + 4UL + capacity * 54UL > (unsigned long)size) goto done;
    for (i = 0; i < capacity; ++i) {
        if (fread(entry, 1, 54, file) != 54) goto done;
        if (!memchr(entry, 0, 16) || strcmp((const char *)entry, name)) continue;
        data = midi_word(entry + 50) | ((unsigned long)midi_word(entry + 52) << 16);
        if (!midi_word(entry + 48) || data < offset + 4UL + capacity * 54UL ||
                data > (unsigned long)size || (unsigned long)size - data < 644) goto done;
        /* Header bytes 6-7 hold the current setup ID, not its list index. */
        if (!fseek(file, 6, SEEK_SET) && fwrite(entry + 48, 1, 2, file) == 2) ok = 1;
        break;
    }
done:
    if (fclose(file)) ok = 0;
    return ok;
}

static int equal(const char *a, const char *b)
{
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { ++a; ++b; }
    return !*a && !*b;
}

int pa_sound_vxd(const char *input, const char *output, const char *device)
{
    FILE *in = fopen(input, "rb"), *out;
    static char line[1024], token[1024];
    char *p, *end, *base;
    int enhanced = 0, seen = 0, ok = 1;
    if (!in) return 0;
    out = fopen(output, "wb");
    if (!out) { fclose(in); return 0; }
    while (fgets(line, sizeof(line), in)) {
        /* Fail rather than split and alter an unusually long INI line. */
        if (!strchr(line, '\n') && !feof(in)) { ok = 0; break; }
        strcpy(token, line);
        p = token;
        while (isspace((unsigned char)*p)) ++p;
        if (*p == '[') {
            if (enhanced && device && *device && !seen) {
                if (fprintf(out, "device=%s\r\n", device) < 0) ok = 0;
                seen = 1;
            }
            end = strchr(p, ']');
            if (end) *end = 0;
            enhanced = end && equal(p + 1, "386Enh");
        } else if (enhanced) {
            end = strchr(p, '=');
            if (end) {
                *end++ = 0;
                base = p + strlen(p);
                while (base > p && isspace((unsigned char)base[-1])) *--base = 0;
                if (equal(p, "device")) {
                    while (isspace((unsigned char)*end)) ++end;
                    p = end;
                    while (*p && *p != ';' && !isspace((unsigned char)*p)) ++p;
                    *p = 0;
                    if (*end == '"') { ++end; p = strchr(end, '"'); if (p) *p = 0; }
                    base = strrchr(end, '\\'); base = base ? base + 1 : end;
                    if (equal(base, "vsbpd.386") || equal(base, "vsbd.386") ||
                            equal(base, "vadlibd.386")) continue;
                }
            }
        }
        if (fputs(line, out) == EOF) ok = 0;
    }
    if (ferror(in)) ok = 0;
    if (device && *device && !seen) {
        if (!enhanced && fputs("\r\n[386Enh]\r\n", out) == EOF) ok = 0;
        if (fprintf(out, "\r\ndevice=%s\r\n", device) < 0) ok = 0;
    }
    fclose(in);
    if (fclose(out)) ok = 0;
    if (!ok) remove(output);
    return ok;
}
