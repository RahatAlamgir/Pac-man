// pacman_stb_readable.cpp
// Goal: Animate Pac-Man using the first 3 tiles (top-left row) of a 16x16 sprite sheet.
// Uses stb_image.h to load PNG. Uses FreeGLUT + old-school OpenGL (immediate mode).
// Controls: Space = pause/resume, Esc = quit.

#include <GL/glew.h>        // OpenGL function loader
#include <GL/freeglut.h>    // Window + input
#include <cstdio>           // printf, fprintf
#include <cstdlib>          // exit

// ------------------------------
// Image loading via stb_image
// ------------------------------
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG           // keep it light: only PNG decoder
#include "stb_image.h"

// ------------------------------
// Constants and small types
// ------------------------------
struct Texture {
    GLuint id = 0;          // OpenGL texture handle
    int w = 0;              // width in pixels
    int h = 0;              // height in pixels
};

static Texture g_sheet;     // the whole sprite sheet
static int g_winW = 320;    // window width
static int g_winH = 240;    // window height

// Sprite sheet specifics.
// Your image is a grid of 16x16 tiles.
static const int TILE = 16;

// Animation state.
static int   g_frame = 0;       // current frame index: 0,1,2
static float g_frameStep = 0.12f; // seconds per frame
static int   g_lastMS = 0;      // last timestamp
static bool  g_paused = false;  // toggle with Space

// ------------------------------
// Utility: load a PNG as GL texture
// ------------------------------
Texture loadTexturePNG(const char* path) {
    Texture t;

    // stb loads top-left origin by default; OpenGL expects bottom-left.
    // Our UVs will match stb's convention, so we keep flipping OFF here.
    stbi_set_flip_vertically_on_load(0);

    int comp = 0; // number of channels in file
    unsigned char* pixels = stbi_load(path, &t.w, &t.h, &comp, 4); // force RGBA
    if (!pixels) {
        std::fprintf(stderr, "Failed to load %s: %s\n", path, stbi_failure_reason());
        return t;
    }

    // Create the GL texture and upload pixels.
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);                // safe for tightly-packed rows
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // crisp pixels
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return t;
}

// ------------------------------
// Compute UVs for a given tile
// col,row are 0-based tile indices into the sheet.
// ------------------------------
static inline void getTileUV(int col, int row,
                             float& u0, float& v0, float& u1, float& v1)
{
    u0 = (col * TILE) / float(g_sheet.w);
    v0 = (row * TILE) / float(g_sheet.h);
    u1 = ((col + 1) * TILE) / float(g_sheet.w);
    v1 = ((row + 1) * TILE) / float(g_sheet.h);
}

// ------------------------------
// Draw Pac-Man at (x,y) with size s (pixels).
// Uses current frame 0..2 from row 0.
// ------------------------------
void drawPacman(float x, float y, float s) {
    int col = g_frame;  // frames 0,1,2 in the top row
    int row = 0;

    float u0,v0,u1,v1;
    getTileUV(col, row, u0, v0, u1, v1);

    glBindTexture(GL_TEXTURE_2D, g_sheet.id);
    glBegin(GL_QUADS);
        // Texture coordinates map the 16x16 tile.
        // Vertex positions draw a screen-aligned quad.
        glTexCoord2f(u0, v1); glVertex2f(x    , y    );
        glTexCoord2f(u1, v1); glVertex2f(x + s, y    );
        glTexCoord2f(u1, v0); glVertex2f(x + s, y + s);
        glTexCoord2f(u0, v0); glVertex2f(x    , y + s);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
}

// ------------------------------
// GLUT display callback
// ------------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_TEXTURE_2D);

    // Draw Pac-Man scaled 4x (16 -> 64 px) at the center.
    const float size = 100.0f;
    const float x = (g_winW - size) * 0.5f;
    const float y = (g_winH - size) * 0.5f;
    drawPacman(x, y, size);

    glutSwapBuffers();
}

// ------------------------------
// Advance time and animation
// ------------------------------
void idle() {
    const int now = glutGet(GLUT_ELAPSED_TIME);
    const float dt = (now - g_lastMS) * 0.001f; // ms -> s
    g_lastMS = now;

    static float acc = 0.0f; // accumulator for fixed frame step
    if (!g_paused) {
        acc += dt;
        while (acc >= g_frameStep) {
            g_frame = (g_frame + 1) % 3; // cycle 0->1->2
            acc -= g_frameStep;
        }
    }

    glutPostRedisplay(); // request redraw
}

// ------------------------------
// Handle window resize and set a 2D pixel projection
// ------------------------------
void reshape(int w, int h) {
    g_winW = w; g_winH = h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 2D orthographic projection: origin at bottom-left, in pixels.
    gluOrtho2D(0, w, 0, h);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ------------------------------
// Keyboard: Space toggles pause. Esc quits.
// ------------------------------
void key(unsigned char k, int, int) {
    if (k == 27) std::exit(0);
    if (k == ' ') g_paused = !g_paused;
}

// ------------------------------
// Program entry
// ------------------------------
int main(int argc, char** argv) {
    // Init GLUT window and GL context.
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
    glutInitWindowSize(g_winW, g_winH);
    glutCreateWindow("Pac-Man animation (3 frames)");

    // Load GL functions.
    glewInit();

    // Basic GL state.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // black
    reshape(g_winW, g_winH);              // set projection

    // Load the sprite sheet PNG.
    g_sheet = loadTexturePNG("image/sprites.png");
    if (g_sheet.id == 0) {
        std::fprintf(stderr, "spritesheet.png not found or invalid.\n");
        return 1;
    }

    // Set callbacks.
    g_lastMS = glutGet(GLUT_ELAPSED_TIME);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(key);

    // Run the loop.
    glutMainLoop();
    return 0;
}
