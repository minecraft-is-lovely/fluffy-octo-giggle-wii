/**
 * FLIPNOTE STUDIO WII — Frame-by-frame animation creator
 *
 * Hold A to draw.  B + draw = erase (paint white).
 * Green flag / Play button → 20 fps playback.
 * + button → new frame (copy of current).
 * Save → writes AVI to sd:/apps/flipnote_studio_wii/videos/
 * HOME → quit.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <sys/stat.h>
#include <sys/types.h>
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

/* ── 8×8 font (ASCII 32–90) ─────────────────────────────────────── */
static const u8 F[59][8]={
{0,0,0,0,0,0,0,0},            /* 32 ' '  */
{24,24,24,24,0,24,0,0},       /* 33 '!'  */
{0},{0},{0},{0},{0},{0},{0},{0},{0}, /* 34-42 */
{0,24,24,126,24,24,0,0},      /* 43 '+'  */
{0},                           /* 44 ','  */
{0,0,0,126,0,0,0,0},          /* 45 '-'  */
{0,0,0,0,0,24,24,0},          /* 46 '.'  */
{0,6,12,24,48,96,0,0},        /* 47 '/'  */
{60,102,102,102,102,102,60,0},/* 48 '0'  */
{24,56,24,24,24,24,126,0},    /* 49 '1'  */
{60,102,6,12,24,48,126,0},    /* 50 '2'  */
{60,102,6,28,6,102,60,0},     /* 51 '3'  */
{6,14,30,102,127,6,6,0},      /* 52 '4'  */
{126,96,124,6,6,102,60,0},    /* 53 '5'  */
{60,102,96,124,102,102,60,0}, /* 54 '6'  */
{126,102,12,24,24,24,24,0},   /* 55 '7'  */
{60,102,102,60,102,102,60,0}, /* 56 '8'  */
{60,102,102,62,6,102,60,0},   /* 57 '9'  */
{0,24,24,0,24,24,0,0},        /* 58 ':'  */
{0},{0},{0},{0},{0},{0},       /* 59-64   */
{24,60,102,126,102,102,102,0},/* 65 'A'  */
{124,102,102,124,102,102,124,0},/* 66 'B' */
{60,102,96,96,96,102,60,0},   /* 67 'C'  */
{120,108,102,102,102,108,120,0},/* 68 'D' */
{126,96,96,124,96,96,126,0},  /* 69 'E'  */
{126,96,96,124,96,96,96,0},   /* 70 'F'  */
{60,102,96,110,102,102,60,0}, /* 71 'G'  */
{102,102,102,126,102,102,102,0},/* 72 'H' */
{60,24,24,24,24,24,60,0},     /* 73 'I'  */
{30,6,6,6,102,102,60,0},      /* 74 'J'  */
{102,108,120,112,120,108,102,0},/* 75 'K' */
{96,96,96,96,96,96,126,0},    /* 76 'L'  */
{99,119,127,107,99,99,99,0},  /* 77 'M'  */
{102,118,126,110,102,102,102,0},/* 78 'N' */
{60,102,102,102,102,102,60,0},/* 79 'O'  */
{124,102,102,124,96,96,96,0}, /* 80 'P'  */
{60,102,102,102,106,100,58,0},/* 81 'Q'  */
{124,102,102,124,108,102,102,0},/* 82 'R' */
{60,102,96,60,6,102,60,0},    /* 83 'S'  */
{126,24,24,24,24,24,24,0},    /* 84 'T'  */
{102,102,102,102,102,102,60,0},/* 85 'U' */
{102,102,102,102,60,60,24,0}, /* 86 'V'  */
{99,99,99,107,127,119,99,0},  /* 87 'W'  */
{102,102,60,24,60,102,102,0}, /* 88 'X'  */
{102,102,102,60,24,24,24,0},  /* 89 'Y'  */
{126,6,12,24,48,96,126,0},    /* 90 'Z'  */
};

static void Glyph(char c,int x,int y,int sc,u8 r,u8 g,u8 b){
    int idx=(int)(unsigned char)c-32;
    if(idx<0||idx>58) return;
    const u8*p=F[idx];
    for(int row=0;row<8;row++)
        for(int col=0;col<8;col++)
            if(p[row]&(0x80>>col)) R(x+col*sc,y+row*sc,sc,sc,r,g,b);
}

