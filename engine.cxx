/*
 * engine.cxx - SDL window, event handling and the orbit camera.
 */
#include "engine.h"

#include <cmath>
#include <cstdio>

bool engine::Init3d(int width, int height, int bpp, bool fullscreen)
{
	this->width = width;
	this->height = height;
	this->bpp = bpp;
	this->fullscreen = fullscreen;
	this->pivx = 0;
	this->pivy = 0;
	this->pivz = 0;
	this->camdist = 20.0;

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
		return false;
	}

	// Core profile: no display lists, no glBegin, no fixed function lighting.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	this->screen = SDL_CreateWindow("Spring Map Editor", width, height, SDL_WINDOW_OPENGL);
	if (!this->screen)
	{
		fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
		return false;
	}

	this->context = SDL_GL_CreateContext(this->screen);
	if (this->context == nullptr)
	{
		fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
		return false;
	}

	SDL_GL_SetSwapInterval(1);

	// Needed for the editor's numeric text fields.
	SDL_StartTextInput(this->screen);

	if (this->fullscreen)
		SDL_SetWindowFullscreen(this->screen, true);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		fprintf(stderr, "glewInit failed: %s\n", glewGetErrorString(glewInit()));
		return false;
	}
	// GLEW can leave a benign GL_INVALID_ENUM behind on core contexts.
	while (glGetError() != GL_NO_ERROR)
	{
	}

	if (!GLEW_VERSION_3_3)
	{
		fprintf(stderr, "OpenGL 3.3 is required for the core profile renderer\n");
		return false;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

	return true;
}

bool engine::Terminate()
{
	if (this->context)
	{
		SDL_GL_DestroyContext(this->context);
		this->context = nullptr;
	}
	if (this->screen)
	{
		SDL_DestroyWindow(this->screen);
		this->screen = nullptr;
	}
	SDL_Quit();
	return true;
}

bool engine::ToggleFullScreen()
{
	if (this->screen == nullptr)
		return false;

	this->fullscreen = !this->fullscreen;
	SDL_SetWindowFullscreen(this->screen, this->fullscreen);
	return true;
}

float engine::getfps()
{
	return this->rendertime > 0 ? 1000.0f / (float)this->rendertime : 0.0f;
}

bool engine::getkey(SDL_Scancode key)
{
	return SDL_GetKeyboardState(nullptr)[key];
}

bool engine::getwup()
{
	if (this->wup)
	{
		this->wup = false;
		return true;
	}
	return false;
}

bool engine::getwdown()
{
	if (this->wdown)
	{
		this->wdown = false;
		return true;
	}
	return false;
}

bool engine::getkeydown(SDL_Scancode key)
{
	if (key < 0 || key >= SDL_SCANCODE_COUNT)
		return false;
	if (this->keyPressed[key])
	{
		this->keyPressed[key] = false;
		return true;
	}
	return false;
}

bool engine::getmousedown()
{
	if (this->mousedown)
	{
		this->mousedown = false;
		return true;
	}
	return false;
}

int engine::GetMouseX()
{
	return this->mousex;
}
int engine::GetMouseY()
{
	return this->mousey;
}

