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
    int idx=0; float acc=0; bool loop=true;
    void update(float dt){
        if(frames.empty()) return;
        acc+=dt;
        while(acc >= frames[idx].dur){
            acc -= frames[idx].dur;
            if(idx+1 < (int)frames.size()) idx++;
            else if(loop) idx=0;
            else { idx=(int)frames.size()-1; break; }
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
static int gLastMS=0;
static bool gPaused=false;

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
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
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
                        std::initializer_list<Frame> frames,bool loop=true){
    Entity e; e.name=name; e.x=x; e.y=y; e.s=size; e.anim.frames=frames; e.anim.loop=loop; return e;
}

// ---------- API ----------
bool draw_init(int win_w,int win_h,const char* maze_png,const char* sheet_png){
    gW=win_w; gH=win_h;
    glEnable(GL_TEXTURE_2D);
    glClearColor(0,0,0,1);
    g_bg = load_png(maze_png);
    g_sheet = load_png(sheet_png);
    gLastMS = glutGet(GLUT_ELAPSED_TIME);

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
}
void draw_update(float dt){
    if(gPaused) return;
    for(auto& e: g_entities) e.anim.update(dt);
}
void draw_render(){
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);
    draw_image(g_bg, 0, 0, (float)gW, (float)gH);
    for(const auto& e: g_entities){
        const Frame& f = e.anim.cur();
        draw_tile(f.col,f.row,e.x,e.y,e.s);
    }
    //glutSwapBuffers();
}
void draw_toggle_pause(){ gPaused = !gPaused; }
void draw_clear_entities(){ g_entities.clear(); }

// dir: 1=right,2=left,3=up,4=down (matches your sheet rows)
void pacman(float x,float y,int dir){
    switch(dir){
        case 1: g_entities.push_back(makeEntity("PacMan",x,y,32,{{0,0,0.12f},{1,0,0.12f},{2,0,0.12f}})); break;
        case 2: g_entities.push_back(makeEntity("PacMan",x,y,32,{{0,1,0.12f},{1,1,0.12f},{2,1,0.12f}})); break;
        case 3: g_entities.push_back(makeEntity("PacMan",x,y,32,{{0,2,0.12f},{1,2,0.12f},{2,2,0.12f}})); break;
        case 4: g_entities.push_back(makeEntity("PacMan",x,y,32,{{0,3,0.12f},{1,3,0.12f},{2,3,0.12f}})); break;
        default: break;
    }
}
void blinky(float x,float y,int dir){
    switch(dir){
        case 1: g_entities.push_back(makeEntity("Blinky",x,y,32,{{0,4,0.16f},{1,4,0.12f}})); break;
        case 2: g_entities.push_back(makeEntity("Blinky",x,y,32,{{2,4,0.12f},{3,4,0.12f}})); break;
        case 3: g_entities.push_back(makeEntity("Blinky",x,y,32,{{4,4,0.12f},{5,4,0.12f}})); break;
        case 4: g_entities.push_back(makeEntity("Blinky",x,y,32,{{6,4,0.12f},{7,4,0.12f}})); break;
        default: break;
    }
}
void pinky(float x,float y,int dir){
    switch(dir){
        case 1: g_entities.push_back(makeEntity("Pinky",x,y,32,{{0,5,0.16f},{1,5,0.12f}})); break;
        case 2: g_entities.push_back(makeEntity("Pinky",x,y,32,{{2,5,0.12f},{3,5,0.12f}})); break;
        case 3: g_entities.push_back(makeEntity("Pinky",x,y,32,{{4,5,0.12f},{5,5,0.12f}})); break;
        case 4: g_entities.push_back(makeEntity("Pinky",x,y,32,{{6,5,0.12f},{7,5,0.12f}})); break;
        default: break;
    }
}
void inky(float x,float y,int dir){
    switch(dir){
        case 1: g_entities.push_back(makeEntity("inky",x,y,32,{{0,6,0.16f},{1,6,0.12f}})); break;
        case 2: g_entities.push_back(makeEntity("inky",x,y,32,{{2,6,0.12f},{3,6,0.12f}})); break;
        case 3: g_entities.push_back(makeEntity("inky",x,y,32,{{4,6,0.12f},{5,6,0.12f}})); break;
        case 4: g_entities.push_back(makeEntity("inky",x,y,32,{{6,6,0.12f},{7,6,0.12f}})); break;
        default: break;
    }
}
void clyde(float x,float y,int dir){
    switch(dir){
        case 1: g_entities.push_back(makeEntity("Clyde",x,y,32,{{0,7,0.16f},{1,7,0.12f}})); break;
        case 2: g_entities.push_back(makeEntity("Clyde",x,y,32,{{2,7,0.12f},{3,7,0.12f}})); break;
        case 3: g_entities.push_back(makeEntity("Clyde",x,y,32,{{4,7,0.12f},{5,7,0.12f}})); break;
        case 4: g_entities.push_back(makeEntity("Clyde",x,y,32,{{6,7,0.12f},{7,7,0.12f}})); break;
        default: break;
    }
}
void draw_load_demo(int px,int py,int pdir){
    draw_clear_entities();
    //pacman(310.0f,160.0f,dir);
    pacman(px,py,pdir);
    blinky(gW*0.5f, gH*0.5f, 1);
    pinky(gW*0.5f-32.0, gH*0.5f, 1);
    inky(gW*0.5f+32.0, gH*0.5f, 1);
    clyde(gW*0.5f-64.0, gH*0.5f, 1);
}


void pellet(float x, float y, float r){
    glColor3f(1,1,1);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(x, y);
    for(int i=0;i<=24;++i){
        float a = 2*PI*i/24;
        glVertex2f(x + r*std::cos(a), y + r*std::sin(a));
    }
    glEnd();
}
