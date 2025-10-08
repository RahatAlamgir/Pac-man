// pacman_multi_named.cpp
// Animate multiple characters from one 16x16 sprite sheet,
// each with its own frames, durations, and a human-readable name.
// Controls: Space = pause/resume, Esc = quit.

#include <GL/glew.h>          // OpenGL function loader
#include <GL/freeglut.h>      // Window + input
#include <vector>             // std::vector
#include <string>             // std::string   <-- needed for names
#include <cstdio>             // printf, fprintf
#include <cstdlib>            // exit
#include <queue>

// -------------------- stb_image single-header PNG loader --------------------
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

// -------------------- Basic texture wrapper --------------------
struct Texture {
    GLuint id = 0;            // OpenGL texture handle
    int w = 0;                // width (px)
    int h = 0;                // height (px)
};

static Texture g_sheet;       // shared sprite sheet
static Texture mazebg;      // background image (static)

static int   g_winW = 226*3;    // window width
static int   g_winH = 248*3;    // window height
static const int TILE = 16;   // each cell in sheet is 16x16
static int   g_lastMS = 0;    // last timestamp in ms
static bool  g_paused = false;// pause flag

constexpr int COLS = 28;
constexpr int ROWS = 31;

static const char* MAZE_RAW[ROWS] = {
// 0123456789012345678901234567
"WWWWWWWWWWWWWWWWWWWWWWWWWWWW"
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







int score = 0;
// -------------------- Animation data types --------------------
// One frame = which tile and how long to show it.
struct Frame {
    int col;                  // tile column index (0-based)
    int row;                  // tile row    index (0-based)
    float dur;                // seconds this frame is displayed
};

// Animator = a list of frames + current playback state.
struct Animator {
    std::vector<Frame> frames; // ordered frames
    int   idx  = 0;            // current frame index
    float acc  = 0.0f;         // time accumulator
    bool  loop = true;         // loop playback

    // Advance animation based on elapsed time.
    void update(float dt){
        if (frames.empty()) return;
        acc += dt;
        while (acc >= frames[idx].dur) {
            acc -= frames[idx].dur;
            if (idx + 1 < (int)frames.size()) idx++;
            else if (loop) idx = 0;
            else { idx = (int)frames.size() - 1; break; }
        }
    }
    // Current frame (safe because idx is maintained).
    const Frame& cur() const { return frames[idx]; }
};

// Entity = one on-screen character with a name.
struct Entity {
    std::string name;         // human-readable label
    float x = 0;              // bottom-left X in pixels
    float y = 0;              // bottom-left Y in pixels
    float s = 64;             // render size (px); sprite is scaled from 16x16
    Animator anim;            // animation state
};

static std::vector<Entity> g_entities; // list of all characters

// -------------------- Texture loading --------------------
Texture loadTexturePNG(const char* path) {
    Texture t;
    stbi_set_flip_vertically_on_load(0);  // keep top-left origin

    int comp = 0;
    unsigned char* pixels = stbi_load(path, &t.w, &t.h, &comp, 4); // force RGBA


    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // crisp pixels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return t;
}

// -------------------- UV helper for a tile --------------------
static inline void tileUV(int col, int row, float& u0, float& v0, float& u1, float& v1) {
    u0 = (col * TILE) / float(g_sheet.w);
    v0 = (row * TILE) / float(g_sheet.h);
    u1 = ((col + 1) * TILE) / float(g_sheet.w);
    v1 = ((row + 1) * TILE) / float(g_sheet.h);
}

// -------------------- Draw a single tile quad --------------------
void drawTile(int col, int row, float x, float y, float size) {
    float u0, v0, u1, v1;
    tileUV(col, row, u0, v0, u1, v1);

    glBindTexture(GL_TEXTURE_2D, g_sheet.id);
    glBegin(GL_QUADS);
        glTexCoord2f(u0, v1); glVertex2f(x      , y      );
        glTexCoord2f(u1, v1); glVertex2f(x+size , y      );
        glTexCoord2f(u1, v0); glVertex2f(x+size , y+size );
        glTexCoord2f(u0, v0); glVertex2f(x      , y+size );
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}
// Draw any texture at screen rectangle [x, y]..[x+w, y+h].
// Renders behind sprites if called first in display().
void drawImage(const Texture& tex, float x, float y, float w, float h) {
    if (tex.id == 0) return;                 // nothing to draw
    glBindTexture(GL_TEXTURE_2D, tex.id);    // bind the image
    glBegin(GL_QUADS);                       // 2D quad with full UVs
        glTexCoord2f(0.f, 1.f); glVertex2f(x    , y    ); // bottom-left
        glTexCoord2f(1.f, 1.f); glVertex2f(x + w, y    ); // bottom-right
        glTexCoord2f(1.f, 0.f); glVertex2f(x + w, y + h); // top-right
        glTexCoord2f(0.f, 0.f); glVertex2f(x    , y + h); // top-left
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);         // unbind
}

