/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.cc
 * Copyright (C) Tiziano 2008 <tiziano@tizbac.dyndns.org>
 *
 * main.cc is free software.
 *
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <algorithm>
#include <cmath>
#include <vector>
#include <set>

#include <GL/glew.h>
#include <FTGL/ftgl.h>

#include "GlobalStuff.h"
#include "engine.h"
#include "light.h"
#include "shader.h"
#include "Map/ReadMap.h"
#include "Map/SMF/mapfile.h"
#include "Map/SMF/SmfReadMap.h"
#include "Map/SMF/BFGroundTextures.h"
#include "texturebrowser.h"
#include "undostack.h"

enum BrushShape { BRUSH_CIRCLE = 0, BRUSH_SQUARE = 1, BRUSH_TRIANGLE = 2 };
enum PaintMode { MODE_HEIGHT = 0, MODE_METAL = 1, MODE_PAINT = 2, MODE_COLOUR = 3 };

static const char* modeNames[] = {"height", "metal", "texture", "colour"};
static const char* shapeNames[] = {"circle", "square", "triangle"};

static engine* Main;
static light l0;
gst* gs;
static shader groundshader;
static CSmfReadMap* map = NULL;
static CBFGroundTextures* tex = NULL;
static TextureBrowser browser;
static UndoStack undoStack;
static Stroke currentStroke;
static std::set<int> strokeSeen;

static GLuint emptyTexture = 0;
static unsigned char* metalmap = NULL;
static GLuint* metaltexture = NULL;
static GLint* landscapecache = NULL;
static char* landscapecachestatus = NULL;
static unsigned char* tmpdata = NULL;

// camera
static float camRotX = 0.0f;
static float camRotY = 93.3f;
static float camRotZ = 246.0f;
static float camDist = 20.0f;
static float pivx = 0.0f, pivy = 0.0f, pivz = 0.0f;
static bool orbiting = false;

// brush
static float brushsize = 1.0f;   // 1.0 = 64 map squares radius
static float hval = 0.0f;        // height/metal value the brush applies
static float falloff = 1.0f;     // 1 = hard edge, 3 = very soft
static int brushShape = BRUSH_CIRCLE;
static int mode = MODE_HEIGHT;
static float paintScale = 1.0f;  // map texels per texture texel
static bool browserVisible = false;
static bool startWithBrowser = false;
static unsigned char fixedColour[4] = {200, 180, 120, 255};

static double mx = 0, my = 0, mz = 0;
static double cval = 0.0;
static double ts = 6.0;

struct v3 {
	float x, y, z;
	v3() : x(0), y(0), z(0) {}
	v3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
	v3 crossproduct(const v3& o) const
	{
		return v3(y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x);
	}
};

// ---------------------------------------------------------------- helpers

static int bigSquaresX() { return map->width * 8 / 1024; }
static int bigSquaresY() { return map->height * 8 / 1024; }

float GetH(CSmfReadMap* m, int x, int y)
{
	if (x < 0 || y < 0)
		return 0.0;
	if (x > gs->mapx || y > gs->mapy)
		return 0.0;
	int i = (gs->mapx + 1) * y + x;
	float f = m->heightmap[i];
	if (f > 20)
		f = 0.0;
	return f;
}

static void markBigSquareDirty(int sqX, int sqY)
{
	int bx = sqX / 128;
	int by = sqY / 128;
	if (bx < 0 || by < 0 || bx >= bigSquaresX() || by >= bigSquaresY())
		return;
	landscapecachestatus[by * bigSquaresX() + bx] = 'u';
}

void SetH(CSmfReadMap* m, int x, int y, float value)
{
	if (x < 0 || y < 0)
		return;
	if (x > gs->mapx || y > gs->mapy)
		return;

	int i = (gs->mapx + 1) * y + x;
	if (!strokeSeen.count(i)) {
		strokeSeen.insert(i);
		currentStroke.type = EDIT_HEIGHT;
		currentStroke.index.push_back(i);
		currentStroke.oldValue.push_back(m->heightmap[i]);
	}
	m->heightmap[i] = value;
	markBigSquareDirty(x, y);
}

float GetM(CSmfReadMap* m, int x_, int y_)
{
	int x = x_ / 2;
	int y = y_ / 2;
	if (x < 0 || y < 0)
		return 0.0;
	if (x > m->width / 2 - 1 || y > m->height / 2 - 1)
		return 0.0;
	int i = (m->width / 2) * y + x;
	return (float)metalmap[i] / 255.0f;
}

void SetM(CSmfReadMap* m, int x_, int y_, float value)
{
	int x = x_ / 2;
	int y = y_ / 2;
	if (x < 0 || y < 0)
		return;
	if (x > m->width / 2 - 1 || y > m->height / 2 - 1)
		return;

	markBigSquareDirty(x_ & ~1, y_ & ~1);

	int i = (m->width / 2) * y + x;
	if (!strokeSeen.count(i)) {
		strokeSeen.insert(i);
		currentStroke.type = EDIT_METAL;
		currentStroke.index.push_back(i);
		currentStroke.oldValue.push_back((float)metalmap[i]);
	}
	metalmap[i] = (unsigned char)std::max(0.0, std::min(255.0, value * 255.0));
}

