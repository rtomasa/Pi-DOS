#!/usr/bin/env python3
"""Focused ARM64 Accept/Cancel, live mixer and Turbo integration under QEMU and dummy SDL.

Interposition maps appliance scratch paths into an isolated test directory;
no real supervisor, devices, mounts or signals are used.
"""
import argparse
import os
import shutil
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--sdl-library-path', type=Path, required=True)
args = parser.parse_args()
tree = next((ROOT/'output/buildroot-arm64/build').glob('pi-dos-runtime-*'))
cc = ROOT/'output/buildroot-arm64/host/bin/aarch64-linux-gcc'
cxx = ROOT/'output/buildroot-arm64/host/bin/aarch64-linux-g++'
qemu = ROOT/'output/tools/qemu-user/usr/bin/qemu-aarch64'
target = ROOT/'output/buildroot-arm64/target'
headers = ROOT/'output/buildroot-arm64/host/aarch64-buildroot-linux-gnu/sysroot/usr/include/SDL2'
work = Path(tempfile.mkdtemp(prefix='session-turbo.', dir=ROOT/'output/smoke'))
source = work/'interpose.c'
source.write_text(r'''
#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
static const char *root(void) {
    static char *(*real_getenv)(const char *);
    if(!real_getenv) real_getenv=dlsym(RTLD_NEXT,"getenv");
    return real_getenv("PIDOS_TEST_ROOT");
}
static const char *mapped(const char *path,char *buffer) {
    if(!strncmp(path,"/run/pidos-",11) || !strcmp(path,"/run/pi-dos-session-restart")) {
        snprintf(buffer,4096,"%s/run/%s",root(),strrchr(path,'/')+1); return buffer;
    }
    if(!strcmp(path,"/dosdata/SYSTEM/PIDOS.CFG")) {
        snprintf(buffer,4096,"%s/SYSTEM/PIDOS.CFG",root());return buffer;
    }
    return path;
}
static int exists(const char *name) {
    char path[4096]; snprintf(path,sizeof(path),"%s/%s",root(),name);
    return faccessat(AT_FDCWD,path,F_OK,0)==0;
}
char *getenv(const char *name) {
    static char *(*real_getenv)(const char *);
    if(!real_getenv) real_getenv=dlsym(RTLD_NEXT,"getenv");
    if(!strcmp(name,"PI_DOS_SUPERVISED")) return exists("ENABLE") ? "1" : NULL;
    return real_getenv(name);
}
pid_t getppid(void) {return 1;}
int kill(pid_t pid,int signal) {(void)pid;(void)signal;errno=EPERM;return -1;}
#define FILE_WRAP(name) FILE *name(const char *path,const char *mode) { \
    static FILE *(*real_fn)(const char *,const char *); char buffer[4096]; \
    if(!real_fn) real_fn=dlsym(RTLD_NEXT,#name); return real_fn(mapped(path,buffer),mode); }
FILE_WRAP(fopen)
FILE_WRAP(fopen64)
#define OPEN_WRAP(name) int name(const char *path,int flags,...) { \
    static int (*real_fn)(const char *,int,...); char buffer[4096]; mode_t mode=0; \
    if(flags&O_CREAT) {va_list a;va_start(a,flags);mode=va_arg(a,int);va_end(a);} \
    if(!real_fn) real_fn=dlsym(RTLD_NEXT,#name);return real_fn(mapped(path,buffer),flags,mode); }
OPEN_WRAP(open)
OPEN_WRAP(open64)
int access(const char *path,int mode) {
    static int (*fn)(const char *,int);char buffer[4096];
    if(!fn) fn=dlsym(RTLD_NEXT,"access");return fn(mapped(path,buffer),mode);
}
int unlink(const char *path) {
    static int (*fn)(const char *);char buffer[4096];
    if(!fn) fn=dlsym(RTLD_NEXT,"unlink");return fn(mapped(path,buffer));
}
int rename(const char *a,const char *b) {
    static int (*fn)(const char *,const char *);char aa[4096],bb[4096];
    if(!fn) fn=dlsym(RTLD_NEXT,"rename");return fn(mapped(a,aa),mapped(b,bb));
}
int SDL_PollEvent(SDL_Event *event) {
    static int (*real_poll)(SDL_Event *);static int toggle=0,phase=0,ui=0;
    if(!real_poll) real_poll=dlsym(RTLD_NEXT,"SDL_PollEvent");
    int result=real_poll(event);if(result || !event) return result;
    if(exists("UIRUN")) {
        static SDL_Scancode keys[32];static int count=-1;
        if(count<0) {
            char path[4096],name[40];snprintf(path,sizeof(path),"%s/UIKEYS",root());
            FILE *file=fopen(path,"r");count=0;
            while(file && count<32 && fscanf(file,"%39s",name)==1) keys[count++]=SDL_GetScancodeFromName(name);
            if(file) fclose(file);
        }
        if(ui<count*2) {
            SDL_zero(*event);event->type=(ui%2)?SDL_KEYUP:SDL_KEYDOWN;event->key.type=event->type;
            event->key.keysym.scancode=keys[ui/2];
            event->key.keysym.sym=SDL_GetKeyFromScancode(keys[ui/2]);
            ui++;return 1;
        }
    }
    static const char *stages[]={"BOOTON","BOOTOFF","TURBO1","TURBO2"};
    if(!phase && (toggle>=4 || !exists(stages[toggle]))) return 0;
    SDL_zero(*event);
    if(phase<3) {
        event->type=phase==2?SDL_KEYUP:SDL_KEYDOWN;
        event->key.keysym.scancode=SDL_SCANCODE_T;
        event->key.keysym.mod=phase==2?0:KMOD_CTRL|KMOD_ALT;
        event->key.repeat=phase==1;
    } else {
        event->type=phase==3?SDL_KEYDOWN:SDL_KEYUP;
        event->key.keysym.scancode=SDL_SCANCODE_RETURN;
        event->key.keysym.sym=SDLK_RETURN;
    }
    event->key.type=event->type;
    if(++phase==5) {phase=0;toggle++;}
    return 1;
}
''')
preload = work/'interpose.so'
subprocess.run([str(cc), '-shared', '-fPIC', '-O2',
                '-I'+str(headers), str(source), '-ldl', '-o', str(preload)], check=True)