static int TW(const char*s,int sc){int n=(int)strlen(s);return n?(n*(9*sc)-sc):0;}
static void T(const char*s,int x,int y,int sc,u8 r,u8 g,u8 b){
    for(;*s;s++,x+=9*sc) Glyph(*s,x,y,sc,r,g,b);
}
static void TC(const char*s,int cx,int y,int sc,u8 r,u8 g,u8 b){
    T(s,cx-TW(s,sc)/2,y,sc,r,g,b);
}

static void BTN(int x,int y,int w,int h,const char*lbl,int sc,
                int hov,u8 br,u8 bg,u8 bb){
    u8 r=C8(br+(hov?50:0)),g=C8(bg+(hov?50:0)),b=C8(bb+(hov?50:0));
    R(x,y,w,h,r,g,b);
    R(x,y,w,2,C8(r+55),C8(g+55),C8(b+55));
    R(x,y,2,h,C8(r+40),C8(g+40),C8(b+40));
    R(x,y+h-2,w,2,C8(r-40),C8(g-40),C8(b-40));
    R(x+w-2,y,2,h,C8(r-40),C8(g-40),C8(b-40));
    T(lbl,x+(w-TW(lbl,sc))/2,y+(h-8*sc)/2,sc,235,240,235);
}

static void CURSOR(int cx,int cy){
    for(int i=-8;i<=8;i++){
        R(cx+i,cy,1,1,255,255,0);
        R(cx,cy+i,1,1,255,255,0);
    }
    R(cx-2,cy-2,5,5,255,110,0);
}

/* ══════════════════════════════════════════════════════════════════
   CONSTANTS
   ══════════════════════════════════════════════════════════════════ */
/* Top bar */
#define TB_H      40

/* Canvas (internal resolution, rendered 2× on screen) */
#define CANVAS_W  320
#define CANVAS_H  180
#define SCALE     2
#define CV_X      0
#define CV_Y      TB_H
#define CV_SW     (CANVAS_W*SCALE)   /* 640 */
#define CV_SH     (CANVAS_H*SCALE)   /* 360 */

/* Frame viewer strip */
#define VW_Y      (CV_Y+CV_SH)       /* 400 */
#define VW_H      (SH-VW_Y)          /* 80  */

/* Thumbnail dimensions (on-screen) */
#define TH_W      62
#define TH_H      36
#define TH_GAP    3
#define TH_STRIDE (TH_W+TH_GAP)
#define TH_X0     22
#define TH_Y0     (VW_Y+6)
#define MAX_VISIBLE_TH 7             /* thumbs visible at once */

/* ── Colour palette ── */
/* index 0=white 1=black 2=red 3=blue */
static const u8 PAL[4][3]={
    {255,255,255}, /* white (background) */
    {10,10,10},    /* black              */
    {210,45,45},   /* red                */
    {45,70,210},   /* blue               */
};

/* ── Frame storage ── */
#define MAX_FRAMES 60

/* 60 * 320 * 180 = 3,456,000 bytes ≈ 3.3 MB — fine on Wii's 88 MB */
static u8 frames[MAX_FRAMES][CANVAS_H][CANVAS_W];
static int frame_count=1;
static int cur_frame=0;
static int thumb_scroll=0;   /* index of leftmost visible thumbnail */

/* ── Drawing state ── */
/* pen colours: 1=black 2=red 3=blue; 0=erase (white) */
static int pen_color=1;
static int prev_cx=-1, prev_cy=-1; /* previous canvas coords for line */

/* ── Playback state ── */
static int playing=0;
static int play_frame=0;
static int play_tick=0;
#define PLAY_TICKS 3   /* 60fps / 20fps = 3 ticks per anim frame */

/* ── Save state ── */
typedef enum { SAVE_IDLE=0, SAVE_OK, SAVE_FAIL } SaveState;
static SaveState save_state=SAVE_IDLE;
static int save_timer=0;

