// g++ main.cpp -o demo -lfreeglut -lopengl32 -lglu32
// Code::Blocks: add freeglut, opengl32, glu32 to linker

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <GL/freeglut.h>
#include <cstdio>

// Some MinGW headers miss this symbol. Define if absent.
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

static GLuint tex = 0;
static int imgW = 0, imgH = 0;
static int winW = 800, winH = 600;

static void loadTexture(const char* file) {
    stbi_set_flip_vertically_on_load(1);
    int channels = 0;
    unsigned char* data = stbi_load(file, &imgW, &imgH, &channels, 4);
    if (!data) {
        std::printf("Failed to load %s\n", file);
        std::exit(1);
    }

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  // or GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  // or GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // safe with old headers now
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgW, imgH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
}

static void reshape(int w, int h) {
    winW = w; winH = h;
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);  // pixel coordinates
    glMatrixMode(GL_MODELVIEW);
}

static void display() {
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, tex);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Target rectangle in window pixels. Change as needed.
    float x = 0;
    float y = 0;
    float w = 226*2.4;  // draw size, can differ from actual image size
    float h = 240*2.4;

    glLoadIdentity();
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(x,     y);
        glTexCoord2f(1, 0); glVertex2f(x + w, y);
        glTexCoord2f(1, 1); glVertex2f(x + w, y + h);
        glTexCoord2f(0, 1); glVertex2f(x,     y + h);
    glEnd();

    glutSwapBuffers();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(winW, winH);
    glutCreateWindow("Image at fixed rect");

    loadTexture("image/maze1.png");  // put your file here

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutMainLoop();
    return 0;
}