BASE = ('[cpu]\ncputype=486\ncore=dynamic_rec\ncycles=fixed 23880\n'
        '[midi]\nmpu401=none\nmididevice=none\n[pidos]\nmidi_profile=none\nturbo_mhz=100\n'
        '[sblaster]\nsbtype=sbpro2\noplmode=opl3\n')


def run(name, keys=(), batch=(), accepts=False):
    case = work/name
    for directory in (case/'run', case/'TEMP', case/'SYSTEM'):
        directory.mkdir(parents=True, exist_ok=True)
    saved = '# setup edits require reboot\n' + BASE
    (case/'SYSTEM/PIDOS.CFG').write_text(saved)
    # Mount the fixture C: before enabling appliance drive protection.
    # The initial on/off pair loads the boot Turbo settings before UI edits.
    if keys:
        shutil.copyfile(ROOT/'br2-external/board/raspberrypi/dos/PIDOSCFG.EXE', case/'PIDOSCFG.EXE')
        (case/'UIKEYS').write_text(' '.join(keys))
    lines = ['@echo off', 'ECHO 1>ENABLE', 'ECHO 1>BOOTON', 'PAUSE',
             'ECHO 1>BOOTOFF', 'PAUSE', 'Z:\\SYSTEM\\MIXER.COM /NOSHOW MASTER 100']
    if keys:
        lines += ['ECHO 1>UIRUN', 'PIDOSCFG.EXE']
    lines += ['Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >CPU.OUT',
              'Z:\\SYSTEM\\CONFIG.COM -GET dos xms >XMS.OUT',
              'Z:\\SYSTEM\\CONFIG.COM -GET dos ems >EMS.OUT',
              'Z:\\SYSTEM\\MIXER.COM >LEVELS.OUT',
              'IF EXIST Z:\\SYSTEM\\PIAPPLY.COM ECHO unexpected >APPLY.OUT', *batch, 'EXIT']
    (case/'RUN.BAT').write_bytes(('\r\n'.join(lines)+'\r\n').encode())
    command = [str(qemu), '-L', str(target), '-E', 'LD_LIBRARY_PATH='+str(args.sdl_library_path.resolve()),
               '-E', 'LD_PRELOAD='+str(preload.resolve()), '-E', 'PIDOS_TEST_ROOT='+str(case.resolve()),
               str(tree/'src/dosbox-x'), '-conf', str(ROOT/'configs/pi-dos.conf'),
               '-conf', str(case/'SYSTEM/PIDOS.CFG'),
               '-defaultmapper', '-nopromptfolder', '-defaultdir', str(case),
               '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '15']
    for setting in ('sdl fullscreen=false', 'sdl videodriver=dummy', 'sdl waitonerror=false'):
        command += ['-set', setting]
    for line in ('mount c '+str(case), 'c:', 'RUN.BAT'):
        command += ['-c', line]
    env = {k: v for k, v in os.environ.items() if not k.startswith('PI_DOS_') and k!='LD_PRELOAD'}
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with (case/'run.log').open('w') as log:
        completed = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=70)
    assert completed.returncode==0, case/'run.log'
    if not accepts:
        assert (case/'SYSTEM/PIDOS.CFG').read_text() == saved, case/'SYSTEM/PIDOS.CFG'
        assert not (case/'SYSTEM/PIDOS.BAK').exists()
    else:
        assert (case/'SYSTEM/PIDOS.BAK').read_text() == saved
    assert b'23880' in (case/'CPU.OUT').read_bytes(), case/'run.log'
    assert b'true' in (case/'XMS.OUT').read_bytes() and b'true' in (case/'EMS.OUT').read_bytes()
    assert not (case/'run/pi-dos-session-restart').exists()
    assert not (case/'run/pidos-session.cfg').exists()
    assert b'unexpected' not in (case/'APPLY.OUT').read_bytes()
    return case


