#include <GL/glew.h>
#include <GL/freeglut.h>
#include <vector>
#include <string>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <algorithm>
#include "draw.h"
#include "audio.h" // Audio
#include <queue>

static int WW = 226 * 3, HH = 248 * 2;

// --- Fullscreen state ---
static bool g_fullscreen = false;
static int g_windowX = 100, g_windowY = 100;
static int g_windowW = WW, g_windowH = HH;

// --- Countdown timer (gamewide) ---
static const int  g_timeLimitSec = 180;     // 3 minutes = 180 seconds
static float      g_timeLeftSec  = (float)g_timeLimitSec;
static bool       g_timerActive  = true;    // ticking when playing (not paused)


static void toggle_fullscreen()
{
#ifdef __FREEGLUT_EXT_H__
    glutFullScreenToggle();
#else
    if (!g_fullscreen)
    {
        g_fullscreen = true;
        glutFullScreen();
    }
    else
    {
        g_fullscreen = false;
        glutReshapeWindow(g_windowW, g_windowH);
        glutPositionWindow(g_windowX, g_windowY);
    }
#endif
}

// ===== Menu state =====
enum GameMode { MODE_MENU, MODE_PLAYING };
static GameMode g_mode = MODE_MENU;   // start in menu
static int g_menuSel = 0;             // 0..3 highlighted item

struct Rect { float x,y,w,h; };       // window-pixel coords (origin bottom-left)
static Rect g_btn[4];                 // Start, Resume, Restart, Quit

// Menu actions (don’t change order; we map labels from these)
enum MenuAction { ACT_START, ACT_RESUME, ACT_RESTART, ACT_QUIT,ACT_FULLWINDOW };

// Current menu composition for this frame
static MenuAction g_menuOrder[4];
static int g_menuCount = 0;



static int  g_highScore    = 0;      // persistent best
static bool g_highDirty    = false;  // changed this session (needs saving)
static const char* kHighFile = "highscore.dat";

// --- SFX paths (put the actual files in these locations or change the paths) ---
static const char *SFX_PELLET = "assets/sfx/pellet.wav";
static const char *SFX_POWER = "assets/sfx/pellet.wav";
static const char *SFX_EAT_GHOST = "assets/sfx/eat_ghost.wav";
static const char *SFX_DEATH = "assets/sfx/eyes_firstloop.wav";
static const char *SFX_INTERMISSION = "assets/sfx/intermission.wav";
static const char *SFX_ARCADE = "assets/sfx/arcade.wav";
enum HudSide
{
    HUD_LEFT = 0,
    HUD_RIGHT = 1
};
static HudSide g_hudSide = HUD_RIGHT; // default: right of maze

enum Dir
{
    UP = 3,
    LEFT = 2,
    DOWN = 4,
    RIGHT = 1,
    NONE = 0
};

static bool g_paused = false;
// Lives & game over state
static int g_lives = 3;
static bool g_gameOver = false;

// Prevent multiple life losses from the same overlap/frame
static int g_deathCooldown = 0; // frames to ignore collisions after a death

// Forward declarations for life handling
static void reset_after_death();
static void lose_life();

struct Pac
{
    float tx = 13, ty = 23; // tile coords
    Dir dir = UP, want = RIGHT;
    float speed = 6.0f; // tiles per second
} pac;

