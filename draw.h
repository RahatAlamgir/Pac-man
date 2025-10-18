#pragma once
// Minimal render/animation module for your sheet + maze background.


bool draw_init(int win_w, int win_h,
               const char* maze_png, const char* sheet_png);

void draw_reshape(int w, int h);   // call from your GLUT reshape
void draw_update(float dt);        // call each tick (e.g., 1/120)
void draw_render();                // call in display()


void draw_clear_entities();

// Add characters (animated) at pixel coords. dir: 1=right,2=left,3=up,4=down.
void pacman(float x, float y, int dir);
void blinky(float x, float y, int dir);
void pellet_colored(float x, float y, float r, float cr, float cg, float cb);


// Optional: quick loader of a demo set
void draw_load_demo(int px,int py,int pdir);
void pellet(float x, float y, float r);

void draw_set_pac(float x, float y, int dir);

// Set ghost position/dir: which = 0(Blinky),1(Pinky),2(Inky),3(Clyde)
// dir: 1=right,2=left,3=up,4= down
void draw_set_ghost(int which, float x, float y, int dir);



// Bitmap text (window pixel coords; origin = bottom-left)
void draw_text(float x, float y, const char* s, float r, float g, float b);

// Same font as draw_text; returns pixel width for centering.
int  draw_text_width(const char* s);

// Convenience: draws a tiny drop shadow for readability
void draw_text_shadow(float x, float y, const char* s,
                      float r, float g, float b);


// Set ghost with mode:
// mode: 0=SCATTER, 1=CHASE, 2=FRIGHTENED, 3=EATEN (same as your enum)
void draw_set_ghost_state(int which, float x, float y, int dir, int mode);

// Big, bold, centered title using GLUT stroke font (pixel coords).
void draw_title_centered(float cx, float y,
                         const char* s,
                         float px_height,
                         float r, float g, float b);