/* ── Brush mask (5×5 disc = "circular pen of squares") ── */
#define BR 2   /* radius in canvas pixels */
static const int BMASK[5][5]={
    {0,1,1,1,0},
    {1,1,1,1,1},
    {1,1,1,1,1},
    {1,1,1,1,1},
    {0,1,1,1,0},
};

/* ══════════════════════════════════════════════════════════════════
   DRAWING HELPERS
   ══════════════════════════════════════════════════════════════════ */

static void stamp(int cx,int cy,u8 color){
    for(int dy=-BR;dy<=BR;dy++)
        for(int dx=-BR;dx<=BR;dx++)
            if(BMASK[dy+BR][dx+BR]){
                int px=cx+dx,py=cy+dy;
                if(px>=0&&px<CANVAS_W&&py>=0&&py<CANVAS_H)
                    frames[cur_frame][py][px]=color;
            }
}

static void paint_line(int x0,int y0,int x1,int y1,u8 color){
    int dx=abs(x1-x0),dy=abs(y1-y0);
    int sx=x0<x1?1:-1,sy=y0<y1?1:-1,err=dx-dy;
    for(;;){
        stamp(x0,y0,color);
        if(x0==x1&&y0==y1) break;
        int e2=2*err;
        if(e2>-dy){err-=dy;x0+=sx;}
        if(e2<dx ){err+=dx;y0+=sy;}
    }
}

/* screen → canvas coords */
static int to_cvx(int sx){ return sx/SCALE; }
static int to_cvy(int sy){ return (sy-CV_Y)/SCALE; }
static int on_canvas(int sx,int sy){
    return sx>=CV_X&&sx<CV_X+CV_SW&&sy>=CV_Y&&sy<CV_Y+CV_SH;
}

/* ══════════════════════════════════════════════════════════════════
   FRAME OPERATIONS
   ══════════════════════════════════════════════════════════════════ */

static void add_frame(void){
    if(frame_count>=MAX_FRAMES) return;
    /* copy current frame to new */
    memcpy(frames[frame_count],frames[cur_frame],CANVAS_W*CANVAS_H);
    frame_count++;
    cur_frame=frame_count-1;
    if(cur_frame-thumb_scroll>=MAX_VISIBLE_TH)
        thumb_scroll=cur_frame-MAX_VISIBLE_TH+1;
}

/* ══════════════════════════════════════════════════════════════════
   AVI WRITER
   ══════════════════════════════════════════════════════════════════ */

static void avi_u32(FILE*f,u32 v){
    u8 b[4]={v&0xFF,(v>>8)&0xFF,(v>>16)&0xFF,(v>>24)&0xFF};
    fwrite(b,1,4,f);
}
static void avi_tag(FILE*f,const char*t){ fwrite(t,1,4,f); }

/* Each uncompressed BGR frame is CANVAS_W*CANVAS_H*3 bytes (rows bottom-up) */
#define AVI_FRAME_BYTES (CANVAS_W*CANVAS_H*3)