// ---------------- Map ----------------
static constexpr int COLS = 28, ROWS = 31;
static const char *MAZE_RAW[ROWS] = {
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
    "WWWWWW.WW WWW  WWW WW.WWWWWW",
    "WWWWWW.WW W      W WW.WWWWWW",
    "T     .   W      W   .     T",
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
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWW"};

static char GRID[ROWS][COLS]; // copy of MAZE_RAW you can edit
static int score = 0;
static int dots_left = 0;
float power_time = 0.0f; // seconds of energizer effect
static int g_eatStreak = 0;

// --- Tiny score popups when eating frightened ghosts ---
struct ScorePopup {
    float x_px;    // screen pixel position (already converted)
    float y_px;
    int   points;  // 200, 400, 800, 1600
    float age;     // seconds since spawn
};
static std::vector<ScorePopup> g_popups;

static constexpr float POPUP_LIFETIME = 1.00f; // seconds on screen
static constexpr float POPUP_RISE_PX  = 24.0f; // how far it floats upward


static void init_grid()
{
    dots_left = 0;
    for (int y = 0; y < ROWS; ++y)
    {
        for (int x = 0; x < COLS; ++x)
        {
            char c = MAZE_RAW[y][x];
            GRID[y][x] = c;
            if (c == '.' || c == 'o')
                ++dots_left;
        }
    }
}

enum GhostMode
{
    SCATTER,
    CHASE,
    FRIGHTENED,
    EATEN
};

struct Ghost
{
    float tx = 13, ty = 11; // tile position
    Dir dir = LEFT;         // current direction
    Dir last = LEFT;        // for reverse checks
    float speed = 3.8f;     // tiles/sec (slightly slower than Pac)
    GhostMode mode = SCATTER;
    float fright_time = 0.0f; // countdown when frightened
    float mode_clock = 0.0f;  // for scatter/chase cycling
};

// 0=Blinky,1=Pinky,2=Inky,3=Clyde (classic)
static Ghost ghosts[4];

static inline bool is_blocked(int tx, int ty)
{
    if (tx < 0 || tx >= COLS || ty < 0 || ty >= ROWS)
        return true;
    char c = MAZE_RAW[ty][tx];
    // Treat walls as blocked; keep the ghost house simple by blocking everything non-path
    return (c == 'W');
}

// Zero-pad to 6 digits like classic cabinets (caps at 999999)
static inline void fmt_score6(int v, char *out, size_t n)
{
    if (v < 0) v = 0;
    if (v > 999999) v = 999999;
    std::snprintf(out, n, "%06d", v);
}
static inline void fmt_time_mmss(int sec, char* out, size_t n) {
    if (sec < 0) sec = 0;
    int m = sec / 60;
    int s = sec % 60;
    std::snprintf(out, n, "%02d:%02d", m, s);
}


static inline int manhattan(int ax, int ay, int bx, int by)
{
    return std::abs(ax - bx) + std::abs(ay - by);
}

static Dir opposite(Dir d)
{
    if (d == LEFT)
        return RIGHT;
    if (d == RIGHT)
        return LEFT;
    if (d == UP)
        return DOWN;
    if (d == DOWN)
        return UP;
    return NONE;
}

// --------------- Pixel helpers ---------------
static inline float cell() { return std::floor(std::min(WW / (float)COLS, HH / (float)ROWS)); }
static inline float offX() { return 0.5f * (WW - cell() * COLS); }
static inline float offY() { return 0.5f * (HH - cell() * ROWS); }
static inline float px_from_tx(float tx) { return offX() + tx * cell() + cell() * 0.5f; }
static inline float py_from_ty(float ty) { return HH - (offY() + ty * cell() + cell() * 0.5f); }

// --------------- Maze helpers ---------------
static inline bool is_wall(int tx, int ty)
{
    if (tx < 0 || tx >= COLS || ty < 0 || ty >= ROWS)
        return true;
    return MAZE_RAW[ty][tx] == 'W';
}
static inline int dx(Dir d) { return d == LEFT ? -1 : d == RIGHT ? 1
                                                                 : 0; }
static inline int dy(Dir d) { return d == UP ? -1 : d == DOWN ? 1
                                                              : 0; }
static inline bool at_center(float x) { return std::fabs(x - std::round(x)) < 1e-3f; }

// optional wrap on tunnel tiles 'T'
static inline void tunnel_wrap()
{
    int cx = (int)std::round(pac.tx);
    int cy = (int)std::round(pac.ty);
    if (cy < 0 || cy >= ROWS)
        return;
    if (MAZE_RAW[cy][cx] == 'T' && at_center(pac.tx) && at_center(pac.ty))
    {
        if (cx == 0 && pac.dir == LEFT)
            pac.tx = (float)(COLS - 1);
        else if (cx == COLS - 1 && pac.dir == RIGHT)
            pac.tx = 0.0f;
    }
}



static void load_high_score()
{
    std::ifstream in(kHighFile, std::ios::binary);
    if (!in) { g_highScore = 0; return; }

    int v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (in && v >= 0 && v < 100000000) g_highScore = v;  // sanity check
}

static void save_high_score()
{
    std::ofstream out(kHighFile, std::ios::binary | std::ios::trunc);
    if (!out) return;
    out.write(reinterpret_cast<const char*>(&g_highScore), sizeof(g_highScore));
}

static inline void try_update_high(int currentScore)
{
    if (currentScore > g_highScore) { g_highScore = currentScore; g_highDirty = true; }
}

static void spawn_score_popup_at_tile(float tx, float ty, int pts)
{
    ScorePopup p;
    p.x_px  = px_from_tx(tx);   // convert tile space to pixel coords
    p.y_px  = py_from_ty(ty);
    p.points = pts;
    p.age    = 0.0f;
    g_popups.push_back(p);
}


// --------------- Dots ---------------

static void draw_dots()
{
    const float r_small = cell() * 0.12f;
    const float r_big = cell() * 0.32f;

    // choose colors
    const float normalR = 1.0f, normalG = 1.0f, normalB = 1.0f; // white
    const float powerR = 1.0f, powerG = 0.84f, powerB = 0.0f;   // gold/yellow

    for (int y = 0; y < ROWS; ++y)
    {
        for (int x = 0; x < COLS; ++x)
        {
            char c = GRID[y][x];
            float px = px_from_tx((float)x);
            float py = py_from_ty((float)y);

            if (c == '.')
            {
                pellet(px, py, r_small); // white
            }
            else if (c == 'o')
            {
                pellet_colored(px, py, r_big, powerR, powerG, powerB); // gold/yellow
            }
        }
    }
}
static void ghost_target_tile(int g, int &tx, int &ty)
{
    // Pac�s current center tile
    int pcx = (int)std::round(pac.tx);
    int pcy = (int)std::round(pac.ty);

    // scatter corners (roughly classic)
    const int cornerX[4] = {COLS - 3, 2, COLS - 3, 2};
    const int cornerY[4] = {2, 2, ROWS - 3, ROWS - 3};

    Ghost &gh = ghosts[g];

    if (gh.mode == SCATTER)
    {
        tx = cornerX[g];
        ty = cornerY[g];
        return;
    }
    if (gh.mode == FRIGHTENED)
    {
        // wander: pick a short target slightly away from Pac
        tx = pcx + (std::rand() % 7 - 3);
        ty = pcy + (std::rand() % 7 - 3);
        return;
    }
    if (gh.mode == EATEN)
    {
        // send home (just pick the center above house so they don't get stuck)
        tx = COLS / 2;
        ty = 13;
        return;
    }

    // CHASE per ghost
    switch (g)
    {
    case 0: // Blinky � target Pac directly
        tx = pcx;
        ty = pcy;
        break;
    case 1:
    { // Pinky � 4 tiles ahead of Pac
        int f = 4;
        tx = pcx + dx(pac.dir) * f;
        ty = pcy + dy(pac.dir) * f;
    }
    break;
    case 2:
    { // Inky � reflect 2 tiles ahead of Pac around Blinky
        int pax = pcx + dx(pac.dir) * 2;
        int pay = pcy + dy(pac.dir) * 2;
        int bx, by; // Blinky�s tile
        bx = (int)std::round(ghosts[0].tx);
        by = (int)std::round(ghosts[0].ty);
        tx = pax + (pax - bx);
        ty = pay + (pay - by);
    }
    break;
    case 3:
    { // Clyde � chase if far, else scatter corner
        int dist = manhattan(pcx, pcy, (int)std::round(ghosts[3].tx), (int)std::round(ghosts[3].ty));
        if (dist >= 8)
        {
            tx = pcx;
            ty = pcy;
        }
        else
        {
            tx = cornerX[3];
            ty = cornerY[3];
        }
    }
    break;
    }

    // clamp target to grid to avoid overflow
    tx = std::clamp(tx, 0, COLS - 1);
    ty = std::clamp(ty, 0, ROWS - 1);
}
static Dir choose_dir(int g, int cx, int cy)
{
    Ghost &gh = ghosts[g];

    // 1) Frightened: keep your random wandering (avoid reverse if possible)
    if (gh.mode == FRIGHTENED)
    {
        Dir candidates[4] = {UP, LEFT, DOWN, RIGHT};
        std::vector<Dir> legal;
        for (Dir d : candidates)
        {
            if (d == NONE)
                continue;
            if (opposite(d) == gh.dir)
                continue;

            int nx = cx + dx(d);
            int ny = cy + dy(d);

            // Tunnel wrap from edge 'T'
            if (MAZE_RAW[cy][cx] == 'T')
            {
                if (cx == 0 && d == LEFT)
                    nx = COLS - 1;
                else if (cx == COLS - 1 && d == RIGHT)
                    nx = 0;
            }

            if (!is_blocked(nx, ny))
                legal.push_back(d);
        }
        if (!legal.empty())
            return legal[std::rand() % legal.size()];
        // if no non-reverse exits, we'll fall through and allow reverse via BFS fallback
    }

    // 2) Compute target normally
    int tx, ty;
    ghost_target_tile(g, tx, ty);

    // If already at target, try to continue straight if possible
    if (cx == tx && cy == ty)
    {
        int nx = cx + dx(gh.dir);
        int ny = cy + dy(gh.dir);
        if (!is_blocked(nx, ny))
            return gh.dir;
    }

    // 3) BFS shortest path from (cx,cy) to (tx,ty), respecting tunnel wrap and
    //    "avoid reversing unless it’s the only way out" at the start tile.
    struct P
    {
        short x, y;
    };
    bool visited[ROWS][COLS] = {};
    P parent[ROWS][COLS];
    for (int y = 0; y < ROWS; ++y)
        for (int x = 0; x < COLS; ++x)
            parent[y][x] = {-1, -1};

    auto in_bounds = [](int x, int y)
    {
        return x >= 0 && x < COLS && y >= 0 && y < ROWS;
    };

    auto enqueue = [&](int x, int y, int px, int py, std::queue<P> &q)
    {
        if (!in_bounds(x, y))
            return;
        if (visited[y][x])
            return;
        if (is_blocked(x, y))
            return;
        visited[y][x] = true;
        parent[y][x] = {(short)px, (short)py};
        q.push({(short)x, (short)y});
    };

    std::queue<P> q;
    visited[cy][cx] = true;
    parent[cy][cx] = {-1, -1};
    q.push({(short)cx, (short)cy});

    Dir rev = opposite(gh.dir);

    auto push_neighbors = [&](int x, int y)
    {
        const Dir order[4] = {UP, LEFT, DOWN, RIGHT}; // classic tie-break: U,L,D,R
        const bool atTunnel = (MAZE_RAW[y][x] == 'T');
        const bool isRoot = (x == cx && y == cy);

        // Count non-reverse options at the root
        int nonRevCount = 0;
        if (isRoot)
        {
            for (Dir d : order)
            {
                if (d == rev)
                    continue;
                int nx = x + dx(d), ny = y + dy(d);
                if (atTunnel)
                {
                    if (x == 0 && d == LEFT)
                        nx = COLS - 1;
                    else if (x == COLS - 1 && d == RIGHT)
                        nx = 0;
                }
                if (in_bounds(nx, ny) && !is_blocked(nx, ny))
                    ++nonRevCount;
            }
        }

        for (Dir d : order)
        {
            if (isRoot && nonRevCount > 0 && d == rev)
                continue; // avoid reverse unless forced
            int nx = x + dx(d), ny = y + dy(d);
            if (atTunnel)
            {
                if (x == 0 && d == LEFT)
                    nx = COLS - 1;
                else if (x == COLS - 1 && d == RIGHT)
                    nx = 0;
            }
            enqueue(nx, ny, x, y, q);
        }
    };

    // BFS loop
    while (!q.empty())
    {
        P p = q.front();
        q.pop();
        if (p.x == tx && p.y == ty)
            break;
        push_neighbors(p.x, p.y);
    }

    // 4) If unreachable (shouldn’t happen on a valid maze), fall back to a simple legal move
    if (!visited[ty][tx])
    {
        // try straight
        int nx = cx + dx(gh.dir), ny = cy + dy(gh.dir);
        if (!is_blocked(nx, ny))
            return gh.dir;

        // try any non-reverse legal
        const Dir order[4] = {UP, LEFT, DOWN, RIGHT};
        for (Dir d : order)
        {
            if (d == rev)
                continue;
            nx = cx + dx(d);
            ny = cy + dy(d);
            if (!is_blocked(nx, ny))
                return d;
        }
        // must reverse
        return rev != NONE ? rev : gh.dir;
    }

    // 5) Reconstruct first step from (cx,cy) toward (tx,ty)
    int rx = tx, ry = ty;
    while (!(parent[ry][rx].x == cx && parent[ry][rx].y == cy))
    {
        P pr = parent[ry][rx];
        if (pr.x == -1 && pr.y == -1)
            break; // safety
        rx = pr.x;
        ry = pr.y;
    }

    if (rx > cx)
        return RIGHT;
    if (rx < cx)
        return LEFT;
    if (ry > cy)
        return DOWN;
    if (ry < cy)
        return UP;
    return gh.dir; // fallback
}
static void init_ghosts()
{
    // reasonable corridor starts (outside the house so they can roam)
    ghosts[0] = Ghost{14, 14, UP};    // Blinky
    ghosts[1] = Ghost{13, 14, LEFT};  // Pinky
    ghosts[2] = Ghost{12, 14, RIGHT}; // Inky
    ghosts[3] = Ghost{15, 14, UP};    // Clyde

    // initial scatter
    for (int i = 0; i < 4; ++i)
    {
        ghosts[i].mode = SCATTER;
        ghosts[i].mode_clock = 0.0f;
        ghosts[i].fright_time = 0.0f;
        ghosts[i].speed = 3.8f; // tweak later
        // sync renderer right now
        // draw_set_ghost(i, px_from_tx(ghosts[i].tx) - cell() * 0.5f,py_from_ty(ghosts[i].ty) - cell() * 0.5f, ghosts[i].dir);
        draw_set_ghost_state(i, px_from_tx(ghosts[i].tx) - cell() * 0.5f,
                             py_from_ty(ghosts[i].ty) - cell() * 0.5f,
                             ghosts[i].dir,
                             (int)ghosts[i].mode);
    }
}

static void reset_after_death()
{
    // Reset Pac to spawn (do NOT reset score or dots here)
    pac.tx = 13;
    pac.ty = 23;
    pac.dir = UP;
    pac.want = RIGHT;
    power_time = 0.0f;
    g_eatStreak = 0;

    // Reset ghosts to their starting tiles & modes
    ghosts[0] = Ghost{14, 14, UP, UP, 3.8f, SCATTER, 0.0f, 0.0f};       // Blinky
    ghosts[1] = Ghost{13, 14, LEFT, LEFT, 3.8f, SCATTER, 0.0f, 0.0f};   // Pinky
    ghosts[2] = Ghost{12, 14, RIGHT, RIGHT, 3.8f, SCATTER, 0.0f, 0.0f}; // Inky
    ghosts[3] = Ghost{15, 14, UP, UP, 3.8f, SCATTER, 0.0f, 0.0f};       // Clyde

    // Sync renderer (Pac + all ghosts)
    draw_set_pac(px_from_tx(pac.tx) - cell() * 0.5f,
                 py_from_ty(pac.ty) - cell() * 0.5f,
                 pac.dir);

    for (int i = 0; i < 4; ++i)
    {
        draw_set_ghost(i,
                       px_from_tx(ghosts[i].tx) - cell() * 0.5f,
                       py_from_ty(ghosts[i].ty) - cell() * 0.5f,
                       ghosts[i].dir);
    }
}

static void lose_life()
{
    g_eatStreak = 0;
    if (g_gameOver)
        return; // already game over; ignore

    if (g_lives > 1)
    {
        --g_lives;            // lose exactly ONE life
        reset_after_death();  // snap Pac & ghosts back to start tiles
        g_deathCooldown = 60; // ~1 sec @60 FPS (adjust if needed)
        // do not pause; gameplay resumes
    }
    else
    {
        g_lives = 0;
        g_gameOver = true;
        g_paused = true; // freeze gameplay on Game Over
        // Play game-over sound here
        try_update_high(score);
        if (g_highDirty) { save_high_score(); g_highDirty = false; }
        audio_play(SFX_INTERMISSION);
    }
}



static void reset_game()
{
    // Clear render entities (so we don�t stack duplicates)
    draw_clear_entities();
    g_timeLeftSec = (float)g_timeLimitSec;
    g_timerActive = true;

    // Reset grid & counters
    init_grid();
    score = 0;
    power_time = 0.0f;
    g_eatStreak = 0;

    g_gameOver = false;


    // Reset Pac to struct defaults
    pac = Pac{}; // uses your default member initializers

    // Re-seed renderer just like startup
    float start_px = px_from_tx(pac.tx);
    float start_py = py_from_ty(pac.ty);
    draw_load_demo((int)start_px, (int)start_py, pac.dir);

    // Reset ghosts and sync their render state
    init_ghosts();

    // Make sure Pac�s sprite is also synced
    draw_set_pac(px_from_tx(pac.tx) - 16.0f, py_from_ty(pac.ty) - 16.0f, pac.dir);

    glutPostRedisplay();
}

static void layout_menu(int n)
{
    const float bw = std::min(360.0f, WW * 0.5f);
    const float bh = 42.0f;
    const float gap = 12.0f;

    const float cx = WW * 0.5f;
    const float cy = HH * 0.55f;

    for (int i=0; i<n; ++i){
        g_btn[i].w = bw;
        g_btn[i].h = bh;
        g_btn[i].x = cx - bw * 0.5f;
        g_btn[i].y = cy - (i * (bh + gap));
    }
}


static inline bool pt_in_rect(float x,float y,const Rect& r){
    return (x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h);
}

static void draw_panel(const Rect& r, bool hot)
{
    // simple filled quad + border
    glDisable(GL_TEXTURE_2D);
    // background (slightly darker when not selected)
    glColor4f(0.f, 0.f, 0.f, hot ? 0.55f : 0.35f);
    glBegin(GL_QUADS);
      glVertex2f(r.x,       r.y);
      glVertex2f(r.x+r.w,   r.y);
      glVertex2f(r.x+r.w,   r.y+r.h);
      glVertex2f(r.x,       r.y+r.h);
    glEnd();
    // outline
    glColor4f(hot ? 1.f : 0.7f, hot ? 1.f : 0.7f, hot ? 1.f : 0.7f, 1.f);
    glBegin(GL_LINE_LOOP);
      glVertex2f(r.x,       r.y);
      glVertex2f(r.x+r.w,   r.y);
      glVertex2f(r.x+r.w,   r.y+r.h);
      glVertex2f(r.x,       r.y+r.h);
    glEnd();
    glEnable(GL_TEXTURE_2D);
}

static void rebuild_menu()
{
    g_menuCount = 0;

    if (g_paused) {
        // Paused by user -> Resume, Restart, Quit
        g_menuOrder[g_menuCount++] = ACT_RESUME;
        g_menuOrder[g_menuCount++] = ACT_RESTART;
        g_menuOrder[g_menuCount++] = ACT_QUIT;
    } else {
        // Not paused -> Start, Quit
        g_menuOrder[g_menuCount++] = ACT_START;
        g_menuOrder[g_menuCount++] = ACT_QUIT;
    }

    // clamp selection
    if (g_menuSel >= g_menuCount) g_menuSel = g_menuCount - 1;
    if (g_menuSel < 0) g_menuSel = 0;
}


static const char* action_label(MenuAction a){
    switch(a){
        case ACT_START:   return "Start";
        case ACT_RESUME:  return "Resume";
        case ACT_RESTART: return "Restart";
        case ACT_QUIT:    return "Quit";
    }
    return "";
}

static void draw_menu()
{
    rebuild_menu();       // ensure correct buttons for current state
    layout_menu(g_menuCount);

    // darken background (you already made it darker)
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.f, 0.f, 0.f, 0.75f);
    glBegin(GL_QUADS);
      glVertex2f(0, 0);   glVertex2f(WW, 0);
      glVertex2f(WW, HH); glVertex2f(0, HH);
    glEnd();
    glEnable(GL_TEXTURE_2D);



    // Big bold title (your stroke helper)




    const float cx     = WW * 0.5f;  // center x
    const float yLabel = HH * 0.19f; // tweak these 2 lines to move up/down
    const float yValue = yLabel - 56.0f;

    const float labelY = HH * 0.19f;
    const float valueY = labelY - 26.0f;

    draw_title_centered(WW * 0.5f, HH * 0.78f, "PAC-MAN", 72.0f, 1.0f, 1.0f, 0.2f);
    char hiBuf[16];
    std::snprintf(hiBuf, sizeof(hiBuf), "%06d", std::min(g_highScore, 999999));

    // add ~6–10 px of extra spacing between letters
    draw_title_centered_spaced(cx, labelY, "HIGH SCORE", 28.0f,
                           0.85f, 0.90f, 1.0f, /*tracking_px=*/8.0f);

    draw_title_centered(cx, yValue, hiBuf,      36.0f, 0.53f, 0.81f, 0.98f);  // sky blue digits

    for (int i=0; i<g_menuCount; ++i){
        Rect r = g_btn[i];
        const bool hot = (i == g_menuSel);
        draw_panel(r, hot);

        const char* s = action_label(g_menuOrder[i]);
        int tw = draw_text_width(s);
        float tx = r.x + (r.w - tw) * 0.5f;
        float ty = r.y + (r.h - 15.f) * 0.5f + 4.f;
        draw_text_shadow(tx, ty, s, 1,1,1);
    }


}


