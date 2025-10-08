#pragma once
// Minimal render/animation module for your sheet + maze background.


bool draw_init(int win_w, int win_h,
               const char* maze_png, const char* sheet_png);

void draw_reshape(int w, int h);   // call from your GLUT reshape
void draw_update(float dt);        // call each tick (e.g., 1/120)
void draw_render();                // call in display()

void draw_toggle_pause();
void draw_clear_entities();

// Add characters (animated) at pixel coords. dir: 1=right,2=left,3=up,4=down.
void pacman(float x, float y, int dir);
void blinky(float x, float y, int dir);

// Optional: quick loader of a demo set
void draw_load_demo(int px,int py,int pdir);
void pellet(float x, float y, float r);
