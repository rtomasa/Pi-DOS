/* Included by misc/programs.cpp. Supervised DOS-session settings and speed-only
 * Turbo. The editor's C89 validation and speed tables are shared with both UIs.
 * GPL-2.0-or-later. */
#ifndef PIDOS_SESSION_CONTROL_H
#define PIDOS_SESSION_CONTROL_H
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include "cpu.h"
#include "shell.h"

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

extern void SBLASTER_OnReset(Section *);
extern void SBLASTER_DOS_Boot(Section *);
extern void GUS_DOS_Exit(Section *);
extern void GUS_ShutDown(Section *);
extern void GUS_OnReset(Section *);
extern void GUS_DOS_Boot(Section *);
extern void TANDYSOUND_ShutDown(Section *);
extern void TANDYSOUND_OnReset(Section *);
extern void PCSPEAKER_ShutDown(Section *);
extern void PCSPEAKER_OnReset(Section *);
extern void MIDI_OnSectionPropChange(Section *);
extern bool winrun;
extern bool CPU_PiDosSetCycles(const std::string &cycles);

static const char *pidos_session_file = "/run/pidos-session.cfg";
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
    if(!cfg_load(access(pidos_session_file,R_OK)==0 ? pidos_session_file : "/dosdata/SYSTEM/PIDOS.CFG")) return;
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