static void menu_activate(MenuAction act)
{
    switch(act){
        case ACT_START:
            g_gameOver = false;
            g_lives = 3;
            g_paused = false;
            reset_game();
            g_mode = MODE_PLAYING;
            break;

        case ACT_RESUME:
            if(!g_gameOver){
                g_paused = false;
                g_mode = MODE_PLAYING;
            }
            break;

        case ACT_RESTART:
            g_gameOver = false;
            g_lives = 3;
            g_paused = false;
            reset_game();
            g_mode = MODE_PLAYING;
            break;
        case ACT_FULLWINDOW:
            toggle_fullscreen();
            glutPostRedisplay();

            break;

        case ACT_QUIT:
            std::exit(0);
            break;
    }
}






// --------------- GLUT callbacks ---------------
static void display()
{

    draw_dots();
    draw_render();



    // --- Floating score popups (draw on top of maze/entities) ---
    for (const auto &p : g_popups) {
        float t = std::min(std::max(p.age / POPUP_LIFETIME, 0.0f), 1.0f);
        float y = p.y_px - POPUP_RISE_PX * t; // rise up over time

        char buf[16];
        std::snprintf(buf, sizeof(buf), "+%d", p.points);

        // center text horizontally at x_px
        int tw = draw_text_width(buf);   // provided by draw.cpp
        float x = p.x_px - tw * 0.5f;

        // use your existing shadowed bitmap text (white looks nice here)
        // sky blue color (RGB)
        draw_text_shadow(x, y, buf, 0.53f, 0.81f, 0.98f);  // light sky blue

    }


    // --- HUD ---
    // --- Maze-anchored HUD (classic layout) ---
    const int GW = 28;
    const int GH = 31;
    const float hudYOffset = 30.0f;

    // Maze bounds in window pixels
    const float y0 = py_from_ty(0);
    const float yN = py_from_ty(GH - 1);
    const float topY = std::max(y0, yN) + cell() * 0.5f;
    const float bottomY = std::min(y0, yN) - cell() * 0.5f;

    const float leftX  = px_from_tx(0)      - cell() * 0.5f;
    const float rightX = px_from_tx(GW - 1) + cell() * 0.5f;

    // Panel placement
    const float gap   = cell() * 0.60f;
    const float padX  = 8.0f;
    const float lineH = 18.0f;

    const bool  hudRight = (g_hudSide == HUD_RIGHT);
    const float panelX   = hudRight ? (rightX + gap) : (leftX - gap);

    // Text helper
    auto anchorX = [&](const char *s) -> float {
        int w = draw_text_width(s);
        return hudRight ? (panelX + padX) : (panelX - padX - w);
    };

    // Labels
    char sOneUp[] = "1UP";
    char sHigh[]  = "HIGH SCORE";

    char sScore[32], sHi[32];
    fmt_score6(score,      sScore, sizeof(sScore));
    fmt_score6(g_highScore, sHi,   sizeof(sHi));

    // NEW: detect “new high” (this frame) for a subtle highlight
    const bool isNewHigh = (score >= g_highScore && g_highScore > 0);

    // Layout from top toward bottom
    float y = topY - 10.0f - hudYOffset;

    // Left/Right header column
    draw_text_shadow(anchorX(sOneUp), y, sOneUp, 1.0f, 1.0f, 1.0f);
    // Current score (warm tint)
    draw_text_shadow(anchorX(sScore), y - lineH, sScore, 1.00f, 0.95f, 0.70f);

    // Spacer
    y -= lineH * 2.0f + 10.0f;

    // High score header (cool tint)
    draw_text_shadow(anchorX(sHigh), y, sHigh, 0.80f, 0.90f, 1.00f);

    // High score value
    // If you just beat it, flash in sky blue this frame.
    const float hx = anchorX(sHi);
    const float hy = y - lineH;
    if (isNewHigh) {
        // sky blue (to match your popups)
        draw_text_shadow(hx, hy, sHi, 0.53f, 0.81f, 0.98f);
    } else {
        draw_text_shadow(hx, hy, sHi, 1.0f, 1.0f, 1.0f);
    }

    char sTimeLbl[] = "TIME";
    char sTime[16];

    // ceil so 0.4s shows as the last “1” visually
    int secs = (int)std::ceil(g_timeLeftSec);
    fmt_time_mmss(secs, sTime, sizeof(sTime));

    // layout
    y -= lineH * 2.0f + 10.0f;  // move down a block (same pattern as above)
    draw_text_shadow(anchorX(sTimeLbl), y, sTimeLbl, 1, 1, 1);

    // Warning color under 10s
    float tr = 1.0f, tg = 1.0f, tb = 1.0f;
    if (g_timeLeftSec <= 10.0f) {
        double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
        if (std::fmod(t, 0.5) < 0.25) { tr = 1, tg = 1, tb = 1; } // flash white
    }

    draw_text_shadow(anchorX(sTime), y - lineH, sTime, tr, tg, tb);

    // Lives line (kept as-is)
    const int lives = std::max(0, g_lives);

    const float iconScale = 1.0f;   // 1.0 = one tile size
    const float ts        = cell(); // tile size in px
    const float spacing   = ts * 1.6f;
    const float hudLivesYOffset = 20.0f;   // try 20–36 px

    // OLD:
    // const float yIcons = (y - lineH * 2.0f) + 6.0f;

    // NEW (lowered by hudLivesYOffset):
    const float yIcons = (y - lineH * 2.0f) + 6.0f - hudLivesYOffset;
    float totalW = lives * spacing;
    float startX = hudRight
        ? (panelX + padX + ts * 0.5f)
        : (panelX - padX - totalW + ts * 0.5f);

    int iconDir = hudRight ? 1 : 2; // face inward if you want symmetry

    for (int i = 0; i < lives; ++i) {
        float cx = startX + i * spacing;
        draw_hud_pac_icon(cx, yIcons, iconScale, iconDir);
    }


    // Game Over overlay
    if (g_gameOver)
    {
        draw_text(WW * 0.5f - 50.0f, HH * 0.5f, "GAME OVER", 1.0f, 0.3f, 0.3f);
    }


    if (g_mode == MODE_MENU) {
        draw_menu();
    }
    glutSwapBuffers();
}

