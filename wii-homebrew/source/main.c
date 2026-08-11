/**
 *  CUBIIS ONLINE — Nintendo Wii Homebrew Multiplayer Platformer
 *
 *  Setup: run game-server.js on a PC on the same WiFi as your iPad.
 *         Enter that PC's local IP in the app.
 *
 *  Controls (in-game):
 *    D-Pad L/R  — walk
 *    A          — jump
 *    HOME       — quit
 */

/* ─── includes ─────────────────────────────────────────────── */
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <network.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── framebuffer (double-buffered) ────────────────────────── */
static void       *xfb[2] = {NULL, NULL};
static int         fbi     = 0;
static GXRModeObj *rmode   = NULL;
#define FB     ((u32*)xfb[fbi])
#define SW     640
#define SH     480

/* ─── colour: RGB → YCbCr big-endian XFB ───────────────────── */
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

/* ─── 8×8 font (ASCII 32–90) ───────────────────────────────── */
/* index = char - 32 */
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
    int idx=(int)c-32;
    if(idx<0||idx>58) return;
    const u8*p=F[idx];
    for(int row=0;row<8;row++)
        for(int col=0;col<8;col++)
            if(p[row]&(0x80>>col))
                R(x+col*sc,y+row*sc,sc,sc,r,g,b);
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

/* ─── animated button helper ───────────────────────────────── */
static void BTN(int x,int y,int w,int h,const char*lbl,int sc,
                float anim,u8 br,u8 bg,u8 bb){
    u8 r=C8(br+(int)(anim*70)), g=C8(bg+(int)(anim*70)), b=C8(bb+(int)(anim*70));
    R(x,y,w,h,r,g,b);
    R(x,y,w,2,C8(r+55),C8(g+55),C8(b+55));
    R(x,y,2,h,C8(r+45),C8(g+45),C8(b+45));
    R(x,y+h-2,w,2,C8(r-40),C8(g-40),C8(b-40));
    R(x+w-2,y,2,h,C8(r-40),C8(g-40),C8(b-40));
    int lx=x+(w-TW(lbl,sc))/2, ly=y+(h-8*sc)/2;
    T(lbl,lx,ly,sc,220,225,220);
}

/* cursor */
static void CURSOR(int cx,int cy){
    for(int i=-10;i<=10;i++){
        u32*p; int s=rmode->fbWidth>>1;
        /* shadow */
        if(cx+i+1>=0&&cx+i+1<SW&&cy+1>=0&&cy+1<SH){
            p=&FB[(cy+1)*s+((cx+i+1)>>1)];
            if((cx+i+1)&1) *p=(*p&0xFF000000u)|0x00100010u;
            else            *p=0x10001000u;
        }
        /* crosshair: yellow */
        R(cx+i,cy,1,1,255,255,0);
        R(cx,cy+i,1,1,255,255,0);
    }
    R(cx-2,cy-2,5,5,255,100,0);
}

/* ─── colour palette ────────────────────────────────────────── */
static const char* CNAME[9]={"RED","ORANGE","YELLOW","LIME","GREEN","LBLUE","BLUE","PURPLE","PINK"};
static const u8 CRGB[9][3]={
    {220,50,50},{220,140,40},{220,210,40},
    {100,210,40},{40,170,40},{40,190,210},
    {40,80,210},{140,40,210},{210,40,140},
};

/* ─── platform map ──────────────────────────────────────────── */
#define NPL 9
static const int PL[NPL][4]={
    {0,448,640,32},   /* ground          */
    {55,376,130,14},  /* left low        */
    {252,348,136,14}, /* centre low      */
    {455,376,130,14}, /* right low       */
    {130,285,110,14}, /* left mid        */
    {400,285,110,14}, /* right mid       */
    {258,210,124,14}, /* centre high     */
    {40,148,90,14},   /* top left        */
    {510,148,90,14},  /* top right       */
};

/* ─── remote players ────────────────────────────────────────── */
#define MAX_REMOTE 7
typedef struct{
    int id,color,active;
    char name[13];
    float x,y,dx,dy;
} Remote;
static Remote REM[MAX_REMOTE];

