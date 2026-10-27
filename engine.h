/*
 * engine.h - SDL window, event handling and the orbit camera.
 *
 * The camera is built as a matrix instead of using the fixed function
 * transformation stack, so nothing here calls glMatrixMode/glLoadMatrix.
 */
#ifndef ENGINE_H
#define ENGINE_H

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include "mat4.h"

class engine {
  public:
	int height = 0;
	int width = 0;
	int bpp = 24;

	// Pixels reserved along the bottom of the window for the editor bar.
	int barHeight = 0;

	float camx = 0.0f, camy = 0.0f, camz = 0.0f;
	float pivx = 0.0f, pivy = 0.0f, pivz = 0.0f;
	float camdist = 20.0f;
	float camrx = 0.0f, camry = 0.0f, camrz = 0.0f;

	int mousex = 0, mousey = 0;
	int mouserelx = 0, mouserely = 0;
	bool mouseleft = false, mouseright = false, mousecenter = false;
	bool mousedown = false;
	bool up = false, down = false, left = false, right = false;
	bool wup = false, wdown = false;
	bool ctrl = false, shift = false;
	bool keyPressed[SDL_SCANCODE_COUNT] = {};

	Mat4 view, projection, vp, invVP;
	float frustum[6][4];

	unsigned long long cleartime = 0, rendertime = 0;
	bool fullscreen = false;
	bool quit = false;

	SDL_Window *screen = nullptr;
	SDL_GLContext context = nullptr;

	bool Init3d(int width = 800, int height = 600, int bpp = 24, bool fullscreen = false);
	bool Terminate();
	bool ToggleFullScreen();
	float getfps();
	bool getkey(SDL_Scancode key);
	bool getwup();
	bool getwdown();
	bool getmousedown();

	// True once per key press (auto repeat is ignored) and consumed by the call.
	bool getkeydown(SDL_Scancode key);
	int GetMouseX();
	int GetMouseY();

	void clearscreen(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
	void setBarHeight(int pixels);

	// Rebuilds the view/projection matrices and the frustum from the camera
	// parameters. Must be called before drawing the 3D scene.
	void updateCamera();

	// Sets the viewport to the part of the window above the editor bar.
	void viewport3D() const;
	// Sets the viewport to the editor bar itself.
	void viewportBar() const;

	bool pointInView(float x, float y, float z) const;

	// World position of the pixel (px, py) given in window coordinates with y
	// measured from the top. Uses the depth buffer; when the pixel is empty the
	// ray is intersected with the ground plane y = 0.
	bool pickGround(int px, int py, double *ox, double *oy, double *oz) const;

	void display();
};

#endif // ENGINE_H
