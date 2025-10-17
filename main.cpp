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
enum HudSide { HUD_LEFT=0, HUD_RIGHT=1 };
static HudSide g_hudSide = HUD_RIGHT;  // default: right of maze
int hiScore = 0;
enum Dir { UP=3, LEFT=2, DOWN=4, RIGHT=1, NONE=0 };

static bool g_paused = false;

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

enum GhostMode { SCATTER, CHASE, FRIGHTENED, EATEN };

struct Ghost {
    float tx=13, ty=11;   // tile position
    Dir   dir=LEFT;       // current direction
    Dir   last=LEFT;      // for reverse checks
    float speed=3.8f;     // tiles/sec (slightly slower than Pac)
    GhostMode mode=SCATTER;
    float fright_time=0.0f;   // countdown when frightened
    float mode_clock=0.0f;    // for scatter/chase cycling
};

// 0=Blinky,1=Pinky,2=Inky,3=Clyde (classic)
static Ghost ghosts[4];


static inline bool is_blocked(int tx,int ty){
    if(tx<0||tx>=COLS||ty<0||ty>=ROWS) return true;
    char c = MAZE_RAW[ty][tx];
    // Treat walls as blocked; keep the ghost house simple by blocking everything non-path
    return (c=='W');
}

static inline int manhattan(int ax,int ay,int bx,int by){
    return std::abs(ax-bx)+std::abs(ay-by);
}