static SaveState write_avi(const char*path){
    FILE*fp=fopen(path,"wb");
    if(!fp) return SAVE_FAIL;

    u32 fc=(u32)frame_count;
    u32 fs=AVI_FRAME_BYTES;
    u32 fps=20;

    /* Pre-compute block sizes */
    u32 strh_sz=56, strf_sz=40;
    u32 strl_sz=4+8+strh_sz+8+strf_sz;
    u32 avih_sz=56;
    u32 hdrl_sz=4+8+avih_sz+8+strl_sz;
    u32 movi_data=fc*(fs+8);
    u32 movi_sz=4+movi_data;
    u32 idx1_sz=fc*16;
    u32 riff_sz=4+8+hdrl_sz+8+movi_sz+8+idx1_sz;

    /* RIFF header */
    avi_tag(fp,"RIFF"); avi_u32(fp,riff_sz); avi_tag(fp,"AVI ");

    /* LIST hdrl */
    avi_tag(fp,"LIST"); avi_u32(fp,hdrl_sz); avi_tag(fp,"hdrl");

    /* avih */
    avi_tag(fp,"avih"); avi_u32(fp,avih_sz);
    avi_u32(fp,1000000/fps);   /* us per frame */
    avi_u32(fp,fs*fps);        /* max bytes/sec */
    avi_u32(fp,0);             /* padding */
    avi_u32(fp,0x10);          /* flags: HASINDEX */
    avi_u32(fp,fc);            /* total frames */
    avi_u32(fp,0);             /* initial frames */
    avi_u32(fp,1);             /* streams */
    avi_u32(fp,fs);            /* buffer size */
    avi_u32(fp,CANVAS_W);      /* width */
    avi_u32(fp,CANVAS_H);      /* height */
    avi_u32(fp,0); avi_u32(fp,0); avi_u32(fp,0); avi_u32(fp,0);

    /* LIST strl */
    avi_tag(fp,"LIST"); avi_u32(fp,strl_sz); avi_tag(fp,"strl");

    /* strh */
    avi_tag(fp,"strh"); avi_u32(fp,strh_sz);
    avi_tag(fp,"vids"); avi_tag(fp,"DIB ");
    avi_u32(fp,0);  /* flags */
    avi_u32(fp,0);  /* priority */
    avi_u32(fp,0);  /* language */
    avi_u32(fp,0);  /* initial frames */
    avi_u32(fp,1);  /* scale */
    avi_u32(fp,fps);/* rate */
    avi_u32(fp,0);  /* start */
    avi_u32(fp,fc); /* length */
    avi_u32(fp,fs); /* buffer size */
    avi_u32(fp,0);  /* quality */
    avi_u32(fp,0);  /* sample size */
    /* RECT: left,top,right,bottom as u16 pairs */
    avi_u32(fp,0); avi_u32(fp,(CANVAS_H<<16)|CANVAS_W);

    /* strf (BITMAPINFOHEADER) */
    avi_tag(fp,"strf"); avi_u32(fp,strf_sz);
    avi_u32(fp,40);          /* header size */
    avi_u32(fp,CANVAS_W);    /* width */
    avi_u32(fp,CANVAS_H);    /* height (positive = bottom-up) */
    /* biPlanes(1) + biBitCount(24) as u32 little-endian */
    avi_u32(fp,(24<<16)|1);
    avi_u32(fp,0);           /* compression: BI_RGB */
    avi_u32(fp,fs);          /* image size */
    avi_u32(fp,0);           /* x pels/meter */
    avi_u32(fp,0);           /* y pels/meter */
    avi_u32(fp,0);           /* colors used */
    avi_u32(fp,0);           /* colors important */

    /* LIST movi */
    avi_tag(fp,"LIST"); avi_u32(fp,movi_sz); avi_tag(fp,"movi");

    /* Write frame data */
    static u8 rowbuf[CANVAS_W*3];
    for(int f=0;f<frame_count;f++){
        avi_tag(fp,"00dc"); avi_u32(fp,fs);
        /* BMP rows are bottom-up */
        for(int row=CANVAS_H-1;row>=0;row--){
            const u8*src=frames[f][row];
            for(int col=0;col<CANVAS_W;col++){
                u8 ci=src[col];
                rowbuf[col*3+0]=PAL[ci][2]; /* B */
                rowbuf[col*3+1]=PAL[ci][1]; /* G */
                rowbuf[col*3+2]=PAL[ci][0]; /* R */
            }
            fwrite(rowbuf,1,CANVAS_W*3,fp);
        }
    }

    /* idx1 */
    avi_tag(fp,"idx1"); avi_u32(fp,idx1_sz);
    u32 offset=4; /* offset from 'movi' data start */
    for(int f=0;f<frame_count;f++){
        avi_tag(fp,"00dc");
        avi_u32(fp,0x10);    /* AVIIF_KEYFRAME */
        avi_u32(fp,offset);
        avi_u32(fp,fs);
        offset+=fs+8;
    }

    fclose(fp);
    return SAVE_OK;
}

static void do_save(void){
    /* ensure directory exists */
    mkdir("sd:/apps",0755);
    mkdir("sd:/apps/flipnote_studio_wii",0755);
    mkdir("sd:/apps/flipnote_studio_wii/videos",0755);

    save_state=write_avi("sd:/apps/flipnote_studio_wii/videos/flipnote.avi");
    save_timer=180; /* show status for 3 seconds */
}