// Paint one map square with coverage `coverage` (0..1) of colour rgba.
static void PaintSquare(int sqX, int sqY, const unsigned char rgba[3], float coverage)
{
	if (coverage <= 0.0f)
		return;

	int bx = sqX / tex->bigSquareSize;
	int by = sqY / tex->bigSquareSize;
	if (bx < 0 || by < 0 || bx >= tex->numBigSquaresX() || by >= tex->numBigSquaresY())
		return;

	int idx = tex->paintTexelIndex(bx, by, sqX % tex->bigSquareSize, sqY % tex->bigSquareSize);

	unsigned char old[4];
	tex->getPaintTexel(idx, old);

	if (!strokeSeen.count(idx)) {
		strokeSeen.insert(idx);
		currentStroke.type = EDIT_PAINT;
		currentStroke.index.push_back(idx);
		currentStroke.oldValue.push_back((float)old[0]);
		currentStroke.oldValue.push_back((float)old[1]);
		currentStroke.oldValue.push_back((float)old[2]);
		currentStroke.oldValue.push_back((float)old[3]);
	}

	float a0 = old[3] / 255.0f;
	float a1 = std::min(1.0f, a0 + coverage);

	unsigned char nw[4];
	nw[0] = (unsigned char)((old[0] * a0 + rgba[0] * coverage) / a1);
	nw[1] = (unsigned char)((old[1] * a0 + rgba[1] * coverage) / a1);
	nw[2] = (unsigned char)((old[2] * a0 + rgba[2] * coverage) / a1);
	nw[3] = (unsigned char)(a1 * 255.0f);

	tex->setPaintTexel(idx, nw);
}

// Weight of the brush at offset (dx,dy) in map squares, 0 outside the brush.
static double brushWeight(double dx, double dy)
{
	double r = brushsize * 64.0;
	if (r <= 0.0)
		return 0.0;

	double d;
	if (brushShape == BRUSH_CIRCLE) {
		d = sqrt(dx * dx + dy * dy) / r;
	} else if (brushShape == BRUSH_SQUARE) {
		d = std::max(fabs(dx), fabs(dy)) / r;
	} else {
		// Right triangle with the right angle at (0,0) and the hypotenuse
		// passing through (r,0) and (0,r): use the distance to the edges.
		double a = -dx;            // distance to the y axis edge
	 double b = -dy;            // distance to the x axis edge
		double c = (dx + dy) / sqrt(2.0); // distance to the hypotenuse
		if (a < 0 || b < 0 || c < 0)
			return 0.0;
		double inradius = r / (1.0 + sqrt(2.0));
		d = 1.0 - std::min(std::min(a, b), c) / inradius;
	}

	if (d >= 1.0)
		return 0.0;
	return pow(1.0 - d, falloff);
}

static void commitStroke()
{
	if (!currentStroke.empty()) {
		undoStack.commit(currentStroke);
		currentStroke.index.clear();
		currentStroke.oldValue.clear();
		currentStroke.type = EDIT_HEIGHT;
		strokeSeen.clear();
		tex->uploadPaintLayers();
	}
}

static void applyValue(const Stroke& s, int i, bool restore)
{
	if (s.type == EDIT_HEIGHT) {
		int idx = s.index[i];
		map->heightmap[idx] = s.oldValue[i];
		markBigSquareDirty(idx % (gs->mapx + 1), idx / (gs->mapx + 1));
	} else if (s.type == EDIT_METAL) {
		int idx = s.index[i];
		metalmap[idx] = (unsigned char)std::max(0.0, std::min(255.0, (double)s.oldValue[i]));
		markBigSquareDirty((idx % (map->width / 2)) * 2, (idx / (map->width / 2)) * 2);
	} else if (s.type == EDIT_PAINT) {
		unsigned char rgba[4] = {
			(unsigned char)s.oldValue[i * 4 + 0],
			(unsigned char)s.oldValue[i * 4 + 1],
			(unsigned char)s.oldValue[i * 4 + 2],
			(unsigned char)s.oldValue[i * 4 + 3]
		};
		tex->setPaintTexel(s.index[i], rgba);
	}
}

// Returns the inverse stroke needed to redo the undo.
static Stroke invertStroke(const Stroke& s)
{
	Stroke r;
	r.type = s.type;

	for (size_t i = 0; i < s.index.size(); ++i) {
		int idx = s.index[i];
		if (s.type == EDIT_HEIGHT) {
			r.index.push_back(idx);
			r.oldValue.push_back(map->heightmap[idx]);
			markBigSquareDirty(idx % (gs->mapx + 1), idx / (gs->mapx + 1));
		} else if (s.type == EDIT_METAL) {
			r.index.push_back(idx);
			r.oldValue.push_back((float)metalmap[idx]);
			markBigSquareDirty((idx % (map->width / 2)) * 2, (idx / (map->width / 2)) * 2);
		} else {
			unsigned char cur[4];
			tex->getPaintTexel(idx, cur);
			r.index.push_back(idx);
			r.oldValue.push_back((float)cur[0]);
			r.oldValue.push_back((float)cur[1]);
			r.oldValue.push_back((float)cur[2]);
			r.oldValue.push_back((float)cur[3]);
		}
	}

	for (size_t i = 0; i < s.index.size(); ++i)
		applyValue(s, (int)i, true);

	return r;
}

static void doUndo()
{
	if (!undoStack.canUndo())
		return;
	Stroke s = undoStack.undoLast();
	Stroke inverse = invertStroke(s);
	undoStack.pushRedo(inverse);
	printf("Undo: %d entries (%s)\n", (int)s.index.size(), modeNames[s.type]);
}

static void doRedo()
{
	if (!undoStack.canRedo())
		return;
	Stroke s = undoStack.redoLast();
	Stroke inverse = invertStroke(s);
	undoStack.pushUndo(inverse);
	printf("Redo: %d entries\n", (int)s.index.size());
}