static void rem_add(int id,const char*nm,int col,int x,int y){
    for(int i=0;i<MAX_REMOTE;i++){
        if(REM[i].active && REM[i].id==id) return;
        if(!REM[i].active){
            REM[i].id=id; REM[i].color=col;
            strncpy(REM[i].name,nm,12); REM[i].name[12]=0;
            REM[i].x=REM[i].dx=(float)x;
            REM[i].y=REM[i].dy=(float)y;
            REM[i].active=1; return;
        }
    }
}
static void rem_pos(int id,int x,int y){
    for(int i=0;i<MAX_REMOTE;i++)
        if(REM[i].active&&REM[i].id==id){ REM[i].x=(float)x; REM[i].y=(float)y; return; }
}
static void rem_del(int id){
    for(int i=0;i<MAX_REMOTE;i++)
        if(REM[i].active&&REM[i].id==id){ REM[i].active=0; return; }
}
static void rem_interp(){
    for(int i=0;i<MAX_REMOTE;i++){
        if(!REM[i].active) continue;
        REM[i].dx+=(REM[i].x-REM[i].dx)*0.25f;
        REM[i].dy+=(REM[i].y-REM[i].dy)*0.25f;
    }
}

/* ─── networking ────────────────────────────────────────────── */
static int  net_ok   =0;       /* net_init() succeeded          */
static int  net_sock =-1;      /* TCP socket                    */
static int  net_conn =0;       /* fully connected + joined      */
static char net_buf[2048];     /* recv accumulation buffer      */
static int  net_bufn =0;       /* bytes in buf                  */
static char net_errmsg[64]=""; /* last connection error         */

static void ns(const char*msg){
    if(net_sock<0) return;
    net_send(net_sock,(void*)msg,strlen(msg),0);
}

static void net_parse_line(char*line){
    char cmd[32]="";
    sscanf(line,"%31s",cmd);
    /* POS id x y */
    if(!strcmp(cmd,"POS")){
        int id,x,y;
        if(sscanf(line,"POS %d %d %d",&id,&x,&y)==3) rem_pos(id,x,y);
    /* JOINED id name color x y */
    } else if(!strcmp(cmd,"JOINED")){
        int id,col,x,y; char nm[13];
        if(sscanf(line,"JOINED %d %12s %d %d %d",&id,nm,&col,&x,&y)==5)
            rem_add(id,nm,col,x,y);
    /* LEFT id */
    } else if(!strcmp(cmd,"LEFT")){
        int id; if(sscanf(line,"LEFT %d",&id)==1) rem_del(id);
    }
    /* WELCOME, ROOMS etc. handled in caller contexts via separate recv pass */
}

/* non-blocking receive + parse all complete lines */
static void net_tick(){
    if(net_sock<0) return;
    struct timeval tv={0,0};
    fd_set rf; FD_ZERO(&rf); FD_SET(net_sock,&rf);
    if(net_select(net_sock+1,&rf,NULL,NULL,&tv)<=0) return;
    char tmp[256];
    int n=net_recv(net_sock,tmp,sizeof(tmp)-1,0);
    if(n<=0){ net_conn=0; net_close(net_sock); net_sock=-1; return; }
    if(net_bufn+n<(int)sizeof(net_buf)){ memcpy(net_buf+net_bufn,tmp,n); net_bufn+=n; }
    while(1){
        char*nl=(char*)memchr(net_buf,'\n',net_bufn);
        if(!nl) break;
        int len=nl-net_buf; net_buf[len]=0;
        net_parse_line(net_buf);
        int rem=net_bufn-len-1;
        memmove(net_buf,nl+1,rem); net_bufn=rem;
    }
}

