#include <GL/glew.h>
#include <GL/freeglut.h>
#include <vector>
#include <string>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>

// ---------- stb_image ----------
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// ---------- Types ----------

static constexpr float PI = 3.14159265358979323846f;

struct Texture { GLuint id=0; int w=0, h=0; };

struct Frame { int col, row; float dur; };

struct Animator {
    std::vector<Frame> frames;
    int idx=0;          // current frame index
    float acc=0.0f;     // time accumulated into current frame
    bool loop=true;

    void set_frames(const std::vector<Frame>& f, bool keep_phase=true){
        // preserve phase when direction changes so animation does not pop
        float phase = 0.0f;
        if(!frames.empty() && keep_phase){
            float cur_dur = frames[idx].dur;
            phase = (cur_dur > 0.0f) ? std::fmod(acc, cur_dur) : 0.0f;
        }
        frames = f;
        if(frames.empty()){ idx=0; acc=0; return; }
        if(idx >= (int)frames.size()) idx = idx % std::max(1,(int)frames.size());
        acc = std::min(phase, frames[idx].dur);
    }

    void update(float dt){
        if(frames.empty()) return;
        acc += dt;
        while(acc >= frames[idx].dur){
            acc -= frames[idx].dur;
            if(idx+1 < (int)frames.size()) idx++;
            else if(loop) idx = 0;
            else { idx = (int)frames.size()-1; break; }
        }
    }

    const Frame& cur() const { return frames[idx]; }
};

struct Entity {
    std::string name;
    float x=0, y=0, s=32;
    Animator anim;
};

// ---------- Module state ----------
static Texture g_sheet;
static Texture g_bg;
static std::vector<Entity> g_entities;
static int gW=0, gH=0;

// Pellets buffered per frame so they render behind Pac-Man
struct Pellet { float x, y, r, cr, cg, cb; };
static std::vector<Pellet> g_pellets;

// keep handle to pac entity
static int   g_pac_i = -1;
static int   g_pac_dir = 1; // 1=R,2=L,3=U,4=D (matches your sheet rows)

// sprite constants
static const int TILE=16;

// ---------- Helpers ----------
static Texture load_png(const char* path){
    Texture t; int comp=0;
    unsigned char* px = stbi_load(path, &t.w, &t.h, &comp, 4);
    if(!px){ std::fprintf(stderr,"stbi_load failed: %s\n", path); return t; }
    glGenTextures(1,&t.id);
    glBindTexture(GL_TEXTURE_2D,t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,t.w,t.h,0,GL_RGBA,GL_UNSIGNED_BYTE,px);
    glBindTexture(GL_TEXTURE_2D,0);
    stbi_image_free(px);
    return t;
}

static inline void tileUV(int col,int row,float& u0,float& v0,float& u1,float& v1){
    u0 = (col*TILE)/float(g_sheet.w);
    v0 = (row*TILE)/float(g_sheet.h);
    u1 = ((col+1)*TILE)/float(g_sheet.w);
    v1 = ((row+1)*TILE)/float(g_sheet.h);
}

static void draw_tile(int col,int row,float x,float y,float size){
    float u0,v0,u1,v1; tileUV(col,row,u0,v0,u1,v1);
    glBindTexture(GL_TEXTURE_2D,g_sheet.id);
    glBegin(GL_QUADS);
      glTexCoord2f(u0, v1); glVertex2f(x      , y      );
      glTexCoord2f(u1, v1); glVertex2f(x+size , y      );
      glTexCoord2f(u1, v0); glVertex2f(x+size , y+size );
      glTexCoord2f(u0, v0); glVertex2f(x      , y+size );
    glEnd();
    glBindTexture(GL_TEXTURE_2D,0);
}

static void draw_image(const Texture& t,float x,float y,float w,float h){
    if(!t.id) return;
    glBindTexture(GL_TEXTURE_2D,t.id);
    glBegin(GL_QUADS);
      glTexCoord2f(0,1); glVertex2f(x    , y    );
      glTexCoord2f(1,1); glVertex2f(x+w  , y    );
      glTexCoord2f(1,0); glVertex2f(x+w  , y+h  );
      glTexCoord2f(0,0); glVertex2f(x    , y+h  );
    glEnd();
    glBindTexture(GL_TEXTURE_2D,0);
}

static Entity makeEntity(const std::string& name,float x,float y,float size,
                        const std::vector<Frame>& frames,bool loop=true){
    Entity e; e.name=name; e.x=x; e.y=y; e.s=size; e.anim.set_frames(frames,false); e.anim.loop=loop; return e;
}

// pac animator frames by direction
static std::vector<Frame> pac_frames_for_dir(int dir){
    // rows: 0=R,1=L,2=U,3=D in your sheet
    int row = 0;
    if(dir==1) row=0;
    else if(dir==2) row=1;
    else if(dir==3) row=2;
    else if(dir==4) row=3;
    return {{0,row,0.12f},{1,row,0.12f},{2,row,0.12f}};
}

// ghost helpers
static std::vector<Frame> ghost_row_pair(int row){ return {{0+0,row,0.12f},{1+0,row,0.12f}}; }
static std::vector<Frame> ghost_dir_frames(int base_row,int dir){
    // sheet uses 4 consecutive columns for the four directions in your original code.
    // We�ll keep the same rows you used: row 4=blinky, 5=pinky, 6=inky, 7=clyde
    switch(dir){
        case 1: return {{0,base_row,0.12f},{1,base_row,0.12f}};
        case 2: return {{2,base_row,0.12f},{3,base_row,0.12f}};
        case 3: return {{4,base_row,0.12f},{5,base_row,0.12f}};
        case 4: return {{6,base_row,0.12f},{7,base_row,0.12f}};
        default: return {{0,base_row,0.12f},{1,base_row,0.12f}};
    }
}