static void reshape(int w, int h)
{
    WW = w;
    HH = h; // keep math in sync with window
    draw_reshape(w, h);
}

static void timer(int)
{
    const float dt = 1.0f / 120.0f;
    const float step = pac.speed * dt; // tiles per frame

    // --- Countdown update ---
    if (!g_paused && !g_gameOver && g_timerActive) {
        g_timeLeftSec -= dt;
        if (g_timeLeftSec < 0.0f) g_timeLeftSec = 0.0f;

        if (g_timeLeftSec <= 0.0f) {
            // time up -> game over
            g_gameOver = true;
            g_paused   = true;

            // capture high score if it’s a new best
            try_update_high(score);
            if (g_highDirty) { save_high_score(); g_highDirty = false; }

            // optional sound (use an existing one if you don’t have a timeout sfx)
            // audio_play(SFX_TIMEOUT);
            // or: audio_play(SFX_DEATH);
        }
    }


    // --- Update floating score popups ---
    for (auto &p : g_popups) {
        p.age += dt;
    }
    // remove expired
    g_popups.erase(
                std::remove_if(g_popups.begin(), g_popups.end(),
                   [](const ScorePopup& p){ return p.age >= POPUP_LIFETIME; }),
                g_popups.end());



    if (g_mode == MODE_MENU) {
        // Keep tiny animation (mouth, ghost blink) while paused
        draw_update(1.0f/120.0f);
        glutPostRedisplay();
        glutTimerFunc(1000/120, timer, 0);
        return;
    }


    // death cooldown tick
    if (g_deathCooldown > 0)
        --g_deathCooldown;

    if (g_paused)
    {
        // keep animations ticking if you like; or comment next line to fully freeze
        draw_update(dt);
        glutPostRedisplay();
        glutTimerFunc(1000 / 120, timer, 0);
        return;
    }

    auto centered = [](float v)
    { return std::fabs(v - std::round(v)) < 1e-2f; };
    auto ddx = [](Dir d)
    { return d == LEFT ? -1 : d == RIGHT ? 1
                                         : 0; };
    auto ddy = [](Dir d)
    { return d == UP ? -1 : d == DOWN ? 1
                                      : 0; };

    // center-turn + eat
    if (centered(pac.tx) && centered(pac.ty))
    {
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);

        // accept desired turn if open
        int wx = cx + ddx(pac.want);
        int wy = cy + ddy(pac.want);
        if (pac.want != NONE && !is_wall(wx, wy))
            pac.dir = pac.want;

        // eat pellet/energizer at center
        char &c = GRID[cy][cx];
        if (c == '.')
        {
            c = ' ';
            score += 10;
            try_update_high(score);
            if(--dots_left<= 0) {
                    g_gameOver = true;
                    g_paused   = true;
                    try_update_high(score);       // make sure best is captured
                    if (g_highDirty) { save_high_score(); g_highDirty = false; }

            }

            audio_play(SFX_ARCADE); // <<< sound: small dot
        }
        else if (c == 'o')
        {
            c = ' ';
            score += 50;
            try_update_high(score);
            power_time = 6.0f;
            g_eatStreak = 0;
            if(--dots_left<= 0) {
                    g_gameOver = true;
                    g_paused   = true;
                    try_update_high(score);       // make sure best is captured
                    if (g_highDirty) { save_high_score(); g_highDirty = false; }

            }
            audio_play(SFX_POWER); // <<< sound: power-up (energizer)
        }
    }


    // decrement power timer
    if (power_time > 0.0f)
        power_time = std::max(0.0f, power_time - dt);

    static bool wasPowered = false;
    if (wasPowered && power_time <= 0.0f) {
        g_eatStreak = 0;
    }
    wasPowered = (power_time > 0.0f);

    // move toward next tile center if not blocked
    {
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);
        int nx = cx + ddx(pac.dir);
        int ny = cy + ddy(pac.dir);

        if (!is_wall(nx, ny))
        {
            float gx = (float)nx; // goal tile center in tile coords
            float gy = (float)ny;
            float vx = gx - pac.tx;
            float vy = gy - pac.ty;
            float dist = std::sqrt(vx * vx + vy * vy);
            if (dist > 1e-6f)
            {
                float adv = std::min(step, dist);
                pac.tx += (vx / dist) * adv;
                pac.ty += (vy / dist) * adv;
                if (adv >= dist - 1e-4f)
                {
                    pac.tx = gx;
                    pac.ty = gy;
                }
            }
        }
        else
        {
            // snap to center and wait for a legal turn
            pac.tx = (float)cx;
            pac.ty = (float)cy;
        }
    }

    // tunnel wrap on 'T' at centers
    if (centered(pac.tx) && centered(pac.ty))
    {
        int cx = (int)std::round(pac.tx);
        int cy = (int)std::round(pac.ty);
        if (cy >= 0 && cy < ROWS && MAZE_RAW[cy][cx] == 'T')
        {
            if (cx == 0 && pac.dir == LEFT)
                pac.tx = (float)(COLS - 1);
            else if (cx == COLS - 1 && pac.dir == RIGHT)
                pac.tx = 0.0f;
        }
    }

    // update renderer (you prefer hardcoded -16,-16)
    draw_set_pac(px_from_tx(pac.tx) - cell() * 0.5f,
                 py_from_ty(pac.ty) - cell() * 0.5f,
                 pac.dir);

    // --- Update ghost modes (scatter/chase cycles) ---
    static const float cycleTimes[] = {7.0f, 20.0f, 7.0f, 20.0f, 5.0f, 20.0f}; // simple cycle: S-C-S-C-S-C
    for (int i = 0; i < 4; ++i)
    {
        Ghost &gh = ghosts[i];

        // frightened comes from power pellets
        if (power_time > 0.0f && gh.mode != EATEN)
        {
            gh.mode = FRIGHTENED;
            gh.fright_time = power_time; // keep synced with Pac�s global
        }
        else if (gh.mode == FRIGHTENED && power_time <= 0.0f)
        {
            // fall back to scatter/chase track
            gh.mode = (gh.mode_clock <= 7.0f || (gh.mode_clock > 7.0f && gh.mode_clock <= 14.0f) || (gh.mode_clock > 27.0f && gh.mode_clock <= 34.0f)) ? SCATTER : CHASE;
        }

        if (gh.mode != FRIGHTENED && gh.mode != EATEN)
        {
            gh.mode_clock += dt;
            float t = gh.mode_clock;
            // very rough cycle mapping
            if (t <= 7.0f)
                gh.mode = SCATTER;
            else if (t <= 27.0f)
                gh.mode = CHASE;
            else if (t <= 34.0f)
                gh.mode = SCATTER;
            else
                gh.mode = CHASE;
        }

        // speed tweaks
        float s = gh.speed;
        if (gh.mode == FRIGHTENED)
            s *= 0.5f;
        if (gh.mode == EATEN)
            s *= 1.5f;

        // movement like Pac: move center-to-center
        auto centered = [](float v)
        { return std::fabs(v - std::round(v)) < 1e-2f; };

        if (centered(gh.tx) && centered(gh.ty))
        {
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
        if (!is_blocked(nx, ny))
        {
            float gx = (float)nx;
            float gy = (float)ny;
            float vx = gx - gh.tx, vy = gy - gh.ty;
            float dist = std::sqrt(vx * vx + vy * vy);
            float adv = std::min(s * dt, dist);
            if (dist > 1e-6f)
            {
                gh.tx += (vx / dist) * adv;
                gh.ty += (vy / dist) * adv;
                if (adv >= dist - 1e-4f)
                {
                    gh.tx = gx;
                    gh.ty = gy;
                }
            }
        }
        else
        {
            gh.tx = (float)cx;
            gh.ty = (float)cy; // wait at center
        }

        // Tunnel wrap for ghosts too (same as Pac)
        if (centered(gh.tx) && centered(gh.ty))
        {
            int gx = (int)std::round(gh.tx);
            int gy = (int)std::round(gh.ty);
            if (gy >= 0 && gy < ROWS && MAZE_RAW[gy][gx] == 'T')
            {
                if (gx == 0 && gh.dir == LEFT)
                    gh.tx = (float)(COLS - 1);
                else if (gx == COLS - 1 && gh.dir == RIGHT)
                    gh.tx = 0.0f;
            }
        }

        // EATEN -> when reaches �home� switch back to scatter
        if (gh.mode == EATEN)
        {
            if ((int)std::round(gh.tx) == COLS / 2 && (int)std::round(gh.ty) == 13)
            {
                gh.mode = SCATTER;
            }
        }

        // Collision with Pac
        float dtx = gh.tx - pac.tx;
        float dty = gh.ty - pac.ty;
        if (dtx * dtx + dty * dty < 0.25f)
        {
            if (power_time > 0.0f && gh.mode != EATEN)
            {
                gh.mode = EATEN;           // send to house
                ++g_eatStreak;
                int add = 200 << (g_eatStreak - 1);  // 200 * 2^(streak-1)
                if (add > 1600) add = 1600;          // cap just in case
                score += add;
                try_update_high(score);
                spawn_score_popup_at_tile(gh.tx, gh.ty, add);
                audio_play(SFX_EAT_GHOST); // <<< sound: chomp ghost
            }
            else if (gh.mode != EATEN)
            {
                audio_play(SFX_DEATH); // <<< sound: Pac-Man death
                if (g_deathCooldown <= 0 && !g_gameOver)
                {
                    lose_life();
                }
            }
        }

        // Tell renderer
        // draw_set_ghost(i, px_from_tx(gh.tx) - cell() * 0.5f, py_from_ty(gh.ty) - cell() * 0.5f, gh.dir);
        draw_set_ghost_state(i,
                             px_from_tx(gh.tx) - cell() * 0.5f,
                             py_from_ty(gh.ty) - cell() * 0.5f,
                             gh.dir,
                             (int)gh.mode);
    }


    draw_update(dt);
    glutPostRedisplay();
    glutTimerFunc(1000 / 120, timer, 0);
}

