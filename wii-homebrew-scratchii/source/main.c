/**
 * SCRATCHII — Scratch-like block programming for Nintendo Wii Homebrew
 *
 * Point Wiimote at screen.  Press A to click.
 * Green flag  → run script.   Red stop → halt.
 * CODE tab    → block editor.  COSTUME tab → pixel editor.
 * HOME button → quit.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ── double-buffered framebuffer ────────────────────────────────── */
static void       *xfb[2]={NULL,NULL};
static int         fbi=0;
static GXRModeObj *rmode=NULL;
#define FB  ((u32*)xfb[fbi])
#define SW  640
#define SH  480

static inline u8 C8(int v){return v<0?0:v>255?255:(u8)v;}
static inline u8 RY(u8 r,u8 g,u8 b){return C8(((77*r+150*g+29*b)>>8)+16);}
static inline u8 CB(u8 r,u8 g,u8 b){return C8(((-43*r-85*g+128*b)>>8)+128);}
static inline u8 CR(u8 r,u8 g,u8 b){return C8(((128*r-107*g-21*b)>>8)+128);}

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

/* ── 8×8 bitmap font (ASCII 32–90) ─────────────────────────────── */
static const u8 F[59][8]={
/* 32 ' ' */{0,0,0,0,0,0,0,0},
/* 33 '!' */{24,24,24,24,0,24,0,0},
/* 34–42  */{0},{0},{0},{0},{0},{0},{0},{0},{0},
/* 43 '+' */{0,24,24,126,24,24,0,0},
/* 44 ',' */{0},
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
/* 59–64  */{0},{0},{0},{0},{0},{0},
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
            if(p[row]&(0x80>>col)) R(x+col*sc,y+row*sc,sc,sc,r,g,b);
}

static int TW(const char*s,int sc){
    int n=(int)strlen(s);
    return n?(n*(8*sc+sc)-sc):0;
}
static void T(const char*s,int x,int y,int sc,u8 r,u8 g,u8 b){
    for(;*s;s++,x+=(8+1)*sc) G(*s,x,y,sc,r,g,b);
}
static void TC(const char*s,int cx,int y,int sc,u8 r,u8 g,u8 b){
    T(s,cx-TW(s,sc)/2,y,sc,r,g,b);
}

/* ── button helper ──────────────────────────────────────────────── */
static void BTN(int x,int y,int w,int h,const char*lbl,int sc,
                float hov,u8 br,u8 bg,u8 bb){
    u8 r=C8(br+(int)(hov*55)),g=C8(bg+(int)(hov*55)),b=C8(bb+(int)(hov*55));
    R(x,y,w,h,r,g,b);
    R(x,y,w,2,C8(r+50),C8(g+50),C8(b+50));
    R(x,y,2,h,C8(r+40),C8(g+40),C8(b+40));
    R(x,y+h-2,w,2,C8(r-35),C8(g-35),C8(b-35));
    R(x+w-2,y,2,h,C8(r-35),C8(g-35),C8(b-35));
    int lx=x+(w-TW(lbl,sc))/2, ly=y+(h-8*sc)/2;
    T(lbl,lx,ly,sc,235,240,235);
}

static void CURSOR(int cx,int cy){
    for(int i=-9;i<=9;i++){
        R(cx+i,cy,1,1,255,255,0);
        R(cx,cy+i,1,1,255,255,0);
    }
    R(cx-2,cy-2,5,5,255,110,0);
}

/* ════════════════════════════════════════════════════════════════
   LAYOUT
   ════════════════════════════════════════════════════════════════ */
#define TB_H   40        /* top bar height                         */

/* Stage panel (left 270 px) */
#define STG_PW  270
#define STG_X   2
#define STG_Y   (TB_H+2)
#define STG_W   266
#define STG_H   180
#define STG_CX  (STG_X+STG_W/2)   /* 135 */
#define STG_CY  (STG_Y+STG_H/2)   /* 132 */

/* Info strip below stage */
#define INFO_Y  (STG_Y+STG_H+3)

/* Palette panel */
#define PAL_X   270
#define PAL_W   185
/* Category tabs */
#define CAT_H   30

/* Script panel */
#define SCR_X   455
#define SCR_W   185

/* ════════════════════════════════════════════════════════════════
   COSTUME PALETTE  (8 colours for pixel editor)
   ════════════════════════════════════════════════════════════════ */
#define PCOLS 8
static const u8 PRGB[PCOLS][3]={
    {10,10,10},      /* 0 black  */
    {240,240,240},   /* 1 white  */
    {220,50,50},     /* 2 red    */
    {40,180,40},     /* 3 green  */
    {50,90,220},     /* 4 blue   */
    {220,210,40},    /* 5 yellow */
    {150,40,210},    /* 6 purple */
    {220,140,30},    /* 7 orange */
};

/* ════════════════════════════════════════════════════════════════
   COSTUMES
   ════════════════════════════════════════════════════════════════ */
#define CST_W  16
#define CST_H  16
#define MAX_COSTUMES 5

typedef struct { u8 pix[CST_H][CST_W]; char name[12]; } Costume;

static Costume costumes[MAX_COSTUMES];
static int num_costumes=1;
static int ed_costume=0;   /* costume open in editor */

/* ════════════════════════════════════════════════════════════════
   BLOCKS
   ════════════════════════════════════════════════════════════════ */
