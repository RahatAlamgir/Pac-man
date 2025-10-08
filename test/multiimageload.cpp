// g++ main.cpp -o demo -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GL/freeglut.h>
#include <cstdio>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// --- globals
static int winW=900, winH=400;
static GLuint tex[3]={0,0,0};
static int tw[3]={0,0,0}, th[3]={0,0,0};   // image sizes
// draw state for the first image (move/scale via keys)
static float x0=50, y0=60, w0=224, h0=240;

// --- GL pixel-space
static void setOrthoPixels(int w,int h){
    glViewport(0,0,w,h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0,w,0,h);
    glMatrixMode(GL_MODELVIEW);
}

// --- loader
static GLuint loadTextureFile(const char* file, int& w, int& h){
    int ch=0;
    stbi_set_flip_vertically_on_load(1);
    unsigned char* data = stbi_load(file, &w, &h, &ch, 4);
    if(!data){ std::printf("Failed: %s\n", file); std::exit(1); }

    GLuint t=0; glGenTextures(1,&t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);   // or GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);   // or GL_NEAREST
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    stbi_image_free(data);
    return t;
}

// --- draw full image to rect (x,y,w,h) in pixels
static void drawImage(GLuint t, float x,float y,float w,float h){
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t);
    glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(x,     y);
        glTexCoord2f(1,0); glVertex2f(x + w, y);
        glTexCoord2f(1,1); glVertex2f(x + w, y + h);
        glTexCoord2f(0,1); glVertex2f(x,     y + h);
    glEnd();
}

// --- draw sub-rect from atlas using UVs [u0,v0]-[u1,v1]
static void drawSubImage(GLuint t, float x,float y,float w,float h,
                         float u0,float v0,float u1,float v1){
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, t);
    glBegin(GL_QUADS);
        glTexCoord2f(u0,v0); glVertex2f(x,     y);
        glTexCoord2f(u1,v0); glVertex2f(x + w, y);
        glTexCoord2f(u1,v1); glVertex2f(x + w, y + h);
        glTexCoord2f(u0,v1); glVertex2f(x,     y + h);
    glEnd();
}

static void display(){
    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLoadIdentity();

    // img0: movable/scalable
    drawImage(tex[0], x0, y0, w0, h0);

    // img1: half-size at fixed spot
    drawImage(tex[1], 320, 60, tw[1]*0.5f, th[1]*0.5f);

    // img2: sprite from atlas top-left 64x64
    float u1 = 64.0f / tw[2], v1 = 64.0f / th[2];
    drawSubImage(tex[2], 600, 60, 64, 64, 0.0f, 0.0f, u1, v1);

    glutSwapBuffers();
}

static void reshape(int w,int h){ winW=w; winH=h; setOrthoPixels(w,h); }

static void special(int key,int,int){
    const float step=10.0f;
    if(key==GLUT_KEY_LEFT)  x0-=step;
    if(key==GLUT_KEY_RIGHT) x0+=step;
    if(key==GLUT_KEY_DOWN)  y0-=step;
    if(key==GLUT_KEY_UP)    y0+=step;
    glutPostRedisplay();
}

static void keyboard(unsigned char k,int,int){
    if(k=='+'){ w0*=1.1f; h0*=1.1f; }
    if(k=='-'){ w0*=0.9f; h0*=0.9f; }
    if(k=='r'){ x0=50; y0=60; w0=224; h0=240; }
    glutPostRedisplay();
}

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA);
    glutInitWindowSize(winW,winH);
    glutCreateWindow("Multi-image demo");

    // change filenames as needed
    tex[0]=loadTextureFile("image_224x240.png", tw[0], th[0]);
    tex[1]=loadTextureFile("logo.png",          tw[1], th[1]);
    tex[2]=loadTextureFile("atlas.png",         tw[2], th[2]);

    glutReshapeFunc(reshape);
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    glutMainLoop();
    return 0;
}