# Accept closes setup and saves a different profile without changing the active CPU.
# Boot Turbo remains live even after saving a different target/profile for next boot.
case = run('accept-cpu', ['Down', 'F2'], [
    'ECHO 1>TURBO1', 'PAUSE', 'Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >ON.OUT',
    'ECHO 1>TURBO2', 'PAUSE', 'Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >OFF.OUT'], accepts=True)
assert b'cycles=fixed 33445' in (case/'SYSTEM/PIDOS.CFG').read_bytes()
assert b'turbo_mhz=0' in (case/'SYSTEM/PIDOS.CFG').read_bytes()
assert b'33445' in (case/'ON.OUT').read_bytes(), (case/'ON.OUT').read_bytes()
assert b'23880' in (case/'OFF.OUT').read_bytes(), (case/'OFF.OUT').read_bytes()
run('cancel-cpu', ['Down', 'Escape'])
run('removed-apply', ['Down', 'F4', 'Escape'])
case = run('accept-memory', ['F6', 'Tab', 'Space', 'Tab', 'Space', 'F2'], accepts=True)
assert b'xms=false' in (case/'SYSTEM/PIDOS.CFG').read_bytes()
assert b'ems=false' in (case/'SYSTEM/PIDOS.CFG').read_bytes()
# Mixer adjustments keep their live path; Accept commits and Cancel restores it.
for action, expected in [('F2', b'95:95'), ('Escape', b'100:100')]:
    case = run('mixer-'+action.lower(), ['F6']*4+['Left', action], accepts=action=='F2')
    levels = (case/'LEVELS.OUT').read_bytes()
    master = next(line for line in levels.splitlines() if line.startswith(b'MASTER'))
    assert expected in master, master
    if action=='F2':
        assert b'mixer_master=95:95' in (case/'SYSTEM/PIDOS.CFG').read_bytes()

# Verify presentation pixels are restored for palette CRT and RGB LCD surfaces.
notice = work/'notice.cpp'
notice.write_text(r'''
#include <cassert>
#include <string>
#include <vector>
#include "turbo_notice.h"
static std::string text;
const char *PiDosTurboText() {return text.c_str();}
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO)==0);
    for(auto format:{SDL_PIXELFORMAT_INDEX8,SDL_PIXELFORMAT_RGB565,SDL_PIXELFORMAT_RGB888}) {
        SDL_Surface *screen=SDL_CreateRGBSurfaceWithFormat(0,640,400,SDL_BITSPERPIXEL(format),format);
        assert(screen);
        if(screen->format->palette) {
            SDL_Color colors[256];for(int i=0;i<256;i++) colors[i]={(Uint8)i,(Uint8)i,(Uint8)i,255};
            SDL_SetPaletteColors(screen->format->palette,colors,0,256);
        }
        SDL_FillRect(screen,nullptr,SDL_MapRGB(screen->format,80,80,80));
        std::vector<unsigned char> before((unsigned char*)screen->pixels,(unsigned char*)screen->pixels+screen->pitch*screen->h);
        PidosTurboNotice notice;text="TURBO ON - 486 100 MHz";
        assert(notice.pulse_due() && notice.draw(screen) && !notice.pulse_due());
        assert(memcmp(screen->pixels,before.data(),before.size()));
        notice.restore(screen);assert(!memcmp(screen->pixels,before.data(),before.size()));
        text="";assert(notice.pulse_due() && notice.draw(screen) && !notice.due());
        assert(!memcmp(screen->pixels,before.data(),before.size()));
        SDL_FreeSurface(screen);
    }
    SDL_Quit();
}
''')
notice_binary = work/'notice'
subprocess.run([str(cxx), '-std=c++14', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I'+str(headers), '-I'+str(ROOT/'src/runtime'),
                str(notice), '-lSDL2', '-o', str(notice_binary)], check=True)
subprocess.run([str(qemu), '-L', str(target), '-E', 'LD_LIBRARY_PATH='+str(args.sdl_library_path.resolve()),
                str(notice_binary)], env=dict(os.environ, SDL_VIDEODRIVER='dummy'), check=True)
print('PASS: ARM64 Accept/Cancel, reboot-only CPU/memory edits, removed Apply, live mixer commit/rollback, Turbo on/off/repeat suppression and CRT/LCD notice restoration: '+str(work))
