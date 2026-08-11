/**
 * WiiBox — Physics Sandbox for the Nintendo Wii
 * Point the Wiimote, press A to spawn boxes or drag them around.
 * Boxes fall with gravity and collide with each other and the walls.
 * Press HOME to exit.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================
// Screen — double-buffered
// ============================================================
static void       *xfb[2] = {NULL, NULL};
static int         fb_idx  = 0;
static GXRModeObj *rmode   = NULL;
#define FB ((u32*)xfb[fb_idx])

// ============================================================
// Colour: RGB -> YCbCr  (BT.601 studio-swing)
// XFB u32 big-endian: [Y0][Cb][Y1][Cr]
// ============================================================
static inline u8 clamp8(int v){ return v<0?0:v>255?255:(u8)v; }
static inline u8 rgb_y (u8 r,u8 g,u8 b){ return clamp8(((77*r+150*g+29*b)>>8)+16); }
static inline u8 rgb_cb(u8 r,u8 g,u8 b){ return clamp8(((-43*r-85*g+128*b)>>8)+128); }
static inline u8 rgb_cr(u8 r,u8 g,u8 b){ return clamp8(((128*r-107*g-21*b)>>8)+128); }

// ============================================================
// Drawing
// ============================================================
static void draw_rect(int x,int y,int w,int h,u8 r,u8 g,u8 b){
    if(w<=0||h<=0) return;
    int stride=rmode->fbWidth>>1;
    u8  yv=rgb_y(r,g,b),cb=rgb_cb(r,g,b),cr=rgb_cr(r,g,b);
    u32 pv=((u32)yv<<24)|((u32)cb<<16)|((u32)yv<<8)|cr;
    int x0=x>>1, x1=(x+w+1)>>1;
    for(int row=y;row<y+h;row++){
        if(row<0||row>=(int)rmode->xfbHeight) continue;
        for(int col=x0;col<x1;col++){
            if(col<0||col>=stride) continue;
            FB[row*stride+col]=pv;
        }
    }
}

static void draw_pixel(int x,int y,u8 r,u8 g,u8 b){
    if(x<0||y<0||x>=(int)rmode->fbWidth||y>=(int)rmode->xfbHeight) return;
    int stride=rmode->fbWidth>>1;
    u32 *p=&FB[y*stride+(x>>1)];
    u8 yv=rgb_y(r,g,b),cb=rgb_cb(r,g,b),cr=rgb_cr(r,g,b);
    if(x&1) *p=(*p&0xFF000000u)|((u32)cb<<16)|((u32)yv<<8)|cr;
    else     *p=((u32)yv<<24)|((u32)cb<<16)|(*p&0x0000FF00u)|cr;
}

// ============================================================
// 8×8 bitmap font  (uppercase letters + digits + symbols)
// ============================================================
static const u8 FONT[][8]={
/* 0  ' ' */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/* 1  '0' */ {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
/* 2  '1' */ {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
/* 3  '2' */ {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},
/* 4  '3' */ {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
/* 5  '4' */ {0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0x00},
/* 6  '5' */ {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
/* 7  '6' */ {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00},
/* 8  '7' */ {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00},
/* 9  '8' */ {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
/*10  '9' */ {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00},
/* Uppercase letters */
/*11  'A' */ {0x18,0x3C,0x66,0x7E,0x66,0x66,0x66,0x00},
/*12  'C' */ {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
/*13  'D' */ {0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00},
/*14  'E' */ {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
/*15  'G' */ {0x3C,0x66,0x60,0x6E,0x66,0x66,0x3C,0x00},
/*16  'L' */ {0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00},
/*17  'N' */ {0x66,0x76,0x7E,0x6E,0x66,0x66,0x66,0x00},
/*18  'P' */ {0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00},
/*19  'R' */ {0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00},
/*20  'S' */ {0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00},
/*21  'W' */ {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00},
/*22  'X' */ {0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00},
/*23  'B' */ {0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00},
/*24  'O' */ {0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00},
/*25  'I' */ {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00},
/*26  'H' */ {0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00},
/*27  'Y' */ {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
/*28  'x' subscript for "boxes" count -- reuse X */ {0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00},
/*29  ':' */ {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00},
};

static int glyph_of(char c){
    if(c==' ') return 0;
    if(c>='0'&&c<='9') return c-'0'+1;
    switch(c){
        case 'A': return 11; case 'C': return 12; case 'D': return 13;
        case 'E': return 14; case 'G': return 15; case 'L': return 16;
        case 'N': return 17; case 'P': return 18; case 'R': return 19;
        case 'S': return 20; case 'W': return 21; case 'X': return 22;
        case 'B': return 23; case 'O': return 24; case 'I': return 25;
        case 'H': return 26; case 'Y': return 27; case ':': return 29;
    }
    return 0;
}

static void draw_char(char c,int x,int y,int sc,u8 r,u8 g,u8 b){
    const u8 *g8=FONT[glyph_of(c)];
    for(int row=0;row<8;row++)
        for(int col=0;col<8;col++)
            if(g8[row]&(0x80>>col))
                draw_rect(x+col*sc,y+row*sc,sc,sc,r,g,b);
}

static int str_w(const char *s,int sc){
    int n=(int)strlen(s);
    return n?(n*(8*sc+sc)-sc):0;
}

static void draw_str(const char *s,int x,int y,int sc,u8 r,u8 g,u8 b){
    for(;*s;s++,x+=(8+1)*sc)
        draw_char(*s,x,y,sc,r,g,b);
}

// ============================================================
// Layout constants
// ============================================================
#define SCREEN_W   640
#define SCREEN_H   480
#define UI_H        58    // height of top UI bar
#define FLOOR_TOP  452    // y of floor surface
#define WALL_L       5
#define WALL_R     635
#define BOX_W       32
#define BOX_H       32
#define MAX_BOXES   30
#define GRAVITY     0.35f
#define RESTITUTION 0.42f
#define FRICTION    0.96f
#define MIN_BOUNCE  0.8f  // min |vy| to bounce vs. rest

// ============================================================
// Box colours (cycling palette)
// ============================================================
static const u8 PALETTE[][3]={
    {220, 60, 60},{60,200, 60},{60, 80,220},{220,190, 40},
    {210, 60,200},{40,200,200},{210,130, 40},{130, 60,210},
    {60,160,220},{220,100, 60},{100,220, 80},{180, 60,100},
};
#define NCOLORS 12

// ============================================================
// Physics box
// ============================================================
typedef struct {
    float x,y,vx,vy;
    int   active;
    u8    r,g,b;
} Box;

static Box  boxes[MAX_BOXES];
static int  num_boxes=0;
static int  color_idx=0;

// Which box is being dragged (-1=none)
static int  drag_idx=-1;
static int  prev_cx=0, prev_cy=0;

// ============================================================
// Spawn a box at pixel (cx,cy)  centred on cursor
// ============================================================
static void spawn_box(int cx,int cy){
    if(num_boxes>=MAX_BOXES) return;
    // Find a slot (first inactive, or append)
    int slot=-1;
    for(int i=0;i<MAX_BOXES;i++) if(!boxes[i].active){slot=i;break;}
    if(slot<0){if(num_boxes<MAX_BOXES) slot=num_boxes++; else return;}
    else if(slot>=num_boxes) num_boxes=slot+1;

    Box *b=&boxes[slot];
    b->x=(float)(cx-BOX_W/2); b->y=(float)(cy-BOX_H/2);
    b->vx=0; b->vy=0;
    b->active=1;
    b->r=PALETTE[color_idx][0];
    b->g=PALETTE[color_idx][1];
    b->b=PALETTE[color_idx][2];
    color_idx=(color_idx+1)%NCOLORS;
}

// ============================================================
// AABB box-box collision resolution
// ============================================================
static void resolve_pair(int i,int j){
    Box *a=&boxes[i], *b=&boxes[j];
    float ax1=a->x,ay1=a->y,ax2=a->x+BOX_W,ay2=a->y+BOX_H;
    float bx1=b->x,by1=b->y,bx2=b->x+BOX_W,by2=b->y+BOX_H;
    if(ax2<=bx1||bx2<=ax1||ay2<=by1||by2<=ay1) return;

    float ovx=(ax2<bx2)?(ax2-bx1):(-(bx2-ax1));
    float ovy=(ay2<by2)?(ay2-by1):(-(by2-ay1));

    if(fabsf(ovx)<fabsf(ovy)){
        float push=ovx*0.5f;
        if(drag_idx!=i) a->x-=push;
        if(drag_idx!=j) b->x+=push;
        float avx=a->vx, bvx=b->vx;
        if(drag_idx!=i) a->vx=bvx*RESTITUTION;
        if(drag_idx!=j) b->vx=avx*RESTITUTION;
    } else {
        float push=ovy*0.5f;
        if(drag_idx!=i) a->y-=push;
        if(drag_idx!=j) b->y+=push;
        float avy=a->vy, bvy=b->vy;
        if(drag_idx!=i) a->vy=bvy*RESTITUTION;
        if(drag_idx!=j) b->vy=avy*RESTITUTION;
    }
}

// ============================================================
// Physics step (one frame)
// ============================================================
static void physics_step(void){
    for(int i=0;i<num_boxes;i++){
        if(!boxes[i].active) continue;
        if(i==drag_idx) continue;  // dragged box skips physics
        Box *b=&boxes[i];

        b->vy+=GRAVITY;
        b->x+=b->vx;
        b->y+=b->vy;

        // Floor
        if(b->y+BOX_H>FLOOR_TOP){
            b->y=(float)(FLOOR_TOP-BOX_H);
            if(fabsf(b->vy)>MIN_BOUNCE) b->vy=-b->vy*RESTITUTION;
            else                          b->vy=0;
            b->vx*=FRICTION;
        }
        // Ceiling (below UI bar)
        if(b->y<UI_H){ b->y=(float)UI_H; b->vy=fabsf(b->vy)*RESTITUTION; }
        // Left wall
        if(b->x<WALL_L){ b->x=(float)WALL_L; b->vx=fabsf(b->vx)*RESTITUTION; }
        // Right wall
        if(b->x+BOX_W>WALL_R){ b->x=(float)(WALL_R-BOX_W); b->vx=-fabsf(b->vx)*RESTITUTION; }
    }

    // Box-box collisions (two passes for stability)
    for(int pass=0;pass<2;pass++)
        for(int i=0;i<num_boxes;i++)
            if(boxes[i].active)
                for(int j=i+1;j<num_boxes;j++)
                    if(boxes[j].active)
                        resolve_pair(i,j);
}

// ============================================================
// Mode
// ============================================================
typedef enum { MODE_SPAWN, MODE_DRAG } Mode;
static Mode mode=MODE_SPAWN;

// ============================================================
// UI Buttons
// ============================================================
typedef struct { int x,y,w,h; const char *label; } UIBtn;

// Three buttons in the top bar
static const UIBtn BTN_SPAWN = {  8, 9, 160, 40, "SPAWN"};
static const UIBtn BTN_DRAG  = {176, 9, 160, 40, "DRAG" };
static const UIBtn BTN_CLEAR = {456, 9, 176, 40, "CLEAR"};

static int ui_hit(const UIBtn *btn, int cx, int cy){
    return cx>=btn->x&&cx<btn->x+btn->w&&cy>=btn->y&&cy<btn->y+btn->h;
}

// Draw one UI button
static void draw_btn(const UIBtn *btn, int active, int hovered){
    u8 r,g,b;
    if(active)       { r=50; g=180; b=80; }  // green = active mode
    else if(hovered) { r=90; g= 90; b=110; }
    else             { r=55; g= 55; b= 65; }

    draw_rect(btn->x, btn->y, btn->w, btn->h, r,g,b);
    // Top-left highlight
    draw_rect(btn->x,       btn->y,        btn->w,2, clamp8(r+55),clamp8(g+55),clamp8(b+55));
    draw_rect(btn->x,       btn->y,        2,btn->h, clamp8(r+55),clamp8(g+55),clamp8(b+55));
    // Bottom-right shadow
    draw_rect(btn->x,       btn->y+btn->h-2, btn->w,2, clamp8(r-35),clamp8(g-35),clamp8(b-35));
    draw_rect(btn->x+btn->w-2,btn->y,      2,btn->h, clamp8(r-35),clamp8(g-35),clamp8(b-35));

    // Centred label
    int sc=2;
    int tw=str_w(btn->label,sc), th=8*sc;
    int tx=btn->x+(btn->w-tw)/2, ty=btn->y+(btn->h-th)/2;
    draw_str(btn->label,tx,ty,sc,220,230,220);
}

// ============================================================
// Box count display (top right)
// ============================================================
static void draw_box_count(int n){
    char buf[16]; snprintf(buf,sizeof(buf),"BOXES:%d",n);
    draw_str(buf, SCREEN_W-str_w(buf,2)-8, 9+8, 2, 150,160,160);
}

// ============================================================
// Render entire frame
// ============================================================
static void render(int cx, int cy, int cv, int hover_spawn, int hover_drag,
                   int hover_clear, int active_boxes){

    // -- Background --
    draw_rect(0,0,SCREEN_W,SCREEN_H, 18,18,22);

    // -- Physics world walls (decorative) --
    draw_rect(0,   UI_H, WALL_L,    SCREEN_H-UI_H, 45,45,55);   // left
    draw_rect(WALL_R, UI_H, 5,      SCREEN_H-UI_H, 45,45,55);   // right
    // Floor
    draw_rect(0, FLOOR_TOP, SCREEN_W, SCREEN_H-FLOOR_TOP, 50,48,45);
    // Floor highlight line
    draw_rect(0, FLOOR_TOP, SCREEN_W, 3, 90,85,80);

    // -- UI bar background --
    draw_rect(0,0,SCREEN_W,UI_H, 32,32,40);
    draw_rect(0,UI_H-3,SCREEN_W,3, 60,60,75);  // bottom border

    // -- Buttons --
    draw_btn(&BTN_SPAWN, mode==MODE_SPAWN, hover_spawn);
    draw_btn(&BTN_DRAG,  mode==MODE_DRAG,  hover_drag);
    draw_btn(&BTN_CLEAR, 0,               hover_clear);

    // -- Box count --
    draw_box_count(active_boxes);

    // -- Boxes --
    for(int i=0;i<num_boxes;i++){
        if(!boxes[i].active) continue;
        Box *bx=&boxes[i];
        int bx_=( int)bx->x, by_=(int)bx->y;
        // Fill
        draw_rect(bx_,by_,BOX_W,BOX_H,bx->r,bx->g,bx->b);
        // Lighter top face (3D feel)
        draw_rect(bx_,by_,BOX_W,4, clamp8(bx->r+60),clamp8(bx->g+60),clamp8(bx->b+60));
        draw_rect(bx_,by_,4,BOX_H, clamp8(bx->r+50),clamp8(bx->g+50),clamp8(bx->b+50));
        // Darker bottom/right shadow
        draw_rect(bx_,       by_+BOX_H-4,BOX_W,4, clamp8(bx->r-50),clamp8(bx->g-50),clamp8(bx->b-50));
        draw_rect(bx_+BOX_W-4,by_,4,BOX_H,        clamp8(bx->r-50),clamp8(bx->g-50),clamp8(bx->b-50));
        // Outline on drag
        if(i==drag_idx){
            draw_rect(bx_-2,by_-2,BOX_W+4,2,     255,255,255);
            draw_rect(bx_-2,by_+BOX_H,BOX_W+4,2, 255,255,255);
            draw_rect(bx_-2,by_-2,2,BOX_H+4,     255,255,255);
            draw_rect(bx_+BOX_W,by_-2,2,BOX_H+4, 255,255,255);
        }
    }

    // -- Cursor --
    if(cv){
        // Shadow
        for(int i=-10;i<=10;i++){
            draw_pixel(cx+i+1,cy+1,0,0,0);
            draw_pixel(cx+1,cy+i+1,0,0,0);
        }
        // Crosshair colour depends on mode
        u8 kr=255,kg=(mode==MODE_SPAWN)?255:160,kb=(mode==MODE_SPAWN)?0:255;
        for(int i=-10;i<=10;i++){
            draw_pixel(cx+i,cy,kr,kg,kb);
            draw_pixel(cx,cy+i,kr,kg,kb);
        }
        draw_rect(cx-2,cy-2,5,5,255,255,255);
    }
}

// ============================================================
// Main
// ============================================================
int main(int argc,char **argv){
    VIDEO_Init();
    WPAD_Init();

    rmode=VIDEO_GetPreferredMode(NULL);
    xfb[0]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    xfb[1]=MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));
    fb_idx=0;

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb[fb_idx]);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0,rmode->fbWidth,rmode->xfbHeight);

    memset(boxes,0,sizeof(boxes));

    int a_was_held=0;
    prev_cx=SCREEN_W/2; prev_cy=SCREEN_H/2;

    while(1){
        WPAD_ScanPads();
        u32 down=WPAD_ButtonsDown(0);
        u32 held=WPAD_ButtonsHeld(0);
        if(down&WPAD_BUTTON_HOME) exit(0);

        // IR
        WPADData *wd=WPAD_Data(WPAD_CHAN_0);
        int cx=prev_cx,cy=prev_cy,cv=0;
        if(wd&&wd->ir.valid){cx=(int)wd->ir.x;cy=(int)wd->ir.y;cv=1;}

        // Button hover
        int h_spawn = ui_hit(&BTN_SPAWN,cx,cy);
        int h_drag  = ui_hit(&BTN_DRAG, cx,cy);
        int h_clear = ui_hit(&BTN_CLEAR,cx,cy);

        // A pressed (edge)
        int a_now=(held&WPAD_BUTTON_A)!=0;
        int a_down=a_now&&!a_was_held;

        if(a_down){
            if(h_spawn){ mode=MODE_SPAWN; drag_idx=-1; }
            else if(h_drag){ mode=MODE_DRAG; }
            else if(h_clear){ memset(boxes,0,sizeof(boxes)); num_boxes=0; drag_idx=-1; }
            else if(cy>=UI_H){ // in physics area
                if(mode==MODE_SPAWN){
                    spawn_box(cx,cy);
                } else { // DRAG: pick up a box
                    drag_idx=-1;
                    for(int i=0;i<num_boxes;i++){
                        if(!boxes[i].active) continue;
                        if(cx>=boxes[i].x && cx<boxes[i].x+BOX_W &&
                           cy>=boxes[i].y && cy<boxes[i].y+BOX_H){
                            drag_idx=i;
                            boxes[i].vx=boxes[i].vy=0;
                            break;
                        }
                    }
                }
            }
        }

        // Drag: move box with cursor
        if(a_now && drag_idx>=0 && boxes[drag_idx].active){
            // Track velocity from cursor movement
            boxes[drag_idx].vx=(float)(cx-prev_cx)*0.8f;
            boxes[drag_idx].vy=(float)(cy-prev_cy)*0.8f;
            boxes[drag_idx].x=(float)(cx-BOX_W/2);
            boxes[drag_idx].y=(float)(cy-BOX_H/2);
            // Keep in bounds
            if(boxes[drag_idx].y<UI_H) boxes[drag_idx].y=(float)UI_H;
        }
        // Release drag
        if(!a_now && drag_idx>=0){
            // Box keeps last velocity for throw
            drag_idx=-1;
        }

        a_was_held=a_now;

        // Count active boxes
        int active_count=0;
        for(int i=0;i<num_boxes;i++) if(boxes[i].active) active_count++;

        // Physics
        physics_step();

        // Render to back-buffer
        render(cx,cy,cv,h_spawn,h_drag,h_clear,active_count);

        // Flip
        VIDEO_SetNextFramebuffer(xfb[fb_idx]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fb_idx^=1;

        prev_cx=cx; prev_cy=cy;
    }

    return 0;
}