static void specialKey(int key, int, int)
{
    if (g_mode == MODE_MENU) {
        if (key == GLUT_KEY_UP)   { g_menuSel = (g_menuSel + g_menuCount - 1) % g_menuCount; glutPostRedisplay(); }
        if (key == GLUT_KEY_DOWN) { g_menuSel = (g_menuSel + 1) % g_menuCount; glutPostRedisplay(); }

        if (key == GLUT_KEY_LEFT) { /* no-op for now */ }
        if (key == GLUT_KEY_RIGHT){ /* no-op for now */ }
        return;
    }

    if (g_paused) return; // ignore arrows while paused (playing mode will never hit here paused)

    if (key == GLUT_KEY_UP)    pac.want = UP;
    if (key == GLUT_KEY_DOWN)  pac.want = DOWN;
    if (key == GLUT_KEY_LEFT)  pac.want = LEFT;
    if (key == GLUT_KEY_RIGHT) pac.want = RIGHT;
}
static void mouseBtn(int button, int state, int x, int y)
{
    if (g_mode != MODE_MENU) return;
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    // GLUT gives y from top; convert to bottom-left origin
    float mx = (float)x;
    float my = (float)(HH - y);

    for (int i=0; i<g_menuCount; ++i){
        if (pt_in_rect(mx, my, g_btn[i])) {
            g_menuSel = i;
            menu_activate(g_menuOrder[i]);
            glutPostRedisplay();
            return;
        }
    }

}





