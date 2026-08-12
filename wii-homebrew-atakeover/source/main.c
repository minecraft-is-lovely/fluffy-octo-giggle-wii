/*
 * A TAKEOVER — Wii Homebrew
 *
 * Patches every glyph in every Wii NAND font file to look like the letter A.
 * After running this and rebooting to the Wii Menu, all system text is A.
 *
 * Controls:
 *   A      — confirm / YES
 *   B      — decline / NO
 *   HOME   — exit at any time
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <ogc/isfs.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── framebuffer ─────────────────────────────────────────────────── */
static void       *xfb[2] = {NULL,NULL};
static int         fbi     = 0;
static GXRModeObj *rmode   = NULL;
#define FB  ((u32*)xfb[fbi])
#define SW  640
#define SH  480

/* ─── colour helpers ──────────────────────────────────────────────── */
static inline u8 C8(int v){ return v<0?0:v>255?255:(u8)v; }
static inline u8 RY(u8 r,u8 g,u8 b){ return C8(((77*r+150*g+29*b)>>8)+16); }
static inline u8 CB(u8 r,u8 g,u8 b){ return C8(((-43*r-85*g+128*b)>>8)+128); }
static inline u8 CR(u8 r,u8 g,u8 b){ return C8(((128*r-107*g-21*b)>>8)+128); }

static void R(int x,int y,int w,int h,u8 r,u8 g,u8 b){
    if(w<=0||h<=0) return;
    int s=rmode->fbWidth>>1;
    u32 p=((u32)RY(r,g,b)<<24)|((u32)CB(r,g,b)<<16)|((u32)RY(r,g,b)<<8)|CR(r,g,b);
    for(int row=y;row<y+h;row++){
        if(row<0||row>=(int)rmode->xfbHeight) continue;
        for(int col=x>>1;col<(x+w+1)>>1;col++){
            if(col<0||col>=s) continue;
            FB[row*s+col]=p;
        }
    }
}

/* ─── 8×8 bitmap font (ASCII 32–90) ──────────────────────────────── */
static u8 F[59][8]={
/* 32 ' ' */{0,0,0,0,0,0,0,0},
/* 33 '!' */{24,24,24,24,0,24,24,0},
/* 34-44 */{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},{0},
/* 45 '-' */{0,0,0,126,0,0,0,0},
/* 46 '.' */{0,0,0,0,0,24,24,0},
/* 47 '/' */{0,6,12,24,48,96,0,0},
/* 48 '0' */{60,102,102,102,102,102,60,0},
/* 49 '1' */{24,56,24,24,24,24,126,0},
/* 50 '2' */{60,102,6,12,24,48,126,0},
/* 51 '3' */{60,102,6,28,6,102,60,0},
/* 52 '4' */{6,14,30,102,127,6,6,0},
/* 53 '5' */{126,96,124,6,6,102,60,0},
/* 54 '6' */{60,102,96,124,102,102,60,0},
/* 55 '7' */{126,102,12,24,24,24,24,0},
/* 56 '8' */{60,102,102,60,102,102,60,0},
/* 57 '9' */{60,102,102,62,6,102,60,0},
/* 58 ':' */{0,24,24,0,24,24,0,0},
/* 59-64 */{0},{0},{0},{0},{0},{0},
/* 65 'A' */{24,60,102,126,102,102,102,0},
/* 66 'B' */{124,102,102,124,102,102,124,0},
/* 67 'C' */{60,102,96,96,96,102,60,0},
/* 68 'D' */{120,108,102,102,102,108,120,0},
/* 69 'E' */{126,96,96,124,96,96,126,0},
/* 70 'F' */{126,96,96,124,96,96,96,0},
/* 71 'G' */{60,102,96,110,102,102,60,0},
/* 72 'H' */{102,102,102,126,102,102,102,0},
/* 73 'I' */{60,24,24,24,24,24,60,0},
/* 74 'J' */{30,6,6,6,102,102,60,0},
/* 75 'K' */{102,108,120,112,120,108,102,0},
/* 76 'L' */{96,96,96,96,96,96,126,0},
/* 77 'M' */{99,119,127,107,99,99,99,0},
/* 78 'N' */{102,118,126,110,102,102,102,0},
/* 79 'O' */{60,102,102,102,102,102,60,0},
/* 80 'P' */{124,102,102,124,96,96,96,0},
/* 81 'Q' */{60,102,102,102,106,100,58,0},
/* 82 'R' */{124,102,102,124,108,102,102,0},
/* 83 'S' */{60,102,96,60,6,102,60,0},
/* 84 'T' */{126,24,24,24,24,24,24,0},
/* 85 'U' */{102,102,102,102,102,102,60,0},
/* 86 'V' */{102,102,102,102,60,60,24,0},
/* 87 'W' */{99,99,99,107,127,119,99,0},
/* 88 'X' */{102,102,60,24,60,102,102,0},
/* 89 'Y' */{102,102,102,60,24,24,24,0},
/* 90 'Z' */{126,6,12,24,48,96,126,0},
};

