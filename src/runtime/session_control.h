/* Included by misc/programs.cpp. Supervised speed-only Turbo. The editor's
 * C89 validation and speed tables are shared with both UIs.
 * GPL-2.0-or-later. */
#ifndef PIDOS_SESSION_CONTROL_H
#define PIDOS_SESSION_CONTROL_H
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include "cpu.h"

namespace pidos_settings {
#define same cpu_same
#include "pidos-config/cpu_profiles.c"
#undef same
#define same video_same
#include "pidos-config/video_profiles.c"
#undef same
#include "pidos-config/midi_profiles.c"
#include "pidos-config/config.c"
}

extern bool CPU_PiDosSetCycles(const std::string &cycles);

static bool pidos_turbo_initialized=false, pidos_turbo_on=false;
static int pidos_turbo_mhz=0;
static std::string pidos_normal_cycles, pidos_turbo_type, pidos_turbo_core;
static std::string pidos_turbo_text;
static std::chrono::steady_clock::time_point pidos_notice_end;

static bool pidos_session_enabled()
{
    const char *s=getenv("PI_DOS_SUPERVISED");
    return s && !strcmp(s,"1") && getppid()==1;
}
static Section_prop *pidos_cpu_section()
{ return static_cast<Section_prop *>(control->GetSection("cpu")); }

static void pidos_turbo_init()
{
    if(pidos_turbo_initialized || !pidos_session_enabled()) return;
    pidos_turbo_initialized=true;
    using namespace pidos_settings;
    if(!cfg_load("/dosdata/SYSTEM/PIDOS.CFG")) return;
    pidos_turbo_mhz=atoi(cfg_get(K_TURBO_MHZ));
    const int keys[]={K_SHORTCUT_REBOOT,K_SHORTCUT_POWEROFF,K_SHORTCUT_PAUSE,
        K_SHORTCUT_SCREENSHOT,K_SHORTCUT_NEXT_CD,K_SHORTCUT_NEXT_FLOPPY,K_SHORTCUT_TURBO};
    const char *env[]={"PI_DOS_SHORTCUT_REBOOT","PI_DOS_SHORTCUT_POWEROFF","PI_DOS_SHORTCUT_PAUSE",
        "PI_DOS_SHORTCUT_SCREENSHOT","PI_DOS_SHORTCUT_NEXT_CD","PI_DOS_SHORTCUT_NEXT_FLOPPY","PI_DOS_SHORTCUT_TURBO"};
    int selected[7]={};
    for(int i=0;i<7;i++) {
        int code=pidos_shortcut_key(cfg_get(keys[i]));
        if(code<0) code=pidos_shortcut_key(cfg_specs[keys[i]].default_value);
        for(int j=0;j<i;j++) if(code && selected[j]==code) code=0;
        selected[i]=code;
        char value[16]; snprintf(value,sizeof(value),"%d",code); setenv(env[i],value,1);
    }
}
void PiDosSessionPrepare() { pidos_turbo_init(); }
const char *PiDosTurboText()
{
    return std::chrono::steady_clock::now()<pidos_notice_end ? pidos_turbo_text.c_str() : "";
}
void PiDosTurboToggle()
{
    pidos_turbo_init();
    if(!pidos_session_enabled()) return;
    using namespace pidos_settings;
    Section_prop *cpu=pidos_cpu_section();
    const std::string type=cpu->Get_string("cputype"), core=cpu->Get_string("core");
    const std::string current=cpu->Get_prop("cycles")->GetValue().ToString();
    int speed=cpu_speed_find(type.c_str(),pidos_turbo_mhz);
    char turbo_cycles[40]={};
    if(speed>=0) snprintf(turbo_cycles,sizeof(turbo_cycles),"fixed %ld",cpu_speeds[speed].cycles);
    /* External CONFIG/core changes invalidate a previous toggle pair. */
    if(pidos_turbo_on && (type!=pidos_turbo_type || core!=pidos_turbo_core || current!=turbo_cycles))
        pidos_turbo_on=false;
    speed=cpu_turbo_speed_find(type.c_str(),(pidos_turbo_on?pidos_normal_cycles:current).c_str(),pidos_turbo_mhz);
    if(speed<0) {
        pidos_turbo_on=false;
        pidos_turbo_text="TURBO DISABLED";
    } else {
        if(!pidos_turbo_on) {
            pidos_normal_cycles=cpu->Get_prop("cycles")->GetValue().ToString();
            pidos_turbo_type=type; pidos_turbo_core=core;
        }
        const std::string cycles=pidos_turbo_on ? pidos_normal_cycles : turbo_cycles;
        if(!CPU_PiDosSetCycles(cycles)) return;
        pidos_turbo_on=!pidos_turbo_on;
        const int active=cpu_speed_from_cycles(type.c_str(),cycles.c_str());
        char label[100];
        if(active>=0) snprintf(label,sizeof(label),"TURBO %s - %s %d MHz",pidos_turbo_on?"ON":"OFF",
            cpu_family_label(type.c_str()),cpu_speeds[active].mhz);
        else snprintf(label,sizeof(label),"TURBO OFF - %s CUSTOM SPEED",cpu_family_label(type.c_str()));
        pidos_turbo_text=label;
    }
    pidos_notice_end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
}

#endif