// -------------------- Render all entities --------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

    drawImage(mazebg, 0, 0,  (float)g_winW,(float)g_winH);

    for (const auto& e : g_entities) {
        const Frame& f = e.anim.cur();
        drawTile(f.col, f.row, e.x, e.y, e.s);
    }
    glutSwapBuffers();
}

// -------------------- Advance all animations --------------------
void idle() {
    const int now = glutGet(GLUT_ELAPSED_TIME);
    const float dt = (now - g_lastMS) * 0.001f; // ms -> s
    g_lastMS = now;

    if (!g_paused)
        for (auto& e : g_entities) e.anim.update(dt);

    glutPostRedisplay();
}

// -------------------- Handle resize, set pixel-space ortho --------------------
void reshape(int w, int h) {
    g_winW = w; g_winH = h;
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, w, 0, h);   // bottom-left origin, units in pixels
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

// -------------------- Keyboard controls --------------------
void key(unsigned char k, int, int) {
    if (k == 27) std::exit(0);     // Esc
    if (k == ' ') g_paused = !g_paused; // Space
}

// -------------------- Helper: construct an Entity with a name --------------------
Entity makeEntity(const std::string& name,
                  float x, float y, float size,
                  std::initializer_list<Frame> frames,
                  bool loop = true)
{
    Entity e;
    e.name = name;                  // set label
    e.x = x; e.y = y;               // position
    e.s = size;                     // on-screen size
    e.anim.frames = frames;         // animation frames {col,row,dur}
    e.anim.loop = loop;             // loop playback flag
    return e;
}



// -------------------- Create sample characters --------------------
void pacman(float x,float y, int dir){
    //g_entities.clear();

    if(dir == 1){
        g_entities.push_back(
        makeEntity("PacMan",  x, y, 32,
                   { {0,0,0.12f}, {1,0,0.12f}, {2,0,0.12f} })
        );
    }else if (dir == 2){
        g_entities.push_back(
        makeEntity("PacMan",  x, y, 32,
                   { {0,1,0.12f}, {1,1,0.12f}, {2,1,0.12f} })
        );
    }else if (dir == 3){
        g_entities.push_back(
        makeEntity("PacMan",  x, y, 32,
                   { {0,2,0.12f}, {1,2,0.12f}, {2,2,0.12f} })
        );
    }else if (dir == 4){
        g_entities.push_back(
        makeEntity("PacMan",  x, y, 32,
                   { {0,3,0.12f}, {1,3,0.12f}, {2,3,0.12f} })
        );
    }
}

void blinky(float x,float y, int dir){


    if(dir == 1){
        g_entities.push_back(
        makeEntity("Blinky",  x, y, 32,
                   { {0,4,0.16f}, {1,4,0.12f} })
        );
    }else if (dir == 2){
        g_entities.push_back(
        makeEntity("Blinky",  x, y, 32,
                   { {2,4,0.12f}, {3,4,0.12f} })
        );
    }else if (dir == 3){
        g_entities.push_back(
        makeEntity("Blinky",  x, y, 32,
                   { {4,4,0.12f}, {5,4,0.12f} })
        );
    }else if (dir == 4){
        g_entities.push_back(
        makeEntity("Blinky",  x, y, 32,
                   { {6,4,0.12f}, {7,4,0.12f}})
        );
    }


}
void loadEntity(){
    g_entities.clear();
    pacman(310.0,160.0,1);
    blinky(g_winW*0.5,g_winH*0.5,3);

}



// -------------------- Entry point --------------------
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(g_winW, g_winH);
    glutCreateWindow("Multi-character sprite animation (named)");

    glewInit();

    glClearColor(0,0,0,1);
    reshape(g_winW, g_winH);

    mazebg = loadTexturePNG("image/maze1.png");
    g_sheet = loadTexturePNG("image/sprites.png");

    loadEntity();

    g_lastMS = glutGet(GLUT_ELAPSED_TIME);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);

    glutMainLoop();
    return 0;
}