void engine::clearscreen(float r, float g, float b, float a)
{
	this->cleartime = SDL_GetTicks();

	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	this->mouserelx = 0;
	this->mouserely = 0;

	SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_EVENT_MOUSE_MOTION:
			this->mousex = (int)ev.motion.x;
			this->mousey = (int)ev.motion.y;
			this->mouserelx = (int)ev.motion.xrel;
			this->mouserely = (int)ev.motion.yrel;
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (ev.button.button == SDL_BUTTON_LEFT)
			{
				this->mouseleft = ev.button.down;
				if (ev.button.down)
					this->mousedown = true;
			} else if (ev.button.button == SDL_BUTTON_RIGHT)
				this->mouseright = ev.button.down;
			else if (ev.button.button == SDL_BUTTON_MIDDLE)
				this->mousecenter = ev.button.down;
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			this->wup = ev.wheel.y > 0;
			this->wdown = ev.wheel.y < 0;
			break;

		case SDL_EVENT_TEXT_INPUT:
			this->textInput += ev.text.text;
			break;

		case SDL_EVENT_KEY_DOWN:
			if (!ev.key.repeat && ev.key.scancode < SDL_SCANCODE_COUNT)
				this->keyPressed[ev.key.scancode] = true;
			if (!ev.key.repeat)
			{
				if (ev.key.scancode == SDL_SCANCODE_BACKSPACE)
					this->backspacePressed = true;
				else if (ev.key.scancode == SDL_SCANCODE_RETURN ||
				         ev.key.scancode == SDL_SCANCODE_KP_ENTER)
					this->enterPressed = true;
				else if (ev.key.scancode == SDL_SCANCODE_ESCAPE)
					this->escapePressed = true;
			}
			// fall through
		case SDL_EVENT_KEY_UP:
			switch (ev.key.scancode)
			{
			case SDL_SCANCODE_LEFT:
				this->left = ev.key.down;
				break;
			case SDL_SCANCODE_RIGHT:
				this->right = ev.key.down;
				break;
			case SDL_SCANCODE_UP:
				this->up = ev.key.down;
				break;
			case SDL_SCANCODE_DOWN:
				this->down = ev.key.down;
				break;
			case SDL_SCANCODE_LCTRL:
			case SDL_SCANCODE_RCTRL:
				this->ctrl = ev.key.down;
				break;
			case SDL_SCANCODE_LSHIFT:
			case SDL_SCANCODE_RSHIFT:
				this->shift = ev.key.down;
				break;
			default:
				break;
			}
			break;

		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			if (ev.window.data1 > 0 && ev.window.data2 > 0)
			{
				this->width = ev.window.data1;
				this->height = ev.window.data2;
				this->setBarHeight(this->height / 5);
			}
			break;

		case SDL_EVENT_QUIT:
			this->closeRequested = true;
			break;

		default:
			break;
		}
	}
}

void engine::setBarHeight(int pixels)
{
	if (pixels < 0)
		pixels = 0;
	if (pixels > this->height)
		pixels = this->height;
	this->barHeight = pixels;
}

void engine::setMenuHeight(int pixels)
{
	if (pixels < 0)
		pixels = 0;
	this->menuHeight = pixels;
}

void engine::viewport3D() const
{
	int h = this->viewTop() - this->barHeight;
	if (h < 1)
		h = 1;
	glViewport(0, this->barHeight, this->width, h);
}

void engine::viewportBar() const
{
	glViewport(0, 0, this->width, this->barHeight);
}

void engine::updateCamera()
{
	int vh = this->viewTop() - this->barHeight;
	float aspect = (float)this->width / (float)(vh > 0 ? vh : 1);

	// The depth range follows the orbit distance: a fixed near plane close to
	// the eye and a far plane far beyond the map keeps the terrain from being
	// clipped at any zoom level without wasting depth precision.
	float nearPlane = std::max(0.05f, camdist * 0.02f);
	float farPlane = std::max(800.0f, camdist * 40.0f);
	projection = Mat4::perspective(60.0f, aspect, nearPlane, farPlane);

	Mat4 rotation = Mat4::multiply(Mat4::rotateDeg(camrx, 1.0f, 0.0f, 0.0f),
	                               Mat4::multiply(Mat4::rotateDeg(camry, 0.0f, 1.0f, 0.0f),
	                                              Mat4::rotateDeg(camrz, 0.0f, 0.0f, 1.0f)));

	view = Mat4::multiply(Mat4::translate(0.0f, 0.0f, -camdist),
	                      Mat4::multiply(rotation, Mat4::translate(-pivx, -pivy, -pivz)));

	vp = Mat4::multiply(projection, view);
	invVP = vp.inverse();

	// Eye position, used by the HUD.
	float ex, ey, ez;
	Mat4 invView = view.inverse();
	invView.transformPoint(0.0f, 0.0f, 0.0f, ex, ey, ez);
	camx = ex;
	camy = ey;
	camz = ez;

	// Frustum planes. A clip space plane a (inside is a . clip <= 0) becomes the
	// world plane (VP^T a) . point = 0. Store it negated so that "inside" is the
	// positive side, which is what the culling tests expect.
	static const float clipPlanes[6][4] = {
	    {1.0f, 0.0f, 0.0f, -1.0f},  // right
	    {-1.0f, 0.0f, 0.0f, -1.0f}, // left
	    {0.0f, 1.0f, 0.0f, -1.0f},  // top
	    {0.0f, -1.0f, 0.0f, -1.0f}, // bottom
	    {0.0f, 0.0f, 1.0f, -1.0f},  // far
	    {0.0f, 0.0f, -1.0f, -1.0f}, // near
	};

	for (int p = 0; p < 6; ++p)
	{
		for (int i = 0; i < 4; ++i)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += vp.m[i * 4 + k] * clipPlanes[p][k];
			frustum[p][i] = -sum;
		}
	}

	for (int p = 0; p < 6; ++p)
	{
		float len = sqrtf(frustum[p][0] * frustum[p][0] + frustum[p][1] * frustum[p][1] +
		                  frustum[p][2] * frustum[p][2]);
		if (len > 1e-8f)
		{
			frustum[p][0] /= len;
			frustum[p][1] /= len;
			frustum[p][2] /= len;
			frustum[p][3] /= len;
		}
	}
}