static void G(char c,int x,int y,int sc,u8 r,u8 g,u8 b){
    int idx=(int)(unsigned char)c-32;
    if(idx<0||idx>58) return;
    const u8*p=F[idx];
    for(int row=0;row<8;row++)
        for(int col=0;col<8;col++)
            if(p[row]&(0x80>>col))
                R(x+col*sc,y+row*sc,sc,sc,r,g,b);
}
static int TW(const char*s,int sc){int n=(int)strlen(s);return n?(n*(8*sc+sc)-sc):0;}
static void T(const char*s,int x,int y,int sc,u8 r,u8 g,u8 b){
    for(;*s;s++,x+=(8+1)*sc) G(*s,x,y,sc,r,g,b);
}
static void TC(const char*s,int cx,int y,int sc,u8 r,u8 g,u8 b){
    T(s,cx-TW(s,sc)/2,y,sc,r,g,b);
}

/* ─── progress bar ────────────────────────────────────────────────── */
static void PBAR(int x,int y,int w,int h,float pct,u8 fr,u8 fg,u8 fb,u8 br,u8 bg,u8 bb){
    R(x,y,w,h,30,30,30);
    R(x+2,y+2,w-4,h-4,20,20,20);
    int filled=(int)((w-4)*pct);
    if(filled>0) R(x+2,y+2,filled,h-4,br,bg,bb);
    R(x,y,w,2,fr,fg,fb);
    R(x,y,2,h,fr,fg,fb);
    R(x,y+h-2,w,2,fr,fg,fb);
    R(x+w-2,y,2,h,fr,fg,fb);
}