static void keyDown(unsigned char key, int, int)
{
    if (g_mode == MODE_MENU) {
        if (key == 13 || key == ' ') { // Enter or Space
            menu_activate(g_menuOrder[g_menuSel]);
            return;
        }
        if (key == 27 || key == 'q' || key == 'Q') { // Esc = Quit from menu
            std::exit(0);
        }
        // allow quick toggle to start with 's'
        if (key=='s' || key=='S'){ menu_activate(ACT_START);   return; }
        if (key=='r' || key=='R'){ menu_activate(ACT_RESTART); return; }
        if (key=='p' || key=='P'){ menu_activate(ACT_RESUME); return; }
        if (key=='f' || key=='F'){ menu_activate(ACT_FULLWINDOW); return; }

        return;
    }

    // --- In-game keys ---
    if ((key == 'p' || key == 'P') && !g_gameOver) {
        g_paused = !g_paused;
        if (g_paused) g_mode = MODE_MENU; // show menu when paused
        glutPostRedisplay();
        return;
    }
    if (key == 'r' || key == 'R') {
        g_paused = false;
        g_lives = 3;
        g_gameOver = false;
        g_deathCooldown = 0;
        reset_game();
        return;
    }
    if (key == 'h' || key == 'H') {
        g_hudSide = (g_hudSide == HUD_LEFT) ? HUD_RIGHT : HUD_LEFT;
        glutPostRedisplay();
        return;
    }
    if (key == 'f' || key == 'F') {
        toggle_fullscreen();
        glutPostRedisplay();
        return;
    }
    if (key == 27 || key == 'q' || key == 'Q') { // Esc opens menu instead of quitting
        g_mode = MODE_MENU;
        g_paused = true;
        glutPostRedisplay();
        return;
    }
}


// --------------- Main ---------------
int main(int argc, char **argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(WW, HH);
    glutCreateWindow("Pac-Man");
    // --- Audio ---
    if (!audio_init())
    {
        std::fprintf(stderr, "[audio] Failed to init audio engine.\n");
    }
    // Make sure we clean up on process exit
    atexit(audio_shutdown);

    glewInit();
    if (!draw_init(WW, HH, "image/maze1.png", "image/sprites3.png"))
        return 1;
    init_grid();
    // initial actors once
    float start_px = px_from_tx(pac.tx);
    float start_py = py_from_ty(pac.ty);
    draw_load_demo((int)start_px, (int)start_py, pac.dir);
    init_ghosts();
    load_high_score();
    glutDisplayFunc(display);
    //glutFullScreen();
    glutReshapeFunc(reshape);

    glutSpecialFunc(specialKey);
    glutKeyboardFunc(keyDown);
    glutMouseFunc(mouseBtn);

    glutTimerFunc(1000 / 120, timer, 0);
    glutMainLoop();
    return 0;
}