// Applies the brush centred on the cursor position (in map squares).
static void applyBrush()
{
	int centreX = (int)(mx * 64.0);
	int centreY = (int)(mz * 64.0);
	int r = (int)(brushsize * 64.0) + 1;

	if (r <= 0)
		return;

	const bool smooth = Main->getkey(SDL_SCANCODE_S);

	for (int dy = -r; dy <= r; ++dy) {
		for (int dx = -r; dx <= r; ++dx) {
			double w = brushWeight(dx, dy);
			if (w <= 0.0)
				continue;

		 int x = centreX + dx;
			int y = centreY + dy;
			if (x < 0 || y < 0 || x > gs->mapx || y > gs->mapy)
				continue;

			if (mode == MODE_HEIGHT) {
				if (smooth) {
					float avg = (GetH(map, x - 1, y) + GetH(map, x + 1, y) +
					             GetH(map, x, y - 1) + GetH(map, x, y + 1)) / 4.0f;
					float nv = hval * (float)w + avg * (1.0f - (float)w);
					SetH(map, x, y, nv);
				} else {
					float cur = GetH(map, x, y);
					float nv = hval * (float)w + cur * (1.0f - (float)w);
					SetH(map, x, y, nv);
				}
			} else if (mode == MODE_METAL) {
				if (smooth) {
					float avg = (GetM(map, x - 2, y) + GetM(map, x + 2, y) +
					             GetM(map, x, y - 2) + GetM(map, x, y + 2)) / 4.0;
					float nv = hval * (float)w + avg * (1.0f - (float)w);
					SetM(map, x, y, nv);
				} else {
					float cur = GetM(map, x, y);
					float nv = hval * (float)w + cur * (1.0f - (float)w);
					SetM(map, x, y, nv);
				}
			} else if (mode == MODE_PAINT || mode == MODE_COLOUR) {
				unsigned char col[4];
				if (mode == MODE_PAINT)
					browser.sample(browser.selected, (double)x * 8.0, (double)y * 8.0, paintScale, col);
				else
					memcpy(col, fixedColour, 4);

				PaintSquare(x, y, col, (float)w);
			}
		}
	}

	tex->uploadPaintLayers();
}

static void drawsquare(int x, int y, CSmfReadMap* m, CBFGroundTextures* t, float tilesize)
{
	Main->PushTransform(x * 2, y * 2, 0, 0, 0, 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, t->squares[y * bigSquaresX() + x].texture);
	groundshader.setuniformint("maintex", 0);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, metaltexture[y * bigSquaresX() + x]);
	groundshader.setuniformint("metaltex", 1);

	GLuint paint = t->paintTexture(x, y);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, paint ? paint : emptyTexture);
	groundshader.setuniformint("painttex", 2);
	groundshader.setuniformint("hasPaint", paint ? 1 : 0);

	glActiveTexture(GL_TEXTURE0);
	groundshader.setuniformfloat("scale", 1.0);
	groundshader.setuniformfloat("tx", 0);
	groundshader.setuniformfloat("ty", 0);
	groundshader.setuniformint("mode", mode);

	if (mode == MODE_METAL)
		glDisable(GL_LIGHTING);

	tilesize = pow(2, ts);

	if (Main->pointinview(x * 2, y * 2, 0) || Main->pointinview(x * 2 + 2, y * 2 + 2, 0) ||
		Main->pointinview(x * 2 + 2, y * 2, 0) || Main->pointinview(x * 2, y * 2 + 2, 0))
	{
		if (landscapecachestatus[y * bigSquaresX() + x] == 'e' || landscapecachestatus[y * bigSquaresX() + x] == 'u')
		{
			if (mode == MODE_METAL)
			{
				int mmapsqsize = (m->header.mapx / 2) / bigSquaresX();

                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, metaltexture[y * bigSquaresX() + x]);

                for (int yb = 0; yb < mmapsqsize && yb + y < m->height / 2; ++yb) {
                    for (int xb = 0; xb < mmapsqsize && xb + x < m->width / 2; ++xb)
                        tmpdata[mmapsqsize * yb + xb] = (unsigned char)(255.0 * GetM(m, x * 1024 / 8 + xb * 2, y * 1024 / 8 + yb * 2));
                }
                gluBuild2DMipmaps(GL_TEXTURE_2D, 1, mmapsqsize, mmapsqsize, GL_LUMINANCE, GL_UNSIGNED_BYTE, tmpdata);
                glActiveTexture(GL_TEXTURE0);
            }

		 glNewList(landscapecache[y * bigSquaresX() + x], GL_COMPILE_AND_EXECUTE);

			int sqsize = (int)(1024.0 / tilesize);

			for (int Y = 0; Y < 1024.0 / tilesize; ++Y) {
				for (int X = 0; X < 1024.0 / tilesize; ++X) {
					float z1 = GetH(m, 1.0 / sqsize + x * 128 + X * tilesize / 8, -1.0 / sqsize + y * 128 + Y * tilesize / 8);
					float z2 = GetH(m, 1.0 / sqsize + x * 128 + (X + 1) * tilesize / 8, -1.0 / sqsize + y * 128 + Y * tilesize / 8);
                    float z3 = GetH(m, 1.0 / sqsize + x * 128 + (X + 1) * tilesize / 8, -1.0 / sqsize + y * 128 + (Y + 1) * tilesize / 8);
                    float z4 = GetH(m, 1.0 / sqsize + x * 128 + X * tilesize / 8, -1.0 / sqsize + y * 128 + (Y + 1) * tilesize / 8);

                    float x1 = X * (1.0 / sqsize * 2.0);
                    float x2 = (X + 1) * (1.0 / sqsize * 2.0);
                    float y1 = Y * (1.0 / sqsize * 2.0);
                    float y2 = (Y + 1) * (1.0 / sqsize * 2.0);

                    glBegin(GL_TRIANGLES);

                    v3 a, b, n;
                    a.x = x2 - x1; a.y = 0; a.z = z2 - z1;
                    b.x = x2 - x1; b.y = y2 - y1; b.z = z3 - z1;
                    n = a.crossproduct(b);
                    glNormal3f(n.x, n.y, n.z);
                    glTexCoord2f((float)X / sqsize, (float)Y / sqsize);
                    glVertex3f(x1, y1, z1);
                    glTexCoord2f((float)(X + 1) / sqsize, (float)Y / sqsize);
                    glVertex3f(x2, y1, z2);
                    glTexCoord2f((float)(X + 1) / sqsize, (float)(Y + 1) / sqsize);
                    glVertex3f(x2, y2, z3);

                    a.x = x1 - x2; a.y = 0; a.z = z4 - z3;
                    b.x = x1 - x2; b.y = y1 - y2; b.z = z1 - z3;
                    n = a.crossproduct(b);
                    glNormal3f(n.x, n.y, n.z);
                    glTexCoord2f((float)(X + 1) / sqsize, (float)(Y + 1) / sqsize);
                    glVertex3f(x2, y2, z3);
                    glTexCoord2f((float)X / sqsize, (float)(Y + 1) / sqsize);
                    glVertex3f(x1, y2, z4);
                    glTexCoord2f((float)X / sqsize, (float)Y / sqsize);
                    glVertex3f(x1, y1, z1);

                    glEnd();
                }
            }

            glEndList();
            landscapecachestatus[y * bigSquaresX() + x] = 'o';
        } else {
            glCallList(landscapecache[y * bigSquaresX() + x]);
        }
    }

    if (mode == MODE_METAL)
        glEnable(GL_LIGHTING);

    glBindTexture(GL_TEXTURE_2D, 0);
    Main->PopTransform();
}

