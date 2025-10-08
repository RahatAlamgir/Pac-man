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
static int   g_winW = 640;    // window width
static int   g_winH = 480;    // window height
static const int TILE = 16;   // each cell in sheet is 16x16
static int   g_lastMS = 0;    // last timestamp in ms
static bool  g_paused = false;// pause flag

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
    if (!pixels) {
        std::fprintf(stderr, "Failed to load %s: %s\n", path, stbi_failure_reason());
        return t;
    }

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

// -------------------- Render all entities --------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

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

// -------------------- Optional: lookup by name later --------------------
Entity* findEntityByName(const std::string& name) {
    for (auto& e : g_entities)
        if (e.name == name) return &e;
    return nullptr;
}

// -------------------- Create sample characters --------------------
void makeEntitiesExample() {
    g_entities.clear();

    // Pac-Man: top-left row, columns 0..2 (3 frames)
    g_entities.push_back(
        makeEntity("PacMan",  80, 300, 32,
                   { {0,0,0.12f}, {1,0,0.12f}, {2,0,0.12f} })
    );

    // Blinky: example 2-frame wiggle at row=1, cols 0..1
    g_entities.push_back(
        makeEntity("Blinky", 260, 300, 32,
                   { {0,4,0.16f}, {1,4,0.16f} , {2,4,0.16f}, {3,4,0.16f}, {4,4,0.16f}, {5,4,0.16f}, {6,4,0.16f}, {7,4,0.16f} })
    );

    // Pinky: example 4 frames at row=2, cols 0..3
    g_entities.push_back(
        makeEntity("Pinky",  440, 300, 32,
                   { {0,5,0.16f}, {1,5,0.16f} })
    );

    // Clyde: example 2 frames at row=3, cols 0..1
    g_entities.push_back(
        makeEntity("Clyde",   80, 180, 32,
                   { {0,6,0.16f}, {1,6,0.16f} })
    );
    g_entities.push_back(
        makeEntity("Inky",  260, 180, 32,
                   { {0,7,0.16f}, {1,7,0.16f} })
    );
    g_entities.push_back(
        makeEntity("Inky",  440, 180, 32,
                   { {8,4,0.16f}, {9,4,0.16f}, {10,4,0.16f}, {11,4,0.16f}  })
    );
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

    g_sheet = loadTexturePNG("image/sprites.png");
    if (g_sheet.id == 0) {
        std::fprintf(stderr, "spritesheet.png not found or invalid.\n");
        return 1;
    }

    makeEntitiesExample();

    g_lastMS = glutGet(GLUT_ELAPSED_TIME);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);

    glutMainLoop();
    return 0;
}
