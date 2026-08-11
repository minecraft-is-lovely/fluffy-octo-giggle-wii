/**
 * Wii Calculator
 * Graphical calculator with Wiimote IR cursor and clickable buttons.
 * Uses direct XFB (framebuffer) rendering — no GX required.
 */

#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================
// Screen state
// ============================================================
static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

// ============================================================
// Colour helpers: RGB -> YCbCr (BT.601 limited range)
// XFB layout per u32 (big-endian PPC): [Y0][Cb][Y1][Cr]
// ============================================================
static inline u8 clamp8(int v) { return v<0?0:v>255?255:(u8)v; }

static inline u8 rgb_y (u8 r,u8 g,u8 b){return clamp8(((77*r+150*g+29*b)>>8)+16);}
static inline u8 rgb_cb(u8 r,u8 g,u8 b){return clamp8(((-43*r-85*g+128*b)>>8)+128);}
static inline u8 rgb_cr(u8 r,u8 g,u8 b){return clamp8(((128*r-107*g-21*b)>>8)+128);}

// Draw a filled rectangle (x must be even-aligned internally)
static void draw_rect(int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    if (w<=0||h<=0) return;
    int stride = rmode->fbWidth >> 1;
    u8  yv = rgb_y(r,g,b), cb = rgb_cb(r,g,b), cr = rgb_cr(r,g,b);
    u32 pv = ((u32)yv<<24)|((u32)cb<<16)|((u32)yv<<8)|cr;
    u32 *fb = (u32*)xfb;
    int x0 = x>>1, x1 = (x+w+1)>>1;
    for (int row=y; row<y+h; row++) {
        if (row<0||row>=(int)rmode->xfbHeight) continue;
        for (int col=x0; col<x1; col++) {
            if (col<0||col>=stride) continue;
            fb[row*stride+col] = pv;
        }
    }
}

// Draw a single pixel (for cursor)
static void draw_pixel(int x, int y, u8 r, u8 g, u8 b) {
    if (x<0||y<0||x>=(int)rmode->fbWidth||y>=(int)rmode->xfbHeight) return;
    int stride = rmode->fbWidth>>1;
    u32 *p = &((u32*)xfb)[y*stride+(x>>1)];
    u8 yv=rgb_y(r,g,b), cb=rgb_cb(r,g,b), cr=rgb_cr(r,g,b);
    if (x&1) *p = (*p&0xFF000000u)|((u32)cb<<16)|((u32)yv<<8)|cr;
    else      *p = ((u32)yv<<24)|((u32)cb<<16)|(*p&0x0000FF00u)|cr;
}

// Draw 2-pixel-wide horizontal line
static void hline(int x, int y, int w, u8 r, u8 g, u8 b) {
    draw_rect(x, y, w, 2, r, g, b);
}
// Draw 2-pixel-wide vertical line
static void vline(int x, int y, int h, u8 r, u8 g, u8 b) {
    draw_rect(x, y, 2, h, r, g, b);
}
// Rounded-looking button outline
static void draw_outline(int x, int y, int w, int h, u8 r, u8 g, u8 b) {
    hline(x+2, y,     w-4, r,g,b);
    hline(x+2, y+h-2, w-4, r,g,b);
    vline(x,   y+2,   h-4, r,g,b);
    vline(x+w-2, y+2, h-4, r,g,b);
}