static std::string FindFont()
{
	const char* env = getenv("SPRINGMAPEDITOR_FONT");
	if (env && *env && access(env, R_OK) == 0)
		return env;

	static const char* candidates[] = {
		"Vera.ttf",
		"assets/Vera.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	};
	for (const char* path : candidates)
		if (access(path, R_OK) == 0)
			return path;
	return "";
}

// Look for a runtime asset next to the working directory first, then next to
// the executable. Returns the original name if it cannot be found.
static std::string FindAsset(const char* name)
{
	if (access(name, R_OK) == 0)
		return name;

	const char* base = SDL_GetBasePath();
	if (base)
	{
		std::string path = base;
		SDL_free((void*)base);
		path += name;
		if (access(path.c_str(), R_OK) == 0)
			return path;
	}
	return name;
}

static volatile sig_atomic_t interrupt_requested = 0;

static void handle_interrupt(int sig)
{
	(void)sig;
	interrupt_requested = 1;
}

// Draws the help/status overlay in screen space.
static void drawOverlay(FTGLPixmapFont& font, bool haveFont)
{
	Main->OverlayBegin();

	// Status line at the bottom left.
	char line[512];
	snprintf(line, sizeof(line), "%.1f fps   mode: %s   brush: %s   radius: %d   falloff: %.2f   zoom: %.1f",
		Main->getfps(), modeNames[mode], shapeNames[brushShape], (int)(brushsize * 64), falloff, camDist);
	if (haveFont)
		font.Render(line, -1, FTPoint(8, 10));

	snprintf(line, sizeof(line), "brush value: %.3f   ground height: %.3f   undo stack: %d", hval, cval, (int)undoStack.undoSize());
	if (haveFont)
		font.Render(line, -1, FTPoint(8, 26));

	// Key reference on the right.
	const int panelW = 260;
	const int panelX = Main->width - panelW - 6;

	glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
	glBegin(GL_QUADS);
	glVertex2f(panelX, 0);
	glVertex2f(Main->width, 0);
	glVertex2f(Main->width, Main->height);
	glVertex2f(panelX, Main->height);
	glEnd();
	glColor4f(1, 1, 1, 1);

	if (haveFont)
	{
		font.FaceSize(10);
		int y = Main->height - 12;
		const char* keys[][2] = {
			{"Left drag", "paint brush"},
			{"Right drag", "orbit camera"},
			{"Wheel / Q E", "zoom"},
			{"Arrows", "pan pivot"},
			{"A / Z", "brush size"},
			{"C / X", "brush value"},
			{"S", "smooth brush"},
			{"B", "pick ground value"},
			{"P", "brush shape"},
			{"- / =", "brush falloff"},
			{"0 1 2 3", "height metal texture colour"},
			{"T / Y", "texture detail"},
			{"W", "wireframe"},
			{"F", "fullscreen"},
			{"G", "texture browser"},
			{"[ ]", "select texture"},
			{", .", "texture scale"},
			{"Ctrl+Z", "undo"},
			{"Ctrl+Shift+Z", "redo"},
			{"Esc", "quit"},
		};
		for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i) {
			char text[128];
			snprintf(text, sizeof(text), "%-13s %s", keys[i][0], keys[i][1]);
			font.Render(text, -1, FTPoint(panelX + 6, y));
			y -= 13;
		}
		font.FaceSize(12);
	}

	if (browserVisible && !browser.empty())
	{
		const int thumb = 40;
		const int cols = 4;
		int bx0 = panelX - (cols * thumb + 16);
		if (bx0 < 8) bx0 = 8;

		glColor4f(0.0f, 0.0f, 0.0f, 0.6f);
		int rows = (browser.count() + cols - 1) / cols;
		int panelH = rows * thumb + 24;
		glBegin(GL_QUADS);
		glVertex2f(bx0, Main->height - panelH);
		glVertex2f(bx0 + cols * thumb + 12, Main->height - panelH);
		glVertex2f(bx0 + cols * thumb + 12, Main->height);
		glVertex2f(bx0, Main->height);
		glEnd();
		glColor4f(1, 1, 1, 1);

		if (haveFont)
		{
			font.FaceSize(10);
			char text[128];
			snprintf(text, sizeof(text), "Textures (%d), G to close", browser.count());
			font.Render(text, -1, FTPoint(bx0 + 6, Main->height - 14));
			font.FaceSize(12);
		}

		glDisable(GL_LIGHTING);
		// The terrain is drawn with shaders, so fixed-function texturing is off
		// elsewhere; the thumbnails need it.
		glEnable(GL_TEXTURE_2D);
		for (int i = 0; i < browser.count(); ++i) {
			int col = i % cols;
			int row = i / cols;
		 float x0 = bx0 + 6 + col * thumb;
            float y0 = Main->height - 20 - (row + 1) * thumb;

			glBindTexture(GL_TEXTURE_2D, browser.at(i).glTex);
		 glBegin(GL_QUADS);
		 glTexCoord2f(0, 0); glVertex2f(x0, y0);
		 glTexCoord2f(1, 0); glVertex2f(x0 + thumb, y0);
		 glTexCoord2f(1, 1); glVertex2f(x0 + thumb, y0 + thumb);
		 glTexCoord2f(0, 1); glVertex2f(x0, y0 + thumb);
		 glEnd();

			if (i == browser.selected) {
			 glBindTexture(GL_TEXTURE_2D, 0);
                glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
                glBegin(GL_LINE_LOOP);
                glVertex2f(x0 - 1, y0 - 1);
                glVertex2f(x0 + thumb + 1, y0 - 1);
                glVertex2f(x0 + thumb + 1, y0 + thumb + 1);
                glVertex2f(x0 - 1, y0 + thumb + 1);
                glEnd();
                glColor4f(1, 1, 1, 1);
            }
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_TEXTURE_2D);
    }

    glDisable(GL_BLEND);
    Main->OverlayEnd();
}