/* blocking connect + send JOIN; returns 1=ok 0=fail */
static int net_join(const char*addr,int port,int room,const char*name,int color){
    if(net_sock>=0){ net_close(net_sock); net_sock=-1; }
    net_sock=net_socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(net_sock<0){ snprintf(net_errmsg,sizeof(net_errmsg),"NO SOCKET"); return 0; }

    /* resolve address */
    unsigned a,b,c,d;
    u32 ip=0;
    if(sscanf(addr,"%u.%u.%u.%u",&a,&b,&c,&d)==4)
        ip=(a<<24)|(b<<16)|(c<<8)|d;
    else {
        struct hostent*he=net_gethostbyname(addr);
        if(!he){ snprintf(net_errmsg,sizeof(net_errmsg),"DNS FAIL"); net_close(net_sock); net_sock=-1; return 0; }
        ip=*(u32*)he->h_addr;
    }

    struct sockaddr_in sa; memset(&sa,0,sizeof(sa));
    sa.sin_family=AF_INET;
    sa.sin_port=htons((u16)port);
    sa.sin_addr.s_addr=ip;

    if(net_connect(net_sock,(struct sockaddr*)&sa,sizeof(sa))<0){
        snprintf(net_errmsg,sizeof(net_errmsg),"CANT CONNECT");
        net_close(net_sock); net_sock=-1; return 0;
    }

    /* send JOIN */
    char msg[128];
    snprintf(msg,sizeof(msg),"JOIN %d %.12s %d\n",room,name,color);
    net_send(net_sock,(void*)msg,strlen(msg),0);

    /* wait for WELCOME (blocking, up to ~200 frames) */
    net_buf[0]=0; net_bufn=0;
    for(int tries=0;tries<200;tries++){
        char tmp[128];
        int n=net_recv(net_sock,tmp,sizeof(tmp)-1,0);
        if(n>0){
            if(net_bufn+n<(int)sizeof(net_buf)){ memcpy(net_buf+net_bufn,tmp,n); net_bufn+=n; }
            char*nl=(char*)memchr(net_buf,'\n',net_bufn);
            if(nl){ *nl=0;
                if(!strncmp(net_buf,"WELCOME",7)){ net_bufn=0; net_conn=1; return 1; }
                if(!strncmp(net_buf,"ERROR",5)){
                    snprintf(net_errmsg,sizeof(net_errmsg),"%.60s",net_buf+6);
                    net_close(net_sock); net_sock=-1; return 0;
                }
                int rem=net_bufn-(nl-net_buf)-1;
                memmove(net_buf,nl+1,rem); net_bufn=rem;
            }
        }
    }
    snprintf(net_errmsg,sizeof(net_errmsg),"NO WELCOME");
    net_close(net_sock); net_sock=-1; return 0;
}

/* ─── app state ─────────────────────────────────────────────── */
typedef enum { S_TITLE,S_ADDR,S_NAME,S_COLOR,S_ROOMS,S_CONNECTING,S_GAME } State;
static State state=S_TITLE;

static char plr_addr[64]=""; /* server IP:port  */
static char plr_name[13]=""; /* display name    */
static int  plr_color=0;     /* 0-8             */
static int  plr_room =0;     /* 0-5             */
static int  offline_mode=0;  /* skip server entirely */

/* local player physics */
static float px=300,py=420,pvx=0,pvy=0;
static int   pon=0; /* on_ground flag */

/* ─── physics step ──────────────────────────────────────────── */
#define GRAV  0.38f
#define SPD   3.5f
#define JVEL  -9.2f
static void phys(int left,int right,int jump){
    if(left)  pvx=-SPD;
    else if(right) pvx=SPD;
    else pvx*=0.75f;

    if(jump && pon){ pvy=JVEL; pon=0; }
    pvy+=GRAV;
    if(pvy>13) pvy=13;

    /* move x */
    px+=pvx;
    if(px<5) { px=5; pvx=0; }
    if(px+20>635) { px=615; pvx=0; }

    /* move y */
    py+=pvy;
    pon=0;
    /* platform collision */
    for(int i=0;i<NPL;i++){
        int px2=(int)px, py2=(int)py;
        int plx=PL[i][0], ply=PL[i][1], plw=PL[i][2], plh=PL[i][3];
        /* AABB overlap? */
        if(px2+20>plx && px2<plx+plw && py2+20>ply && py2<ply+plh){
            /* resolve: which side has smallest overlap? */
            float ovy = (py2+20 > ply && pvy>=0) ? (float)(py2+20-ply) : 9999;
            float oyu = (py2    < ply+plh && pvy<0) ? (float)(ply+plh-py2) : 9999;
            float ovx1= (px2+20 > plx) ? (float)(px2+20-plx) : 9999;
            float ovx2= (px2    < plx+plw) ? (float)(plx+plw-px2) : 9999;
            float mn=ovy; if(oyu<mn) mn=oyu; if(ovx1<mn) mn=ovx1; if(ovx2<mn) mn=ovx2;
            if(mn==ovy){ py=(float)(ply-20); pvy=0; pon=1; }
            else if(mn==oyu){ py=(float)(ply+plh); pvy=0; }
            else if(mn==ovx1){ px=(float)(plx-20); pvx=0; }
            else              { px=(float)(plx+plw); pvx=0; }
        }
    }
    /* game boundary ceiling */
    if(py<52){ py=52; pvy=0; }
}