typedef enum {
    BLK_NONE=0,
    /* Events */
    BLK_WHEN_FLAG,
    /* Motion */
    BLK_MOVE,
    BLK_TURN_CW,
    BLK_TURN_CCW,
    BLK_GOTO_XY,
    BLK_SET_X,
    BLK_SET_Y,
    BLK_BOUNCE,
    /* Looks */
    BLK_SWITCH_COSTUME,
    BLK_NEXT_COSTUME,
    BLK_SET_SIZE,
    BLK_SAY,
    /* Control */
    BLK_WAIT,
    BLK_REPEAT,
    BLK_FOREVER,
} BlockType;

typedef struct { BlockType type; int val; int val2; } Block;

/* palette entry */
typedef struct { BlockType type; int dv; int dv2; const char*lbl; } PalBlock;

/* Events */
static const PalBlock PE[]={
    {BLK_WHEN_FLAG,  0,0,"WHEN FLAG CLICKED"},
};
#define NE 1

/* Motion */
static const PalBlock PM[]={
    {BLK_MOVE,    10,0,"MOVE 10 STEPS"},
    {BLK_TURN_CW, 15,0,"TURN CW 15"},
    {BLK_TURN_CCW,15,0,"TURN CCW 15"},
    {BLK_GOTO_XY,  0,0,"GO TO X:0 Y:0"},
    {BLK_SET_X,    0,0,"SET X TO 0"},
    {BLK_SET_Y,    0,0,"SET Y TO 0"},
    {BLK_BOUNCE,   0,0,"BOUNCE ON EDGE"},
};
#define NM 7

/* Looks */
static const PalBlock PL[]={
    {BLK_SWITCH_COSTUME,1,0,"SWITCH COSTUME 1"},
    {BLK_NEXT_COSTUME,  0,0,"NEXT COSTUME"},
    {BLK_SET_SIZE,    100,0,"SET SIZE 100"},
    {BLK_SAY,           2,0,"SAY HELLO 2S"},
};
#define NL 4

/* Control */
static const PalBlock PC[]={
    {BLK_WAIT,    1,0,"WAIT 1 SEC"},
    {BLK_REPEAT, 10,0,"REPEAT 10"},
    {BLK_FOREVER, 0,0,"FOREVER"},
};
#define NCT 3

typedef enum {CAT_EVENTS=0,CAT_MOTION,CAT_LOOKS,CAT_CONTROL,CAT_N} Category;

static const char*CNAME[CAT_N]={"EVENTS","MOTION","LOOKS","CTRL"};
static const u8   CRGB[CAT_N][3]={{210,170,20},{45,105,205},{145,45,205},{210,115,20}};

/* ════════════════════════════════════════════════════════════════
   SCRIPT
   ════════════════════════════════════════════════════════════════ */
#define MAX_SCRIPT 30
static Block  script[MAX_SCRIPT];
static int    script_len=0;
static int    sel_block=-1;   /* -1 = none selected */

/* ════════════════════════════════════════════════════════════════
   SPRITE STATE
   ════════════════════════════════════════════════════════════════ */
static float spr_x=0, spr_y=0;
static float spr_size=100;   /* 10..300 % */
static float spr_dir=90;     /* 0=up 90=right 180=down 270=left */
static int   spr_costume=0;
static int   spr_say_timer=0;
#define SAY_TEXT "HELLO"

/* ════════════════════════════════════════════════════════════════
   EXECUTION
   ════════════════════════════════════════════════════════════════ */
static int exec_running=0;
static int exec_pc=0;
static int exec_wait=0;
static int exec_repeat_pc=0;
static int exec_repeat_n=0;
static int exec_forever=0;

/* ════════════════════════════════════════════════════════════════
   UI STATE
   ════════════════════════════════════════════════════════════════ */
typedef enum {VIEW_CODE=0, VIEW_COSTUME} View;
static View     cur_view=VIEW_CODE;
static Category sel_cat=CAT_EVENTS;
/* costume editor */
static int ed_color=1;    /* 0=black, 1=white … */
static int ed_erase=0;

/* ════════════════════════════════════════════════════════════════
   HELPERS
   ════════════════════════════════════════════════════════════════ */

static void block_label(const Block*b, char*out, int sz){
    switch(b->type){
        case BLK_WHEN_FLAG:       snprintf(out,sz,"WHEN FLAG");       break;
        case BLK_MOVE:            snprintf(out,sz,"MOVE %d",b->val);  break;
        case BLK_TURN_CW:         snprintf(out,sz,"CW %d",b->val);    break;
        case BLK_TURN_CCW:        snprintf(out,sz,"CCW %d",b->val);   break;
        case BLK_GOTO_XY:         snprintf(out,sz,"GOTO %d %d",b->val,b->val2); break;
        case BLK_SET_X:           snprintf(out,sz,"SET X %d",b->val); break;
        case BLK_SET_Y:           snprintf(out,sz,"SET Y %d",b->val); break;
        case BLK_BOUNCE:          snprintf(out,sz,"BOUNCE");           break;
        case BLK_SWITCH_COSTUME:  snprintf(out,sz,"COSTUME %d",b->val);break;
        case BLK_NEXT_COSTUME:    snprintf(out,sz,"NEXT CST");         break;
        case BLK_SET_SIZE:        snprintf(out,sz,"SIZE %d",b->val);  break;
        case BLK_SAY:             snprintf(out,sz,"SAY %dS",b->val);  break;
        case BLK_WAIT:            snprintf(out,sz,"WAIT %dS",b->val); break;
        case BLK_REPEAT:          snprintf(out,sz,"REPEAT %d",b->val);break;
        case BLK_FOREVER:         snprintf(out,sz,"FOREVER");         break;
        default:                  snprintf(out,sz,"...");              break;
    }
}