static int browserHitTest()
{
	if (!browserVisible || browser.empty())
		return -1;

	const int panelW = 260;
	const int panelX = Main->width - panelW - 6;
	const int thumb = 40;
	const int cols = 4;
	int bx0 = panelX - (cols * thumb + 16);
	if (bx0 < 8) bx0 = 8;

	int px = Main->mousex;
	int py = Main->mousey; // from the top
	if (px < bx0 || px > bx0 + cols * thumb + 12)
		return -1;
	int rows = (browser.count() + cols - 1) / cols;
	if (py > Main->height - 20 || py < Main->height - 20 - rows * thumb)
		return -1;

	int row = (Main->height - 20 - py) / thumb;
	int col = (px - bx0 - 6) / thumb;
	if (row < 0 || col < 0 || col >= cols)
		return -1;
	int index = row * cols + col;
	if (index >= browser.count())
		return -1;
	return index;
}

int main(int argc, char** argv)
{
	char txt[512];
	bool quite = false;
	bool checkOnly = (argc > 1 && strcmp(argv[1], "--check") == 0);

	std::string textureDir;
	const char* smffile = NULL;
	std::string smfpathstorage;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--check") == 0) {
			checkOnly = true;
		} else if (strcmp(argv[i], "-textures") == 0 && i + 1 < argc) {
			textureDir = argv[++i];
		} else if (strncmp(argv[i], "-textures=", 10) == 0) {
			textureDir = argv[i] + 10;
		} else if (strcmp(argv[i], "-browser") == 0) {
			startWithBrowser = true;
		} else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf("Usage: %s <map.smf> [map directory] [-textures <dir>] [-browser]\n", argv[0]);
			printf("       %s --check\n", argv[0]);
			return 0;
		} else if (smffile == NULL) {
			smffile = argv[i];
		}
    }

    if (!checkOnly && smffile == NULL)
    {
        fprintf(stderr, "Usage: %s <map.smf> [map directory] [-textures <dir>]\n", argv[0]);
        fprintf(stderr, "       %s --check    (initialize video/OpenGL and exit)\n", argv[0]);
        return 1;
    }

    mode = MODE_HEIGHT;
    browserVisible = startWithBrowser;
    if (argc > 2 && !checkOnly && strcmp(argv[1], "--check") != 0 && argv[2][0] != '-')
        smfpathstorage = argv[2];
    else if (!checkOnly)
    {
        std::string mapfile = smffile;
        size_t slash = mapfile.find_last_of("/\\");
        smfpathstorage = (slash == std::string::npos) ? "." : mapfile.substr(0, slash);
    }

    Main = new engine;
    gs = new gst;

    std::string fontpath = FindFont();
    bool haveFont = !fontpath.empty();
    FTGLPixmapFont font(haveFont ? fontpath.c_str() : "");
    if (haveFont)
        font.FaceSize(12);
    else
        fprintf(stderr, "Warning: no TrueType font found, on-screen text disabled. "
                        "Set SPRINGMAPEDITOR_FONT to a .ttf file.\n");

    if (!Main->Init3d(1024, 768, 8, false))
    {
        fprintf(stderr, "Failed to initialize the video subsystem.\n");
        return 1;
    }

    glGenTextures(1, &emptyTexture);
    glBindTexture(GL_TEXTURE_2D, emptyTexture);
    {
        unsigned char transparent[4] = {0, 0, 0, 0};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, transparent);
    }

    if (checkOnly)
    {
        printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
        printf("OpenGL vendor:    %s\n", glGetString(GL_VENDOR));
        printf("OpenGL renderer:  %s\n", glGetString(GL_RENDERER));
        printf("OpenGL version:   %s\n", glGetString(GL_VERSION));
        Main->Terminate();
        return 0;
    }

    l0.lightinit(GL_LIGHT0, -50, 50, 50, 1, 1, 1, 1);

    map = (CSmfReadMap*)CReadMap::LoadMap(smffile, smfpathstorage.c_str());
    if (map == NULL)
    {
        fprintf(stderr, "Failed to load map '%s' from '%s'\n", smffile, smfpathstorage.c_str());
        Main->Terminate();
        return 1;
    }

    std::string fragmentShader = FindAsset("groundshader.frag");
    std::string vertexShader = FindAsset("groundshader.vert");
    groundshader.createfromfiles(fragmentShader.data(), vertexShader.data());

    int totalSquares = bigSquaresX() * bigSquaresY();
    landscapecache = (GLint*)malloc(totalSquares * sizeof(GLint));
    landscapecachestatus = (char*)malloc(totalSquares + 1);
    landscapecache[0] = glGenLists(totalSquares);
    for (int i = 0; i < totalSquares; ++i) {
        if (i > 0)
            landscapecache[i] = landscapecache[0] + i;
        landscapecachestatus[i] = 'e'; // empty
    }
    landscapecachestatus[totalSquares] = 0;

    metaltexture = (GLuint*)malloc(sizeof(GLuint) * totalSquares);
    glGenTextures(totalSquares, metaltexture);

    MapBitmapInfo mbi;
    mbi.width = map->width / 2;
    mbi.height = map->height / 2;
    metalmap = map->GetInfoMap("metal", &mbi);
    if (!metalmap) {
        mbi.width = map->width / 2;
        mbi.height = map->height / 2;
        metalmap = (unsigned char*)calloc((size_t)(map->width / 2) * (map->height / 2), 1);
    }

    int mmapsqsize = (int)((float)map->header.mapx / 2.0 / (double)bigSquaresX());
    printf("Metal map square size: %i\n", mmapsqsize);
    tmpdata = (unsigned char*)malloc(sizeof(unsigned char) * mmapsqsize * mmapsqsize);

    tex = new CBFGroundTextures(map);

    if (!textureDir.empty()) {
        int n = browser.LoadDirectory(textureDir);
        printf("Loaded %d textures from %s\n", n, textureDir.c_str());
    }

    // Initial camera: look at the centre of the map.
    pivx = map->width * 8.0 / 1024.0;
    pivy = 0.0f;
    pivz = map->height * 8.0 / 1024.0;
    camDist = 20.0f;

    printf("Entering main loop\n");

    SDL_SetWindowRelativeMouseMode(Main->screen, true);
    signal(SIGINT, handle_interrupt);
    signal(SIGTERM, handle_interrupt);

    bool prevUndoKey = false;
    bool painting = false;
    int poly = 0;

    while (quite == false && !Main->quit && !interrupt_requested)
    {
        Main->clearscreen(0.5, 0.5, 0.5, 1.0);
        glLoadIdentity();

        Main->PushTransform(0, 0, 0, 90.0, 0, 0);
        Main->ExtractFrustum();
        glBegin(GL_QUADS);
        glVertex3f(0.0, 0.0, 0.01);
        glVertex3f(map->width * 8 / 1024 * 2, 0.0, 0.0);
        glVertex3f(map->width * 8 / 1024 * 2, map->height * 8 / 1024 * 2, 0);
        glVertex3f(0.0, map->height * 8 / 1024 * 2, 0);
        glEnd();
        Main->PopTransform();

        Main->Make2DPositionInto3DPosition((float)Main->mousex, (float)Main->mousey, &mx, &my, &mz);

        if (Main->getkey(SDL_SCANCODE_ESCAPE))
            quite = true;

        // --- undo / redo ---
        bool undoKey = Main->getkey(SDL_SCANCODE_Z) && (Main->getkey(SDL_SCANCODE_LCTRL) || Main->getkey(SDL_SCANCODE_RCTRL));
        if (undoKey && !prevUndoKey)
        {
            commitStroke();
            if (Main->getkey(SDL_SCANCODE_LSHIFT) || Main->getkey(SDL_SCANCODE_RSHIFT))
                doRedo();
            else
                doUndo();
        }
        prevUndoKey = undoKey;

        // --- camera ---
        if (Main->mouseright)
        {
            if (!orbiting)
            {
                pivx = (float)mx;
                pivy = (float)my;
                pivz = (float)mz;
                orbiting = true;
            }
            camRotY += (float)Main->mouserelx / 8.0;
            camRotZ += (float)Main->mouserely / 8.0;
            SDL_HideCursor();
        }
        else
        {
            orbiting = false;
            SDL_ShowCursor();
        }

        // The camera distance lives in main.cc, so zoom there and let
        // SetCamera() clamp it every frame.
        if (Main->getwup())
            camDist *= 0.85f;
        if (Main->getwdown())
            camDist *= 1.18f;
        if (Main->getkey(SDL_SCANCODE_Q))
            camDist *= 0.97f;
        if (Main->getkey(SDL_SCANCODE_E))
            camDist *= 1.03f;
        camDist = std::max(0.2f, std::min(500.0f, camDist));

        float pan = 20.0f / std::max(1.0, (double)Main->getfps());
        if (Main->up)
            pivx += pan;
        if (Main->down)
            pivx -= pan;
        if (Main->left)
            pivz += pan;
        if (Main->right)
            pivz -= pan;

        // --- brush ---
        if (Main->getkey(SDL_SCANCODE_A) && brushsize < 100)
            brushsize += 5 / std::max(1.0, (double)Main->getfps());
        if (Main->getkey(SDL_SCANCODE_Z) && !Main->getkey(SDL_SCANCODE_LCTRL) && brushsize > 0)
            brushsize -= 5 / std::max(1.0, (double)Main->getfps());
        if (Main->getkey(SDL_SCANCODE_C))
            hval += 5 / std::max(1.0, (double)Main->getfps());
        if (Main->getkey(SDL_SCANCODE_X))
            hval -= 5 / std::max(1.0, (double)Main->getfps());
        if (Main->getkey(SDL_SCANCODE_B))
            hval = (float)cval;
        if (Main->getkey(SDL_SCANCODE_MINUS))
            falloff = std::max(0.2f, falloff - 0.2f);
        if (Main->getkey(SDL_SCANCODE_EQUALS))
            falloff = std::min(4.0f, falloff + 0.2f);

        if (Main->getkey(SDL_SCANCODE_T) && ts > 4)
            ts -= 1;
        if (Main->getkey(SDL_SCANCODE_Y) && ts < 10)
            ts += 1;

        int newMode = mode;
        if (Main->getkey(SDL_SCANCODE_0))
            newMode = MODE_HEIGHT;
        if (Main->getkey(SDL_SCANCODE_1))
            newMode = MODE_METAL;
        if (Main->getkey(SDL_SCANCODE_2))
            newMode = MODE_PAINT;
        if (Main->getkey(SDL_SCANCODE_3))
            newMode = MODE_COLOUR;
        if (newMode != mode)
        {
            commitStroke();
            mode = newMode;
            printf("mode -> %s\n", modeNames[mode]);
            for (int i = 0; i < totalSquares; ++i)
                landscapecachestatus[i] = 'u';
        }

        static bool prevP = false;
        bool pDown = Main->getkey(SDL_SCANCODE_P);
        if (pDown && !prevP)
            brushShape = (brushShape + 1) % 3;
        prevP = pDown;

        if (Main->getkey(SDL_SCANCODE_L))
        {
            if (mode == MODE_METAL)
                glDrawPixels(mbi.width, mbi.height, GL_LUMINANCE, GL_UNSIGNED_BYTE, metalmap);
            else
                glDrawPixels(gs->mapx + 1, gs->mapy + 1, GL_LUMINANCE, GL_FLOAT, map->heightmap);
        }

        if (Main->getkey(SDL_SCANCODE_W))
        {
            glPolygonMode(GL_FRONT, GL_LINE);
            glPolygonMode(GL_BACK, GL_LINE);
        }
        else
        {
            glPolygonMode(GL_FRONT, GL_FILL);
            glPolygonMode(GL_BACK, GL_FILL);
        }

        if (Main->getkey(SDL_SCANCODE_F))
            Main->ToggleFullScreen();

        static bool prevG = false;
        bool gDown = Main->getkey(SDL_SCANCODE_G);
        if (gDown && !prevG)
            browserVisible = !browserVisible;
        prevG = gDown;

        int selectedBefore = browser.selected;
        if (Main->getkey(SDL_SCANCODE_LEFTBRACKET))
            browser.select(browser.selected - 1);
        if (Main->getkey(SDL_SCANCODE_RIGHTBRACKET))
            browser.select(browser.selected + 1);
        if (Main->getkey(SDL_SCANCODE_COMMA))
            paintScale = std::max(0.1f, paintScale * 0.9f);
        if (Main->getkey(SDL_SCANCODE_PERIOD))
            paintScale = std::min(64.0f, paintScale * 1.1f);

        // Painting with the left mouse button, unless the click is on the
        // texture browser.
        int hit = browserHitTest();
        if (hit >= 0)
        {
            static int lastHit = -1;
            if (Main->mouseleft && lastHit != hit)
                browser.select(hit);
            lastHit = hit;
        }
        if (browser.selected != selectedBefore && !browser.empty())
            printf("texture: %s\n", browser.current()->name.c_str());

        Main->SetCamera(pivx, pivy, pivz, camRotX, camRotY, camRotZ, camDist);
        l0.setpos(0.0f, -10.0f, 0.0f);

        Main->PushTransform(0, 0, 0, 90.0, 0, 0);

        for (int y_ = 0; y_ < bigSquaresY(); ++y_)
            for (int x_ = 0; x_ < bigSquaresX(); ++x_)
            {
                groundshader.useshader();
                drawsquare(x_, y_, map, tex, pow(2, ts));
                glUseProgram(0);
            }

        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDisable(GL_LIGHTING);
        glBegin(GL_QUADS);
        glColor4f(0.0, 0.4, 1.0, 0.4);
        glVertex3f(0.0, 0.0, 0.01);
        glVertex3f(map->width * 8 / 1024 * 2, 0.0, 0.01);
        glVertex3f(map->width * 8 / 1024 * 2, map->height * 8 / 1024 * 2, 0.01);
        glVertex3f(0.0, map->height * 8 / 1024 * 2, 0.01);
        glEnd();
        glDisable(GL_BLEND);
        glColor4f(1, 1, 1, 1);
        Main->PopTransform();

        if (Main->mouseleft && hit < 0)
        {
            if (!painting)
            {
                painting = true;
                commitStroke();
            }
            applyBrush();
        }
        else if (painting)
        {
            painting = false;
            commitStroke();
        }

        // Brush outline.
        Main->PushTransform((float)mx, 0, (float)mz, 0, 0, 0);
        glPolygonMode(GL_FRONT, GL_LINE);
        glPolygonMode(GL_BACK, GL_LINE);
        glColor3f(Main->getkey(SDL_SCANCODE_S) ? 0.0 : 1.0, Main->getkey(SDL_SCANCODE_S) ? 1.0 : 0.0, 0.0);

        {
            int r = (int)(brushsize * 64.0);
            glBegin(GL_POLYGON);
            for (int k = 0; k < 360; k += 360 / 64)
            {
                float xx1 = sin((float)k / 360.0 * 2 * 3.141592653) * brushsize;
                float yy1 = cos((float)k / 360.0 * 2 * 3.141592653) * brushsize;
                glVertex3f(xx1, -(GetH(map, (int)(mx * 64.0 + xx1 * 64.0), (int)(mz * 64.0 + yy1 * 64.0)) + 0.1), yy1);
            }
            glEnd();
            glBegin(GL_POLYGON);
            glColor4f(0, 1.0, 1.0, 0.2);
            for (int k = 0; k < 360; k += 360 / 64)
            {
                float xx1 = sin((float)k / 360.0 * 2 * 3.141592653) * brushsize;
                float yy1 = cos((float)k / 360.0 * 2 * 3.141592653) * brushsize;
                glVertex3f(xx1, -(GetH(map, (int)(mx * 64.0 + xx1 * 64.0), (int)(mz * 64.0 + yy1 * 64.0)) + 0.1), yy1);
            }
            glEnd();
            (void)r;
        }

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT, GL_FILL);
        Main->PopTransform();
        glColor4f(1, 1.0, 1.0, 1.0);
        glEnable(GL_LIGHTING);

        cval = GetH(map, (int)(mx * 64.0), (int)(mz * 64.0));
        poly = 0;

        drawOverlay(font, haveFont);

        Main->display();
    }

    commitStroke();

    SDL_SetWindowRelativeMouseMode(Main->screen, false);
    Main->Terminate();

    bool save = false;
    while (true)
    {
        printf("Save changes? [y/n] ");
        fflush(stdout);
        int ans = getchar();
        if (ans == 'y')
        {
            save = true;
            break;
        }
        if (ans == 'n' || ans == EOF)
        {
            save = false;
            break;
        }
    }

    if (save)
    {
        printf("Writing new maps to smf file...\n");
        SMFHeader maph;
        FILE* mapfout = fopen(smffile, "rb+");
        if (!mapfout)
        {
            printf("Error, cannot open the map file to save data\n");
            return 1;
        }
        if (fread(&maph, sizeof(maph), 1, mapfout) != 1)
        {
            printf("Error, cannot read the map header\n");
            fclose(mapfout);
            return 1;
        }
        printf("Header Magic: %s\n", maph.magic);
        printf("metalmapPtr: %i      MAPX=%i    MAPY=%i\n", maph.metalmapPtr, maph.mapx, maph.mapy);
        printf("typemapPtr: %i      MAPX=%i    MAPY=%i\n", maph.typeMapPtr, maph.mapx, maph.mapy);
        printf("heightmapPtr: %i      MAPX=%i    MAPY=%i\n", maph.heightmapPtr, maph.mapx, maph.mapy);
        printf("minimapPtr: %i      MAPX=%i    MAPY=%i\n", maph.minimapPtr, maph.mapx, maph.mapy);

        size_t datasize = (size_t)(maph.mapx + 1) * (maph.mapy + 1) * sizeof(unsigned short);
        printf("(%i+1)*(%i+1)*%zu=%zu\n", maph.mapx, maph.mapy, sizeof(unsigned short), datasize);
        std::vector<unsigned short> hmapdata(datasize);
        printf("Converting height map...\n");
        for (int y = 0; y < maph.mapy + 1; ++y)
            for (int x = 0; x < maph.mapx + 1; ++x)
                hmapdata[y * (maph.mapx + 1) + x] =
                    (unsigned short)std::min(65535.0, 65536.0 * std::max(0.0, std::min(1.0, (double)map->heightmap[y * (maph.mapx + 1) + x])));
        printf("Done converting heightmap\n");

        fseek(mapfout, maph.heightmapPtr, SEEK_SET);
        fwrite(hmapdata.data(), datasize, 1, mapfout);
        fseek(mapfout, maph.metalmapPtr, SEEK_SET);
        fwrite(metalmap, 1, (size_t)(map->width / 2) * (map->height / 2), mapfout);
        fclose(mapfout);

        if (tex->bakePaint())
        {
            printf("Compressing painted tiles...\n");
            if (tex->writeTileFiles(smffile))
                printf("Tile file updated (%d tiles)\n", tex->numTiles());
        }

        printf("Save complete\n");
    }

    glDeleteTextures(totalSquares, metaltexture);
    glDeleteLists(landscapecache[0], totalSquares);
    glDeleteTextures(1, &emptyTexture);
    free(metaltexture);
    free(landscapecache);
    free(tmpdata);
    if (metalmap)
        map->FreeInfoMap("metal", metalmap);
    free(landscapecachestatus);

    delete tex;
    delete map;
    delete Main;
    delete gs;

    printf("Exiting\n");
    return 0;
}