static Dir opposite(Dir d){
    if(d==LEFT) return RIGHT;
    if(d==RIGHT) return LEFT;
    if(d==UP) return DOWN;
    if(d==DOWN) return UP;
    return NONE;
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

static void draw_dots(){
    const float r_small = cell()*0.12f;
    const float r_big   = cell()*0.32f;

    // choose colors
    const float normalR=1.0f, normalG=1.0f, normalB=1.0f;        // white
    const float powerR =1.0f, powerG =0.84f, powerB =0.0f;        // gold/yellow

    for(int y=0; y<ROWS; ++y){
        for(int x=0; x<COLS; ++x){
            char c = GRID[y][x];
            float px = px_from_tx((float)x);
            float py = py_from_ty((float)y);

            if(c=='.'){
                pellet(px, py, r_small); // white
            } else if(c=='o'){
                pellet_colored(px, py, r_big, powerR, powerG, powerB); // gold/yellow
            }
        }
    }
}
static void ghost_target_tile(int g, int& tx, int& ty){
    // Pac�s current center tile
    int pcx = (int)std::round(pac.tx);
    int pcy = (int)std::round(pac.ty);

    // scatter corners (roughly classic)
    const int cornerX[4] = { COLS-3,  2, COLS-3, 2 };
    const int cornerY[4] = {  2,     2, ROWS-3, ROWS-3 };

    Ghost& gh = ghosts[g];

    if(gh.mode==SCATTER){ tx = cornerX[g]; ty = cornerY[g]; return; }
    if(gh.mode==FRIGHTENED){
        // wander: pick a short target slightly away from Pac
        tx = pcx + (std::rand()%7 - 3);
        ty = pcy + (std::rand()%7 - 3);
        return;
    }
    if(gh.mode==EATEN){
        // send home (just pick the center above house so they don't get stuck)
        tx = COLS/2; ty = 11;
        return;
    }

    // CHASE per ghost
    switch(g){
        case 0: // Blinky � target Pac directly
            tx = pcx; ty = pcy; break;
        case 1: { // Pinky � 4 tiles ahead of Pac
            int f = 4;
            tx = pcx + dx(pac.dir)*f;
            ty = pcy + dy(pac.dir)*f;
        } break;
        case 2: { // Inky � reflect 2 tiles ahead of Pac around Blinky
            int pax = pcx + dx(pac.dir)*2;
            int pay = pcy + dy(pac.dir)*2;
            int bx, by; // Blinky�s tile
            bx = (int)std::round(ghosts[0].tx);
            by = (int)std::round(ghosts[0].ty);
            tx = pax + (pax - bx);
            ty = pay + (pay - by);
        } break;
        case 3: { // Clyde � chase if far, else scatter corner
            int dist = manhattan(pcx,pcy, (int)std::round(ghosts[3].tx), (int)std::round(ghosts[3].ty));
            if(dist >= 8){ tx = pcx; ty = pcy; }
            else { tx = cornerX[3]; ty = cornerY[3]; }
        } break;
    }

    // clamp target to grid to avoid overflow
    tx = std::clamp(tx, 0, COLS-1);
    ty = std::clamp(ty, 0, ROWS-1);
}
static Dir choose_dir(int g, int cx, int cy){
    Ghost& gh = ghosts[g];

    // valid directions (no reversing unless forced)
    Dir candidates[4] = {UP, LEFT, DOWN, RIGHT};
    Dir best = NONE;
    int bestScore = 1e9;

    int tx, ty; ghost_target_tile(g, tx, ty);

    int openCount = 0;
    for(Dir d : candidates){
        if(d==NONE) continue;
        if(opposite(d)==gh.dir) continue; // don't reverse unless only option

        int nx = cx + dx(d);
        int ny = cy + dy(d);
        if(!is_blocked(nx,ny)){
            ++openCount;
            // use squared distance (Manhattan is also fine)
            int score = (tx-nx)*(tx-nx) + (ty-ny)*(ty-ny);
            if(score < bestScore){ bestScore = score; best = d; }
        }
    }

    // If dead end or only reverse is possible, allow reverse
    if(best==NONE){
        for(Dir d : candidates){
            int nx = cx + dx(d);
            int ny = cy + dy(d);
            if(!is_blocked(nx,ny)){
                best = d; break;
            }
        }
    }

    // Frightened mode: pick a random legal direction (still avoid walls)
    if(ghosts[g].mode==FRIGHTENED){
        std::vector<Dir> legal;
        for(Dir d : candidates){
            if(d==NONE) continue;
            if(opposite(d)==gh.dir) continue;
            int nx = cx + dx(d), ny = cy + dy(d);
            if(!is_blocked(nx,ny)) legal.push_back(d);
        }
        if(!legal.empty()){
            best = legal[std::rand()%legal.size()];
        }
    }
    return best==NONE ? gh.dir : best;
}
static void init_ghosts(){
    // reasonable corridor starts (outside the house so they can roam)
    ghosts[0] = Ghost{13, 14, UP};   // Blinky
    ghosts[1] = Ghost{14, 14, UP};     // Pinky
    ghosts[2] = Ghost{12, 14, UP};     // Inky
    ghosts[3] = Ghost{15, 14, UP};     // Clyde

    // initial scatter
    for(int i=0;i<4;++i){
        ghosts[i].mode = SCATTER;
        ghosts[i].mode_clock = 0.0f;
        ghosts[i].fright_time = 0.0f;
        ghosts[i].speed = 3.8f; // tweak later
        // sync renderer right now
        draw_set_ghost(i, px_from_tx(ghosts[i].tx) - cell()*0.5f,
                  py_from_ty(ghosts[i].ty) - cell()*0.5f, ghosts[i].dir);
    }
}

static void reset_after_death(){
    // Reset Pac to spawn (do NOT reset score or dots here)
    pac.tx = 13; pac.ty = 23; pac.dir = UP; pac.want = RIGHT;

    // Reset ghosts to their starting tiles & modes
    ghosts[0] = Ghost{13, 14, LEFT, LEFT, 3.8f, SCATTER, 0.0f, 0.0f}; // Blinky
    ghosts[1] = Ghost{14, 14, UP,   UP,   3.8f, SCATTER, 0.0f, 0.0f}; // Pinky
    ghosts[2] = Ghost{12, 14, UP,   UP,   3.8f, SCATTER, 0.0f, 0.0f}; // Inky
    ghosts[3] = Ghost{15, 14, UP,   UP,   3.8f, SCATTER, 0.0f, 0.0f}; // Clyde

    // Sync renderer (Pac + all ghosts)
    draw_set_pac(px_from_tx(pac.tx) - cell()*0.5f,
                 py_from_ty(pac.ty) - cell()*0.5f,
                 pac.dir);

    for(int i=0;i<4;++i){
        draw_set_ghost(i,
            px_from_tx(ghosts[i].tx) - cell()*0.5f,
            py_from_ty(ghosts[i].ty) - cell()*0.5f,
            ghosts[i].dir);
    }
}


// --------------- GLUT callbacks ---------------
static void display(){

    draw_dots();
    draw_render();
    if(score > hiScore) hiScore = score;

    // --- HUD ---
    // --- Maze-anchored HUD (classic layout) ---
    // Grid width/height (replace with your actual names if different)
    const int GW = 28;//grid_w();   // or your constant, e.g., 28
    const int GH = 31;//grid_h();   // or your constant, e.g., 31

    // Maze bounds in window pixels.
    // left = center of tile 0 minus half-cell; right/top similar.
    // Convert tiles to pixels and compute maze rect regardless of Y direction
    const float y0 = py_from_ty(0);
    const float yN = py_from_ty(GH-1);
    const float topY    = std::max(y0, yN) + cell()*0.5f;
    const float bottomY = std::min(y0, yN) - cell()*0.5f;

    const float leftX  = px_from_tx(0)      - cell()*0.5f;
    const float rightX = px_from_tx(GW-1)   + cell()*0.5f;

    // Panel placement
    const float gap   = cell()*0.60f;    // spacing from maze border
    const float padX  = 8.0f;            // inner pad
    const float lineH = 18.0f;           // line height

    const bool hudRight = (g_hudSide==HUD_RIGHT);
    const float panelX  = hudRight ? (rightX + gap) : (leftX - gap);

    // Text helpers
    auto anchorX = [&](const char* s)->float {
        int w = draw_text_width(s);
        return hudRight ? (panelX + padX) : (panelX - padX - w);
    };

    // Strings
    char sOneUp[] = "1UP";
    char sHigh[]  = "HIGH SCORE";
    char sScore[32], sHi[32];
    std::snprintf(sScore, sizeof(sScore), "%d", score);
    std::snprintf(sHi,    sizeof(sHi),    "%d", hiScore);

    // Layout from top toward bottom
    float y = topY - 10.0f;

    draw_text_shadow(anchorX(sOneUp), y, sOneUp, 1,1,1);
    draw_text_shadow(anchorX(sScore), y - lineH, sScore, 1,1,1);

    y -= lineH*2.0f + 10.0f;

    draw_text_shadow(anchorX(sHigh), y, sHigh, 1,1,1);
    draw_text_shadow(anchorX(sHi),   y - lineH, sHi,   1,1,1);



    if(g_paused){
        draw_text(WW*0.5f - 40.0f, HH*0.5f, "PAUSED", 1.0f, 1.0f, 0.2f);
    }
    glutSwapBuffers();
}

static void reshape(int w,int h){
    WW=w; HH=h;                 // keep math in sync with window
    draw_reshape(w,h);
}

static void timer(int){
    const float dt   = 1.0f/120.0f;
    const float step = pac.speed * dt; // tiles per frame


    if(g_paused){
        // keep animations ticking if you like; or comment next line to fully freeze
        draw_update(dt);
        glutPostRedisplay();
        glutTimerFunc(1000/120, timer, 0);
        return;
    }



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
    draw_set_pac(px_from_tx(pac.tx) - cell()*0.5f,
             py_from_ty(pac.ty) - cell()*0.5f,
             pac.dir);

    // --- Update ghost modes (scatter/chase cycles) ---
    static const float cycleTimes[] = {7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f}; // simple cycle: S-C-S-C-S-C
    for(int i=0;i<4;++i){
        Ghost& gh = ghosts[i];

        // frightened comes from power pellets
        if(power_time > 0.0f && gh.mode != EATEN){
            gh.mode = FRIGHTENED;
            gh.fright_time = power_time;  // keep synced with Pac�s global
        } else if(gh.mode == FRIGHTENED && power_time <= 0.0f){
            // fall back to scatter/chase track
            gh.mode = (gh.mode_clock <= 7.0f || (gh.mode_clock>7.0f && gh.mode_clock<=14.0f) || (gh.mode_clock>27.0f && gh.mode_clock<=34.0f)) ? SCATTER : CHASE;
        }

        if(gh.mode != FRIGHTENED && gh.mode != EATEN){
            gh.mode_clock += dt;
            float t = gh.mode_clock;
            // very rough cycle mapping
            if(t <= 7.0f) gh.mode = SCATTER;
            else if(t <= 27.0f) gh.mode = CHASE;
            else if(t <= 34.0f) gh.mode = SCATTER;
            else gh.mode = CHASE;
        }

        // speed tweaks
        float s = gh.speed;
        if(gh.mode==FRIGHTENED) s *= 0.5f;
        if(gh.mode==EATEN)      s *= 1.5f;

        // movement like Pac: move center-to-center
        auto centered = [](float v){ return std::fabs(v - std::round(v)) < 1e-2f; };

        if(centered(gh.tx) && centered(gh.ty)){
            int cx = (int)std::round(gh.tx);
            int cy = (int)std::round(gh.ty);
            Dir nd = choose_dir(i, cx, cy);
            gh.dir = nd;
        }

        // advance toward next tile
        int cx = (int)std::round(gh.tx);
        int cy = (int)std::round(gh.ty);
        int nx = cx + dx(gh.dir);
        int ny = cy + dy(gh.dir);
        if(!is_blocked(nx,ny)){
            float gx = (float)nx;
            float gy = (float)ny;
            float vx = gx - gh.tx, vy = gy - gh.ty;
            float dist = std::sqrt(vx*vx + vy*vy);
            float adv = std::min(s*dt, dist);
            if(dist > 1e-6f){
                gh.tx += (vx/dist)*adv;
                gh.ty += (vy/dist)*adv;
                if(adv >= dist - 1e-4f){ gh.tx=gx; gh.ty=gy; }
            }
        } else {
            gh.tx = (float)cx; gh.ty = (float)cy; // wait at center
        }

        // Tunnel wrap for ghosts too (same as Pac)
        if(centered(gh.tx) && centered(gh.ty)){
            int gx = (int)std::round(gh.tx);
            int gy = (int)std::round(gh.ty);
            if(gy>=0 && gy<ROWS && MAZE_RAW[gy][gx]=='T'){
                if(gx==0 && gh.dir==LEFT)            gh.tx = (float)(COLS-1);
                else if(gx==COLS-1 && gh.dir==RIGHT) gh.tx = 0.0f;
            }
        }

        // EATEN -> when reaches �home� switch back to scatter
        if(gh.mode==EATEN){
            if((int)std::round(gh.tx)==COLS/2 && (int)std::round(gh.ty)==11){
                gh.mode = SCATTER;
            }
        }

        // Collision with Pac
        float dtx = gh.tx - pac.tx;
        float dty = gh.ty - pac.ty;
        if(dtx*dtx + dty*dty < 0.25f){ // within ~0.5 tile
            if(power_time > 0.0f && gh.mode!=EATEN){
                gh.mode = EATEN; // send to house
            } else if(gh.mode!=EATEN){
                // Pac dies (simple reset)
                //pac.tx=13; pac.ty=23; pac.dir=UP; pac.want=RIGHT;
                reset_after_death();
            }
        }

        // Tell renderer
        draw_set_ghost(i, px_from_tx(gh.tx) - cell()*0.5f,
                  py_from_ty(gh.ty) - cell()*0.5f, gh.dir);
    }



    draw_update(dt);
    glutPostRedisplay();
    glutTimerFunc(1000/120, timer, 0);
}


static void specialKey(int key,int,int){
    if(g_paused) return; // ignore arrows while paused
    if(key==GLUT_KEY_UP)    pac.want=UP;
    if(key==GLUT_KEY_DOWN)  pac.want=DOWN;
    if(key==GLUT_KEY_LEFT)  pac.want=LEFT;
    if(key==GLUT_KEY_RIGHT) pac.want=RIGHT;
}

static void reset_game(){
    // Clear render entities (so we don�t stack duplicates)
    draw_clear_entities();

    // Reset grid & counters
    init_grid();
    score       = 0;
    power_time  = 0.0f;

    // Reset Pac to struct defaults
    pac = Pac{};  // uses your default member initializers

    // Re-seed renderer just like startup
    float start_px = px_from_tx(pac.tx);
    float start_py = py_from_ty(pac.ty);
    draw_load_demo((int)start_px, (int)start_py, pac.dir);

    // Reset ghosts and sync their render state
    init_ghosts();

    // Make sure Pac�s sprite is also synced
    draw_set_pac(px_from_tx(pac.tx)-16.0f, py_from_ty(pac.ty)-16.0f, pac.dir);

    glutPostRedisplay();
}
static void keyDown(unsigned char key, int, int){
    if(key=='p' || key=='P'){ g_paused = !g_paused; glutPostRedisplay(); return; }
    if(key=='r' || key=='R'){ g_paused = false; reset_game(); return; }
    if(key=='h' || key=='H'){
        g_hudSide = (g_hudSide==HUD_LEFT)? HUD_RIGHT : HUD_LEFT;
        glutPostRedisplay();
        return;
    }
    if(key==27){ std::exit(0); }
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
    init_ghosts();


    glutDisplayFunc(display);
    glutReshapeFunc(reshape);

    glutSpecialFunc(specialKey);
    glutKeyboardFunc(keyDown);

    glutTimerFunc(1000/120, timer, 0);
    glutMainLoop();
    return 0;
}