/* ─── render platform world ─────────────────────────────────── */
static void draw_world(){
    R(0,50,SW,SH-50,18,20,30); /* bg */
    R(0,50,5,SH-50,30,32,45);  /* left wall */
    R(635,50,5,SH-50,30,32,45);/* right wall */
    for(int i=0;i<NPL;i++){
        int x=PL[i][0],y=PL[i][1],w=PL[i][2],h=PL[i][3];
        u8 r=(i==0)?50:90, g=(i==0)?85:62, b=(i==0)?40:38;
        R(x,y,w,h,r,g,b);
        R(x,y,w,3,C8(r+55),C8(g+55),C8(b+40)); /* top highlight */
    }
}

/* draw a player cube (20×20) with name above */
static void draw_cube(int x,int y,int col,const char*nm){
    u8 r=CRGB[col][0],g=CRGB[col][1],b=CRGB[col][2];
    R(x,y,20,20,r,g,b);
    R(x,y,20,5,C8(r+60),C8(g+60),C8(b+60));
    R(x,y,5,20,C8(r+40),C8(g+40),C8(b+40));
    R(x,y+16,20,4,C8(r-50),C8(g-50),C8(b-50));
    /* name above (scale 1) */
    int nw=TW(nm,1);
    T(nm,x+(20-nw)/2,y-10,1,220,220,220);
}

/* ─── keyboard (QWERTY) ─────────────────────────────────────── */
/* 3 rows of 10, bottom row: ZXCVBNM + SPACE + OK */
/* key w=52 h=36 gap=4 step=56                     */
#define KX0  42
#define KY0  188
#define KW   52
#define KH   36
#define KS   56   /* step */

static const char KR0[11]="1234567890";
static const char KR1[11]="QWERTYUIOP";
static const char KR2[10]="ASDFGHJKL"; /* + backspace at [9] */
static const char KR3[8] ="ZXCVBNM";   /* + space at [7], OK at [8] */

/* 39 animation values: 0-9 row0, 10-19 row1, 20-28 row2 letters, 29=bksp,
   30-36 row3 letters, 37=space, 38=ok */
static float  KA[39];
static int    KP[39]; /* press countdown */

static int kb_hit(int cx,int cy){
    for(int r=0;r<3;r++)
        for(int k=0;k<10;k++){
            int kx=KX0+k*KS, ky=KY0+r*(KH+6);
            if(cx>=kx&&cx<kx+KW&&cy>=ky&&cy<ky+KH) return r*10+k;
        }
    /* row 3 letters */
    for(int k=0;k<7;k++){
        int kx=30+k*KS, ky=KY0+3*(KH+6);
        if(cx>=kx&&cx<kx+KW&&cy>=ky&&cy<ky+KH) return 30+k;
    }
    /* space: x=30+7*56=422, w=116 */
    if(cx>=422&&cx<538&&cy>=KY0+3*(KH+6)&&cy<KY0+3*(KH+6)+KH) return 37;
    /* ok: x=542, w=98 */
    if(cx>=542&&cx<640&&cy>=KY0+3*(KH+6)&&cy<KY0+3*(KH+6)+KH) return 38;
    return -1;
}

static char kb_char(int idx){
    if(idx>=0  && idx<=9)  return KR0[idx];
    if(idx>=10 && idx<=19) return KR1[idx-10];
    if(idx>=20 && idx<=28) return KR2[idx-20];
    if(idx==29)            return '\b';
    if(idx>=30 && idx<=36) return KR3[idx-30];
    if(idx==37)            return ' ';
    if(idx==38)            return '\n';
    return 0;
}

static void draw_kb(int cx,int cy,const char*input,const char*title,int blink){
    R(0,0,SW,SH,20,22,32);
    TC(title,320,18,2,180,190,200);

    /* input display */
    R(80,50,480,44,10,12,18);
    R(80,50,480,2,60,65,80); R(80,92,480,2,60,65,80);
    R(80,50,2,44,60,65,80);  R(558,50,2,44,60,65,80);

    /* text + blinking cursor */
    char buf[16]; snprintf(buf,sizeof(buf),"%.12s",input);
    int bx=92, by=58;
    T(buf,bx,by,3,220,235,220);
    if(blink/30%2==0){
        int cpos=TW(buf,3);
        R(bx+cpos+2,by,2,24,220,225,220);
    }

    /* keyboard rows 0-2 */
    static const char*rows[3]={KR0,KR1,KR2};
    for(int row=0;row<3;row++){
        for(int k=0;k<10;k++){
            int idx=row*10+k;
            int kx=KX0+k*KS, ky=KY0+row*(KH+6);
            char lbl[3]; lbl[0]=(row<2)?rows[row][k]:KR2[k>8?8:k]; lbl[1]=0;
            if(k==9&&row==2){ lbl[0]='<'; lbl[1]='-'; lbl[2]=0; }
            float a=KA[idx]-(KP[idx]/8.0f)*0.5f;
            BTN(kx,ky,KW,KH,lbl,1,a, 55,58,70);
        }
    }
    /* row 3 letters */
    for(int k=0;k<7;k++){
        int idx=30+k;
        int kx=30+k*KS, ky=KY0+3*(KH+6);
        char lbl[2]={KR3[k],0};
        float a=KA[idx]-(KP[idx]/8.0f)*0.5f;
        BTN(kx,ky,KW,KH,lbl,1,a,55,58,70);
    }
    /* space */
    BTN(422,KY0+3*(KH+6),116,KH,"SPACE",1,KA[37]-(KP[37]/8.0f)*0.5f,55,58,70);
    /* OK */
    BTN(542,KY0+3*(KH+6),94,KH,"OK",1,KA[38]-(KP[38]/8.0f)*0.5f,30,130,60);

    CURSOR(cx,cy);
}

