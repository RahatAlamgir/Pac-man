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

enum Dir { UP=3, LEFT=2, DOWN=4, RIGHT=1, NONE=0 };

struct Pac {
    float tx=13, ty=23;   // tile coords
    Dir dir=UP, want=RIGHT;
    float speed=6.0f;     // tiles per second
} pac;

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

static char GRID[ROWS][COLS];   // copy of MAZE_RAW you can edit
static int  score = 0;
static int  dots_left = 0;
static float power_time = 3.0f; // seconds of energizer effect

static void init_grid(){
    dots_left = 0;
    for(int y=0;y<ROWS;++y){
        for(int x=0;x<COLS;++x){
            char c = MAZE_RAW[y][x];
            GRID[y][x] = c;
            if(c=='.' || c=='o') ++dots_left;
        }
    }
}


// --------------- Pixel helpers ---------------
static inline float cell(){ return std::floor(std::min(WW/(float)COLS, HH/(float)ROWS)); }
static inline float offX(){ return 0.5f*(WW - cell()*COLS); }
static inline float offY(){ return 0.5f*(HH - cell()*ROWS); }
static inline float px_from_tx(float tx){ return offX() + tx*cell() + cell()*0.5f; }
static inline float py_from_ty(float ty){ return HH - (offY() + ty*cell() + cell()*0.5f); }

// --------------- Maze helpers ---------------
static inline bool is_wall(int tx,int ty){
    if(tx<0||tx>=COLS||ty<0||ty>=ROWS) return true;
    return MAZE_RAW[ty][tx]=='W';
}
static inline int dx(Dir d){ return d==LEFT?-1 : d==RIGHT?1 : 0; }
static inline int dy(Dir d){ return d==UP?-1   : d==DOWN ?1 : 0; }
static inline bool at_center(float x){ return std::fabs(x - std::round(x)) < 1e-3f; }

// optional wrap on tunnel tiles 'T'
static inline void tunnel_wrap(){
    int cx = (int)std::round(pac.tx);
    int cy = (int)std::round(pac.ty);
    if(cy<0||cy>=ROWS) return;
    if(MAZE_RAW[cy][cx]=='T' && at_center(pac.tx) && at_center(pac.ty)){
        if(cx==0 && pac.dir==LEFT)            pac.tx = (float)(COLS-1);
        else if(cx==COLS-1 && pac.dir==RIGHT) pac.tx = 0.0f;
    }
}

// --------------- Dots ---------------
static void draw_dotsold(){
    const float r_small = cell()*0.12f;
    const float r_big   = cell()*0.32f;
    for(int y=0; y<ROWS; ++y){
        for(int x=0; x<COLS; ++x){
            char c = MAZE_RAW[y][x];
            if(c=='.')      pellet(px_from_tx((float)x), py_from_ty((float)y), r_small);
            else if(c=='o') pellet(px_from_tx((float)x), py_from_ty((float)y), r_big);
        }
    }
}
static void draw_dots(){
    const float r_small = cell()*0.12f;
    const float r_big   = cell()*0.32f;
    for(int y=0; y<ROWS; ++y){
        for(int x=0; x<COLS; ++x){
            char c = GRID[y][x];
            if(c=='.')      pellet(px_from_tx((float)x), py_from_ty((float)y), r_small);
            else if(c=='o') pellet(px_from_tx((float)x), py_from_ty((float)y), r_big);
        }
    }
}


// --------------- GLUT callbacks ---------------
static void display(){
    draw_render();
    draw_dots();
    glutSwapBuffers();
}

static void reshape(int w,int h){
    WW=w; HH=h;                 // keep math in sync with window
    draw_reshape(w,h);
}

static void timer(int){
    const float dt   = 1.0f/120.0f;
    const float step = pac.speed * dt; // tiles per frame

    auto centered = [](float v){ return std::fabs(v - std::round(v)) < 1e-2f; };
    auto ddx = [](Dir d){ return d==LEFT?-1 : d==RIGHT?1 : 0; };
    auto ddy = [](Dir d){ return d==UP  ?-1 : d==DOWN ?1 : 0; };

    // center-turn + eat
    if(centered(pac.tx) && centered(pac.ty)){
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);

        // accept desired turn if open
        int wx = cx + ddx(pac.want);
        int wy = cy + ddy(pac.want);
        if(pac.want!=NONE && !is_wall(wx,wy)) pac.dir = pac.want;

        // eat pellet/energizer at center
        char &c = GRID[cy][cx];
        if(c=='.'){ c=' '; score += 10; --dots_left; }
        else if(c=='o'){ c=' '; score += 50; power_time = 6.0f; --dots_left; }
    }

    // decrement power timer
    if(power_time > 0.0f) power_time = std::max(0.0f, power_time - dt);

    // move toward next tile center if not blocked
    {
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);
        int nx = cx + ddx(pac.dir);
        int ny = cy + ddy(pac.dir);

        if(!is_wall(nx,ny)){
            float gx = (float)nx;  // goal tile center in tile coords
            float gy = (float)ny;
            float vx = gx - pac.tx;
            float vy = gy - pac.ty;
            float dist = std::sqrt(vx*vx + vy*vy);
            if(dist > 1e-6f){
                float adv = std::min(step, dist);
                pac.tx += (vx/dist) * adv;
                pac.ty += (vy/dist) * adv;
                if(adv >= dist - 1e-4f){ pac.tx=gx; pac.ty=gy; }
            }
        } else {
            // snap to center and wait for a legal turn
            pac.tx = (float)cx;
            pac.ty = (float)cy;
        }
    }

    // tunnel wrap on 'T' at centers
    if(centered(pac.tx) && centered(pac.ty)){
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);
        if(cy>=0 && cy<ROWS && MAZE_RAW[cy][cx]=='T'){
            if(cx==0 && pac.dir==LEFT)            pac.tx = (float)(COLS-1);
            else if(cx==COLS-1 && pac.dir==RIGHT) pac.tx = 0.0f;
        }
    }

    // update renderer (you prefer hardcoded -16,-16)
    draw_set_pac(px_from_tx(pac.tx)-16.0f, py_from_ty(pac.ty)-16.0f, pac.dir);

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

// --------------- Main ---------------
int main(int argc,char** argv){
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE);
    glutInitWindowSize(WW, HH);
    glutCreateWindow("Pac-Man");

    glewInit();
    if(!draw_init(WW, HH, "image/maze1.png", "image/sprites.png")) return 1;
    init_grid();
    // initial actors once
    float start_px = px_from_tx(pac.tx);
    float start_py = py_from_ty(pac.ty);
    draw_load_demo((int)start_px, (int)start_py, pac.dir);

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);
    glutSpecialFunc(specialKey);
    glutTimerFunc(1000/120, timer, 0);
    glutMainLoop();
    return 0;
}