bool engine::pointInView(float x, float y, float z) const
{
	for (int p = 0; p < 6; ++p)
		if (frustum[p][0] * x + frustum[p][1] * y + frustum[p][2] * z + frustum[p][3] <= 0)
			return false;
	return true;
}

bool engine::pickGround(int px, int py, double *ox, double *oy, double *oz) const
{
	if (px < 0 || py < 0 || px >= this->width || py >= this->height)
		return false;

	int gy = this->height - 1 - py; // GL window coordinates, origin bottom left
	if (gy < this->barHeight || gy >= this->viewTop())
		return false;

	float depth = 1.0f;
	glReadPixels(px, gy, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

	int vpW = this->width;
	int vpH = this->viewTop() - this->barHeight;
	if (vpH < 1)
		return false;

	float ndcX = 2.0f * (float)px / (float)vpW - 1.0f;
	float ndcY = 2.0f * (float)(gy - this->barHeight) / (float)vpH - 1.0f;

	if (depth < 1.0f)
	{
		float ndcZ = 2.0f * depth - 1.0f;
		float x, y, z;
		invVP.transformPoint(ndcX, ndcY, ndcZ, x, y, z);
		*ox = x;
		*oy = y;
		*oz = z;
		return true;
	}

	// Nothing was hit: intersect the ray with the ground plane y = 0.
	float p0x, p0y, p0z, p1x, p1y, p1z;
	invVP.transformPoint(ndcX, ndcY, -1.0f, p0x, p0y, p0z);
	invVP.transformPoint(ndcX, ndcY, 1.0f, p1x, p1y, p1z);

	float dirY = p1y - p0y;
	if (std::fabs(dirY) < 1e-9f)
		return false;

	float t = -p0y / dirY;
	if (t < 0.0f)
		return false;

	*ox = p0x + (p1x - p0x) * t;
	*oy = 0.0;
	*oz = p0z + (p1z - p0z) * t;
	return true;
}

bool engine::groundPoint(int px, int py, float *x, float *z) const
{
	int vpW = this->width;
	int vpH = this->viewTop() - this->barHeight;
	if (vpH < 1 || px < 0 || px >= vpW || py < 0 || py >= vpH)
		return false;

	float ndcX = 2.0f * (float)px / (float)vpW - 1.0f;
	float ndcY = 2.0f * (float)(vpH - 1 - py) / (float)vpH - 1.0f;

	float p0x, p0y, p0z, p1x, p1y, p1z;
	invVP.transformPoint(ndcX, ndcY, -1.0f, p0x, p0y, p0z);
	invVP.transformPoint(ndcX, ndcY, 1.0f, p1x, p1y, p1z);

	float dirY = p1y - p0y;
	if (std::fabs(dirY) < 1e-9f)
		return false;

	float t = -p0y / dirY;
	if (t < 0.0f)
		return false;

	*x = p0x + (p1x - p0x) * t;
	*z = p0z + (p1z - p0z) * t;
	return true;
}

void engine::display()
{
	SDL_GL_SwapWindow(this->screen);
	this->rendertime = SDL_GetTicks() - this->cleartime;
}