/* ══════════════════════════════════════════════════════════════════
   RENDERING
   ══════════════════════════════════════════════════════════════════ */

/* Draw the canvas (current or playing frame) */
static void draw_canvas_area(void){
    int fi=playing?play_frame:cur_frame;
    /* white bg first (fast fill) */
    R(CV_X,CV_Y,CV_SW,CV_SH,255,255,255);
    /* draw non-white pixels */
    for(int cy2=0;cy2<CANVAS_H;cy2++){
        const u8*row=frames[fi][cy2];
        for(int cx2=0;cx2<CANVAS_W;cx2++){
            u8 ci=row[cx2];
            if(ci==0) continue; /* white, already filled */
            R(CV_X+cx2*SCALE,CV_Y+cy2*SCALE,SCALE,SCALE,
              PAL[ci][0],PAL[ci][1],PAL[ci][2]);
        }
    }
}

/* Draw the ghost of the previous frame (onion skin) as light grey */
static void draw_onion(void){
    if(cur_frame==0) return;
    int pf=cur_frame-1;
    for(int cy2=0;cy2<CANVAS_H;cy2++){
        const u8*row=frames[pf][cy2];
        for(int cx2=0;cx2<CANVAS_W;cx2++){
            if(row[cx2]!=0){
                R(CV_X+cx2*SCALE,CV_Y+cy2*SCALE,SCALE,SCALE,180,180,200);
            }
        }
    }
}

/* Thumbnail of frame fi into screen rect (x,y,w,h) */
static void draw_thumb(int fi,int x,int y,int w,int h){
    R(x,y,w,h,200,200,200); /* bg */
    float sx=(float)CANVAS_W/w, sy=(float)CANVAS_H/h;
    for(int ty=0;ty<h;ty++){
        int cy2=(int)(ty*sy);
        if(cy2>=CANVAS_H) cy2=CANVAS_H-1;
        for(int tx=0;tx<w;tx++){
            int cx2=(int)(tx*sx);
            if(cx2>=CANVAS_W) cx2=CANVAS_W-1;
            u8 ci=frames[fi][cy2][cx2];
            R(x+tx,y+ty,1,1,PAL[ci][0],PAL[ci][1],PAL[ci][2]);
        }
    }
}

/* Draw the brush cursor shape on screen at cursor position */
static void draw_brush_cursor(int scx,int scy){
    if(!on_canvas(scx,scy)) return;
    int cvx=to_cvx(scx),cvy=to_cvy(scy);
    for(int dy=-BR;dy<=BR;dy++){
        for(int dx=-BR;dx<=BR;dx++){
            if(!BMASK[dy+BR][dx+BR]) continue;
            int px=(cvx+dx)*SCALE+CV_X;
            int py=(cvy+dy)*SCALE+CV_Y;
            /* draw outline of each square */
            R(px,py,SCALE,1,80,80,255);
            R(px,py+SCALE-1,SCALE,1,80,80,255);
            R(px,py,1,SCALE,80,80,255);
            R(px+SCALE-1,py,1,SCALE,80,80,255);
        }
    }
}