static inline float tile_size_px(){ return std::floor(std::min(gW/28.0f, gH/31.0f)); }
static inline float offX_px(){ return 0.5f*(gW - tile_size_px()*28.0f); }
static inline float offY_px(){ return 0.5f*(gH - tile_size_px()*31.0f); }

// ---------- API ----------
bool draw_init(int win_w,int win_h,const char* maze_png,const char* sheet_png){
    gW=win_w; gH=win_h;

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0,0,0,1);
    g_bg    = load_png(maze_png);
    g_sheet = load_png(sheet_png);

    glViewport(0,0,gW,gH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, gW, 0, gH);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    return g_bg.id && g_sheet.id;
}

void draw_reshape(int w,int h){
    gW=w; gH=h;
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
     float ts = tile_size_px();
    for(auto& e : g_entities) e.s = ts;
}

void draw_update(float dt){

    for(auto& e: g_entities) e.anim.update(dt);
}

static void draw_pellets() {
    if (g_pellets.empty()) return;

    glDisable(GL_TEXTURE_2D);

    for (const auto& p : g_pellets) {
        glColor3f(p.cr, p.cg, p.cb); // use per-pellet color
        glBegin(GL_TRIANGLE_FAN);
            glVertex2f(p.x, p.y);
            for (int i=0;i<=24;++i){
                float a = 2*PI*i/24.0f;
                glVertex2f(p.x + p.r*std::cos(a), p.y + p.r*std::sin(a));
            }
        glEnd();
    }

    glEnable(GL_TEXTURE_2D);
    glColor4f(1,1,1,1);

    g_pellets.clear();
}


void draw_render(){
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    float ts = tile_size_px();
    draw_image(g_bg, offX_px(), offY_px(), ts*28.0f, ts*31.0f);

    // ensure textured quads draw with full color
    glColor4f(1,1,1,1);

    draw_pellets();

    for(const auto& e: g_entities){
        const Frame& f = e.anim.cur();
        draw_tile(f.col, f.row, e.x, e.y, e.s);
    }
    // glutSwapBuffers() is done by your main.cpp
}

void draw_text(float x, float y, const char* s, float r, float g, float b){
    if(!s) return;
    glDisable(GL_TEXTURE_2D);
    glColor3f(r,g,b);
    glRasterPos2f(x, y);
    for(const char* p = s; *p; ++p){
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *p);
    }
    glEnable(GL_TEXTURE_2D);
    glColor4f(1,1,1,1);
}


void draw_clear_entities(){ g_entities.clear(); }

// ---------- Public helpers you use ----------

// Create initial scene once
void draw_load_demo(int px,int py,int pdir){
    draw_clear_entities();

    // Pac-Man
    float ts = tile_size_px();
    Entity pac = makeEntity("PacMan", (float)px, (float)py, ts, pac_frames_for_dir(pdir));
    g_entities.push_back(pac);
    g_pac_i   = (int)g_entities.size()-1;
    g_pac_dir = pdir;

    // Ghosts (positions kept from your old code)
    g_entities.push_back(makeEntity("Blinky", gW*0.5f,     gH*0.5f,     ts, ghost_dir_frames(4,1)));
    g_entities.push_back(makeEntity("Pinky",  gW*0.5f-ts, gH*0.5f,    ts, ghost_dir_frames(5,1)));
    g_entities.push_back(makeEntity("Inky",   gW*0.5f+ts, gH*0.5f,    ts, ghost_dir_frames(6,1)));
    g_entities.push_back(makeEntity("Clyde",  gW*0.5f-2*ts, gH*0.5f,    ts, ghost_dir_frames(7,2)));
}

// Update Pac-Man pose each frame without resetting animation
void draw_set_pac(float x, float y, int dir){
    if(g_pac_i < 0 || g_pac_i >= (int)g_entities.size()) return;
    Entity& p = g_entities[g_pac_i];
    p.x = x; p.y = y;

    if(dir != g_pac_dir){
        // switch animation rows, keep phase
        p.anim.set_frames(pac_frames_for_dir(dir), /*keep_phase=*/true);
        g_pac_dir = dir;
    }
}

static int ghost_base_row_from_index(int which){
    // rows in your sheet: 4=blinky,5=pinky,6=inky,7=clyde
    switch(which){
        case 0: return 4; // Blinky
        case 1: return 5; // Pinky
        case 2: return 6; // Inky
        case 3: return 7; // Clyde
        default: return 4;
    }
}

void draw_set_ghost(int which, float x, float y, int dir){
    // after Pac: indices 1..4 are ghosts created by draw_load_demo
    int idx = 1 + which;
    if(idx < 0 || idx >= (int)g_entities.size()) return;
    Entity& g = g_entities[idx];
    g.x = x; g.y = y;
    int base = ghost_base_row_from_index(which);
    g.anim.set_frames(ghost_dir_frames(base, dir), /*keep_phase=*/true);
}


void pellet(float x, float y, float r){
    g_pellets.push_back({x, y, r, 1.0f, 1.0f, 1.0f}); // white
}

void pellet_colored(float x, float y, float r, float cr, float cg, float cb){
    g_pellets.push_back({x, y, r, cr, cg, cb});
}
