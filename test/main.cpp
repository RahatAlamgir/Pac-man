#include <GL/glew.h>
#include <GL/freeglut.h>
#include <vector>
#include <string>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include "draw.h"

static int WW = 226*3, HH = 248*3;
enum Dir{UP=3,LEFT=2,DOWN=4,RIGHT=1,NONE=0};

struct Pac {
    float tx=13, ty=23;     // tile coords (centered on integers)
    Dir dir=UP, want=RIGHT;
    float speed=6.0f;       // tiles per second
} pac;

Pac preDir;

// ---------------- Map ----------------
static constexpr int COLS=28, ROWS=31;
static const char* MAZE_RAW[ROWS] = {
"WWWWWWWWWWWWWWWWWWWWWWWWWWWW",
"W............WW............W",
"W.WWWW.WWWWW.WW.WWWWW.WWWW.W",
"WoWWWW.WWWWW.WW.WWWWW.WWWWoW",
"W.WWWW.WWWWW.WW.WWWWW.WWWW.W",
"W..........................W",
"W.WWWW.WW.WWWWWWWW.WW.WWWW.W",
"W.WWWW.WW.WWWWWWWW.WW.WWWW.W",
"W......WW....WW....WW......W",
"WWWWWW.WWWWW WW WWWWW.WWWWWW",
"WWWWWW.WWWWW WW WWWWW.WWWWWW",
"WWWWWW.WW          WW.WWWWWW",
"WWWWWW.WW WWWGGWWW WW.WWWWWW",
"WWWWWW.WW W      W WW.WWWWWW",
"T     .   W BNIC W   .     T",
"WWWWWW.WW W      W WW.WWWWWW",
"WWWWWW.WW WWWWWWWW WW.WWWWWW",
"WWWWWW.WW          WW.WWWWWW",
"WWWWWW.WW WWWWWWWW WW.WWWWWW",
"WWWWWW.WW WWWWWWWW WW.WWWWWW",
"W............WW............W",
"W.WWWW.WWWWW.WW.WWWWW.WWWW.W",
"W.WWWW.WWWWW.WW.WWWWW.WWWW.W",
"Wo..WW.......P .......WW..oW",
"WWW.WW.WW.WWWWWWWW.WW.WW.WWW",
"WWW.WW.WW.WWWWWWWW.WW.WW.WWW",
"W......WW....WW....WW......W",
"W.WWWWWWWWWW.WW.WWWWWWWWWW.W",
"W.WWWWWWWWWW.WW.WWWWWWWWWW.W",
"W..........................W",
"WWWWWWWWWWWWWWWWWWWWWWWWWWWW"
};



// ---- add below your MAZE_RAW[] ----
static inline bool is_wall(int tx,int ty){
    if(tx<0||tx>=COLS||ty<0||ty>=ROWS) return true;
    return MAZE_RAW[ty][tx]=='W';
}
static inline int dx(Dir d){ return d==LEFT?-1 : d==RIGHT?1 : 0; }
static inline int dy(Dir d){ return d==UP?-1   : d==DOWN ?1 : 0; }
static inline bool at_center(float x){ return std::fabs(x - std::round(x)) < 1e-3f; }

// optional: simple tunnel wrap on 'T' tiles (left/right)
static inline void tunnel_wrap(){
    int tx = (int)std::round(pac.tx);
    int ty = (int)std::round(pac.ty);
    if(ty<0||ty>=ROWS) return;
    if(MAZE_RAW[ty][tx]=='T' && at_center(pac.tx) && at_center(pac.ty)){
        if(tx==0 && pac.dir==LEFT)       pac.tx = COLS-1;
        else if(tx==COLS-1 && pac.dir==RIGHT) pac.tx = 0;
    }
}








static inline float cell() {
    return std::floor(std::min(WW/(float)COLS, HH/(float)ROWS));
}
static inline float offX(){ return 0.5f*(WW - cell()*COLS); }
static inline float offY(){ return 0.5f*(HH - cell()*ROWS); }
static inline float px_from_tx(int tx){ return offX() + tx*cell() + cell()*0.5f; }
static inline float py_from_ty(int ty){ return HH - (offY() + ty*cell() + cell()*0.5f); }



static void draw_dots(){
    const float r_small = cell()*0.12f;
    const float r_big   = cell()*0.32f;
    for(int y=0; y<ROWS; ++y){
        for(int x=0; x<COLS; ++x){
            char c = MAZE_RAW[y][x];
            if(c=='.')      pellet(px_from_tx(x), py_from_ty(y), r_small);
            else if(c=='o') pellet(px_from_tx(x), py_from_ty(y), r_big);
        }
    }
}



static void display(){
    draw_render();
    draw_dots();

    glutSwapBuffers();


}
static void reshape(int w,int h){
    draw_reshape(w,h);
}
static void timerold(int){
    int px = pac.tx, py= pac.ty , pdir = pac.dir;
    draw_update(1.0f/120.0f);


    glutPostRedisplay();
    //glutTimerFunc(1000/120, timer, 0);
}


static void timer(int){
    const float dt = 1.0f/120.0f;

    // turning only when centered on a tile
    if(at_center(pac.tx) && at_center(pac.ty)){
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);

        // try desired direction first
        int nx = cx + dx(pac.want);
        int ny = cy + dy(pac.want);
        if(!is_wall(nx,ny)) pac.dir = pac.want;

        // snap to exact center to avoid drift
        pac.tx = (float)cx;
        pac.ty = (float)cy;
    }

    // move if next tile is not a wall
    {
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);
        int nx = cx + dx(pac.dir);
        int ny = cy + dy(pac.dir);
        float step = pac.speed * dt; // tiles per frame

        if(!is_wall(nx,ny)){
            pac.tx += dx(pac.dir) * step;
            pac.ty += dy(pac.dir) * step;
        }
    }

    // tunnel wrap
    tunnel_wrap();

    // drive your renderer: reuse the demo loader as a simple “set position”
    // (If your draw.h exposes a setter like draw_set_pac(px,py,dir), call that instead.)
    float px = offX() + pac.tx * cell() + cell()*0.5f;
    float py = HH   - (offY() + pac.ty * cell() + cell()*0.5f);
    draw_load_demo(px, py, pac.dir);

    draw_update(dt);
    glutPostRedisplay();
    glutTimerFunc(1000/120, timer, 0);
}


static void key(unsigned char k,int,int){
    if(k==27) std::exit(0);
    if(k==' ') draw_toggle_pause();
}
static void specialKey(int key,int,int){
    if(key==GLUT_KEY_UP)    pac.want=UP;
    if(key==GLUT_KEY_DOWN)  pac.want=DOWN;
    if(key==GLUT_KEY_LEFT)  pac.want=LEFT;
    if(key==GLUT_KEY_RIGHT) pac.want=RIGHT;
}


int main(int argc,char** argv){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE);
    glutInitWindowSize(WW, HH);
    glutCreateWindow("Pac-Man draw module");

    glewInit();

    if(!draw_init(WW, HH, "image/maze1.png", "image/sprites.png")) return 1;

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    draw_load_demo(310.0f,160.0f,pac.dir); // draw pacman , ghost with animation demo
    glutKeyboardFunc(key);
    glutSpecialFunc(specialKey);
    glutTimerFunc(1000/120, timer, 0);
    glutMainLoop();
    return 0;
}