/* ─── flip framebuffer ────────────────────────────────────────────── */
static void flip(void){
    VIDEO_SetNextFramebuffer(xfb[fbi]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    fbi^=1;
}

/* ─── NAND font patching via ISFS ─────────────────────────────────── */
#define FONT_DIR "/shared1/font"
#define ISFS_MAXPATH 64

static int fonts_patched = 0;   /* how many font files we successfully patched */
static int isfs_ok       = 0;   /* did ISFS init succeed? */

/*
 * Patch one BRFNT file buffer in-place.
 * Strategy: find the "TGLP" section, read cell dims + texture size,
 * divide evenly to get bytes-per-glyph, copy glyph-A bytes over every other slot.
 * Returns 1 if patched, 0 if not recognised.
 */
static int patch_brfnt(u8 *buf, u32 sz){
    /* scan for TGLP magic */
    int toff=-1;
    for(u32 i=0;i+3<sz;i++){
        if(buf[i]=='T'&&buf[i+1]=='G'&&buf[i+2]=='L'&&buf[i+3]=='P'){
            toff=(int)i; break;
        }
    }
    if(toff<0) return 0;

    /* TGLP layout (big-endian):
       +0  "TGLP"
       +4  section_size (u32)
       +8  cell_width  (u8)
       +9  cell_height (u8)
       +10 baseline    (u8)
       +11 max_char_width (u8)
       +12 texture_data_offset (u32)  -- offset from start of file
       +16 num_textures (u16)
       +18 tex_format  (u16)  0=I4 1=I8 2=IA4 3=IA8
       +20 num_cols    (u16)  glyphs per row in texture
       +22 num_rows    (u16)  glyph rows per texture
       +24 tex_width   (u32)
       +28 tex_height  (u32)
    */
    if(toff+32>(int)sz) return 0;

    u8  cell_w  = buf[toff+8];
    u8  cell_h  = buf[toff+9];
    u32 tex_off = ((u32)buf[toff+12]<<24)|((u32)buf[toff+13]<<16)|
                  ((u32)buf[toff+14]<<8)| buf[toff+15];
    u16 num_tex = ((u16)buf[toff+16]<<8)| buf[toff+17];
    u16 tex_fmt = ((u16)buf[toff+18]<<8)| buf[toff+19];
    u16 num_cols= ((u16)buf[toff+20]<<8)| buf[toff+21];
    u16 num_rows= ((u16)buf[toff+22]<<8)| buf[toff+23];
    u32 tex_w   = ((u32)buf[toff+24]<<24)|((u32)buf[toff+25]<<16)|
                  ((u32)buf[toff+26]<<8)| buf[toff+27];
    u32 tex_h   = ((u32)buf[toff+28]<<24)|((u32)buf[toff+29]<<16)|
                  ((u32)buf[toff+30]<<8)| buf[toff+31];

    if(cell_w==0||cell_h==0||num_cols==0||num_rows==0||tex_w==0||tex_h==0) return 0;
    if(num_tex==0) num_tex=1;

    /* bytes per pixel × 2 (we work in half-bytes) to keep integer math */
    u32 bpp2; /* bits per pixel × 2 */
    switch(tex_fmt){
        case 0: bpp2=4;  break;  /* I4:  4 bpp  */
        case 1: bpp2=8;  break;  /* I8:  8 bpp  */
        case 2: bpp2=8;  break;  /* IA4: 8 bpp  */
        case 3: bpp2=16; break;  /* IA8: 16 bpp */
        default:bpp2=8;  break;
    }
    u32 tex_bytes = tex_w * tex_h * bpp2 / 8;  /* bytes per texture */
    u32 glyphs_per_tex = (u32)num_cols * num_rows;
    if(glyphs_per_tex==0) return 0;
    u32 bytes_per_glyph = tex_bytes / glyphs_per_tex;
    if(bytes_per_glyph==0) return 0;

    /* 'A' = code point 0x41 = 65.  Most Wii fonts start at 0x20 (space).
       Glyph index = code_point - start_char.
       65 - 32 = 33  → glyph slot 33. */
    u32 a_idx = 33;

    u32 total_glyphs = (u32)num_tex * glyphs_per_tex;

    /* bounds check */
    if(tex_off + total_glyphs * bytes_per_glyph > sz) return 0;
    if(a_idx >= total_glyphs) return 0;

    u8 *tex   = buf + tex_off;
    u8 *a_gly = tex + a_idx * bytes_per_glyph;

    /* Copy 'A' glyph data over every other glyph slot */
    for(u32 k=0;k<total_glyphs;k++){
        if(k==a_idx) continue;
        memcpy(tex + k*bytes_per_glyph, a_gly, bytes_per_glyph);
    }
    return 1;
}

static void do_nand_patch(void){
    if(ISFS_Initialize()<0){ isfs_ok=0; return; }
    isfs_ok=1;

    /* list /shared1/font */
    static char names[32*ISFS_MAXPATH];
    u32 count=32;
    if(ISFS_ReadDir(FONT_DIR, names, &count)<0){
        ISFS_Deinitialize(); return;
    }

    for(u32 i=0;i<count;i++){
        char *name = names + i*ISFS_MAXPATH;
        if(name[0]=='\0') continue;

        char path[128];
        snprintf(path,sizeof(path),"%s/%s",FONT_DIR,name);

        /* open read-write */
        s32 fd=ISFS_Open(path, ISFS_OPEN_RW);
        if(fd<0) continue;

        fstats st;
        if(ISFS_GetFileStats(fd,&st)<0){ ISFS_Close(fd); continue; }
        u32 sz=st.file_length;
        if(sz==0||sz>2*1024*1024){ ISFS_Close(fd); continue; }

        u8 *buf=(u8*)malloc(sz);
        if(!buf){ ISFS_Close(fd); continue; }

        if(ISFS_Read(fd,buf,sz)<0){ free(buf); ISFS_Close(fd); continue; }
        ISFS_Close(fd);

        /* patch in memory */
        if(!patch_brfnt(buf,sz)){ free(buf); continue; }

        /* write back: reopen for write (starts at beginning) */
        fd=ISFS_Open(path, ISFS_OPEN_WRITE);
        if(fd>=0){
            ISFS_Write(fd,buf,sz);
            ISFS_Close(fd);
            fonts_patched++;
        }
        free(buf);
    }
    ISFS_Deinitialize();
}

/* ─── replace our own in-app font so everything renders as A ─────── */
static void infect_local_font(void){
    const u8 *a = F['A'-32]; /* glyph for 'A' */
    for(int i=0;i<59;i++) memcpy(F[i],a,8);
}

/* ─── states ──────────────────────────────────────────────────────── */
typedef enum { S_CONFIRM, S_RUNNING, S_DONE, S_DECLINED } State;
static State state = S_CONFIRM;
static u32   frame = 0;

/* Running-phase progress (0..1) */
static float run_pct  = 0.0f;
static int   nand_done = 0;  /* have we called do_nand_patch yet? */
static int   font_infected = 0; /* have we replaced the local font? */

/* ─── log lines shown during takeover ────────────────────────────── */
#define NLINES 8
static const char *LOG_MSGS[NLINES]={
    "INITIALISING A PROTOCOL",
    "SCANNING NAND FILESYSTEM",
    "LOCATING FONT ARCHIVES",
    "DECOMPRESSING BRFNT DATA",
    "EXTRACTING GLYPH A BITMAP",
    "OVERWRITING ALL GLYPH SLOTS",
    "FLUSHING CACHE TO NAND",
    "A TAKEOVER COMPLETE",
};

/* ─── draw confirm screen ─────────────────────────────────────────── */
static void draw_confirm(void){
    /* deep red background */
    R(0,0,SW,SH,18,0,0);

    /* title banner */
    R(0,60,SW,80,140,0,0);
    R(0,60,SW,3,220,40,40);
    R(0,137,SW,3,220,40,40);
    TC("A TAKEOVER",SW/2,80,4,255,220,220);

    /* question */
    TC("RUN THE A TAKEOVER?",SW/2,190,2,240,200,200);
    TC("THIS WILL REPLACE ALL WII SYSTEM",SW/2,220,1,180,140,140);
    TC("TEXT WITH THE LETTER A FOREVER.",SW/2,235,1,180,140,140);

    /* YES button */
    int yw=160,yh=52,yx=SW/2-yw-12,yy=290;
    R(yx,yy,yw,yh,0,120,0);
    R(yx,yy,yw,3,80,220,80); R(yx,yy,3,yh,80,220,80);
    R(yx,yy+yh-3,yw,3,0,60,0); R(yx+yw-3,yy,3,yh,0,60,0);
    TC("YES (A)",yx+yw/2,yy+yh/2-8,2,200,255,200);

    /* NO button */
    int nw=160,nh=52,nx=SW/2+12,ny=290;
    R(nx,ny,nw,nh,0,0,110);
    R(nx,ny,nw,3,80,80,220); R(nx,ny,3,nh,80,80,220);
    R(nx,ny+nh-3,nw,3,0,0,55); R(nx+nw-3,ny,3,nh,0,0,55);
    TC("NO (B)",nx+nw/2,ny+nh/2-8,2,180,180,255);

    /* warning stripe at bottom */
    for(int i=0;i<SW;i+=32){
        R(i,440,16,30,180,120,0);
        R(i+16,440,16,30,20,20,20);
    }
    TC("WARNING: PERMANENT UNLESS NAND IS RESET",SW/2,448,1,255,200,0);
}

/* ─── draw running/animation screen ──────────────────────────────── */
static void draw_running(void){
    R(0,0,SW,SH,0,0,0);

    /* pulsing red header */
    u8 pulse=(u8)(80+70*(float)(frame%30)/30.0f);
    R(0,0,SW,50,pulse,0,0);
    TC("A TAKEOVER IN PROGRESS",SW/2,14,2,255,200,200);

    /* log messages — reveal one per ~45 frames */
    int visible = (int)(run_pct * NLINES);
    if(visible>NLINES) visible=NLINES;
    for(int i=0;i<visible;i++){
        u8 bright=(i==visible-1)?255:160;
        u8 col_r=bright, col_g=(i==NLINES-1)?255:40, col_b=40;
        if(i==NLINES-1){ col_r=80; col_g=255; col_b=80; }
        T(LOG_MSGS[i],60,80+i*34,1,col_r,col_g,col_b);
        /* draw a little 'A' icon next to completed lines */
        if(i<visible-1)
            T("A",30,80+i*34,1,200,50,50);
        else
            T(">",30,80+i*34,1,255,255,100);
    }

    /* progress bar */
    PBAR(60,400,SW-120,28,run_pct,200,0,0,180,0,0);
    char pstr[16]; snprintf(pstr,sizeof(pstr),"%d%%",(int)(run_pct*100));
    TC(pstr,SW/2,406,1,255,255,255);
}

/* ─── draw "infected" Wii Menu mockup ────────────────────────────── */
/* by this point the local font is infected, so T() renders A's for all letters */
static void draw_done(void){
    /* Wii Menu blue-grey background */
    R(0,0,SW,SH,30,60,100);

    /* top bar */
    R(0,0,SW,38,20,40,80);
    T("AAA AA:AA",10,10,2,200,220,255);      /* fake "WED 12:00" */
    T("AAAA",SW-80,10,2,200,220,255);         /* fake "DISC" */

    /* channel grid  4 cols × 3 rows */
    int gw=128,gh=96,gx0=28,gy0=55,gxs=148,gys=108;
    for(int row=0;row<3;row++){
        for(int col=0;col<4;col++){
            int cx=gx0+col*gxs, cy=gy0+row*gys;
            /* channel box */
            R(cx,cy,gw,gh,180,190,210);
            R(cx+3,cy+3,gw-6,gh-26,80,100,140);
            /* big A in channel */
            TC("A",cx+gw/2,cy+12,4,255,255,80);
            /* channel label below */
            R(cx,cy+gh-22,gw,22,140,150,170);
            TC("AAAA",cx+gw/2,cy+gh-16,1,20,20,20);
        }
    }

    /* bottom bar */
    R(0,SH-34,SW,34,20,40,80);
    T("A: AAAA   B: AAAA   HOME: AAAA",60,SH-24,1,180,200,255);

    /* banner message */
    R(0,SH-70,SW,34,100,0,0);
    TC("A TAKEOVER COMPLETE - REBOOT TO WII MENU TO SEE FULL EFFECT",SW/2,SH-60,1,255,200,200);

    /* NAND result line */
    char msg[80];
    if(!isfs_ok)
        snprintf(msg,sizeof(msg),"NAND: ISFS UNAVAILABLE (TRY REAL WII)");
    else if(fonts_patched==0)
        snprintf(msg,sizeof(msg),"NAND: NO FONT FILES FOUND IN /SHARED1/FONT");
    else
        snprintf(msg,sizeof(msg),"NAND: %d FONT FILE%s PATCHED",fonts_patched,fonts_patched==1?"":"S");
    TC(msg,SW/2,SH-86,1,200,255,200);

    T("HOME: EXIT",10,SH-108,1,160,160,160);
}

/* ─── draw declined screen ────────────────────────────────────────── */
static void draw_declined(void){
    R(0,0,SW,SH,0,0,20);
    TC("WISE CHOICE.",SW/2,200,3,160,160,220);
    TC("THE A REMAINS CONTAINED.",SW/2,250,2,100,100,180);
    TC("FOR NOW.",SW/2,290,2,80,80,140);
    T("HOME: EXIT",10,SH-30,1,100,100,100);
}

/* ─── main ────────────────────────────────────────────────────────── */
int main(int argc,char**argv){
    (void)argc;(void)argv;

    VIDEO_Init();
    rmode=VIDEO_GetPreferredMode(NULL);
    xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS);

    while(1){
        WPAD_ScanPads();
        u32 down=WPAD_ButtonsDown(0);

        if(down&WPAD_BUTTON_HOME) exit(0);

        switch(state){
        /* ── confirm ── */
        case S_CONFIRM:
            if(down&WPAD_BUTTON_A){ state=S_RUNNING; frame=0; run_pct=0; nand_done=0; font_infected=0; }
            if(down&WPAD_BUTTON_B){ state=S_DECLINED; }
            break;

        /* ── running ── */
        case S_RUNNING:
            /* advance progress */
            run_pct += 0.004f;
            if(run_pct>1.0f) run_pct=1.0f;

            /* patch NAND once we're ~30% through */
            if(!nand_done && run_pct>=0.30f){
                do_nand_patch();
                nand_done=1;
            }

            /* infect local font once we're ~80% through */
            if(!font_infected && run_pct>=0.80f){
                infect_local_font();
                font_infected=1;
            }

            if(run_pct>=1.0f) state=S_DONE;
            break;

        /* ── done ── */
        case S_DONE:
            break;

        /* ── declined ── */
        case S_DECLINED:
            break;
        }

        /* ── draw ── */
        R(0,0,SW,SH,0,0,0);
        switch(state){
        case S_CONFIRM:  draw_confirm();  break;
        case S_RUNNING:  draw_running();  break;
        case S_DONE:     draw_done();     break;
        case S_DECLINED: draw_declined(); break;
        }

        flip();
        frame++;
    }
    return 0;
}
