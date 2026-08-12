/*
 * LINE TAKEOVER — Wii Homebrew
 *
 * Draws scrolling black stripes over everything on screen.
 * Effect is purely in-memory — exits cleanly to Wii Menu on HOME,
 * taking all stripes with it.
 *
 * Controls:
 *   A      — confirm YES
 *   B      — decline / exit to Wii Menu
 *   HOME   — exit to Wii Menu (effect disappears)
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

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
static const u8 F[59][8]={
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

/* ─── flip ────────────────────────────────────────────────────────── */
static void flip(void){
    VIDEO_SetNextFramebuffer(xfb[fbi]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    fbi^=1;
}

/* ─── return to Wii Menu ──────────────────────────────────────────── */
static void goto_menu(void){
    SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0);
}

/* ─── states ──────────────────────────────────────────────────────── */
typedef enum { S_CONFIRM, S_ACTIVE, S_DECLINED } State;
static State state   = S_CONFIRM;
static u32   frame   = 0;

/* stripe scroll offset — advances each frame */
static int stripe_offset = 0;

/* stripe config */
#define STRIPE_BLACK  5   /* pixels of black per stripe */
#define STRIPE_GAP   10   /* pixels of visible content per stripe */
#define STRIPE_PERIOD (STRIPE_BLACK + STRIPE_GAP)

/* ─── draw the "Wii screen" background that sits behind the stripes ─ */
static void draw_background(void){
    /* Wii Menu blue sky gradient */
    for(int y=0;y<SH;y++){
        u8 t=(u8)(y*255/SH);
        R(0,y,SW,1, C8(20+t/5), C8(60+t/4), C8(130+t/3));
    }

    /* top bar */
    R(0,0,SW,38,15,35,75);
    T("WII MENU",12,10,2,200,220,255);
    T("12:00",SW-80,10,2,200,220,255);

    /* channel grid  4 × 2 */
    static const char *labels[8]={"DISC","SHOP","PHOTOS","WEATHER","NEWS","VOTES","MIIRZZ","INTERNET"};
    int gw=130,gh=100,gxs=150,gys=112,gx0=18,gy0=50;
    for(int row=0;row<2;row++){
        for(int col=0;col<4;col++){
            int cx=gx0+col*gxs, cy=gy0+row*gys;
            /* channel tile */
            R(cx,cy,gw,gh,170,185,210);
            /* coloured icon area */
            u8 cr=(u8)((col*80+row*40)%220+35);
            u8 cg=(u8)((col*50+row*90+80)%200+55);
            u8 cb=(u8)((col*30+row*60+120)%200+55);
            R(cx+4,cy+4,gw-8,gh-28,cr,cg,cb);
            /* label bar */
            R(cx,cy+gh-24,gw,24,130,145,170);
            TC(labels[row*4+col],cx+gw/2,cy+gh-18,1,20,20,40);
        }
    }

    /* bottom bar */
    R(0,SH-34,SW,34,15,35,75);
    T("A: SELECT   HOME: MENU   +: OPTIONS",60,SH-24,1,160,180,220);

    /* spinning LINE TAKEOVER badge */
    int bx=SW-130,by=SH-115,bw=118,bh=70;
    u8 pulse=(u8)(160+80*((frame%60)/60.0f));
    R(bx,by,bw,bh,0,0,0);
    R(bx+2,by+2,bw-4,bh-4,20,20,20);
    TC("LINE",bx+bw/2,by+8,2,pulse,pulse,pulse);
    TC("TAKEOVER",bx+bw/2,by+36,1,pulse,pulse,pulse);
    TC("ACTIVE",bx+bw/2,by+52,1,0,200,0);
}

/* ─── draw scrolling black stripes on top of whatever is on screen ── */
static void draw_stripes(void){
    /* stripe_offset makes stripes scroll downward each frame */
    int base = stripe_offset % STRIPE_PERIOD;
    for(int y = base - STRIPE_PERIOD; y < SH; y += STRIPE_PERIOD){
        int black_y = y;
        int black_h = STRIPE_BLACK;
        /* clamp */
        if(black_y < 0){ black_h += black_y; black_y = 0; }
        if(black_y + black_h > SH) black_h = SH - black_y;
        if(black_h <= 0) continue;
        R(0, black_y, SW, black_h, 0, 0, 0);
    }
}