/* ─── IP numpad ─────────────────────────────────────────────── */
#define NX0  192
#define NY0  158
#define NKW  80
#define NKH  58
#define NGS  8

static const char NP_CHARS[13]="789456123.0\b";
static float NA[14];
static int   NP2[14];

static int np_hit(int cx,int cy){
    for(int r=0;r<4;r++)
        for(int c=0;c<3;c++){
            int kx=NX0+c*(NKW+NGS), ky=NY0+r*(NKH+NGS);
            if(cx>=kx&&cx<kx+NKW&&cy>=ky&&cy<ky+NKH) return r*3+c;
        }
    /* OK: y=NY0+4*(NKH+NGS)+8=430, x=230 w=180 */
    if(cx>=230&&cx<410&&cy>=430&&cy<478) return 12;
    /* PLAY OFFLINE button: sits between input box and numpad */
    if(cx>=210&&cx<430&&cy>=116&&cy<150) return 13;
    return -1;
}

static void draw_numpad(int cx,int cy,const char*input){
    R(0,0,SW,SH,20,22,32);
    TC("SERVER ADDRESS",320,18,2,180,190,200);
    TC("ENTER IP   E.G. 192.168.1.10",320,46,1,120,130,140);

    /* input box */
    R(80,68,480,42,10,12,18);
    R(80,68,480,2,60,65,80); R(80,108,480,2,60,65,80);
    R(80,68,2,42,60,65,80);  R(558,68,2,42,60,65,80);
    T(input,92,76,3,220,235,220);

    static const char KLBL[12][3]={"7","8","9","4","5","6","1","2","3",".",".0","<-"};
    for(int r=0;r<4;r++){
        for(int c=0;c<3;c++){
            int idx=r*3+c;
            int kx=NX0+c*(NKW+NGS), ky=NY0+r*(NKH+NGS);
            char lbl[3]; lbl[0]=NP_CHARS[idx]; lbl[1]=0;
            if(idx==10){ lbl[0]='0'; lbl[1]=0; }
            if(idx==11){ lbl[0]='<'; lbl[1]='-'; lbl[2]=0; }
            float a=NA[idx]-(NP2[idx]/8.0f)*0.5f;
            BTN(kx,ky,NKW,NKH,lbl,2,a,55,58,70);
        }
    }
    BTN(210,116,220,34,"PLAY OFFLINE",1,NA[13]-(NP2[13]/8.0f)*0.5f,100,50,150);
    BTN(230,430,180,44,"OK",2,NA[12]-(NP2[12]/8.0f)*0.5f,30,130,60);

    CURSOR(cx,cy);
}

/* ─── colour picker ─────────────────────────────────────────── */
/* 9 swatches centred: total = 9*58+8*6=570, start x=(640-570)/2=35 */
#define SW_X0 35
#define SW_Y0 168
#define SW_W  58
#define SW_GAP 6
static float SWA[9];
static int   sel_color=0;

