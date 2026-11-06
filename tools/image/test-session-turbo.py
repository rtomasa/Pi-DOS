#!/usr/bin/env python3
"""Focused ARM64 Apply/Turbo integration under QEMU and dummy SDL.

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
    if(ui<14 && exists("UIMEM")) {
        static const SDL_Scancode scans[]={SDL_SCANCODE_F6,SDL_SCANCODE_TAB,SDL_SCANCODE_SPACE,
            SDL_SCANCODE_TAB,SDL_SCANCODE_SPACE,SDL_SCANCODE_F4,SDL_SCANCODE_Y};
        static const SDL_Keycode symbols[]={SDLK_F6,SDLK_TAB,SDLK_SPACE,SDLK_TAB,SDLK_SPACE,SDLK_F4,SDLK_y};
        SDL_zero(*event);event->type=(ui%2)?SDL_KEYUP:SDL_KEYDOWN;event->key.type=event->type;
        event->key.keysym.scancode=scans[ui/2];event->key.keysym.sym=symbols[ui/2];
        ui++;return 1;
    }
    if(ui<4 && exists("UIEXIT")) {
        SDL_zero(*event);event->type=(ui%2)?SDL_KEYUP:SDL_KEYDOWN;
        event->key.type=event->type;
        event->key.keysym.scancode=ui<2?SDL_SCANCODE_DOWN:SDL_SCANCODE_ESCAPE;
        event->key.keysym.sym=ui<2?SDLK_DOWN:SDLK_ESCAPE;
        ui++;return 1;
    }
    if(ui<2 && exists("UIRUN")) {
        SDL_zero(*event);event->type=ui++?SDL_KEYUP:SDL_KEYDOWN;
        event->key.type=event->type;event->key.keysym.scancode=SDL_SCANCODE_F4;event->key.keysym.sym=SDLK_F4;
        return 1;
    }
    if(!phase && !((toggle==0 && exists("TURBO1")) || (toggle==1 && exists("TURBO2")))) return 0;
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


def run(name, request, batch, ui=False):
    case = work/name
    for directory in (case/'run', case/'TEMP', case/'SYSTEM'):
        directory.mkdir(parents=True, exist_ok=True)
    (case/'TEMP/PIDAPPLY.CFG').write_text(request)
    saved = '# saved settings must survive Apply\n[pidos]\nturbo_mhz=0\n' + (request if ui else '')
    (case/'SYSTEM/PIDOS.CFG').write_text(saved)
    if ui:
        shutil.copyfile(ROOT/'br2-external/board/raspberrypi/dos/PIDOSCFG.EXE', case/'PIDOSCFG.EXE')
    (case/'RUN.BAT').write_bytes(('\r\n'.join(['@echo off', 'echo 1>ENABLE', *batch])+'\r\n').encode())
    command = [str(qemu), '-L', str(target), '-E', 'LD_LIBRARY_PATH='+str(args.sdl_library_path.resolve()),
               '-E', 'LD_PRELOAD='+str(preload.resolve()), '-E', 'PIDOS_TEST_ROOT='+str(case.resolve()),
               str(tree/'src/dosbox-x'), '-conf', str(ROOT/'configs/pi-dos.conf'),
               '-defaultmapper', '-nopromptfolder', '-defaultdir', str(case),
               '-fastlaunch', '-nomenu', '-nogui', '-time-limit', '15']
    for setting in ('sdl fullscreen=false', 'sdl videodriver=dummy', 'sdl waitonerror=false',
                    'midi mpu401=none', 'midi mididevice=none'):
        command += ['-set', setting]
    for line in ('mount c '+str(case), 'c:', 'RUN.BAT'):
        command += ['-c', line]
    env = {k: v for k, v in os.environ.items() if not k.startswith('PI_DOS_') and k!='LD_PRELOAD'}
    env.update(SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with (case/'run.log').open('w') as log:
        completed = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=70)
    assert completed.returncode==0, case/'run.log'
    assert (case/'SYSTEM/PIDOS.CFG').read_text() == saved
    return case


request = ('[cpu]\ncputype=486\ncore=dynamic_rec\ncycles=fixed 23880\n'
           '[midi]\nmpu401=none\nmididevice=none\n[pidos]\nmidi_profile=none\nturbo_mhz=100\n'
           '[sblaster]\nsbtype=sb16\nirq=5\n')
case = run('live', request, [
    'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT',
    'Z:\\SYSTEM\\PIAPPLY.COM', 'TYPE C:\\TEMP\\PIDAPPLY.STA >APPLY.OUT',
    'Z:\\SYSTEM\\CONFIG.COM -GET cpu cputype >MODEL.OUT',
    'Z:\\SYSTEM\\CONFIG.COM -GET cpu core >CORE.OUT', 'SET BLASTER >SOUND.OUT',
    'ECHO 1>TURBO1', 'PAUSE', 'Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >ON.OUT',
    'ECHO 1>TURBO2', 'PAUSE', 'Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >OFF.OUT',
    'Z:\\SYSTEM\\PIAPPLY.COM /QUERY', 'EXIT'])
assert (case/'CHECK.OUT').read_text().strip()=='LIVE', case/'run.log'
assert (case/'APPLY.OUT').read_text().strip()=='OK', case/'run.log'
assert b'486' in (case/'MODEL.OUT').read_bytes()
assert b'dynamic_rec' in (case/'CORE.OUT').read_bytes()
assert b'I5' in (case/'SOUND.OUT').read_bytes() and b'T6' in (case/'SOUND.OUT').read_bytes()
assert b'33445' in (case/'ON.OUT').read_bytes(), (case/'ON.OUT').read_bytes()
assert b'23880' in (case/'OFF.OUT').read_bytes(), (case/'OFF.OUT').read_bytes()
assert b'cycles=fixed 23880' in (case/'TEMP/PIDLIVE.CFG').read_bytes()
assert not (case/'run/pi-dos-session-restart').exists()

case = run('dos-ui', request, ['ECHO 1>UIRUN', 'PIDOSCFG.EXE',
    'Z:\\SYSTEM\\CONFIG.COM -GET cpu cycles >UI.OUT', 'EXIT'], ui=True)
assert b'23880' in (case/'UI.OUT').read_bytes(), case/'run.log'
assert b'sbtype=sb16' in (case/'run/pidos-session.cfg').read_bytes()

case = run('dos-exit', request, ['ECHO 1>UIEXIT', 'PIDOSCFG.EXE',
    'Z:\\SYSTEM\\CONFIG.COM -GET cpu cputype >UI.OUT', 'EXIT'], ui=True)
assert b'pentium' in (case/'UI.OUT').read_bytes(), case/'run.log'
assert not (case/'run/pidos-session.cfg').exists()
case = run('invalid-turbo', request+'[pidos]\nturbo_mhz=66\n', [
    'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT', 'EXIT'])
assert (case/'CHECK.OUT').read_text().strip()=='ERROR'
assert not (case/'run/pidos-session.cfg').exists()

case = run('restart', request+'[dosbox]\nmemsize=32\n', [
    'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT',
    'Z:\\SYSTEM\\PIAPPLY.COM', 'ECHO should-not-run >AFTER.OUT', 'EXIT'])
assert (case/'CHECK.OUT').read_text().strip()=='RESTART'
assert (case/'run/pi-dos-session-restart').exists()
assert b'memsize=32' in (case/'run/pidos-session.cfg').read_bytes()
assert not (case/'AFTER.OUT').exists()
for manager in ('xms', 'ems'):
    case = run('disable-'+manager, request+'[dos]\n'+manager+'=false\n', [
        'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT',
        'Z:\\SYSTEM\\PIAPPLY.COM', 'ECHO should-not-run >AFTER.OUT', 'EXIT'])
    assert (case/'CHECK.OUT').read_text().strip()=='RESTART'
    assert (case/'run/pi-dos-session-restart').exists()
    assert (manager+'=false').encode() in (case/'run/pidos-session.cfg').read_bytes()
    assert not (case/'AFTER.OUT').exists()
case = run('dos-memory-ui', request, ['ECHO 1>UIMEM', 'PIDOSCFG.EXE',
    'ECHO should-not-run >AFTER.OUT', 'EXIT'], ui=True)
assert (case/'run/pi-dos-session-restart').exists(), case/'run.log'
assert b'xms=false' in (case/'run/pidos-session.cfg').read_bytes()
assert b'ems=false' in (case/'run/pidos-session.cfg').read_bytes()
assert not (case/'AFTER.OUT').exists()
case = run('286-boundary', request.replace('cputype=486','cputype=286').replace(
    'core=dynamic_rec','core=normal').replace('cycles=fixed 23880','cycles=fixed 1510').replace(
    'turbo_mhz=100','turbo_mhz=25'), [
    'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT', 'EXIT'])
assert (case/'CHECK.OUT').read_text().strip()=='RESTART'
assert not (case/'run/pidos-session.cfg').exists()
case = run('invalid', request+'[pidos]\nshortcut_pause=Ctrl+Alt+T\n', [
    'Z:\\SYSTEM\\PIAPPLY.COM /CHECK', 'TYPE C:\\TEMP\\PIDAPPLY.STA >CHECK.OUT', 'EXIT'])
assert (case/'CHECK.OUT').read_text().strip()=='ERROR'
assert not (case/'run/pidos-session.cfg').exists()

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
print('PASS: ARM64 Apply, XMS/EMS checkboxes and restart requests, sound environment, Turbo on/off/repeat suppression, invalid-setting rejection, and CRT/LCD notification restoration: '+str(work))