static void block_color(BlockType t, u8*r, u8*g, u8*b){
    if(t==BLK_WHEN_FLAG){ *r=210;*g=170;*b=20; return; }
    if(t>=BLK_MOVE&&t<=BLK_BOUNCE){ *r=45;*g=105;*b=205; return; }
    if(t>=BLK_SWITCH_COSTUME&&t<=BLK_SAY){ *r=145;*g=45;*b=205; return; }
    if(t>=BLK_WAIT&&t<=BLK_FOREVER){ *r=210;*g=115;*b=20; return; }
    *r=70;*g=70;*b=70;
}

static int block_has_val(BlockType t){
    return (t!=BLK_BOUNCE && t!=BLK_NEXT_COSTUME &&
            t!=BLK_FOREVER && t!=BLK_WHEN_FLAG);
}
static int block_has_val2(BlockType t){ return t==BLK_GOTO_XY; }

/* clamp sprite inside stage */
static void clamp_spr(void){
    float sc=spr_size/100.0f*2.0f;
    float hw=CST_W*sc/2.0f, hh=CST_H*sc/2.0f;
    float xm=STG_W/2.0f-hw, ym=STG_H/2.0f-hh;
    if(xm<0)xm=0; if(ym<0)ym=0;
    if(spr_x> xm)spr_x= xm;
    if(spr_x<-xm)spr_x=-xm;
    if(spr_y> ym)spr_y= ym;
    if(spr_y<-ym)spr_y=-ym;
}

static void bounce_spr(void){
    float sc=spr_size/100.0f*2.0f;
    float hw=CST_W*sc/2.0f, hh=CST_H*sc/2.0f;
    float xm=STG_W/2.0f-hw, ym=STG_H/2.0f-hh;
    float rad=spr_dir*3.14159f/180.0f;
    float dx=sinf(rad), dy=-cosf(rad);
    if(spr_x>=xm||spr_x<=-xm) dx=-dx;
    if(spr_y>=ym||spr_y<=-ym) dy=-dy;
    spr_dir=atan2f(dx,-dy)*180.0f/3.14159f;
    if(spr_dir<0) spr_dir+=360.0f;
    clamp_spr();
}

/* ════════════════════════════════════════════════════════════════
   EXECUTION ENGINE
   ════════════════════════════════════════════════════════════════ */
static void exec_start(void){
    exec_running=1;
    exec_pc=0;
    exec_wait=0;
    exec_repeat_pc=0;
    exec_repeat_n=0;
    exec_forever=0;
    spr_say_timer=0;
    /* skip hat block */
    if(script_len>0 && script[0].type==BLK_WHEN_FLAG) exec_pc=1;
}