static void draw_colorpick(int cx,int cy){
    R(0,0,SW,SH,20,22,32);
    TC("CHOOSE YOUR COLOR",320,25,2,180,190,200);
    TC("POINT AND PRESS A",320,52,1,120,130,140);

    for(int i=0;i<9;i++){
        int sx=SW_X0+i*(SW_W+SW_GAP);
        int sy=SW_Y0;
        int exp=(int)(SWA[i]*8);
        int x2=sx-exp,y2=sy-exp,s2=SW_W+exp*2;
        u8 r=CRGB[i][0],g=CRGB[i][1],b=CRGB[i][2];
        R(x2,y2,s2,s2,r,g,b);
        /* shine */
        R(x2,y2,s2,4,C8(r+70),C8(g+70),C8(b+70));
        R(x2,y2,4,s2,C8(r+50),C8(g+50),C8(b+50));
        if(i==sel_color){
            R(x2-3,y2-3,s2+6,3,240,240,240);
            R(x2-3,y2+s2,s2+6,3,240,240,240);
            R(x2-3,y2-3,3,s2+6,240,240,240);
            R(x2+s2,y2-3,3,s2+6,240,240,240);
        }
        TC(CNAME[i],sx+SW_W/2,sy+SW_W+10,1,160,165,170);
    }

    BTN(220,360,200,50,"CONFIRM",2,0,30,130,60);
    BTN(570,12,65,36,"BACK",1,0,80,40,40);

    CURSOR(cx,cy);
}

/* ─── room selection ─────────────────────────────────────────── */
/* 3×2 grid: x=35,230,425 y=150,240 w=175 h=72 */
static float RMA[6];
static int   sel_room=0;
static char  err_msg[64]="";

static void draw_rooms(int cx,int cy){
    R(0,0,SW,SH,20,22,32);
    TC("SELECT ROOM",320,25,2,180,190,200);
    TC("CHOOSE A ROOM TO JOIN",320,52,1,120,130,140);

    static const int RX[3]={35,230,425};
    for(int i=0;i<6;i++){
        int col=i%3,row=i/3;
        int bx=RX[col], by=150+row*100;
        char lbl[12]; snprintf(lbl,sizeof(lbl),"ROOM %d",i+1);
        BTN(bx,by,175,72,lbl,2,RMA[i],50,55,75);
        if(i==sel_room){
            R(bx-3,by-3,181,3,60,200,100);
            R(bx-3,by+72,181,3,60,200,100);
            R(bx-3,by-3,3,78,60,200,100);
            R(bx+175,by-3,3,78,60,200,100);
        }
    }

    BTN(220,375,200,50,"JOIN",2,0,30,130,60);
    BTN(570,12,65,36,"BACK",1,0,80,40,40);

    if(err_msg[0]) TC(err_msg,320,440,1,220,80,80);

    CURSOR(cx,cy);
}

/* ─── connecting screen ─────────────────────────────────────── */
static void draw_connecting(int frame){
    R(0,0,SW,SH,20,22,32);
    TC("CONNECTING",320,200,3,160,180,200);
    static const char*dots[4]={"",".","..","...."};
    TC(dots[(frame/15)%4],320,260,2,100,110,120);
}

/* ─── title screen ──────────────────────────────────────────── */
static void draw_title(int blink){
    R(0,0,SW,SH,12,14,22);

    /* starfield: deterministic "random" pixels */
    for(int i=0;i<200;i++){
        int sx=(i*137+i*i*31)%640, sy=(i*97+i*3)%430+40;
        u8 br=80+(i%3)*40;
        R(sx,sy,1,1,br,br,br+20);
    }

    TC("CUBIIS",320,100,5,60,140,255);
    TC("ONLINE",320,158,5,255,80,160);

    TC("MULTIPLAYER PLATFORMER",320,248,1,120,130,150);

    if(blink/30%2==0) TC("PRESS A TO START",320,310,2,180,190,200);
    TC("POINT WIIMOTE AT SCREEN",320,370,1,80,90,100);

    if(!net_ok) TC("NETWORK UNAVAILABLE  OFFLINE ONLY",320,430,1,180,80,80);
}

/* ─── game UI bar ────────────────────────────────────────────── */
static void draw_game_ui(int cx,int cy,int cv){
    R(0,0,SW,52,20,22,34);
    R(0,50,SW,2,50,55,80);

    /* room number */
    char rbuf[16]; snprintf(rbuf,sizeof(rbuf),"ROOM %d",plr_room+1);
    T(rbuf,8,16,2,80,200,120);

    /* player name + colour */
    int nw=TW(plr_name,2);
    R(SW/2-nw/2-6,8,nw+12,36,CRGB[plr_color][0]/2,CRGB[plr_color][1]/2,CRGB[plr_color][2]/2);
    TC(plr_name,SW/2,16,2,CRGB[plr_color][0],CRGB[plr_color][1],CRGB[plr_color][2]);

    /* connection */
    const char*cs=net_conn?"ONLINE":"OFFLINE";
    T(cs,SW-TW(cs,1)-8,20,1,net_conn?80:160,net_conn?200:80,80);

    if(cv) CURSOR(cx,cy);
}