// ============================================================
// Minimal 8×8 bitmap font
// Glyphs: space 0-9 + - * / = . C E % ± ( )
// ============================================================
static const u8 FONT[][8] = {
/*  0 space */ {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
/*  1   0   */ {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00},
/*  2   1   */ {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
/*  3   2   */ {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00},
/*  4   3   */ {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00},
/*  5   4   */ {0x06,0x0E,0x1E,0x66,0x7F,0x06,0x06,0x00},
/*  6   5   */ {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00},
/*  7   6   */ {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00},
/*  8   7   */ {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00},
/*  9   8   */ {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00},
/* 10   9   */ {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00},
/* 11   +   */ {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
/* 12   -   */ {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
/* 13   ×   */ {0x00,0x66,0x3C,0x18,0x3C,0x66,0x00,0x00},
/* 14   /   */ {0x00,0x06,0x0C,0x18,0x30,0x60,0x00,0x00},
/* 15   =   */ {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
/* 16   .   */ {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
/* 17   C   */ {0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00},
/* 18   E   */ {0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00},
/* 19   %   */ {0x62,0x66,0x0C,0x18,0x30,0x66,0x46,0x00},
/* 20   ±   */ {0x18,0x18,0x7E,0x18,0x18,0x00,0x7E,0x00},
/* 21   (   */ {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},
/* 22   )   */ {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
};

static int glyph_of(char c) {
    if (c==' ') return 0;
    if (c>='0'&&c<='9') return c-'0'+1;
    if (c=='+') return 11;
    if (c=='-') return 12;
    if (c=='*') return 13;
    if (c=='/') return 14;
    if (c=='=') return 15;
    if (c=='.') return 16;
    if (c=='C') return 17;
    if (c=='E') return 18;
    if (c=='%') return 19;
    if (c=='~') return 20;  // ± symbol
    if (c=='(') return 21;
    if (c==')') return 22;
    return 0;
}

static void draw_char(char c, int x, int y, int scale, u8 r, u8 g, u8 b) {
    const u8 *g8 = FONT[glyph_of(c)];
    for (int row=0; row<8; row++) {
        for (int col=0; col<8; col++) {
            if (g8[row]&(0x80>>col))
                draw_rect(x+col*scale, y+row*scale, scale, scale, r,g,b);
        }
    }
}

static void draw_str(const char *s, int x, int y, int scale, u8 r, u8 g, u8 b) {
    for (; *s; s++, x+=(8+1)*scale)
        draw_char(*s, x, y, scale, r,g,b);
}

// Text width in pixels
static int str_width(const char *s, int scale) {
    int len = (int)strlen(s);
    return len*(8+1)*scale - scale; // trailing gap removed
}

// ============================================================
// Button definitions
// ============================================================
#define NBUTTONS 20
#define SCREEN_W  640
#define SCREEN_H  480
#define DISP_X    20
#define DISP_Y    12
#define DISP_W    600
#define DISP_H    88
#define BTN_X0    20
#define BTN_Y0    112
#define BTN_GAP   8
#define BTN_W     142
#define BTN_H     68

typedef struct {
    int x, y, w, h;
    const char *label;   // displayed label
    char action;         // '0'-'9', '+','-','*','/','=','C','E','.','~','%'
    // base colour
    u8 br, bg, bb;
} Button;

static Button BUTTONS[NBUTTONS] = {
    // Row 0
    {BTN_X0+0*(BTN_W+BTN_GAP), BTN_Y0+0*(BTN_H+BTN_GAP), BTN_W,BTN_H, "C",  'C', 100,100,100},
    {BTN_X0+1*(BTN_W+BTN_GAP), BTN_Y0+0*(BTN_H+BTN_GAP), BTN_W,BTN_H, "CE", 'E', 100,100,100},
    {BTN_X0+2*(BTN_W+BTN_GAP), BTN_Y0+0*(BTN_H+BTN_GAP), BTN_W,BTN_H, "%",  '%', 100,100,100},
    {BTN_X0+3*(BTN_W+BTN_GAP), BTN_Y0+0*(BTN_H+BTN_GAP), BTN_W,BTN_H, "/",  '/', 210,100, 20},
    // Row 1
    {BTN_X0+0*(BTN_W+BTN_GAP), BTN_Y0+1*(BTN_H+BTN_GAP), BTN_W,BTN_H, "7",  '7',  60, 60, 70},
    {BTN_X0+1*(BTN_W+BTN_GAP), BTN_Y0+1*(BTN_H+BTN_GAP), BTN_W,BTN_H, "8",  '8',  60, 60, 70},
    {BTN_X0+2*(BTN_W+BTN_GAP), BTN_Y0+1*(BTN_H+BTN_GAP), BTN_W,BTN_H, "9",  '9',  60, 60, 70},
    {BTN_X0+3*(BTN_W+BTN_GAP), BTN_Y0+1*(BTN_H+BTN_GAP), BTN_W,BTN_H, "*",  '*', 210,100, 20},
    // Row 2
    {BTN_X0+0*(BTN_W+BTN_GAP), BTN_Y0+2*(BTN_H+BTN_GAP), BTN_W,BTN_H, "4",  '4',  60, 60, 70},
    {BTN_X0+1*(BTN_W+BTN_GAP), BTN_Y0+2*(BTN_H+BTN_GAP), BTN_W,BTN_H, "5",  '5',  60, 60, 70},
    {BTN_X0+2*(BTN_W+BTN_GAP), BTN_Y0+2*(BTN_H+BTN_GAP), BTN_W,BTN_H, "6",  '6',  60, 60, 70},
    {BTN_X0+3*(BTN_W+BTN_GAP), BTN_Y0+2*(BTN_H+BTN_GAP), BTN_W,BTN_H, "-",  '-', 210,100, 20},
    // Row 3
    {BTN_X0+0*(BTN_W+BTN_GAP), BTN_Y0+3*(BTN_H+BTN_GAP), BTN_W,BTN_H, "1",  '1',  60, 60, 70},
    {BTN_X0+1*(BTN_W+BTN_GAP), BTN_Y0+3*(BTN_H+BTN_GAP), BTN_W,BTN_H, "2",  '2',  60, 60, 70},
    {BTN_X0+2*(BTN_W+BTN_GAP), BTN_Y0+3*(BTN_H+BTN_GAP), BTN_W,BTN_H, "3",  '3',  60, 60, 70},
    {BTN_X0+3*(BTN_W+BTN_GAP), BTN_Y0+3*(BTN_H+BTN_GAP), BTN_W,BTN_H, "+",  '+', 210,100, 20},
    // Row 4
    {BTN_X0+0*(BTN_W+BTN_GAP), BTN_Y0+4*(BTN_H+BTN_GAP), BTN_W,BTN_H, "~",  '~', 100,100,100},
    {BTN_X0+1*(BTN_W+BTN_GAP), BTN_Y0+4*(BTN_H+BTN_GAP), BTN_W,BTN_H, "0",  '0',  60, 60, 70},
    {BTN_X0+2*(BTN_W+BTN_GAP), BTN_Y0+4*(BTN_H+BTN_GAP), BTN_W,BTN_H, ".",  '.',  60, 60, 70},
    {BTN_X0+3*(BTN_W+BTN_GAP), BTN_Y0+4*(BTN_H+BTN_GAP), BTN_W,BTN_H, "=",  '=',  20,180, 20},
};

// Hit test: which button index is at (cx,cy)? -1 if none
static int hit_button(int cx, int cy) {
    for (int i=0; i<NBUTTONS; i++) {
        Button *b = &BUTTONS[i];
        if (cx>=b->x && cx<b->x+b->w && cy>=b->y && cy<b->y+b->h)
            return i;
    }
    return -1;
}

// ============================================================
// Calculator state
// ============================================================
typedef enum { S_INPUT, S_RESULT } CalcMode;

static char   disp[64]     = "0";
static double left_val     = 0.0;
static char   pending_op   = 0;
static int    fresh        = 1;   // next digit replaces display
static int    has_decimal  = 0;
static CalcMode mode       = S_INPUT;

static void format_num(char *buf, double v) {
    // Try to display cleanly without unnecessary decimal places
    if (v == (long long)v && fabs(v) < 1e15)
        snprintf(buf, 32, "%lld", (long long)v);
    else
        snprintf(buf, 32, "%.10g", v);
}

static double parse_disp(void) {
    return atof(disp);
}

static double apply_op(double a, char op, double b) {
    switch(op) {
        case '+': return a+b;
        case '-': return a-b;
        case '*': return a*b;
        case '/': return (b!=0.0) ? a/b : 0.0; // show 0 on div-by-zero for simplicity
    }
    return b;
}

static void calc_press(char action) {
    if (action>='0' && action<='9') {
        if (fresh || mode==S_RESULT) {
            snprintf(disp, sizeof(disp), "%c", action);
            fresh=0; has_decimal=0; mode=S_INPUT;
        } else {
            if (strlen(disp)<16) {
                char tmp[2]={action,0};
                strcat(disp, tmp);
            }
        }
        return;
    }
    if (action=='.') {
        if (mode==S_RESULT) { strcpy(disp,"0"); mode=S_INPUT; fresh=0; }
        if (fresh) { strcpy(disp,"0"); fresh=0; }
        if (!has_decimal) { strcat(disp,"."); has_decimal=1; }
        return;
    }
    if (action=='+' || action=='-' || action=='*' || action=='/') {
        double cur = parse_disp();
        if (pending_op && !fresh && mode!=S_RESULT) {
            double result = apply_op(left_val, pending_op, cur);
            format_num(disp, result);
            left_val = result;
        } else {
            left_val = cur;
        }
        pending_op = action;
        fresh = 1;
        has_decimal = 0;
        mode = S_INPUT;
        return;
    }
    if (action=='=') {
        double cur = parse_disp();
        if (pending_op) {
            double result = apply_op(left_val, pending_op, cur);
            format_num(disp, result);
            left_val = result;
            pending_op = 0;
        }
        fresh = 1;
        has_decimal = 0;
        mode = S_RESULT;
        return;
    }
    if (action=='C') {
        strcpy(disp,"0"); left_val=0; pending_op=0;
        fresh=1; has_decimal=0; mode=S_INPUT;
        return;
    }
    if (action=='E') { // CE — clear current entry only
        strcpy(disp,"0"); fresh=1; has_decimal=0; mode=S_INPUT;
        return;
    }
    if (action=='~') { // negate
        double v = parse_disp();
        format_num(disp, -v);
        return;
    }
    if (action=='%') {
        double v = parse_disp();
        format_num(disp, v/100.0);
        has_decimal=1;
        return;
    }
}

// ============================================================
// Render one frame
// ============================================================
static void render(int cursor_x, int cursor_y, int cursor_valid, int hover) {
    // Background
    draw_rect(0, 0, SCREEN_W, SCREEN_H, 30, 30, 35);

    // --- Display area ---
    draw_rect(DISP_X, DISP_Y, DISP_W, DISP_H, 10, 10, 14);
    draw_outline(DISP_X, DISP_Y, DISP_W, DISP_H, 90, 90, 100);

    // Operator indicator (top-left of display)
    if (pending_op) {
        char op_str[2] = {pending_op, 0};
        draw_str(op_str, DISP_X+8, DISP_Y+8, 2, 180, 180, 50);
    }

    // Number (right-aligned, scale 4)
    {
        int scale = 4;
        int tw = str_width(disp, scale);
        int tx = DISP_X + DISP_W - tw - 10;
        int ty = DISP_Y + (DISP_H - 8*scale)/2;
        if (tx < DISP_X+8) tx = DISP_X+8;
        draw_str(disp, tx, ty, scale, 230, 240, 230);
    }

    // --- Buttons ---
    for (int i=0; i<NBUTTONS; i++) {
        Button *b = &BUTTONS[i];
        int hovered = (i==hover);
        u8 r = b->br, g = b->bg, bl = b->bb;
        if (hovered) {
            // Lighten on hover
            r = (u8)(r + (255-r)/2);
            g = (u8)(g + (255-g)/2);
            bl= (u8)(bl+ (255-bl)/2);
        }
        draw_rect(b->x, b->y, b->w, b->h, r, g, bl);
        // Highlight edge (top+left brighter)
        draw_rect(b->x, b->y, b->w, 2,
                  clamp8(r+60), clamp8(g+60), clamp8(bl+60));
        draw_rect(b->x, b->y, 2, b->h,
                  clamp8(r+60), clamp8(g+60), clamp8(bl+60));
        // Shadow edge (bottom+right darker)
        draw_rect(b->x, b->y+b->h-2, b->w, 2,
                  clamp8(r-40), clamp8(g-40), clamp8(bl-40));
        draw_rect(b->x+b->w-2, b->y, 2, b->h,
                  clamp8(r-40), clamp8(g-40), clamp8(bl-40));

        // Label — centred in button, scale 3
        {
            int scale = 3;
            const char *lbl = b->label;
            int tw = str_width(lbl, scale);
            int th = 8*scale;
            int tx = b->x + (b->w - tw)/2;
            int ty = b->y + (b->h - th)/2;
            draw_str(lbl, tx, ty, scale, 220, 225, 220);
        }
    }

    // --- Cursor ---
    if (cursor_valid) {
        int cx = cursor_x, cy = cursor_y;
        // Black shadow first, then white/yellow
        for (int i=-8; i<=8; i++) {
            draw_pixel(cx+i+1, cy+1, 0,0,0);
            draw_pixel(cx+1, cy+i+1, 0,0,0);
        }
        for (int i=-8; i<=8; i++) {
            draw_pixel(cx+i, cy, 255,255,0);
            draw_pixel(cx, cy+i, 255,255,0);
        }
        // Centre dot
        draw_rect(cx-2, cy-2, 4, 4, 255, 80, 0);
    }
}

// ============================================================
// Main
// ============================================================
int main(int argc, char **argv) {
    VIDEO_Init();
    WPAD_Init();

    rmode = VIDEO_GetPreferredMode(NULL);
    xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

    VIDEO_Configure(rmode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (rmode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    // Enable Wiimote IR (needs sensor bar powered — works in Dolphin too)
    WPAD_SetDataFormat(WPAD_CHAN_0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0, rmode->fbWidth, rmode->xfbHeight);

    int last_hover = -1;
    int a_was_held = 0;

    while (1) {
        WPAD_ScanPads();

        u32 down = WPAD_ButtonsDown(0);
        u32 held = WPAD_ButtonsHeld(0);

        if (down & WPAD_BUTTON_HOME) exit(0);

        // IR cursor
        WPADData *wdata = WPAD_Data(WPAD_CHAN_0);
        int cx = 0, cy = 0, cv = 0;
        if (wdata && wdata->ir.valid) {
            cx = (int)wdata->ir.x;
            cy = (int)wdata->ir.y;
            cv = 1;
        }

        int hover = cv ? hit_button(cx, cy) : -1;

        // A button click
        int a_held_now = (held & WPAD_BUTTON_A) != 0;
        if (a_held_now && !a_was_held && hover>=0) {
            calc_press(BUTTONS[hover].action);
        }
        a_was_held = a_held_now;

        // Also support d-pad / B + A for non-IR play
        // (omitted for clarity — IR is the primary input)

        // Redraw only when something changed
        if (hover != last_hover || cv) {
            render(cx, cy, cv, hover);
            VIDEO_SetNextFramebuffer(xfb);
            VIDEO_Flush();
        }
        last_hover = hover;

        VIDEO_WaitVSync();
    }

    return 0;
}