static void exec_tick(void){
    if(!exec_running) return;
    if(exec_wait>0){ exec_wait--; return; }

    int steps=0;
    while(exec_running && steps<12){
        if(exec_pc>=script_len){
            if(exec_forever){ exec_pc=exec_repeat_pc; }
            else if(exec_repeat_n>0){ exec_repeat_n--; exec_pc=exec_repeat_pc; }
            else { exec_running=0; }
            break;
        }
        Block*bk=&script[exec_pc++];
        steps++;
        float rad;
        switch(bk->type){
            case BLK_WHEN_FLAG: break;
            case BLK_MOVE:
                rad=(spr_dir-90)*3.14159f/180.0f;
                spr_x+=cosf(rad)*(float)bk->val;
                spr_y+=sinf(rad)*(float)bk->val;
                clamp_spr();
                break;
            case BLK_TURN_CW:
                spr_dir+=(float)bk->val;
                if(spr_dir>=360) spr_dir-=360;
                break;
            case BLK_TURN_CCW:
                spr_dir-=(float)bk->val;
                if(spr_dir<0) spr_dir+=360;
                break;
            case BLK_GOTO_XY:
                spr_x=(float)bk->val;
                spr_y=(float)bk->val2;
                clamp_spr();
                break;
            case BLK_SET_X: spr_x=(float)bk->val; clamp_spr(); break;
            case BLK_SET_Y: spr_y=(float)bk->val; clamp_spr(); break;
            case BLK_BOUNCE: bounce_spr(); break;
            case BLK_SWITCH_COSTUME:
                spr_costume=bk->val-1;
                if(spr_costume<0) spr_costume=0;
                if(spr_costume>=num_costumes) spr_costume=num_costumes-1;
                break;
            case BLK_NEXT_COSTUME:
                spr_costume=(spr_costume+1)%num_costumes;
                break;
            case BLK_SET_SIZE:
                spr_size=(float)bk->val;
                if(spr_size<10) spr_size=10;
                if(spr_size>300) spr_size=300;
                break;
            case BLK_SAY:
                spr_say_timer=bk->val*60;
                exec_wait=bk->val*60;
                return;
            case BLK_WAIT:
                exec_wait=bk->val*60;
                return;
            case BLK_REPEAT:
                exec_repeat_pc=exec_pc;
                exec_repeat_n=bk->val>0?bk->val-1:0;
                exec_forever=0;
                break;
            case BLK_FOREVER:
                exec_repeat_pc=exec_pc;
                exec_forever=1;
                exec_repeat_n=0;
                break;
            default: break;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — costume thumbnail
   ════════════════════════════════════════════════════════════════ */
static void draw_thumb(int ci, int x, int y, int cell){
    Costume*c=&costumes[ci];
    float sc=(float)cell/CST_W;
    for(int py=0;py<CST_H;py++){
        for(int px=0;px<CST_W;px++){
            u8 ci2=c->pix[py][px];
            int x0=x+(int)(px*sc), y0=y+(int)(py*sc);
            int pw=(int)((px+1)*sc)-(int)(px*sc);
            int ph=(int)((py+1)*sc)-(int)(py*sc);
            if(pw<1)pw=1; if(ph<1)ph=1;
            R(x0,y0,pw,ph,PRGB[ci2][0],PRGB[ci2][1],PRGB[ci2][2]);
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — stage panel
   ════════════════════════════════════════════════════════════════ */
static void draw_stage_panel(void){
    /* panel bg */
    R(0,TB_H,STG_PW,SH-TB_H,22,24,34);
    /* stage area: light sky */
    R(STG_X,STG_Y,STG_W,STG_H,36,42,66);
    /* faint grid cross */
    R(STG_CX,STG_Y,1,STG_H,50,55,80);
    R(STG_X,STG_CY,STG_W,1,50,55,80);
    /* stage border */
    R(STG_X,STG_Y,STG_W,2,70,80,120);
    R(STG_X,STG_Y+STG_H-2,STG_W,2,70,80,120);
    R(STG_X,STG_Y,2,STG_H,70,80,120);
    R(STG_X+STG_W-2,STG_Y,2,STG_H,70,80,120);

    /* ── sprite ── */
    Costume*c=&costumes[spr_costume];
    float sc2=spr_size/100.0f*2.0f;
    int sw=(int)(CST_W*sc2), sh=(int)(CST_H*sc2);
    if(sw<1)sw=1; if(sh<1)sh=1;
    int sx=STG_CX+(int)spr_x-sw/2;
    int sy=STG_CY-(int)spr_y-sh/2;
    for(int py=0;py<CST_H;py++){
        for(int px=0;px<CST_W;px++){
            u8 ci=c->pix[py][px];
            int x0=sx+(int)(px*sc2), y0=sy+(int)(py*sc2);
            int pw=(int)((px+1)*sc2)-(int)(px*sc2);
            int ph=(int)((py+1)*sc2)-(int)(py*sc2);
            if(pw<1)pw=1; if(ph<1)ph=1;
            if(x0+pw<=STG_X||x0>=STG_X+STG_W) continue;
            if(y0+ph<=STG_Y||y0>=STG_Y+STG_H) continue;
            R(x0,y0,pw,ph,PRGB[ci][0],PRGB[ci][1],PRGB[ci][2]);
        }
    }

    /* ── say bubble ── */
    if(spr_say_timer>0){
        int bx=sx+sw+4, by=sy-20;
        if(bx+60>STG_X+STG_W) bx=sx-64;
        if(bx<STG_X) bx=STG_X;
        if(by<STG_Y) by=STG_Y+2;
        R(bx,by,60,16,245,245,245);
        R(bx,by,60,2,180,180,180);
        R(bx,by+14,60,2,180,180,180);
        R(bx,by,2,16,180,180,180);
        R(bx+58,by,2,16,180,180,180);
        T(SAY_TEXT,bx+3,by+4,1,20,20,20);
    }

    /* ── execution indicator ── */
    if(exec_running){
        R(STG_X,STG_Y,STG_W,3,80,220,80);
    }

    /* ── info strip ── */
    int iy=INFO_Y;
    R(0,iy,STG_PW,SH-iy,18,20,30);
    R(0,iy,STG_PW,1,45,50,70);

    char buf[24];
    snprintf(buf,sizeof(buf),"X:%d",(int)spr_x);
    T(buf,4,iy+5,1,120,150,200);
    snprintf(buf,sizeof(buf),"Y:%d",(int)spr_y);
    T(buf,60,iy+5,1,120,150,200);
    snprintf(buf,sizeof(buf),"SIZE:%d",(int)spr_size);
    T(buf,4,iy+18,1,120,150,200);
    snprintf(buf,sizeof(buf),"DIR:%d",(int)spr_dir);
    T(buf,100,iy+18,1,120,150,200);
    T(costumes[spr_costume].name,4,iy+31,1,100,130,180);

    /* thumbnail */
    draw_thumb(spr_costume,200,iy+3,44);
    R(198,iy+1,48,2,80,110,180);
    R(198,iy+47,48,2,80,110,180);
    R(198,iy+1,2,48,80,110,180);
    R(246,iy+1,2,48,80,110,180);
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — block palette
   ════════════════════════════════════════════════════════════════ */
static const PalBlock* cat_pal(Category c, int*n){
    switch(c){
        case CAT_EVENTS:  *n=NE;  return PE;
        case CAT_MOTION:  *n=NM;  return PM;
        case CAT_LOOKS:   *n=NL;  return PL;
        case CAT_CONTROL: *n=NCT; return PC;
        default: *n=0; return NULL;
    }
}

static void draw_palette(int cx, int cy){
    R(PAL_X,TB_H,PAL_W,SH-TB_H,18,20,30);
    R(PAL_X,TB_H,2,SH-TB_H,50,55,72);

    /* category tabs */
    int tw=PAL_W/CAT_N;
    for(int i=0;i<CAT_N;i++){
        int tx=PAL_X+i*tw;
        int sel=(sel_cat==i);
        int hov=(cx>=tx&&cx<tx+tw&&cy>=TB_H&&cy<TB_H+CAT_H)?1:0;
        u8 r=CRGB[i][0],g=CRGB[i][1],b=CRGB[i][2];
        if(sel)      R(tx,TB_H,tw,CAT_H,r,g,b);
        else if(hov) R(tx,TB_H,tw,CAT_H,r/4,g/4,b/4+10);
        else         R(tx,TB_H,tw,CAT_H,r/4,g/4,b/4);
        TC(CNAME[i],tx+tw/2,TB_H+11,1,230,235,245);
        if(sel) R(tx,TB_H+CAT_H-3,tw,3,r,g,b);
    }
    R(PAL_X,TB_H+CAT_H,PAL_W,2,50,55,72);

    /* block list */
    int n; const PalBlock*pb=cat_pal(sel_cat,&n);
    int bh=34, by=TB_H+CAT_H+4;
    u8 cr=CRGB[sel_cat][0],cg=CRGB[sel_cat][1],cb=CRGB[sel_cat][2];
    for(int i=0;i<n&&by+bh<=SH-2;i++){
        int hov=(cx>=PAL_X+4&&cx<PAL_X+PAL_W-4&&cy>=by&&cy<by+bh-2)?1:0;
        int bx=PAL_X+4, bw=PAL_W-8;

        /* hat bump for when-flag */
        if(pb[i].type==BLK_WHEN_FLAG){
            R(bx,by-5,36,7,hov?C8(cr+30):cr/3+10,hov?C8(cg+30):cg/3+5,hov?C8(cb+30):cb/3+5);
            R(bx,by-5,36,2,C8(cr+60),C8(cg+50),C8(cb+30));
        }
        R(bx,by,bw,bh-2,hov?C8(cr/3+30):cr/3+10,hov?C8(cg/3+30):cg/3+5,hov?C8(cb/3+30):cb/3+5);
        R(bx,by,bw,2,C8(cr+50),C8(cg+40),C8(cb+20));
        R(bx,by,2,bh-2,C8(cr+30),C8(cg+30),C8(cb+10));
        T(pb[i].lbl,bx+6,by+(bh-2-8)/2,1,240,245,240);
        by+=bh+3;
    }
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — script panel
   ════════════════════════════════════════════════════════════════ */
static void draw_script(int cx, int cy){
    R(SCR_X,TB_H,SCR_W,SH-TB_H,14,16,26);
    R(SCR_X,TB_H,2,SH-TB_H,50,55,72);

    T("SCRIPTS",SCR_X+7,TB_H+6,1,100,120,160);
    R(SCR_X,TB_H+18,SCR_W,1,38,42,58);

    int bw=SCR_W-10, bh=26;
    int by=TB_H+22;
    int bot=SH-64;

    for(int i=0;i<script_len && by+bh<=bot;i++){
        Block*bk=&script[i];
        char lbl[22]; block_label(bk,lbl,sizeof(lbl));
        u8 r,g,b; block_color(bk->type,&r,&g,&b);
        int sel=(i==sel_block);
        int hov=(cx>=SCR_X+5&&cx<SCR_X+5+bw&&cy>=by&&cy<by+bh)?1:0;

        /* selection glow */
        if(sel) R(SCR_X+3,by-2,bw+4,bh+4,200,180,30);

        /* hat bump */
        if(bk->type==BLK_WHEN_FLAG){
            R(SCR_X+5,by-5,32,7,r,g,b);
            R(SCR_X+5,by-5,32,2,C8(r+50),C8(g+40),C8(b+20));
            by+=3;
        }

        R(SCR_X+5,by,bw,bh,
          hov?C8(r+20):r, hov?C8(g+20):g, hov?C8(b+20):b);
        R(SCR_X+5,by,bw,2,C8(r+55),C8(g+45),C8(b+25));
        R(SCR_X+5,by,3,bh,C8(r+65),C8(g+55),C8(b+30));
        T(lbl,SCR_X+11,by+(bh-8)/2,1,240,245,240);
        by+=bh+3;
    }

    if(script_len==0){
        TC("CLICK BLOCKS",SCR_X+SCR_W/2,TB_H+50,1,50,55,75);
        TC("TO BUILD",SCR_X+SCR_W/2,TB_H+62,1,50,55,75);
        TC("YOUR SCRIPT",SCR_X+SCR_W/2,TB_H+74,1,50,55,75);
    }

    /* separator */
    R(SCR_X,bot,SCR_W,1,38,42,58);

    /* val editor */
    if(sel_block>=0 && sel_block<script_len){
        Block*bk=&script[sel_block];
        int by2=bot+4;
        if(block_has_val(bk->type)){
            char vb[10]; snprintf(vb,sizeof(vb),"%d",bk->val);
            T("VAL:",SCR_X+5,by2,1,120,135,170);
            R(SCR_X+36,by2-1,32,13,22,24,38);
            TC(vb,SCR_X+36+16,by2+2,1,210,225,210);
            BTN(SCR_X+70,by2-1,14,12,"-",1,0,90,35,35);
            BTN(SCR_X+86,by2-1,14,12,"+",1,0,35,90,35);
            by2+=16;
        }
        if(block_has_val2(bk->type)){
            char vb2[10]; snprintf(vb2,sizeof(vb2),"%d",bk->val2);
            T("Y:",SCR_X+5,by2,1,120,135,170);
            R(SCR_X+20,by2-1,32,13,22,24,38);
            TC(vb2,SCR_X+20+16,by2+2,1,210,225,210);
            BTN(SCR_X+54,by2-1,14,12,"-",1,0,90,35,35);
            BTN(SCR_X+70,by2-1,14,12,"+",1,0,35,90,35);
            by2+=16;
        }
        BTN(SCR_X+5,by2,bw,14,"DEL BLOCK",1,0,110,28,28);
    }
    BTN(SCR_X+5,SH-16,bw,14,"CLEAR ALL",1,0,60,32,32);
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — top bar
   ════════════════════════════════════════════════════════════════ */
static void draw_topbar(int cx, int cy){
    R(0,0,SW,TB_H,24,26,38);
    R(0,TB_H-1,SW,1,50,55,72);

    /* green flag button */
    int fh=(cx>=5&&cx<37&&cy>=5&&cy<35)?1:0;
    R(5,5,32,30,fh?50:30,fh?190:140,fh?50:30);
    R(5,5,32,2,60,220,80);
    /* flag icon: triangle */
    for(int i=0;i<12;i++) R(14,9+i,11-i*8/12,2,200,240,80);

    /* stop button */
    int sh2=(cx>=42&&cx<74&&cy>=5&&cy<35)?1:0;
    R(42,5,32,30,sh2?210:160,sh2?35:25,sh2?35:25);
    R(42,5,32,2,220,60,60);
    R(50,12,16,14,240,210,210);

    /* running dot */
    if(exec_running) R(80,12,10,10,80,255,80);

    /* title */
    TC("SCRATCHII",280,13,2,80,170,255);

    /* CODE tab */
    int cc=(cur_view==VIEW_CODE);
    int ch=(cx>=470&&cx<552&&cy>=4&&cy<36)?1:0;
    R(470,4,82,32,cc?55:ch?40:28,cc?85:ch?60:38,cc?140:ch?90:55);
    if(cc) R(470,4,82,3,80,160,255);
    TC("CODE",511,15,1,225,235,250);

    /* COSTUME tab */
    int vc=(cur_view==VIEW_COSTUME);
    int vh=(cx>=556&&cx<638&&cy>=4&&cy<36)?1:0;
    R(556,4,82,32,vc?55:vh?40:28,vc?85:vh?60:38,vc?140:vh?90:55);
    if(vc) R(556,4,82,3,80,160,255);
    TC("COSTUME",597,15,1,225,235,250);
}

/* ════════════════════════════════════════════════════════════════
   RENDERING — costume editor
   ════════════════════════════════════════════════════════════════ */
#define CE_CELL 14
#define CE_GX   5
#define CE_GY   (TB_H+5)

static void draw_costume_editor(int cx, int cy){
    R(0,TB_H,SW,SH-TB_H,14,16,26);

    Costume*c=&costumes[ed_costume];

    /* checkerboard background */
    for(int py=0;py<CST_H;py++)
        for(int px=0;px<CST_W;px++){
            u8 cb2=((py+px)&1)?50:36;
            R(CE_GX+px*CE_CELL,CE_GY+py*CE_CELL,CE_CELL,CE_CELL,cb2,cb2,cb2);
        }

    /* pixels */
    for(int py=0;py<CST_H;py++)
        for(int px=0;px<CST_W;px++){
            u8 ci=c->pix[py][px];
            if(ci==0) continue;
            R(CE_GX+px*CE_CELL,CE_GY+py*CE_CELL,CE_CELL,CE_CELL,
              PRGB[ci][0],PRGB[ci][1],PRGB[ci][2]);
        }

    /* grid lines */
    for(int i=0;i<=CST_W;i++) R(CE_GX+i*CE_CELL,CE_GY,1,CST_H*CE_CELL,44,48,65);
    for(int j=0;j<=CST_H;j++) R(CE_GX,CE_GY+j*CE_CELL,CST_W*CE_CELL,1,44,48,65);

    /* hover highlight */
    if(cx>=CE_GX&&cy>=CE_GY&&cx<CE_GX+CST_W*CE_CELL&&cy<CE_GY+CST_H*CE_CELL){
        int hc=(cx-CE_GX)/CE_CELL, hr=(cy-CE_GY)/CE_CELL;
        R(CE_GX+hc*CE_CELL,CE_GY+hr*CE_CELL,CE_CELL,1,255,255,100);
        R(CE_GX+hc*CE_CELL,CE_GY+hr*CE_CELL,1,CE_CELL,255,255,100);
        R(CE_GX+hc*CE_CELL,CE_GY+hr*CE_CELL+CE_CELL-1,CE_CELL,1,255,255,100);
        R(CE_GX+hc*CE_CELL+CE_CELL-1,CE_GY+hr*CE_CELL,1,CE_CELL,255,255,100);
    }

    /* costume name */
    T(c->name,CE_GX,CE_GY+CST_H*CE_CELL+5,1,130,150,190);

    /* ── color palette (right of grid) ── */
    int px2=240, py2=TB_H+8;
    T("COLORS",px2,py2,1,120,140,180);
    py2+=13;
    for(int i=0;i<PCOLS;i++){
        int col=i%4, row=i/4;
        int sx=px2+col*32, sy=py2+row*32;
        R(sx,sy,28,28,PRGB[i][0],PRGB[i][1],PRGB[i][2]);
        /* border */
        R(sx,sy,28,1,80,80,80); R(sx,sy+27,28,1,80,80,80);
        R(sx,sy,1,28,80,80,80); R(sx+27,sy,1,28,80,80,80);
        /* selected */
        if(i==ed_color && !ed_erase){
            R(sx-2,sy-2,32,2,255,220,50);
            R(sx-2,sy+28,32,2,255,220,50);
            R(sx-2,sy-2,2,32,255,220,50);
            R(sx+28,sy-2,2,32,255,220,50);
        }
    }
    py2+=70;

    /* eraser */
    int eh=(cx>=px2&&cx<px2+80&&cy>=py2&&cy<py2+20);
    R(px2,py2,80,20,ed_erase?100:35,ed_erase?90:40,ed_erase?100:50);
    R(px2,py2,80,2,ed_erase?160:60,ed_erase?140:55,ed_erase?160:65);
    T(ed_erase?"ERASE ON":"ERASE OFF",px2+3,py2+6,1,225,230,225);
    py2+=26;

    /* clear */
    BTN(px2,py2,80,18,"CLEAR",1,0,100,30,30);

    /* ── costume list ── */
    int rx=340, ry=TB_H+8;
    T("COSTUMES",rx,ry,1,120,140,180);
    ry+=13;
    for(int i=0;i<num_costumes;i++){
        int hov2=(cx>=rx&&cx<rx+50&&cy>=ry&&cy<ry+50)?1:0;
        u8 bc=(i==ed_costume)?70:(u8)(hov2?50:28);
        R(rx,ry,50,50,bc,bc+(u8)(i==ed_costume?35:0),bc+(u8)(i==ed_costume?60:0));
        draw_thumb(i,rx+1,ry+1,48);
        if(i==ed_costume){
            R(rx-2,ry-2,54,2,80,160,255);
            R(rx-2,ry+50,54,2,80,160,255);
            R(rx-2,ry-2,2,54,80,160,255);
            R(rx+50,ry-2,2,54,80,160,255);
        }
        T(costumes[i].name,rx+54,ry+20,1,110,130,170);
        ry+=56;
    }
    /* + NEW */
    if(num_costumes<MAX_COSTUMES){
        float nh2=(cx>=rx&&cx<rx+70&&cy>=ry&&cy<ry+20)?1.0f:0.0f;
        BTN(rx,ry,70,20,"+ NEW",1,nh2,28,70,28);
    }
}

/* ════════════════════════════════════════════════════════════════
   INIT
   ════════════════════════════════════════════════════════════════ */
static void init_costumes(void){
    /* Costume 1: white square with blue border */
    strncpy(costumes[0].name,"COSTUME1",11);
    for(int py=0;py<CST_H;py++)
        for(int px=0;px<CST_W;px++){
            if(py==0||py==CST_H-1||px==0||px==CST_W-1)
                costumes[0].pix[py][px]=4; /* blue */
            else
                costumes[0].pix[py][px]=1; /* white */
        }
    num_costumes=1;
}

/* ════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════ */
int main(int argc,char**argv){
    (void)argc;(void)argv;

    VIDEO_Init();
    WPAD_Init();

    rmode=VIDEO_GetPreferredMode(NULL);
    xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[fbi]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0,rmode->fbWidth,rmode->xfbHeight);

    init_costumes();

    int aw=0; /* A held last frame */

    while(1){
        WPAD_ScanPads();
        u32 held=WPAD_ButtonsHeld(0);
        u32 down=WPAD_ButtonsDown(0);

        if(down&WPAD_BUTTON_HOME) exit(0);

        WPADData*wd=WPAD_Data(WPAD_CHAN_0);
        int cx=320,cy=240;
        if(wd&&wd->ir.valid){ cx=(int)wd->ir.x; cy=(int)wd->ir.y; }

        int an=(held&WPAD_BUTTON_A)!=0;
        int ad=an&&!aw;
        aw=an;

        /* ── global buttons ── */
        /* green flag */
        if(ad&&cx>=5&&cx<37&&cy>=5&&cy<35){
            exec_start();
            cur_view=VIEW_CODE;
        }
        /* stop */
        if(ad&&cx>=42&&cx<74&&cy>=5&&cy<35){
            exec_running=0;
            exec_wait=0;
            spr_say_timer=0;
        }
        /* tab switches */
        if(ad&&cx>=470&&cx<552&&cy>=4&&cy<36) cur_view=VIEW_CODE;
        if(ad&&cx>=556&&cx<638&&cy>=4&&cy<36) cur_view=VIEW_COSTUME;

        /* ── execution tick ── */
        exec_tick();
        if(spr_say_timer>0) spr_say_timer--;

        /* ── view input ── */
        if(cur_view==VIEW_CODE){

            /* category tab */
            int tw=PAL_W/CAT_N;
            for(int i=0;i<CAT_N;i++){
                int tx=PAL_X+i*tw;
                if(ad&&cx>=tx&&cx<tx+tw&&cy>=TB_H&&cy<TB_H+CAT_H)
                    sel_cat=(Category)i;
            }

            /* palette block click → add to script */
            int n; const PalBlock*pb=cat_pal(sel_cat,&n);
            int bh=34, by=TB_H+CAT_H+4;
            for(int i=0;i<n&&by+bh<=SH-2;i++){
                if(ad&&cx>=PAL_X+4&&cx<PAL_X+PAL_W-4&&cy>=by&&cy<by+bh-2){
                    if(script_len<MAX_SCRIPT){
                        script[script_len].type=pb[i].type;
                        script[script_len].val =pb[i].dv;
                        script[script_len].val2=pb[i].dv2;
                        sel_block=script_len;
                        script_len++;
                    }
                }
                by+=bh+3;
            }

            /* script block click → select / deselect */
            int bw=SCR_W-10; bh=26;
            by=TB_H+22;
            int bot=SH-64;
            for(int i=0;i<script_len&&by+bh<=bot;i++){
                if(script[i].type==BLK_WHEN_FLAG) by+=3;
                if(ad&&cx>=SCR_X+5&&cx<SCR_X+5+bw&&cy>=by&&cy<by+bh)
                    sel_block=(sel_block==i)?-1:i;
                by+=bh+3;
            }

            /* value editor */
            if(sel_block>=0&&sel_block<script_len){
                Block*bk=&script[sel_block];
                int by2=bot+4;
                /* val -/+ */
                if(block_has_val(bk->type)){
                    if(ad&&cx>=SCR_X+70&&cx<SCR_X+84&&cy>=by2-1&&cy<by2+11) bk->val--;
                    if(ad&&cx>=SCR_X+86&&cx<SCR_X+100&&cy>=by2-1&&cy<by2+11) bk->val++;
                    by2+=16;
                }
                /* val2 -/+ */
                if(block_has_val2(bk->type)){
                    if(ad&&cx>=SCR_X+54&&cx<SCR_X+68&&cy>=by2-1&&cy<by2+11) bk->val2--;
                    if(ad&&cx>=SCR_X+70&&cx<SCR_X+84&&cy>=by2-1&&cy<by2+11) bk->val2++;
                    by2+=16;
                }
                /* delete */
                if(ad&&cx>=SCR_X+5&&cx<SCR_X+5+bw&&cy>=by2&&cy<by2+14){
                    for(int i=sel_block;i<script_len-1;i++) script[i]=script[i+1];
                    script_len--;
                    sel_block=-1;
                }
            }
            /* clear all */
            if(ad&&cx>=SCR_X+5&&cx<SCR_X+5+SCR_W-10&&cy>=SH-16&&cy<SH-2){
                script_len=0; sel_block=-1; exec_running=0;
            }

        } else { /* VIEW_COSTUME */

            /* color palette */
            int px3=240, py3=TB_H+8+13;
            for(int i=0;i<PCOLS;i++){
                int col=i%4, row=i/4;
                int sx=px3+col*32, sy=py3+row*32;
                if(ad&&cx>=sx&&cx<sx+28&&cy>=sy&&cy<sy+28){
                    ed_color=i; ed_erase=0;
                }
            }
            /* eraser toggle */
            int ep=py3+70;
            if(ad&&cx>=px3&&cx<px3+80&&cy>=ep&&cy<ep+20) ed_erase=!ed_erase;
            /* clear costume */
            if(ad&&cx>=px3&&cx<px3+80&&cy>=ep+26&&cy<ep+44)
                memset(costumes[ed_costume].pix,0,sizeof(costumes[ed_costume].pix));

            /* costume list */
            int rx=340, ry=TB_H+8+13;
            for(int i=0;i<num_costumes;i++){
                if(ad&&cx>=rx&&cx<rx+50&&cy>=ry&&cy<ry+50){
                    ed_costume=i; spr_costume=i;
                }
                ry+=56;
            }
            /* new costume */
            if(num_costumes<MAX_COSTUMES){
                if(ad&&cx>=340&&cx<410&&cy>=ry&&cy<ry+20){
                    char nm[12];
                    snprintf(nm,sizeof(nm),"COSTUME%d",num_costumes+1);
                    strncpy(costumes[num_costumes].name,nm,11);
                    memset(costumes[num_costumes].pix,0,sizeof(costumes[num_costumes].pix));
                    ed_costume=num_costumes;
                    spr_costume=num_costumes;
                    num_costumes++;
                }
            }

            /* grid drawing: A held while pointing at grid */
            if(an&&cx>=CE_GX&&cy>=CE_GY&&
               cx<CE_GX+CST_W*CE_CELL&&cy<CE_GY+CST_H*CE_CELL){
                int hc=(cx-CE_GX)/CE_CELL, hr=(cy-CE_GY)/CE_CELL;
                if(hc>=0&&hc<CST_W&&hr>=0&&hr<CST_H)
                    costumes[ed_costume].pix[hr][hc]=ed_erase?0:(u8)ed_color;
            }
        }

        /* ── render ── */
        R(0,0,SW,SH,10,12,20);

        if(cur_view==VIEW_CODE){
            draw_stage_panel();
            draw_palette(cx,cy);
            draw_script(cx,cy);
        } else {
            draw_costume_editor(cx,cy);
        }
        draw_topbar(cx,cy);
        CURSOR(cx,cy);

        VIDEO_SetNextFramebuffer(xfb[fbi]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fbi^=1;
    }
    return 0;
}