/* ─── animation update ──────────────────────────────────────── */
static void anim_update(int cx,int cy){
    float SP=0.18f;
    int kh=kb_hit(cx,cy), nh=np_hit(cx,cy);
    for(int i=0;i<39;i++){
        float t=(i==kh)?1.0f:0.0f;
        KA[i]+=(t-KA[i])*SP;
        if(KP[i]>0) KP[i]--;
    }
    for(int i=0;i<14;i++){
        float t=(i==nh)?1.0f:0.0f;
        NA[i]+=(t-NA[i])*SP;
        if(NP2[i]>0) NP2[i]--;
    }
    for(int i=0;i<9;i++){
        int sx=SW_X0+i*(SW_W+SW_GAP), sy=SW_Y0;
        float t=(cx>=sx&&cx<sx+SW_W&&cy>=sy&&cy<sy+SW_W)?1.0f:0.0f;
        SWA[i]+=(t-SWA[i])*SP;
    }
    static const int RX[3]={35,230,425};
    for(int i=0;i<6;i++){
        int bx=RX[i%3], by=150+(i/3)*100;
        float t=(cx>=bx&&cx<bx+175&&cy>=by&&cy<by+72)?1.0f:0.0f;
        RMA[i]+=(t-RMA[i])*SP;
    }
}

