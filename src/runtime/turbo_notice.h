/* Host-only transient notification. Save/restore the presentation rectangle so
 * neither DOS pixels nor the incremental-render backing surface retain text.
 * Works on both CRT and LCD surface paths. GPL-2.0-or-later. */
#ifndef PIDOS_TURBO_NOTICE_H
#define PIDOS_TURBO_NOTICE_H
#include <SDL.h>
#include <cstring>
#include <string>
extern const char *PiDosTurboText();
class PidosTurboNotice {
    bool shown_=false;
    std::string last_text_;
    SDL_Surface *backup_=nullptr;
    SDL_Rect area_={0,0,0,0};
    static const uint8_t *glyph(char c) {
        static const uint8_t letters[26][7]={
          {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
          {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
          {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
          {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
          {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
          {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
          {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
          {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
          {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
        static const uint8_t digits[10][7]={
          {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
          {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
          {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
          {14,17,17,15,1,1,14}};
        static const uint8_t blank[7]={},dash[7]={0,0,0,31,0,0,0};
        if(c>='a' && c<='z') c-=32;
        if(c>='A' && c<='Z') return letters[c-'A'];
        if(c>='0' && c<='9') return digits[c-'0'];
        return c=='-'?dash:blank;
    }
public:
    ~PidosTurboNotice() {SDL_FreeSurface(backup_);}
    void reset() {SDL_FreeSurface(backup_);backup_=nullptr;shown_=false;last_text_.clear();}
    bool pulse_due() const {return last_text_!=PiDosTurboText();}
    bool due() const {return shown_ || *PiDosTurboText();}
    bool draw(SDL_Surface *surface) {
        const char *text=PiDosTurboText();
        last_text_=text;
        const bool changed=shown_ || *text;
        shown_=*text!=0;
        if(!*text || !surface) return changed;
        const int scale=surface->w>=960 && surface->h>=600 ? 2 : 1;
        const int count=(int)strlen(text), width=count*6*scale+8*scale, height=15*scale;
        area_={surface->w-width-4*scale,4*scale,width,height};
        if(area_.x<0) area_.x=0;
        if(area_.w>surface->w) area_.w=surface->w;
        if(area_.y+area_.h>surface->h) return false;
        SDL_FreeSurface(backup_);
        backup_=SDL_CreateRGBSurfaceWithFormat(0,area_.w,area_.h,surface->format->BitsPerPixel,surface->format->format);
        if(!backup_) return false;
        if(surface->format->palette && backup_->format->palette)
            SDL_SetPaletteColors(backup_->format->palette,surface->format->palette->colors,0,surface->format->palette->ncolors);
        SDL_SetSurfaceBlendMode(backup_,SDL_BLENDMODE_NONE);
        if(SDL_BlitSurface(surface,&area_,backup_,nullptr)) {reset();return false;}
        SDL_FillRect(surface,&area_,SDL_MapRGB(surface->format,12,16,24));
        const Uint32 ink=SDL_MapRGB(surface->format,240,240,220);
        for(int i=0;i<count;i++) {
            const uint8_t *g=glyph(text[i]);
            for(int y=0;y<7;y++) for(int x=0;x<5;x++) if(g[y]&(16>>x)) {
                SDL_Rect pixel={area_.x+(4+i*6+x)*scale,area_.y+(4+y)*scale,scale,scale};
                if(pixel.x+pixel.w<=area_.x+area_.w) SDL_FillRect(surface,&pixel,ink);
            }
        }
        return true;
    }
    void restore(SDL_Surface *surface) {
        if(backup_) {SDL_Rect dest=area_;SDL_BlitSurface(backup_,nullptr,surface,&dest);SDL_FreeSurface(backup_);backup_=nullptr;}
    }
};
#endif