/* Top bar */
static void draw_topbar(int scx,int scy){
    R(0,0,SW,TB_H,28,30,42);
    R(0,TB_H-1,SW,1,50,55,72);

    /* Title */
    T("FLIPNOTE STUDIO WII",5,14,1,90,160,255);

    /* Colour swatches */
    static const char*CLR_LBL[3]={"BLACK","RED","BLUE"};
    int sx=240;
    for(int i=0;i<3;i++){
        u8 r2=PAL[i+1][0],g2=PAL[i+1][1],b2=PAL[i+1][2];
        int hov=(scx>=sx&&scx<sx+56&&scy>=4&&scy<36)?1:0;
        R(sx,4,56,32,r2/3+hov*15,g2/3+hov*15,b2/3+hov*15);
        R(sx,4,56,2,C8(r2/2+60),C8(g2/2+40),C8(b2/2+40));
        /* selected indicator */
        if(pen_color==i+1){
            R(sx-2,2,60,2,255,220,50);
            R(sx-2,36,60,2,255,220,50);
            R(sx-2,2,2,36,255,220,50);
            R(sx+56,2,2,36,255,220,50);
        }
        T(CLR_LBL[i],sx+(56-TW(CLR_LBL[i],1))/2,15,1,220,225,220);
        sx+=62;
    }

    /* SAVE button */
    int sh2=(scx>=502&&scx<556&&scy>=5&&scy<35)?1:0;
    BTN(502,5,54,30,"SAVE",1,sh2,40,90,40);

    /* Save status */
    if(save_timer>0 && save_state!=SAVE_IDLE){
        const char*msg=(save_state==SAVE_OK)?"SAVED!":"FAIL";
        u8 mr=(save_state==SAVE_OK)?80:200;
        u8 mg=(save_state==SAVE_OK)?220:60;
        T(msg,562,14,1,mr,mg,80);
    }

    /* Frame counter */
    char fc_buf[16]; snprintf(fc_buf,sizeof(fc_buf),"%d/%d",cur_frame+1,frame_count);
    T(fc_buf,590,14,1,160,170,200);
}

/* Frame viewer strip */
static void draw_viewer(int scx,int scy){
    R(0,VW_Y,SW,VW_H,20,22,34);
    R(0,VW_Y,SW,2,50,55,72);

    /* [<] scroll */
    int lh=(scx>=2&&scx<20&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4)?1:0;
    BTN(2,VW_Y+4,18,VW_H-8,"<",1,lh,45,50,70);

    /* Thumbnails */
    for(int i=0;i<MAX_VISIBLE_TH;i++){
        int fi=thumb_scroll+i;
        if(fi>=frame_count) break;
        int tx=TH_X0+i*TH_STRIDE;
        int ty=TH_Y0;
        draw_thumb(fi,tx,ty,TH_W,TH_H);
        /* selection highlight */
        if(fi==cur_frame){
            R(tx-2,ty-2,TH_W+4,2,255,220,50);
            R(tx-2,ty+TH_H,TH_W+4,2,255,220,50);
            R(tx-2,ty-2,2,TH_H+4,255,220,50);
            R(tx+TH_W,ty-2,2,TH_H+4,255,220,50);
        }
        /* frame number */
        char nb[4]; snprintf(nb,sizeof(nb),"%d",fi+1);
        TC(nb,tx+TH_W/2,ty+TH_H+3,1,130,140,170);
        /* play position indicator while playing */
        if(playing&&fi==play_frame) R(tx,ty+TH_H-3,TH_W,3,80,220,80);
    }

    /* [>] scroll */
    int rh=(scx>=SW-20&&scx<SW-2&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4)?1:0;
    BTN(SW-20,VW_Y+4,18,VW_H-8,">",1,rh,45,50,70);

    /* [+] add frame */
    int ah=(scx>=SW-84&&scx<SW-44&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4)?1:0;
    BTN(SW-84,VW_Y+4,38,VW_H-8,"+",2,ah&&frame_count<MAX_FRAMES,30,80,30);

    /* [PLAY/STOP] */
    int ph=(scx>=SW-42&&scx<SW-2&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4)?1:0;
    if(playing) BTN(SW-42,VW_Y+4,40,VW_H-8,"STOP",1,ph,140,40,40);
    else        BTN(SW-42,VW_Y+4,40,VW_H-8,"PLAY",1,ph,40,120,40);
}

/* ══════════════════════════════════════════════════════════════════
   FLIP — present current framebuffer
   ══════════════════════════════════════════════════════════════════ */
static void flip(void){
    VIDEO_SetNextFramebuffer(xfb[fbi]);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    fbi^=1;
}

/* ══════════════════════════════════════════════════════════════════
   MAIN
   ══════════════════════════════════════════════════════════════════ */