/* ─── main ───────────────────────────────────────────────────── */
int main(int argc,char**argv){
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

    /* show "initialising" before blocking net_init */
    R(0,0,SW,SH,12,14,22);
    TC("CUBIIS ONLINE",320,200,3,60,140,255);
    TC("INITIALISING NETWORK",320,268,2,120,130,140);
    VIDEO_SetNextFramebuffer(xfb[fbi]); VIDEO_Flush(); VIDEO_WaitVSync(); fbi^=1;

    if(net_init()>=0) net_ok=1;

    WPAD_SetDataFormat(WPAD_CHAN_0,WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(WPAD_CHAN_0,rmode->fbWidth,rmode->xfbHeight);

    memset(REM,0,sizeof(REM));
    memset(KA,0,sizeof(KA)); memset(KP,0,sizeof(KP));
    memset(NA,0,sizeof(NA)); memset(NP2,0,sizeof(NP2));
    memset(SWA,0,sizeof(SWA)); memset(RMA,0,sizeof(RMA));

    int blink=0, frame=0;
    int aw=0; /* A was held */
    int net_send_t=0;

    while(1){
        WPAD_ScanPads();
        u32 held=WPAD_ButtonsHeld(0);
        u32 down=WPAD_ButtonsDown(0);
        if(down&WPAD_BUTTON_HOME){
            if(net_conn){ ns("QUIT\n"); }
            if(net_sock>=0){ net_close(net_sock); net_sock=-1; }
            exit(0);
        }

        WPADData*wd=WPAD_Data(WPAD_CHAN_0);
        int cx=320,cy=240,cv=0;
        if(wd&&wd->ir.valid){ cx=(int)wd->ir.x; cy=(int)wd->ir.y; cv=1; }

        int an=(held&WPAD_BUTTON_A)!=0;
        int ad=an&&!aw;
        aw=an;

        blink++; frame++;
        anim_update(cx,cy);

        /* ── state machine ─────────────────────────────────── */
        switch(state){

        case S_TITLE:
            if(ad) state=S_ADDR;
            draw_title(blink);
            break;

        case S_ADDR:{
            int nh=np_hit(cx,cy);
            if(ad && nh>=0){
                NP2[nh]=8;
                if(nh==13){ /* PLAY OFFLINE */
                    offline_mode=1;
                    state=S_NAME;
                } else if(nh<12){
                    char ch=NP_CHARS[nh<10?nh:nh];
                    if(nh==10) ch='0';
                    if(nh==11){ /* backspace */
                        int ln=strlen(plr_addr);
                        if(ln>0) plr_addr[ln-1]=0;
                    } else if(strlen(plr_addr)<sizeof(plr_addr)-2){
                        char s2[2]={ch,0}; strcat(plr_addr,s2);
                    }
                } else { /* OK */
                    if(strlen(plr_addr)>0){ offline_mode=0; state=S_NAME; }
                }
            }
            /* BACK via B */
            if(down&WPAD_BUTTON_B) state=S_TITLE;
            draw_numpad(cx,cy,plr_addr);
            break;}

        case S_NAME:{
            int kh2=kb_hit(cx,cy);
            if(ad && kh2>=0){
                KP[kh2]=8;
                char ch=kb_char(kh2);
                if(ch=='\b'){ int ln=strlen(plr_name); if(ln>0) plr_name[ln-1]=0; }
                else if(ch=='\n'){ if(strlen(plr_name)>0) state=S_COLOR; }
                else if(strlen(plr_name)<12){ char s2[2]={ch,0}; strcat(plr_name,s2); }
            }
            if(down&WPAD_BUTTON_B) state=S_ADDR;
            draw_kb(cx,cy,plr_name,"YOUR NAME",blink);
            break;}

        case S_COLOR:{
            /* swatch pick */
            for(int i=0;i<9;i++){
                int sx=SW_X0+i*(SW_W+SW_GAP), sy=SW_Y0;
                if(ad && cx>=sx&&cx<sx+SW_W&&cy>=sy&&cy<sy+SW_W) sel_color=i;
            }
            /* confirm */
            if(ad && cx>=220&&cx<420&&cy>=360&&cy<410){
                plr_color=sel_color;
                state=offline_mode?S_GAME:S_ROOMS;
            }
            /* back */
            if((ad&&cx>=570&&cx<635&&cy>=12&&cy<48)||(down&WPAD_BUTTON_B)) state=S_NAME;
            draw_colorpick(cx,cy);
            break;}

        case S_ROOMS:{
            /* room select */
            static const int RX2[3]={35,230,425};
            for(int i=0;i<6;i++){
                int bx=RX2[i%3], by=150+(i/3)*100;
                if(ad&&cx>=bx&&cx<bx+175&&cy>=by&&cy<by+72) sel_room=i;
            }
            /* join */
            if(ad&&cx>=220&&cx<420&&cy>=375&&cy<425){
                plr_room=sel_room;
                state=S_CONNECTING;
            }
            /* back */
            if((ad&&cx>=570&&cx<635&&cy>=12&&cy<48)||(down&WPAD_BUTTON_B)) state=S_COLOR;
            draw_rooms(cx,cy);
            break;}

        case S_CONNECTING:
            px=300; py=420; pvx=0; pvy=0; pon=0;
            memset(REM,0,sizeof(REM));
            net_bufn=0;
            if(offline_mode){
                err_msg[0]=0;
                state=S_GAME;
            } else {
                draw_connecting(frame);
                VIDEO_SetNextFramebuffer(xfb[fbi]); VIDEO_Flush(); VIDEO_WaitVSync(); fbi^=1;
                if(net_ok && net_join(plr_addr,4001,plr_room,plr_name,plr_color)){
                    err_msg[0]=0;
                    state=S_GAME;
                } else {
                    snprintf(err_msg,sizeof(err_msg),"%.60s",net_errmsg[0]?net_errmsg:"NETWORK UNAVAILABLE");
                    state=S_ROOMS;
                }
            }
            break;

        case S_GAME:{
            int left =(held&WPAD_BUTTON_LEFT )!=0;
            int right=(held&WPAD_BUTTON_RIGHT)!=0;
            int jump =(down&WPAD_BUTTON_A    )!=0;
            /* also try home to disconnect */
            if(down&WPAD_BUTTON_B){
                if(net_conn){ ns("QUIT\n"); }
                if(net_sock>=0){ net_close(net_sock); net_sock=-1; }
                net_conn=0;
                state=offline_mode?S_TITLE:S_ROOMS;
                break;
            }
            phys(left,right,jump);
            rem_interp();
            net_tick();
            /* send position */
            if(++net_send_t>=3){ net_send_t=0;
                char pb[48]; snprintf(pb,sizeof(pb),"POS %d %d\n",(int)px,(int)py); ns(pb); }
            /* render */
            draw_world();
            /* remote players */
            for(int i=0;i<MAX_REMOTE;i++)
                if(REM[i].active)
                    draw_cube((int)REM[i].dx,(int)REM[i].dy,REM[i].color,REM[i].name);
            /* local player */
            draw_cube((int)px,(int)py,plr_color,plr_name);
            draw_game_ui(cx,cy,cv);
            break;}
        }

        /* flip */
        VIDEO_SetNextFramebuffer(xfb[fbi]);
        VIDEO_Flush();
        VIDEO_WaitVSync();
        fbi^=1;
    }
    return 0;
}