/* ─── confirm screen ──────────────────────────────────────────────── */
static void draw_confirm(void){
    R(0,0,SW,SH,10,10,10);

    /* animated stripe preview in top half */
    int prev_h=180, prev_y=20;
    R(0,prev_y,SW,prev_h,20,60,100);

    /* draw a mini channel grid in preview */
    for(int col=0;col<4;col++){
        int cx=18+col*150, cy=prev_y+10;
        R(cx,cy,130,90,150,170,200);
        R(cx+4,cy+4,122,62,(u8)(col*60+40),(u8)(col*30+80),(u8)(col*20+140));
        R(cx,cy+66,130,24,110,130,160);
    }

    /* animated stripes over preview */
    int base=(stripe_offset*2) % STRIPE_PERIOD;
    for(int y=prev_y+base-STRIPE_PERIOD; y<prev_y+prev_h; y+=STRIPE_PERIOD){
        int by=y, bh=STRIPE_BLACK;
        if(by<prev_y){bh+=by-prev_y; by=prev_y;}
        if(by+bh>prev_y+prev_h) bh=prev_y+prev_h-by;
        if(bh>0) R(0,by,SW,bh,0,0,0);
    }

    /* label over preview */
    TC("PREVIEW",SW/2,prev_y+prev_h/2-8,2,200,200,200);

    /* question */
    R(0,215,SW,3,80,80,80);
    TC("RUN THE LINE TAKEOVER?",SW/2,230,3,255,255,255);
    TC("COVERS THE WHOLE SCREEN WITH SCROLLING BLACK LINES.",SW/2,272,1,180,180,180);
    TC("PRESS HOME AT ANY TIME TO EXIT TO WII MENU.",SW/2,287,1,180,180,180);
    TC("EFFECT IS MEMORY ONLY - DISAPPEARS ON EXIT.",SW/2,302,1,100,220,100);
    R(0,318,SW,3,80,80,80);

    /* YES button */
    int yw=160,yh=52,yx=SW/2-yw-12,yy=335;
    R(yx,yy,yw,yh,0,120,0);
    R(yx,yy,yw,3,80,220,80); R(yx,yy,3,yh,80,220,80);
    R(yx,yy+yh-3,yw,3,0,60,0); R(yx+yw-3,yy,3,yh,0,60,0);
    TC("YES (A)",yx+yw/2,yy+yh/2-8,2,200,255,200);

    /* NO button */
    int nw=160,nh=52,nx=SW/2+12,ny=335;
    R(nx,ny,nw,nh,60,60,60);
    R(nx,ny,nw,3,140,140,140); R(nx,ny,3,nh,140,140,140);
    R(nx,ny+nh-3,nw,3,30,30,30); R(nx+nw-3,ny,3,nh,30,30,30);
    TC("NO (B)",nx+nw/2,ny+nh/2-8,2,200,200,200);

    T("HOME: EXIT TO WII MENU",10,SH-24,1,120,120,120);
}

/* ─── declined screen ─────────────────────────────────────────────── */
static void draw_declined(void){
    R(0,0,SW,SH,10,10,20);
    TC("THE LINES REMAIN CONTAINED.",SW/2,210,2,160,160,200);
    TC("RETURNING TO WII MENU...",SW/2,260,2,100,100,160);
}

/* ─── main ────────────────────────────────────────────────────────── */
int main(int argc, char **argv){
    (void)argc;(void)argv;

    VIDEO_Init();
    rmode = VIDEO_GetPreferredMode(NULL);
    xfb[0] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1] = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[0]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    WPAD_Init();
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS);

    while(1){
        WPAD_ScanPads();
        u32 down = WPAD_ButtonsDown(0);

        /* HOME always returns to Wii Menu — effect vanishes instantly */
        if(down & WPAD_BUTTON_HOME) goto_menu();

        switch(state){
        case S_CONFIRM:
            if(down & WPAD_BUTTON_A){ state = S_ACTIVE; frame = 0; }
            if(down & WPAD_BUTTON_B){ state = S_DECLINED; frame = 0; }
            break;
        case S_ACTIVE:
            /* advance stripe scroll — 1 pixel per frame = smooth 60fps scroll */
            stripe_offset = (stripe_offset + 1) % STRIPE_PERIOD;
            break;
        case S_DECLINED:
            /* wait half a second then jump to Wii Menu */
            if(frame > 35) goto_menu();
            break;
        }

        /* ── draw ── */
        switch(state){
        case S_CONFIRM:
            draw_confirm();
            break;
        case S_ACTIVE:
            draw_background();
            draw_stripes();
            /* reminder in the gaps */
            T("HOME: EXIT TO WII MENU",10,SH-24,1,200,200,200);
            break;
        case S_DECLINED:
            draw_declined();
            break;
        }

        flip();
        frame++;
    }
    return 0;
}