int main(int argc,char**argv){
    (void)argc;(void)argv;

    VIDEO_Init();
    WPAD_Init();
    fatInitDefault();

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

    /* Frame 1 starts white (already zero from BSS, 0=white) */

    int aw=0; /* A held previous frame */
    int bw=0; /* B held previous frame */

    while(1){
        WPAD_ScanPads();
        u32 held=WPAD_ButtonsHeld(0);
        u32 down=WPAD_ButtonsDown(0);

        if(down&WPAD_BUTTON_HOME) exit(0);

        WPADData*wd=WPAD_Data(WPAD_CHAN_0);
        int scx=320,scy=240;
        if(wd&&wd->ir.valid){ scx=(int)wd->ir.x; scy=(int)wd->ir.y; }

        int an=(held&WPAD_BUTTON_A)!=0;
        int bn=(held&WPAD_BUTTON_B)!=0;
        int ad=an&&!aw;
        aw=an;
        bw=bn;
        (void)bw;

        /* ── save timer ── */
        if(save_timer>0) save_timer--;

        /* ── playback tick ── */
        if(playing){
            play_tick++;
            if(play_tick>=PLAY_TICKS){
                play_tick=0;
                play_frame++;
                if(play_frame>=frame_count){
                    play_frame=0;
                    playing=0; /* stop at end */
                }
            }
        }

        /* ── input ── */

        /* colour selector */
        if(!playing){
            int csx=240;
            for(int i=0;i<3;i++){
                if(ad&&scx>=csx&&scx<csx+56&&scy>=4&&scy<36) pen_color=i+1;
                csx+=62;
            }

            /* SAVE */
            if(ad&&scx>=502&&scx<556&&scy>=5&&scy<35){
                /* render saving message first */
                R(0,0,SW,SH,20,22,34);
                TC("SAVING...",SW/2,SH/2-8,2,80,200,80);
                flip();
                do_save();
            }
        }

        /* frame viewer */
        /* [<] scroll left */
        if(ad&&scx>=2&&scx<20&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4){
            if(thumb_scroll>0) thumb_scroll--;
        }
        /* [>] scroll right */
        if(ad&&scx>=SW-20&&scx<SW-2&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4){
            if(thumb_scroll+MAX_VISIBLE_TH<frame_count) thumb_scroll++;
        }
        /* thumbnail click → select frame */
        if(!playing&&ad){
            for(int i=0;i<MAX_VISIBLE_TH;i++){
                int fi=thumb_scroll+i;
                if(fi>=frame_count) break;
                int tx=TH_X0+i*TH_STRIDE;
                if(scx>=tx&&scx<tx+TH_W&&scy>=TH_Y0&&scy<TH_Y0+TH_H){
                    cur_frame=fi;
                    prev_cx=-1; prev_cy=-1;
                }
            }
        }
        /* [+] add frame */
        if(ad&&scx>=SW-84&&scx<SW-44&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4){
            if(!playing&&frame_count<MAX_FRAMES){
                add_frame();
                prev_cx=-1; prev_cy=-1;
            }
        }
        /* [PLAY/STOP] */
        if(ad&&scx>=SW-42&&scx<SW-2&&scy>=VW_Y+4&&scy<VW_Y+VW_H-4){
            if(playing){
                playing=0;
            } else {
                playing=1;
                play_frame=cur_frame;
                play_tick=0;
            }
        }

        /* ── drawing ── */
        if(!playing&&on_canvas(scx,scy)){
            int cvx=to_cvx(scx);
            int cvy=to_cvy(scy);
            u8 color=(u8)(bn?0:pen_color); /* B held = erase */
            if(an){
                if(prev_cx>=0)
                    paint_line(prev_cx,prev_cy,cvx,cvy,color);
                else
                    stamp(cvx,cvy,color);
                prev_cx=cvx; prev_cy=cvy;
            } else {
                prev_cx=-1; prev_cy=-1;
            }
        } else if(!an){
            prev_cx=-1; prev_cy=-1;
        }

        /* ── render ── */
        R(0,0,SW,SH,15,17,25);

        draw_canvas_area();
        if(!playing) draw_onion();
        if(!playing) draw_brush_cursor(scx,scy);
        draw_topbar(scx,scy);
        draw_viewer(scx,scy);
        if(!playing) CURSOR(scx,scy);

        flip();
    }
    return 0;
}
