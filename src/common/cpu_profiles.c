/* Shared by the 8086 DOS setup utility and the native ARM64 boot summary. */
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "cpu_profiles.h"

const CpuProfile cpu_profiles[PROFILE_COUNT] = {
    {"auto", "Automatic", "auto", "dynamic_rec", "auto",
     "Automatically choose execution speed for the running program."},
    {"286-12", "286 12 MHz", "286", "normal", "fixed 1510",
     "A slower AT-class configuration for earlier DOS software."},
    {"386-33", "386DX 33 MHz", "386", "dynamic_rec", "fixed 6075",
     "A representative 386 configuration for DOS applications and games."},
    {"486-33", "486DX 33 MHz", "486", "dynamic_rec", "fixed 12010",
     "A lower-speed 486 configuration."},
    {"486-66", "486DX2 66 MHz", "486", "dynamic_rec", "fixed 23880",
     "A classic middle-ground 486 configuration."},
    {"486-100", "486DX4 100 MHz", "486", "dynamic_rec", "fixed 33445",
     "A faster 486 configuration."},
    {"pentium-75", "Pentium 75 MHz", "pentium", "dynamic_rec", "fixed 40072",
     "An entry-level Pentium configuration for DOS software."},
    {"pentium-100", "Pentium 100 MHz", "pentium", "dynamic_rec", "fixed 53430",
     "A representative Pentium configuration for later DOS software."},
    {"pentium-120", "Pentium 120 MHz", "pentium", "dynamic_rec", "fixed 64180",
     "A faster Pentium configuration for later DOS software."}
};

static int same(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a++) != tolower((unsigned char)*b++)) return 0;
    }
    return *a == *b;
}

static const char *cycle_value(const char *text)
{
    /* Both cycles=3000 and cycles=fixed 3000 mean a fixed budget. */
    if (strlen(text) >= 6 && tolower((unsigned char)text[0]) == 'f' &&
        tolower((unsigned char)text[1]) == 'i' &&
        tolower((unsigned char)text[2]) == 'x' &&
        tolower((unsigned char)text[3]) == 'e' &&
        tolower((unsigned char)text[4]) == 'd' && isspace((unsigned char)text[5])) {
        text += 6;
        while (isspace((unsigned char)*text)) ++text;
    }
    return text;
}

const CpuSpeed cpu_speeds[CPU_SPEED_COUNT] = {
    {"286",8,750}, {"286",12,1510}, {"286",16,2060}, {"286",20,2600}, {"286",25,3300},
    {"386",16,2870}, {"386",20,3640}, {"386",25,4595}, {"386",33,6075}, {"386",40,7390},
    {"486",25,9000}, {"486",33,12010}, {"486",50,18091}, {"486",66,23880},
    {"486",75,26225}, {"486",100,33445}, {"486",120,42064}, {"486",133,47810},
    {"pentium",60,32090}, {"pentium",66,35620}, {"pentium",75,40072},
    {"pentium",90,48087}, {"pentium",100,53430}, {"pentium",120,64180},
    {"pentium",133,71240}, {"pentium",150,80145}, {"pentium",166,88694},
    {"pentium",200,106860}
};

int cpu_speed_find(const char *type,int mhz)
{
    int i;
    for(i=0;i<CPU_SPEED_COUNT;++i)
        if(cpu_speeds[i].type && same(type,cpu_speeds[i].type) && cpu_speeds[i].mhz==mhz) return i;
    return -1;
}
int cpu_speed_from_cycles(const char *type,const char *cycles)
{
    int i; char *end; long value=strtol(cycle_value(cycles),&end,10);
    if(*end) return -1;
    for(i=0;i<CPU_SPEED_COUNT;++i)
        if(cpu_speeds[i].type && same(type,cpu_speeds[i].type) && cpu_speeds[i].cycles==value) return i;
    return -1;
}
int cpu_turbo_speed_find(const char *type,const char *normal_cycles,int mhz)
{
    int normal=cpu_speed_from_cycles(type,normal_cycles),turbo=cpu_speed_find(type,mhz);
    if(normal<0 || turbo<0 || cpu_speeds[turbo].mhz<=cpu_speeds[normal].mhz) return -1;
    return turbo;
}
const char *cpu_family_label(const char *type)
{
    if(same(type,"286")) return "286";
    if(same(type,"386")) return "386DX";
    if(same(type,"486")) return "486";
    if(same(type,"pentium")) return "Pentium";
    return "Automatic / custom";
}

int cpu_profile_find(const char *type, const char *core, const char *cycles)
{
    int i;
    for (i = 0; i < PROFILE_COUNT; ++i) {
        const CpuProfile *p = &cpu_profiles[i];
        if (same(type, p->type) && (!core || same(core, p->core)) &&
            same(cycle_value(cycles), cycle_value(p->cycles))) return i;
    }
    return -1;
}

int cpu_profile_default(void)
{
    int i;
    for (i = 0; i < PROFILE_COUNT; ++i)
        if (!strcmp(cpu_profiles[i].id, DEFAULT_CPU_PROFILE_ID)) return i;
    return 0;
}

int cpu_profile_for_board(const char *model)
{
    const char *id = DEFAULT_CPU_PROFILE_ID;
    int i;
    if (model && (!strncmp(model, "Raspberry Pi 3 ", 15) ||
                  !strncmp(model, "Raspberry Pi Compute Module 3", 29) ||
                  !strncmp(model, "Raspberry Pi Zero 2", 19) ||
                  !strncmp(model, "Raspberry Pi 2 ", 15))) id = "386-33";
    else if (model && (!strncmp(model, "Raspberry Pi 4 ", 15) ||
                       !strncmp(model, "Raspberry Pi 400 ", 17) ||
                       !strncmp(model, "Raspberry Pi Compute Module 4", 29))) id = "486-66";
    for (i = 0; i < PROFILE_COUNT; ++i)
        if (!strcmp(cpu_profiles[i].id, id)) return i;
    return cpu_profile_default();
}

const char *cpu_profile_recommendation(int profile)
{
    if (profile < 0 || profile >= PROFILE_COUNT) return "";
    if (!strcmp(cpu_profiles[profile].id, "386-33")) return " (Pi 2/3 default)";
    if (!strcmp(cpu_profiles[profile].id, "486-66")) return " (Pi 4 default)";
    if (!strcmp(cpu_profiles[profile].id, "pentium-120")) return " (Pi 5 default)";
    return "";
}