class PiDosApply : public Program {
    static void result(const char *text) {
        uint16_t handle=0;
        if(!DOS_CreateFile("C:\\TEMP\\PIDAPPLY.STA",0,&handle)) return;
        uint16_t size=(uint16_t)strlen(text);
        DOS_WriteFile(handle,(uint8_t *)text,&size); DOS_CloseFile(handle);
    }
    bool request() {
        using namespace pidos_settings;
        uint16_t handle=0;
        if(!DOS_OpenFile("C:\\TEMP\\PIDAPPLY.CFG",0,&handle)) return false;
        FILE *out=fopen("/run/pidos-apply-input.cfg","wb");
        bool ok=out!=nullptr;
        unsigned total=0; uint8_t buffer[1024];
        while(ok) {
            uint16_t size=sizeof(buffer);
            if(!DOS_ReadFile(handle,buffer,&size)) {ok=false;break;}
            if(!size) break;
            total+=size;
            if(total>24576 || memchr(buffer,0,size) || fwrite(buffer,1,size,out)!=size) {ok=false;break;}
        }
        DOS_CloseFile(handle);
        if(out && fclose(out)) ok=false;
        if(ok) ok=total && cfg_load("/run/pidos-apply-input.cfg");
        unlink("/run/pidos-apply-input.cfg");
        if(!ok) return false;
        /* Validate every applied value, including values loaded from disk. */
        for(int key=0;key<K_COUNT;key++) if(cfg_apply_key(key) && !cfg_set(key,cfg_get(key))) return false;
        if(!cfg_validate()) {WriteOut("%s\n",cfg_error); return false;}
        for(int key=0;key<K_COUNT;key++) if(cfg_apply_key(key) && strcmp(cfg_specs[key].section,"pidos")) {
            Section_prop *sec=static_cast<Section_prop *>(control->GetSection(cfg_specs[key].section));
            Property *prop=sec ? sec->Get_prop(cfg_specs[key].key) : nullptr;
            if(!prop) return false;
            const std::string old=prop->GetValue().ToString();
            const bool valid=prop->SetValue(cfg_get(key));
            prop->SetValue(old);
            if(!valid) return false;
        }
        return true;
    }
    bool restart_needed() {
        using namespace pidos_settings;
        const bool old286=!strcmp(pidos_cpu_section()->Get_string("cputype"),"286");
        const bool new286=!strcmp(cfg_get(K_CPU_TYPE),"286");
        if(old286!=new286) return true; /* BIOS entry-point instruction set changes. */
        for(int key=0;key<K_COUNT;key++) if(cfg_apply_key(key) && strcmp(cfg_specs[key].section,"pidos")) {
            Section_prop *sec=static_cast<Section_prop *>(control->GetSection(cfg_specs[key].section));
            Property *prop=sec ? sec->Get_prop(cfg_specs[key].key) : nullptr;
            if(!prop) return true;
            if((key==K_XMS || key==K_EMS || prop->getChange()==Property::Changeable::OnlyAtStart || !strcmp(cfg_specs[key].section,"voodoo")) &&
               prop->GetValue().ToString()!=cfg_get(key)) return true;
        }
        return false;
    }
public:
    void Run() override {
        using namespace pidos_settings;
        exit_status=1;
        if(cmd->FindExist("/QUERY",true)) {
            if(!pidos_session_enabled() || dos_kernel_disabled || winrun || cpu.pmode) return;
            FILE *in=fopen(pidos_session_file,"rb");
            uint16_t handle=0;
            if(!in) return;
            if(DOS_CreateFile("C:\\TEMP\\PIDLIVE.CFG",0,&handle)) {
                uint8_t buffer[1024]; size_t count;
                while((count=fread(buffer,1,sizeof(buffer),in))) {
                    uint16_t size=(uint16_t)count;
                    if(!DOS_WriteFile(handle,buffer,&size) || size!=count) break;
                }
                DOS_CloseFile(handle);
            }
            fclose(in); exit_status=0; return;
        }
        result("ERROR");
        if(!pidos_session_enabled() || dos_kernel_disabled || winrun || cpu.pmode) {
            WriteOut("Apply is available only in standalone Pi-DOS, outside Windows.\n"); return;
        }
        if(!request()) {WriteOut("Invalid or unavailable Apply request.\n"); return;}
        const bool restart=restart_needed();
        if(cmd->FindExist("/CHECK",true)) {
            result(restart ? "RESTART" : "LIVE"); exit_status=0; return;
        }
        if(cmd->GetCount()) return;
        if(!cfg_export_apply("/run/pidos-session.new")) return;
        if(rename("/run/pidos-session.new",pidos_session_file)) {unlink("/run/pidos-session.new");return;}
        if(restart) {
            const int marker=open("/run/pi-dos-session-restart",O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
            if(marker<0) return;
            close(marker);
            DOS_UnlinkFile("C:\\TEMP\\PIDAPPLY.CFG"); result("OK");
            WriteOut("Restarting the DOS session; the Raspberry Pi stays running.\n");
            throw 0;
        }
        bool sb=false,gus=false,speaker=false,midi=false;
        /* Stage CPU properties together to avoid transient core/model pairs.
         * CPU config callbacks run once, after all three have been validated. */
        Section_prop *cpusec=pidos_cpu_section();
        for(int key=0;key<3;key++) {
            Property *prop=cpusec->Get_prop(cfg_specs[key].key);
            if(!prop->SetValue(cfg_get(key))) return;
        }
        for(auto callback:cpusec->onpropchange) callback(cpusec);
        for(int key=K_SB_TYPE;key<=K_MIDI_DELAY;key++) {
            if(!cfg_apply_key(key) || !strcmp(cfg_specs[key].section,"pidos")) continue;
            const std::string section=cfg_specs[key].section;
            Section_prop *sec=static_cast<Section_prop *>(control->GetSection(section.c_str()));
            Property *prop=sec ? sec->Get_prop(cfg_specs[key].key) : nullptr;
            std::string value=cfg_get(key);
            if(key==K_MIDI_CONFIG && !strcmp(cfg_get(K_MIDI_DEVICE),"alsa") && !strcmp(cfg_get(K_MIDI_DELAY),"true")) value+=" delaysysex";
            if(!prop || prop->GetValue().ToString()==value) continue;
            if(!prop->SetValue(value)) return;
            if(section=="sblaster") sb=true;
            if(section=="gus") gus=true;
            if(section=="speaker") speaker=true;
            if(section=="midi") midi=true;
        }
        /* Tear down old port owners before constructing replacement devices. */
        if(gus) {GUS_DOS_Exit(nullptr);GUS_ShutDown(nullptr);}
        if(speaker) {TANDYSOUND_ShutDown(nullptr);PCSPEAKER_ShutDown(nullptr);}
        if(sb) {SBLASTER_OnReset(nullptr);SBLASTER_DOS_Boot(nullptr);}
        if(gus) {GUS_OnReset(nullptr);GUS_DOS_Boot(nullptr);}
        if(speaker) {TANDYSOUND_OnReset(nullptr);PCSPEAKER_OnReset(nullptr);}
        if(midi) MIDI_OnSectionPropChange(control->GetSection("midi"));
        /* Upstream device startup installs @SET lines for AUTOEXEC, which
         * does not update an already-running shell's environment. */
        if(first_shell && (sb || midi)) {
            const int card=cfg_card();
            std::string blaster;
            if(card==CARD_SB2 || card==CARD_PRO2 || card==CARD_SB16) {
                blaster=std::string("A")+cfg_get(K_SB_PORT)+" I"+cfg_get(K_SB_IRQ)+" D"+cfg_get(K_SB_DMA);
                if(card==CARD_SB16) blaster+=std::string(" H")+cfg_get(K_SB_HDMA);
                if(strcmp(cfg_get(K_MPU),"none")) blaster+=std::string(" P")+(!strcmp(cfg_get(K_MPU_BASE),"0")?"330":cfg_get(K_MPU_BASE));
                blaster+=card==CARD_SB16?" T6":card==CARD_PRO2?" T5":" T3";
            }
            if(!first_shell->SetEnv("BLASTER",blaster.c_str())) WriteOut("Unable to update BLASTER; restart DOS if its environment is full.\n");
        }
        if(first_shell && gus) {
            std::string ultrasnd;
            if(!strcmp(cfg_get(K_GUS),"true")) ultrasnd=std::string(cfg_get(K_GUS_PORT))+","+
                cfg_get(K_GUS_DMA)+","+cfg_get(K_GUS_DMA)+","+cfg_get(K_GUS_IRQ)+","+cfg_get(K_GUS_IRQ);
            first_shell->SetEnv("ULTRASND",ultrasnd.c_str());
            first_shell->SetEnv("ULTRADIR",ultrasnd.empty()?"":cfg_get(K_GUS_DIR));
            first_shell->SetEnv("ULTRA16","");
        }
        static const char *channels[]={"MASTER","SB","FM","GUS","CMS","SPKR","TANDY","TANDYDAC","DISNEY","SYNTH","CDAUDIO","USBCD"};
        for(int key=K_VOL_MASTER;key<=K_VOL_USB_CD;key++) {
            const char *channel=channels[key-K_VOL_MASTER];
            if(key==K_VOL_MIDI && !strcmp(cfg_get(K_MIDI_DEVICE),"mt32")) channel="MT32";
            /* Existing mixer command handles absent channels and stereo values. */
            DOS_Shell shell; char command[160];
            snprintf(command,sizeof(command),"Z:\\SYSTEM\\MIXER.COM /NOSHOW %s %s",channel,cfg_get(key));
            shell.ParseLine(command);
        }
        /* Reset the toggle pair; the next enable captures the applied Normal speed. */
        pidos_turbo_on=false; pidos_turbo_initialized=false; pidos_turbo_init();
        DOS_UnlinkFile("C:\\TEMP\\PIDAPPLY.CFG"); result("OK"); exit_status=0;
        WriteOut("Hardware and shortcuts applied. Other System/Input/Network changes require REBOOT.\n");
    }
};
void PiDosApply_ProgramStart(Program **make) { *make=new PiDosApply; }
#endif
