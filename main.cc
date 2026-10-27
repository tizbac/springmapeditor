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
#include <algorithm>
#include <cmath>
#include <set>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef HAVE_FREEIMAGE
#include <FreeImage.h>
#endif

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include "Map/GlobalStuff.h"
#include "Map/ReadMap.h"
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"
#include "Map/SMF/mapfile.h"
#include "engine.h"
#include "font.h"
#include "mapwriter.h"
#include "mat4.h"
#include "terrain.h"
#include "texturebrowser.h"
#include "ui.h"
#include "undostack.h"

#include <archive.h>
#include <archive_entry.h>

enum BrushShape { BRUSH_CIRCLE = 0, BRUSH_SQUARE = 1, BRUSH_TRIANGLE = 2 };
enum PaintMode { MODE_HEIGHT = 0, MODE_METAL = 1, MODE_PAINT = 2, MODE_COLOUR = 3 };
enum Picker { PICK_NONE = 0, PICK_MAP = 1, PICK_TEXTURES = 2, PICK_PROJECT = 3, PICK_MAPTEX = 4 };

// Modal dialogs.
enum Dialog {
	DLG_NONE = 0,
	DLG_EXIT,
	DLG_SAVE_AS,
	DLG_EXPORT,
	DLG_NEW,
	DLG_CONFIRM,
	DLG_MESSAGE,
	DLG_PROGRESS
};

enum Action {
	ACT_NONE = 0,
	ACT_OPEN_MAP,
	ACT_SAVE,
	ACT_OPEN_TEXTURES,
	ACT_SHAPE_CIRCLE,
	ACT_SHAPE_SQUARE,
	ACT_SHAPE_TRIANGLE,
	ACT_SIZE_DOWN,
	ACT_SIZE_UP,
	ACT_FALL_DOWN,
	ACT_FALL_UP,
	ACT_VALUE_DOWN,
	ACT_VALUE_UP,
	ACT_BRUSH_SIGN,
	ACT_BRUSH_APPLY,
	ACT_MODE_HEIGHT,
	ACT_MODE_METAL,
	ACT_MODE_TEXTURE,
	ACT_MODE_COLOUR,
	ACT_RECT_FILL,
	ACT_HELP,
	ACT_DEBUG,
	ACT_QUIT,
	ACT_PICKER_PREV,
	ACT_PICKER_NEXT,
	ACT_PICKER_UP,
	ACT_PICKER_CLOSE,
	ACT_PALETTE_OPEN,
	ACT_PALETTE_CLOSE,
	ACT_MENU_FILE,
	ACT_MENU_EDIT,
	ACT_MENU_VIEW,
	ACT_NEW_PROJECT,
	ACT_OPEN_PROJECT,
	ACT_SAVE_PROJECT,
	ACT_SAVE_PROJECT_AS,
	ACT_IMPORT_MAP,
	ACT_EXPORT_MAP,
	ACT_IMPORT_TEXTURE,
	ACT_APPLY_TEXTURE,
	ACT_EDIT_UNDO,
	ACT_EDIT_REDO,
	ACT_MAP_SETTINGS,
	ACT_MAP_CLOSE,
	ACT_WATER_TOGGLE,

	ACT_TEXTURE_BASE = 1000,    // + index in the texture list
	ACT_COLOUR_BASE = 2000,     // + 0 red, 1 green, 2 blue
	ACT_PICKER_BASE = 3000,     // + index in the current directory listing
	ACT_PALETTE_BASE = 10000,   // + index in the colour palette
	ACT_SLIDER_BASE = 11000,    // + 0 min z, 1 max z, 2 water z
	ACT_FIELD_BASE = 12000,     // + index in the map info fields
	ACT_MENU_ITEM_BASE = 20000, // + index in the open menu
	ACT_DIALOG_BASE = 21000     // + index of the dialog button
};

// Dialog kinds.

// Menu items, per menu.
struct MenuItem {
	const char *label;
	int action;
};
static const MenuItem fileMenu[] = {
    {"New map...", ACT_NEW_PROJECT},
    {"Open project...", ACT_OPEN_PROJECT},
    {"Save project", ACT_SAVE_PROJECT},
    {"Save project as...", ACT_SAVE_PROJECT_AS},
    {"Import map...", ACT_IMPORT_MAP},
    {"Import map texture...", ACT_IMPORT_TEXTURE},
    {"Export map...", ACT_EXPORT_MAP},
    {"Texture folder...", ACT_OPEN_TEXTURES},
    {"Quit", ACT_QUIT},
};
static const MenuItem editMenu[] = {
    {"Undo", ACT_EDIT_UNDO},
    {"Redo", ACT_EDIT_REDO},
    {"Map settings...", ACT_MAP_SETTINGS},
};
static const MenuItem viewMenu[] = {
    {"Help panel", ACT_HELP},
    {"Metal / height overlay", ACT_DEBUG},
};
static const MenuItem *menus[3] = {fileMenu, editMenu, viewMenu};
static const int menuCounts[3] = {9, 3, 3};
static int openMenu = -1;
static int dialog = DLG_NONE;
static std::string dialogPath;
static std::string dialogTitle;
static std::string dialogMessage;
static bool dirty = false;
static std::string projectDir;
static float progressValue = 0.0f;
static std::string progressText;
static bool quitAfterSave = false;
static bool dialogTextActive = false;
static int dialogField = 0; // 0 = the path field, 1 = the size field
static int confirmAction = 0;
static std::string pendingTexturePath;
static std::string dialogSizeText = "1024x1024";
static const int kMenuHeight = 24;

static const char *modeNames[] = {"height", "metal", "texture", "colour"};
static const char *shapeNames[] = {"circle", "square", "triangle"};

static engine *Main = nullptr;
gst *gs = nullptr;

static CSmfReadMap *map = nullptr;
static CBFGroundTextures *tex = nullptr;
static TextureBrowser browser;
static UndoStack undoStack;
static Stroke currentStroke;
static std::set<int> strokeSeen;

static Terrain terrain;
static FlatRenderer ui;
static Font fontSmall, fontLarge;

static unsigned char *metalmap = nullptr;
static GLuint metalTexture = 0;
static GLuint heightTexture = 0;
static int metalWidth = 0, metalHeight = 0;

static std::string mapPath;
static std::string mapDir;
static std::string textureDir;

// camera
static float camRotX = 55.0f;
static float camRotY = 20.0f;
static float camRotZ = 0.0f;
static float camDist = 17.0f;
static float pivx = 0.0f, pivy = 0.0f, pivz = 0.0f;
static bool orbiting = false;

// brush
static int detail = 6;             // lower is a finer mesh
static float brushsize = 1.0f;     // 1.0 = 64 map squares radius
static float brushstrength = 1.0f; // 0.05 .. 5, scales the per stamp change
static bool brushSubtract = false; // true: the brush digs instead of raising
static bool brushSetMode = false;  // true: stamps the brush value instead of adding
static float setHeight = 0.0f;     // target height in world units (set mode)
static float setMetal = 1.0f;      // target metal 0..1 (set mode)
static float falloff = 1.0f;       // 1 = hard edge, 3 = very soft
static int brushShape = BRUSH_CIRCLE;
static int mode = MODE_HEIGHT;
static float paintScale = 1.0f; // map squares per texture texel
static unsigned char fixedColour[4] = {200, 180, 120, 255};

static bool helpVisible = true;
static bool rectFill = false;
static bool rectActive = false;
static double rectAx = 0, rectAz = 0;
static int debugView = 0; // 0 none, 1 metal, 2 height

static double mx = 0, my = 0, mz = 0;
static double cval = 0.0;
static bool painting = false;

static bool paletteVisible = false;
static bool mapSettingsVisible = false;

// Height range of the map (the SMF header's minHeight/maxHeight) and the water
// settings from the map info file. Water is at y = waterHeight in the editor;
// Spring itself keeps its water plane at zero.
static float mapMinZ = 0.0f, mapMaxZ = 1.0f;
static bool waterPresent = false;
static float waterHeight = 0.0f;

// Values of the map info file ([MAP] and [ATMOSPHERE] sections).
static float infoTidalStrength = 0.0f;
static float infoMaxMetal = 0.02f;
static float infoMinWind = 5.0f;
static float infoMaxWind = 25.0f;

// Text editing state for the map info fields.
enum InfoField { FIELD_TIDAL = 0, FIELD_MAXMETAL, FIELD_MINWIND, FIELD_MAXWIND, FIELD_COUNT };
static float infoValues[FIELD_COUNT] = {0.0f, 0.02f, 5.0f, 25.0f};
static int activeField = -1;
static std::string fieldText;
static int activeSlider = -1;
static float sliderRect[3][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

// Preset colours: one row per shade, the last row is a grey ramp.
static const int kPaletteCols = 24;
static const int kPaletteRows = 8;

static void hsvToRgb(float h, float s, float v, unsigned char out[3])
{
	float c = v * s;
	float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
	float m = v - c;
	float r = 0.0f, g = 0.0f, b = 0.0f;
	int sector = (int)(h * 6.0f) % 6;
	switch (sector)
	{
	case 0:
		r = c;
		g = x;
		break;
	case 1:
		r = x;
		g = c;
		break;
	case 2:
		g = c;
		b = x;
		break;
	case 3:
		g = x;
		b = c;
		break;
	case 4:
		r = x;
		b = c;
		break;
	default:
		r = c;
		b = x;
		break;
	}
	out[0] = (unsigned char)((r + m) * 255.0f);
	out[1] = (unsigned char)((g + m) * 255.0f);
	out[2] = (unsigned char)((b + m) * 255.0f);
}

static void paletteColour(int index, unsigned char out[3])
{
	int row = index / kPaletteCols;
	int col = index % kPaletteCols;
	if (row >= kPaletteRows - 1)
	{
		unsigned char v = (unsigned char)(col * 255 / (kPaletteCols - 1));
		out[0] = out[1] = out[2] = v;
		return;
	}
	float h = (float)col / (float)kPaletteCols;
	float v = 1.0f - (float)row * 0.12f;
	hsvToRgb(h, 1.0f, v, out);
}

static void requestQuit();
static void saveProject(const std::string &dir);
static void openProject(const std::string &dir);
static void createNewMap(const std::string &path, const std::string &sizeText);
static bool loadImageRgb(const std::string &path, std::vector<unsigned char> &rgb, int &outW,
                         int &outH);
static std::string fileStem(const std::string &path);
static std::string extractArchive(const std::string &archivePath, const std::string &dir);

static int picker = PICK_NONE;
static std::string pickerDir;
static int pickerOffset = 0;

static volatile sig_atomic_t interrupt_requested = 0;

static void handle_interrupt(int sig)
{
	(void)sig;
	interrupt_requested = 1;
}

// ---------------------------------------------------------------- helpers

static int bigSquaresX()
{
	return map->width * 8 / 1024;
}
static int bigSquaresY()
{
	return map->height * 8 / 1024;
}

float GetH(CSmfReadMap *m, int x, int y)
{
	if (x < 0 || y < 0)
		return 0.0;
	if (x > gs->mapx || y > gs->mapy)
		return 0.0;
	int i = (gs->mapx + 1) * y + x;
	float f = m->heightmap[i];
	if (f < 0.0f)
		f = 0.0f;
	if (f > 1.0f)
		f = 1.0f;
	return f;
}

// One world unit is 64 map squares and a square is 8 elmos, so heights (stored
// in elmos, like the SMF header's minHeight/maxHeight) are scaled by this to be
// drawn in the same units as the ground plane.
static const float kElmosPerUnit = 512.0f;

// Height of a map square in world units (for drawing).
static float GetHWorld(int x, int y)
{
	return (mapMinZ + GetH(map, x, y) * (mapMaxZ - mapMinZ)) / kElmosPerUnit;
}

static void markTerrainDirty(int squareX, int squareY)
{
	int bx = squareX / 128;
	int by = squareY / 128;
	if (bx < 0 || by < 0 || bx >= bigSquaresX() || by >= bigSquaresY())
		return;
	terrain.markDirty(bx, by);

	// Vertices on a square border sample heights from the neighbouring square,
	// so refresh those too when the edit is close to an edge.
	int px = squareX - bx * 128;
	int py = squareY - by * 128;
	if (px < 2)
		terrain.markDirty(bx - 1, by);
	if (px > 125)
		terrain.markDirty(bx + 1, by);
	if (py < 2)
		terrain.markDirty(bx, by - 1);
	if (py > 125)
		terrain.markDirty(bx, by + 1);
}

static void rebuildMetalTexture()
{
	if (!metalTexture)
		glGenTextures(1, &metalTexture);

	glBindTexture(GL_TEXTURE_2D, metalTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::vector<unsigned char> rgba((size_t)metalWidth * metalHeight * 4, 0);
	for (int i = 0; i < metalWidth * metalHeight; ++i)
	{
		rgba[(size_t)i * 4 + 0] = metalmap[i];
		rgba[(size_t)i * 4 + 1] = metalmap[i];
		rgba[(size_t)i * 4 + 2] = metalmap[i];
		rgba[(size_t)i * 4 + 3] = 255;
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, metalWidth, metalHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	             rgba.data());
}

static void rebuildHeightTexture()
{
	int w = gs->mapx + 1, h = gs->mapy + 1;
	if (!heightTexture)
		glGenTextures(1, &heightTexture);

	glBindTexture(GL_TEXTURE_2D, heightTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	std::vector<unsigned char> rgba((size_t)w * h * 4, 255);
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			float v = map->heightmap[(size_t)y * w + x];
			unsigned char c = (unsigned char)std::min(255.0, std::max(0.0, v / 20.0 * 255.0));
			size_t i = ((size_t)y * w + x) * 4;
			rgba[i + 0] = c;
			rgba[i + 1] = c;
			rgba[i + 2] = c;
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}

static float GetM(CSmfReadMap *m, int x_, int y_)
{
	int x = x_ / 2;
	int y = y_ / 2;
	if (x < 0 || y < 0)
		return 0.0;
	if (x > m->width / 2 - 1 || y > m->height / 2 - 1)
		return 0.0;
	int i = (m->width / 2) * y + x;
	return (float)metalmap[i] / 255.0;
}

static void SetM(CSmfReadMap *m, int x_, int y_, float value)
{
	int x = x_ / 2;
	int y = y_ / 2;
	if (x < 0 || y < 0)
		return;
	if (x > m->width / 2 - 1 || y > m->height / 2 - 1)
		return;

	int i = (m->width / 2) * y + x;
	if (!strokeSeen.count(i))
	{
		strokeSeen.insert(i);
		currentStroke.type = EDIT_METAL;
		currentStroke.index.push_back(i);
		currentStroke.oldValue.push_back((float)metalmap[i]);
	}
	metalmap[i] = (unsigned char)std::max(0.0, std::min(255.0, value * 255.0));
}

static void SetH(CSmfReadMap *m, int x, int y, float value)
{
	if (x < 0 || y < 0)
		return;
	if (x > gs->mapx || y > gs->mapy)
		return;

	int i = (gs->mapx + 1) * y + x;
	if (!strokeSeen.count(i))
	{
		strokeSeen.insert(i);
		currentStroke.type = EDIT_HEIGHT;
		currentStroke.index.push_back(i);
		currentStroke.oldValue.push_back(m->heightmap[i]);
	}
	m->heightmap[i] = value;
	markTerrainDirty(x, y);
}

static double brushWeight(double dx, double dy, double r)
{
	double d;
	if (brushShape == BRUSH_CIRCLE)
	{
		d = sqrt(dx * dx + dy * dy) / r;
	} else if (brushShape == BRUSH_SQUARE)
	{
		d = std::max(fabs(dx), fabs(dy)) / r;
	} else
	{
		// Right triangle with the right angle at (0,0) and the hypotenuse
		// through (r,0) and (0,r): use the distance to the three edges.
		double a = -dx;
		double b = -dy;
		double c = (dx + dy) / sqrt(2.0);
		if (a < 0 || b < 0 || c < 0)
			return 0.0;
		double inradius = r / (1.0 + sqrt(2.0));
		d = 1.0 - std::min(std::min(a, b), c) / inradius;
	}

	if (d >= 1.0)
		return 0.0;
	return pow(1.0 - d, falloff);
}

// Paints one map square with the current mode and value, weighted by w.
static void applyAt(int x, int y, double w)
{
	if (x < 0 || y < 0 || x > gs->mapx || y > gs->mapy)
		return;

	const bool smooth = Main->getkey(SDL_SCANCODE_S);

	if (mode == MODE_HEIGHT)
	{
		float cur = GetH(map, x, y);
		float target;
		if (smooth)
		{
			// Move the square towards the average of its neighbours.
			float avg = (GetH(map, x - 1, y) + GetH(map, x + 1, y) + GetH(map, x, y - 1) +
			             GetH(map, x, y + 1)) /
			            4.0f;
			target = cur + (avg - cur) * (float)w;
		} else if (brushSetMode)
		{
			// Stamp the chosen height: the value is approached by the brush
			// weight, so repeating a stamp does not add up.
			float range = mapMaxZ - mapMinZ;
			if (range < 0.001f)
				return;
			float targetN = (setHeight - mapMinZ) / range;
			target = cur + (targetN - cur) * (float)w;
		} else
		{
			// Additive: the height map is normalised, so convert the world space
			// strength into a normalised step and clamp to the height range.
			float range = mapMaxZ - mapMinZ;
			if (range < 0.001f)
				return;
			float step = (5.0f * brushstrength) * (float)w / range;
			target = cur + (brushSubtract ? -step : step);
		}
		SetH(map, x, y, std::max(0.0f, std::min(1.0f, target)));
		return;
	}

	if (mode == MODE_METAL)
	{
		float cur = GetM(map, x, y);
		if (smooth)
		{
			float avg = (GetM(map, x - 2, y) + GetM(map, x + 2, y) + GetM(map, x, y - 2) +
			             GetM(map, x, y + 2)) /
			            4.0;
			SetM(map, x, y, cur + (avg - cur) * (double)w);
		} else if (brushSetMode)
		{
			cur = cur + (setMetal - cur) * (float)w;
		} else
		{
			float step = (0.25f * brushstrength) * (float)w;
			cur = cur + (brushSubtract ? -step : step);
		}
		SetM(map, x, y, std::max(0.0f, std::min(1.0f, cur)));
		return;
	}

	int bx = x / 128;
	int by = y / 128;
	int px = x - bx * 128;
	int py = y - by * 128;
	int paintIndex = tex->paintTexelIndex(bx, by, px, py);

	unsigned char old[4];
	tex->getPaintTexel(paintIndex, old);

	if (!strokeSeen.count(paintIndex))
	{
		strokeSeen.insert(paintIndex);
		currentStroke.type = EDIT_PAINT;
		currentStroke.index.push_back(paintIndex);
		currentStroke.oldValue.push_back((float)old[0]);
		currentStroke.oldValue.push_back((float)old[1]);
		currentStroke.oldValue.push_back((float)old[2]);
		currentStroke.oldValue.push_back((float)old[3]);
	}

	unsigned char dest[4];
	if (mode == MODE_COLOUR)
	{
		// Blend the fixed colour over whatever is already painted, weighted by
		// the brush falloff, instead of replacing it outright.
		dest[0] = (unsigned char)((int)old[0] + ((int)fixedColour[0] - (int)old[0]) * w);
		dest[1] = (unsigned char)((int)old[1] + ((int)fixedColour[1] - (int)old[1]) * w);
		dest[2] = (unsigned char)((int)old[2] + ((int)fixedColour[2] - (int)old[2]) * w);
		dest[3] = (unsigned char)std::min(255.0, std::max((double)old[3], 255.0 * w));
	} else
	{
		unsigned char sampled[4] = {255, 255, 255, 0};
		if (browser.current())
			browser.sample(browser.selected, (double)px * paintScale, (double)py * paintScale,
			               paintScale, sampled);

		dest[0] = (unsigned char)((int)old[0] + ((int)sampled[0] - (int)old[0]) * w);
		dest[1] = (unsigned char)((int)old[1] + ((int)sampled[1] - (int)old[1]) * w);
		dest[2] = (unsigned char)((int)old[2] + ((int)sampled[2] - (int)old[2]) * w);
		dest[3] = (unsigned char)std::min(255.0, std::max((double)old[3], 255.0 * w));
	}

	tex->setPaintTexel(paintIndex, dest);
}

static void commitStroke()
{
	if (currentStroke.empty())
		return;

	undoStack.commit(currentStroke);
	dirty = true;
	currentStroke.index.clear();
	currentStroke.oldValue.clear();
	currentStroke.type = EDIT_HEIGHT;
	strokeSeen.clear();

	tex->uploadPaintLayers();
	rebuildMetalTexture();
	if (debugView == 2)
		rebuildHeightTexture();
}

static void applyValue(const Stroke &s, size_t i)
{
	if (s.type == EDIT_HEIGHT)
	{
		int idx = s.index[i];
		map->heightmap[idx] = s.oldValue[i];
		markTerrainDirty(idx % (gs->mapx + 1), idx / (gs->mapx + 1));
	} else if (s.type == EDIT_METAL)
	{
		int idx = s.index[i];
		metalmap[idx] = (unsigned char)std::max(0.0, std::min(255.0, (double)s.oldValue[i]));
	} else if (s.type == EDIT_PAINT)
	{
		unsigned char rgba[4] = {
		    (unsigned char)s.oldValue[i * 4 + 0], (unsigned char)s.oldValue[i * 4 + 1],
		    (unsigned char)s.oldValue[i * 4 + 2], (unsigned char)s.oldValue[i * 4 + 3]};
		tex->setPaintTexel(s.index[i], rgba);
	}
}

// Returns the stroke that undoes the undo, i.e. the redo stroke.
static Stroke invertStroke(const Stroke &s)
{
	Stroke r;
	r.type = s.type;

	for (size_t i = 0; i < s.index.size(); ++i)
	{
		int idx = s.index[i];
		if (s.type == EDIT_HEIGHT)
		{
			r.index.push_back(idx);
			r.oldValue.push_back(map->heightmap[idx]);
		} else if (s.type == EDIT_METAL)
		{
			r.index.push_back(idx);
			r.oldValue.push_back((float)metalmap[idx]);
		} else
		{
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
		applyValue(s, i);

	return r;
}

static void doUndo()
{
	if (!undoStack.canUndo())
		return;

	Stroke s = undoStack.undoLast();
	Stroke inverse = invertStroke(s);
	undoStack.pushRedo(inverse);

	tex->uploadPaintLayers();
	rebuildMetalTexture();
	if (debugView == 2)
		rebuildHeightTexture();

	printf("Undo: %d entries (%s)\n", (int)s.index.size(), modeNames[s.type]);
}

static void doRedo()
{
	if (!undoStack.canRedo())
		return;

	Stroke s = undoStack.redoLast();
	Stroke inverse = invertStroke(s);
	undoStack.pushUndo(inverse);

	tex->uploadPaintLayers();
	rebuildMetalTexture();
	if (debugView == 2)
		rebuildHeightTexture();

	printf("Redo: %d entries\n", (int)s.index.size());
}

static void applyBrushAt(double cx, double cz)
{
	int centreX = (int)(cx * 64.0);
	int centreY = (int)(cz * 64.0);
	int r = (int)(brushsize * 64.0) + 1;

	if (r <= 0)
		return;

	for (int dy = -r; dy <= r; ++dy)
		for (int dx = -r; dx <= r; ++dx)
		{
			double w = brushWeight(dx, dy, r);
			if (w <= 0.0)
				continue;
			applyAt(centreX + dx, centreY + dy, w);
		}
}

// Adjusts the brush strength (add mode) or the stamped value (set mode).
static void stepBrushValue(float amount)
{
	if (brushSetMode)
	{
		if (mode == MODE_METAL)
			setMetal = std::max(0.0f, std::min(1.0f, setMetal + amount * 0.3f));
		else
			setHeight += amount * 60.0f;
	} else
	{
		brushstrength = std::max(0.05f, std::min(5.0f, brushstrength + amount));
	}
}

// The stroke is drawn as a chain of brush stamps between where the cursor was
// on the previous frame and where it is now, otherwise a fast drag would leave
// gaps between the stamps.
static void paintStrokeSegment()
{
	static double lastX = 0.0, lastZ = 0.0;
	static bool started = false;

	if (!painting)
	{
		painting = true;
		started = false;
		commitStroke();
	}

	if (!started)
	{
		applyBrushAt(mx, mz);
		started = true;
		lastX = mx;
		lastZ = mz;
		return;
	}

	double dx = mx - lastX;
	double dz = mz - lastZ;
	double dist = sqrt(dx * dx + dz * dz);
	double spacing = std::max(0.02, (double)brushsize * 0.25);

	// A long jump is the pick moving to another part of the map, not a stroke.
	if (dist > 4.0)
	{
		applyBrushAt(mx, mz);
	} else
	{
		int steps = (int)(dist / spacing);
		if (steps < 1)
		{
			applyBrushAt(mx, mz);
		} else
		{
			for (int i = 1; i <= steps; ++i)
			{
				double t = (double)i / (double)steps;
				applyBrushAt(lastX + dx * t, lastZ + dz * t);
			}
		}
	}

	lastX = mx;
	lastZ = mz;
}

static void fillRectangle(double ax, double az, double bx, double bz)
{
	int x0 = (int)std::min(ax, bx) * 64;
	int x1 = (int)std::max(ax, bx) * 64;
	int y0 = (int)std::min(az, bz) * 64;
	int y1 = (int)std::max(az, bz) * 64;

	for (int y = y0; y <= y1; ++y)
		for (int x = x0; x <= x1; ++x)
			applyAt(x, y, 1.0);
}

// ---------------------------------------------------------------- UI

struct Button {
	float x, y, w, h;
	std::string label;
	bool active;
	int id;
};

static std::vector<Button> buttons;

static Mat4 screenMatrix()
{
	return Mat4::ortho(0.0f, (float)Main->width, 0.0f, (float)Main->height, -1.0f, 1.0f);
}

static void addButton(float x, float y, float w, float h, const char *label, bool active, int id)
{
	Button b;
	b.x = x;
	b.y = y;
	b.w = w;
	b.h = h;
	b.label = label ? label : "";
	b.active = active;
	b.id = id;
	buttons.push_back(b);
	ui.addHit(id, x, y, x + w, y + h);
}

static void addPanel(float x0, float y0, float x1, float y1)
{
	ui.quad(x0, y0, x1, y1, 0.16f, 0.18f, 0.22f, 1.0f);
	ui.outline(x0, y0, x1, y1, 1.0f, 0.40f, 0.44f, 0.50f, 1.0f);
}

static std::vector<std::pair<std::string, bool>> listDirectory(const std::string &dir)
{
	std::vector<std::pair<std::string, bool>> entries;
	DIR *d = opendir(dir.c_str());
	if (!d)
		return entries;

	static const char *imageExtensions[] = {".png", ".jpg",  ".jpeg", ".bmp", ".gif",
	                                        ".tga", ".webp", ".sd7",  ".sdz"};

	struct dirent *ent;
	while ((ent = readdir(d)) != nullptr)
	{
		std::string name = ent->d_name;
		if (name == "." || name == "..")
			continue;

		bool isDir = (ent->d_type == DT_DIR);
		bool isMap = name.size() > 4 && name.compare(name.size() - 4, 4, ".smf") == 0;
		bool isImage = false;
		for (const char *ext : imageExtensions)
		{
			size_t len = strlen(ext);
			if (name.size() >= len && name.compare(name.size() - len, len, ext) == 0)
			{
				isImage = true;
				break;
			}
		}

		if (isDir || isMap || isImage)
			entries.push_back(std::make_pair(name, isDir));
	}
	closedir(d);

	std::sort(entries.begin(), entries.end(),
	          [](const std::pair<std::string, bool> &a, const std::pair<std::string, bool> &b) {
		          if (a.second != b.second)
			          return a.second > b.second;
		          return a.first < b.first;
	          });
	return entries;
}

static std::string joinPath(const std::string &a, const std::string &b)
{
	if (a.empty())
		return b;
	if (a[a.size() - 1] == '/')
		return a + b;
	return a + "/" + b;
}

static std::string dirName(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return ".";
	if (slash == 0)
		return "/";
	return path.substr(0, slash);
}

// Base name of a path.
static std::string fileName(const std::string &path)
{
	size_t slash = path.find_last_of('/');
	return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Shortens text until it fits in maxWidth pixels of the small font, so labels
// never spill over their panel.
static std::string fitText(const std::string &text, float maxWidth)
{
	if (fontSmall.textWidth(text.c_str()) <= maxWidth)
		return text;
	std::string out = text;
	while (!out.empty() && fontSmall.textWidth((out + "...").c_str()) > maxWidth)
		out.erase(out.size() - 1);
	return out + "...";
}

// Number of directory entries shown at once by the picker panels.
static const int kPickerRows = 12;

static std::string parentDirectory(const std::string &dir)
{
	if (dir.empty() || dir == "." || dir == "/")
		return std::string();
	std::string parent = dirName(dir);
	if (parent == dir)
		return std::string();
	return parent;
}

// ---------------------------------------------------------------- map info
// The map settings live in the legacy .smd text file next to the .smf:
//   [MAP] TidalStrength, MaxMetal
//   [ATMOSPHERE] MinWind, MaxWind
//   [WATER] WaterPlaneHeight
// Everything else in that file is left untouched.

static std::string pathWithoutExtension(const std::string &path)
{
	size_t dot = path.find_last_of('.');
	size_t slash = path.find_last_of('/');
	if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return path;
	return path.substr(0, dot);
}

static std::string mapInfoPath()
{
	return pathWithoutExtension(mapPath) + ".smd";
}

static std::string trimmed(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos)
		return std::string();
	size_t b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

static std::string lowered(std::string s)
{
	for (size_t i = 0; i < s.size(); ++i)
		s[i] = (char)std::tolower((unsigned char)s[i]);
	return s;
}

static std::vector<std::string> readLines(const std::string &path)
{
	std::vector<std::string> lines;
	FILE *f = fopen(path.c_str(), "r");
	if (!f)
		return lines;
	char buf[1024];
	while (fgets(buf, sizeof(buf), f))
	{
		std::string line(buf);
		while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			line.pop_back();
		lines.push_back(line);
	}
	fclose(f);
	return lines;
}

static bool writeLines(const std::string &path, const std::vector<std::string> &lines)
{
	FILE *f = fopen(path.c_str(), "w");
	if (!f)
		return false;
	for (size_t i = 0; i < lines.size(); ++i)
		fprintf(f, "%s\n", lines[i].c_str());
	fclose(f);
	return true;
}

// Reads a key from a [SECTION] block, returns false when it is missing.
static bool tdfGet(const std::vector<std::string> &lines, const std::string &section,
                   const std::string &key, std::string &value)
{
	std::string wanted = "[" + lowered(section) + "]";
	bool inSection = false;
	int depth = 0;

	for (size_t i = 0; i < lines.size(); ++i)
	{
		std::string line = lowered(trimmed(lines[i]));
		if (line.empty() || line.compare(0, 2, "//") == 0)
			continue;

		if (!inSection)
		{
			if (line == wanted)
				inSection = true;
			continue;
		}

		if (line[0] == '{')
		{
			++depth;
			continue;
		}
		if (line[0] == '}')
		{
			if (--depth <= 0)
				return false;
			continue;
		}
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		if (trimmed(line.substr(0, eq)) != lowered(key))
			continue;

		std::string v = trimmed(lines[i].substr(lines[i].find('=') + 1));
		size_t end = v.find_first_of(";/");
		if (end != std::string::npos)
			v = trimmed(v.substr(0, end));
		value = v;
		return true;
	}
	return false;
}

// Sets a key in a [SECTION] block, creating the section if needed.
static void tdfSet(std::vector<std::string> &lines, const std::string &section,
                   const std::string &key, const std::string &value)
{
	std::string wanted = "[" + lowered(section) + "]";
	std::string entry = "\t" + key + "=" + value + ";";

	long sectionLine = -1;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (lowered(trimmed(lines[i])) == wanted)
		{
			sectionLine = (long)i;
			break;
		}
	}

	if (sectionLine < 0)
	{
		lines.push_back("");
		lines.push_back("[" + section + "]");
		lines.push_back("{");
		lines.push_back(entry);
		lines.push_back("}");
		return;
	}

	// Find the block and its closing brace.
	long openLine = -1, closeLine = -1;
	int depth = 0;
	for (size_t i = (size_t)sectionLine + 1; i < lines.size(); ++i)
	{
		std::string line = trimmed(lines[i]);
		for (size_t c = 0; c < line.size(); ++c)
		{
			if (line[c] == '{')
			{
				if (depth == 0 && openLine < 0)
					openLine = (long)i;
				++depth;
			} else if (line[c] == '}')
			{
				--depth;
				if (depth == 0)
				{
					closeLine = (long)i;
					break;
				}
			}
		}
		if (closeLine >= 0)
			break;
	}

	if (closeLine < 0)
	{
		lines.push_back("{");
		lines.push_back(entry);
		lines.push_back("}");
		return;
	}

	std::string keyName = lowered(key);
	for (long i = openLine + 1; i < closeLine; ++i)
	{
		std::string line = lowered(trimmed(lines[i]));
		size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		if (trimmed(line.substr(0, eq)) == keyName)
		{
			lines[(size_t)i] = entry;
			return;
		}
	}

	lines.insert(lines.begin() + closeLine, entry);
}

// Removes a whole [SECTION] { ... } block.
static void tdfRemoveSection(std::vector<std::string> &lines, const std::string &section)
{
	std::string wanted = "[" + lowered(section) + "]";
	for (size_t i = 0; i < lines.size(); ++i)
	{
		if (lowered(trimmed(lines[i])) != wanted)
			continue;

		size_t end = i;
		int depth = 0;
		bool seen = false;
		for (size_t j = i + 1; j < lines.size(); ++j)
		{
			std::string line = trimmed(lines[j]);
			for (size_t c = 0; c < line.size(); ++c)
			{
				if (line[c] == '{')
				{
					++depth;
					seen = true;
				} else if (line[c] == '}')
				{
					--depth;
				}
			}
			end = j;
			if (seen && depth <= 0)
				break;
		}
		lines.erase(lines.begin() + (long)i, lines.begin() + (long)end + 1);
		--i;
	}
}

static float floatOrDefault(const std::vector<std::string> &lines, const std::string &section,
                            const std::string &key, float fallback)
{
	std::string value;
	if (!tdfGet(lines, section, key, value))
		return fallback;
	return (float)atof(value.c_str());
}

static void loadMapInfo()
{
	std::vector<std::string> lines = readLines(mapInfoPath());

	infoValues[FIELD_TIDAL] = floatOrDefault(lines, "MAP", "TidalStrength", 0.0f);
	infoValues[FIELD_MAXMETAL] = floatOrDefault(lines, "MAP", "MaxMetal", 0.02f);
	infoValues[FIELD_MINWIND] = floatOrDefault(lines, "ATMOSPHERE", "MinWind", 5.0f);
	infoValues[FIELD_MAXWIND] = floatOrDefault(lines, "ATMOSPHERE", "MaxWind", 25.0f);

	std::string water;
	waterPresent = tdfGet(lines, "WATER", "WaterPlaneHeight", water) ||
	               tdfGet(lines, "WATER", "WaterPlaneColor", water);
	waterHeight = 0.0f;
	if (tdfGet(lines, "WATER", "WaterPlaneHeight", water))
		waterHeight = (float)atof(water.c_str());
}

static void saveMapInfo()
{
	std::vector<std::string> lines = readLines(mapInfoPath());

	char buf[64];
	snprintf(buf, sizeof(buf), "%g", infoValues[FIELD_TIDAL]);
	tdfSet(lines, "MAP", "TidalStrength", buf);
	snprintf(buf, sizeof(buf), "%g", infoValues[FIELD_MAXMETAL]);
	tdfSet(lines, "MAP", "MaxMetal", buf);
	snprintf(buf, sizeof(buf), "%g", infoValues[FIELD_MINWIND]);
	tdfSet(lines, "ATMOSPHERE", "MinWind", buf);
	snprintf(buf, sizeof(buf), "%g", infoValues[FIELD_MAXWIND]);
	tdfSet(lines, "ATMOSPHERE", "MaxWind", buf);

	if (waterPresent)
	{
		snprintf(buf, sizeof(buf), "%g", waterHeight);
		tdfSet(lines, "WATER", "WaterPlaneHeight", buf);
		tdfSet(lines, "WATER", "WaterPlaneColor", "0.0 0.4 0.0");
	} else
	{
		tdfRemoveSection(lines, "WATER");
	}

	writeLines(mapInfoPath(), lines);
}

// Refines the per square level of detail from how large each square is on
// screen. Only the squares whose level actually changed are rebuilt, a few per
// frame, so it is cheap enough to call every frame.
static void refreshTerrainLods()
{
	if (!terrain.built() || !Main)
		return;
	terrain.updateLods(Main->view, Main->height - Main->barHeight, 60.0f, detail - 6);
}

static void loadMap(const std::string &path)
{
	commitStroke();

	if (tex)
	{
		delete tex;
		tex = nullptr;
	}
	if (map)
	{
		if (metalmap)
			map->FreeInfoMap("metal", metalmap);
		delete map;
		map = nullptr;
	}

	undoStack.clear();
	currentStroke.index.clear();
	currentStroke.oldValue.clear();
	strokeSeen.clear();
	painting = false;
	rectActive = false;
	browser.DestroyGLTextures();

	map = (CSmfReadMap *)CReadMap::LoadMap(path, dirName(path));
	if (!map)
	{
		printf("Failed to load map %s\n", path.c_str());
		return;
	}

	mapPath = path;
	mapDir = dirName(path);

	tex = new CBFGroundTextures(map);

	MapBitmapInfo metalInfo;
	metalmap = map->GetInfoMap("metal", &metalInfo);
	if (!metalmap)
	{
		printf("No metal map in %s, creating an empty one\n", path.c_str());
		metalWidth = map->width / 2;
		metalHeight = map->height / 2;
		metalmap = (unsigned char *)calloc((size_t)metalWidth * metalHeight, 1);
	} else
	{
		metalWidth = metalInfo.width;
		metalHeight = metalInfo.height;
	}
	rebuildMetalTexture();

	// The height map is stored normalised; the header gives the world range.
	mapMinZ = map->header.minHeight;
	mapMaxZ = map->header.maxHeight;
	if (!(mapMaxZ > mapMinZ + 0.001f))
	{
		mapMinZ = -20.0f;
		mapMaxZ = 20.0f;
	}
	terrain.setHeightRange(mapMinZ, mapMaxZ);
	loadMapInfo();

	terrain.build(map, tex);

	camRotX = 55.0f;
	camRotY = 20.0f;
	camRotZ = 0.0f;
	camDist = 17.0f;
	pivx = map->width * 8 / 1024.0;
	pivy = 0.0f;
	pivz = map->height * 8 / 1024.0;

	if (!textureDir.empty())
		browser.LoadDirectory(textureDir);

	printf("Loaded %s (%dx%d)\n", path.c_str(), map->width, map->height);
}

static void saveMap(void (*progress)(int, int, void *) = nullptr)
{
	if (mapPath.empty())
		return;

	commitStroke();

	FILE *mapfout = fopen(mapPath.c_str(), "rb+");
	if (!mapfout)
	{
		printf("Error, cannot open %s to save data\n", mapPath.c_str());
		return;
	}

	SMFHeader maph;
	if (fread(&maph, sizeof(maph), 1, mapfout) != 1)
	{
		printf("Error, cannot read the map header\n");
		fclose(mapfout);
		return;
	}

	size_t datasize = (size_t)(maph.mapx + 1) * (maph.mapy + 1) * sizeof(unsigned short);
	std::vector<unsigned short> hmapdata(datasize);
	for (int y = 0; y < maph.mapy + 1; ++y)
	{
		for (int x = 0; x < maph.mapx + 1; ++x)
		{
			float v = map->heightmap[y * (maph.mapx + 1) + x];
			hmapdata[y * (maph.mapx + 1) + x] = (unsigned short)std::min(
			    65535.0, 65536.0 * std::max(0.0, std::min(1.0, (double)v)));
		}
	}

	maph.minHeight = mapMinZ;
	maph.maxHeight = mapMaxZ;
	fseek(mapfout, 0, SEEK_SET);
	fwrite(&maph, sizeof(maph), 1, mapfout);

	fseek(mapfout, maph.heightmapPtr, SEEK_SET);
	fwrite(hmapdata.data(), datasize, 1, mapfout);
	fseek(mapfout, maph.metalmapPtr, SEEK_SET);
	fwrite(metalmap, 1, (size_t)(map->width / 2) * (map->height / 2), mapfout);
	fclose(mapfout);

	saveMapInfo();

	printf("Height and metal maps written\n");

	if (tex->bakePaint(progress, nullptr))
	{
		printf("Compressing painted tiles...\n");
		if (tex->writeTileFiles(mapPath))
			printf("Tile file updated (%d tiles)\n", tex->numTiles());
	}

	printf("Save complete\n");
}

static void handleAction(int action)
{
	switch (action)
	{
	case ACT_OPEN_MAP:
		picker = PICK_MAP;
		pickerDir = mapDir.empty() ? "." : mapDir;
		pickerOffset = 0;
		break;
	case ACT_SAVE:
		saveMap();
		break;
	case ACT_OPEN_TEXTURES:
		picker = PICK_TEXTURES;
		pickerDir = textureDir.empty() ? (mapDir.empty() ? "." : mapDir) : textureDir;
		pickerOffset = 0;
		break;
	case ACT_SHAPE_CIRCLE:
		brushShape = BRUSH_CIRCLE;
		break;
	case ACT_SHAPE_SQUARE:
		brushShape = BRUSH_SQUARE;
		break;
	case ACT_SHAPE_TRIANGLE:
		brushShape = BRUSH_TRIANGLE;
		break;
	case ACT_SIZE_DOWN:
		brushsize = std::max(0.1f, brushsize - 0.5f);
		break;
	case ACT_SIZE_UP:
		brushsize = std::min(100.0f, brushsize + 0.5f);
		break;
	case ACT_FALL_DOWN:
		falloff = std::max(0.2f, falloff - 0.2f);
		break;
	case ACT_FALL_UP:
		falloff = std::min(4.0f, falloff + 0.2f);
		break;
	case ACT_VALUE_DOWN:
		if (brushSetMode)
		{
			if (mode == MODE_METAL)
				setMetal = std::max(0.0f, setMetal - 0.05f);
			else
				setHeight -= 5.0f;
		} else
		{
			brushstrength = std::max(0.05f, brushstrength - 0.1f);
		}
		break;
	case ACT_VALUE_UP:
		if (brushSetMode)
		{
			if (mode == MODE_METAL)
				setMetal = std::min(1.0f, setMetal + 0.05f);
			else
				setHeight += 5.0f;
		} else
		{
			brushstrength = std::min(5.0f, brushstrength + 0.1f);
		}
		break;
	case ACT_BRUSH_APPLY:
		brushSetMode = !brushSetMode;
		break;
	case ACT_BRUSH_SIGN:
		brushSubtract = !brushSubtract;
		break;
	case ACT_MODE_HEIGHT:
		if (mode != MODE_HEIGHT)
		{
			commitStroke();
			mode = MODE_HEIGHT;
		}
		break;
	case ACT_MODE_METAL:
		if (mode != MODE_METAL)
		{
			commitStroke();
			mode = MODE_METAL;
		}
		break;
	case ACT_MODE_TEXTURE:
		if (mode != MODE_PAINT)
		{
			commitStroke();
			mode = MODE_PAINT;
		}
		break;
	case ACT_MODE_COLOUR:
		if (mode != MODE_COLOUR)
		{
			commitStroke();
			mode = MODE_COLOUR;
		}
		break;
	case ACT_RECT_FILL:
		rectFill = !rectFill;
		printf("rectangle fill %s\n", rectFill ? "on" : "off");
		break;
	case ACT_HELP:
		helpVisible = !helpVisible;
		break;
	case ACT_DEBUG:
		debugView = (debugView + 1) % 3;
		if (debugView == 2)
			rebuildHeightTexture();
		break;
	case ACT_QUIT:
		requestQuit();
		break;
	case ACT_PICKER_PREV:
		pickerOffset = std::max(0, pickerOffset - kPickerRows);
		break;
	case ACT_PICKER_NEXT: {
		int count = (int)listDirectory(pickerDir).size();
		if (pickerOffset + kPickerRows < count)
			pickerOffset += kPickerRows;
		break;
	}
	case ACT_PICKER_UP: {
		std::string parent = parentDirectory(pickerDir);
		if (!parent.empty())
		{
			pickerDir = parent;
			pickerOffset = 0;
		}
		break;
	}
	case ACT_PICKER_CLOSE:
		picker = PICK_NONE;
		break;
	case ACT_PALETTE_OPEN:
		paletteVisible = true;
		break;
	case ACT_PALETTE_CLOSE:
		paletteVisible = false;
		break;
	case ACT_MAP_SETTINGS:
		mapSettingsVisible = true;
		break;
	case ACT_MENU_FILE:
	case ACT_MENU_EDIT:
	case ACT_MENU_VIEW:
		openMenu = (openMenu == action - ACT_MENU_FILE) ? -1 : action - ACT_MENU_FILE;
		break;
	case ACT_NEW_PROJECT:
		dialog = DLG_NEW;
		dialogTitle = "New map";
		dialogMessage = "Path for the new .smf and its size in map squares";
		fieldText = mapDir.empty() ? "newmap.smf" : joinPath(mapDir, "newmap.smf");
		dialogSizeText = "1024x1024";
		dialogField = 0;
		dialogTextActive = true;
		break;
	case ACT_OPEN_PROJECT:
		picker = PICK_PROJECT;
		pickerDir = projectDir.empty() ? (mapDir.empty() ? "." : mapDir) : projectDir;
		pickerOffset = 0;
		break;
	case ACT_SAVE_PROJECT:
		if (projectDir.empty())
		{
			dialog = DLG_SAVE_AS;
			dialogTitle = "Save project as";
			dialogMessage = "Directory to write the project into";
			fieldText = mapPath.empty() ? "project" : pathWithoutExtension(mapPath) + ".proj";
			dialogTextActive = true;
		} else
		{
			saveProject(projectDir);
		}
		break;
	case ACT_SAVE_PROJECT_AS:
		dialog = DLG_SAVE_AS;
		dialogTitle = "Save project as";
		dialogMessage = "Directory to write the project into";
		fieldText = projectDir.empty()
		                ? (mapPath.empty() ? "project" : pathWithoutExtension(mapPath) + ".proj")
		                : projectDir;
		dialogTextActive = true;
		break;
	case ACT_IMPORT_TEXTURE:
		confirmAction = ACT_IMPORT_TEXTURE;
		dialog = DLG_CONFIRM;
		dialogTitle = "Import map texture";
		dialogMessage = "This replaces all the ground textures. Continue?";
		break;
	case ACT_IMPORT_MAP:
		picker = PICK_MAP;
		pickerDir = mapDir.empty() ? "." : mapDir;
		pickerOffset = 0;
		break;
	case ACT_EXPORT_MAP:
		dialog = DLG_EXPORT;
		dialogTitle = "Export map";
		dialogMessage = "Target .smf or .sd7 path";
		fieldText = mapPath.empty() ? "map.sd7" : pathWithoutExtension(mapPath) + ".sd7";
		dialogTextActive = true;
		break;
	case ACT_EDIT_UNDO:
		commitStroke();
		doUndo();
		break;
	case ACT_EDIT_REDO:
		commitStroke();
		doRedo();
		break;
	case ACT_MAP_CLOSE:
		mapSettingsVisible = false;
		activeField = -1;
		activeSlider = -1;
		break;
	case ACT_WATER_TOGGLE:
		waterPresent = !waterPresent;
		break;
	default:
		break;
	}
}

static void applyMapTexture(const std::string &path, bool skipAspectCheck = false)
{
	if (!tex)
		return;

	std::vector<unsigned char> rgb;
	int w = 0, h = 0;
	if (!loadImageRgb(path, rgb, w, h) || w <= 0 || h <= 0)
	{
		dialog = DLG_MESSAGE;
		dialogTitle = "Import map texture";
		dialogMessage = "Could not read " + fileName(path);
		return;
	}

	// The image is stretched over the whole map, so warn when its aspect ratio
	// does not match the map's (that stretches the texture noticeably).
	if (!skipAspectCheck && map && gs->mapx > 0 && gs->mapy > 0)
	{
		float imageAspect = (float)w / (float)h;
		float mapAspect = (float)gs->mapx / (float)gs->mapy;
		if (fabsf(imageAspect - mapAspect) > mapAspect * 0.01f)
		{
			char text[384];
			snprintf(text, sizeof(text), "Image %dx%d, map %dx%d: aspect differs, stretch it?", w,
			         h, gs->mapx, gs->mapy);
			pendingTexturePath = path;
			confirmAction = ACT_APPLY_TEXTURE;
			dialog = DLG_CONFIRM;
			dialogTitle = "Import map texture";
			dialogMessage = text;
			return;
		}
	}

	if (!tex->setColormap(rgb.data(), w, h))
	{
		dialog = DLG_MESSAGE;
		dialogTitle = "Import map texture";
		dialogMessage = "Could not apply the texture";
		return;
	}

	dirty = true;
	dialog = DLG_MESSAGE;
	dialogTitle = "Import map texture";
	dialogMessage = "Ground texture replaced with " + fileName(path);
}

static void handlePickerClick(int index)
{
	std::vector<std::pair<std::string, bool>> entries = listDirectory(pickerDir);
	if (index < 0 || index >= (int)entries.size())
		return;

	const std::string &name = entries[index].first;
	bool isDir = entries[index].second;
	std::string full = joinPath(pickerDir, name);

	if (isDir)
	{
		pickerDir = full;
		pickerOffset = 0;
		return;
	}

	if (picker == PICK_MAPTEX)
	{
		applyMapTexture(full);
		picker = PICK_NONE;
		return;
	}

	if (picker == PICK_PROJECT)
	{
		openProject(full);
		picker = PICK_NONE;
		return;
	}

	if (picker == PICK_MAP)
	{
		if (name.size() > 4 && (name.compare(name.size() - 4, 4, ".sd7") == 0 ||
		                        name.compare(name.size() - 4, 4, ".sdz") == 0))
		{
			std::string found = extractArchive(full, "/tmp/springmapeditor-import");
			if (!found.empty())
				loadMap(found);
		} else
		{
			loadMap(full);
		}
		picker = PICK_NONE;
	} else
	{
		textureDir = pickerDir;
		int loaded = browser.LoadDirectory(textureDir);
		for (int i = 0; i < loaded; ++i)
		{
			if (browser.at(i).name == name)
			{
				browser.select(i);
				break;
			}
		}
		printf("Texture directory set to %s (%d textures)\n", textureDir.c_str(), loaded);
		picker = PICK_NONE;
	}
}

static void drawPicker()
{
	if (picker == PICK_NONE)
		return;

	std::vector<std::pair<std::string, bool>> entries = listDirectory(pickerDir);

	float panelW = 340.0f;
	float panelH = 356.0f;
	float x0 = 10.0f;
	float y1 = (float)Main->height - 10.0f;
	float y0 = y1 - panelH;

	ui.begin();
	addPanel(x0, y0, x0 + panelW, y1);

	const int rows = kPickerRows;
	float rowHeight = 22.0f;
	float top = y1 - 34.0f;

	int shown = 0;
	for (size_t i = (size_t)pickerOffset; i < entries.size() && shown < rows; ++i, ++shown)
	{
		float y = top - (float)(shown + 1) * rowHeight;
		bool isDir = entries[i].second;

		ui.quad(x0 + 6, y, x0 + panelW - 6, y + rowHeight - 2, isDir ? 0.20f : 0.16f,
		        isDir ? 0.28f : 0.19f, isDir ? 0.40f : 0.25f, 1.0f);
		ui.addHit(ACT_PICKER_BASE + (int)i, x0 + 6, y, x0 + panelW - 6, y + rowHeight - 2);
	}

	addButton(x0 + 6, y0 + 6, 30, 20, "<", false, ACT_PICKER_PREV);
	addButton(x0 + 40, y0 + 6, 30, 20, ">", false, ACT_PICKER_NEXT);
	addButton(x0 + 74, y0 + 6, 34, 20, "Up", false, ACT_PICKER_UP);
	addButton(x0 + panelW - 40, y0 + 6, 34, 20, "Close", false, ACT_PICKER_CLOSE);

	ui.flush(screenMatrix(), 0);

	ui.begin();
	char title[256];
	snprintf(title, sizeof(title), "%s", picker == PICK_MAP ? "Open map" : "Open textures");
	ui.text(fontLarge, title, x0 + 8, y1 - 16, 1.0f, 1.0f, 1.0f, 1.0f);
	ui.flush(screenMatrix(), fontLarge.ok() ? fontLarge.atlasTexture() : 0);

	ui.begin();
	char path[256];
	snprintf(path, sizeof(path), "%s", pickerDir.c_str());
	ui.text(fontSmall, path, x0 + 8, y1 - 30, 0.7f, 0.78f, 0.85f, 1.0f);

	for (size_t i = (size_t)pickerOffset;
	     i < entries.size() && shown > 0 && (int)(i - (size_t)pickerOffset) < rows; ++i)
	{
		int row = (int)(i - (size_t)pickerOffset);
		float y = top - (float)(row + 1) * rowHeight;
		char label[256];
		snprintf(label, sizeof(label), "%s%s", entries[i].first.c_str(),
		         entries[i].second ? "/" : "");
		ui.text(fontSmall, label, x0 + 12, y + 6, 0.95f, 0.95f, 0.95f, 1.0f);
	}

	char count[64];
	snprintf(count, sizeof(count), "%d entries", (int)entries.size());
	ui.text(fontSmall, count, x0 + 8, y0 + 34, 0.6f, 0.7f, 0.8f, 1.0f);

	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

static void drawPalette()
{
	if (!paletteVisible)
		return;

	const float swatch = 18.0f;
	const float gap = 2.0f;
	const float panelW = kPaletteCols * (swatch + gap) + 12.0f;
	const float panelH = kPaletteRows * (swatch + gap) + 48.0f;
	const float x0 = 10.0f;
	const float y1 = (float)Main->height - 10.0f;
	const float y0 = y1 - panelH;

	ui.begin();
	addPanel(x0, y0, x0 + panelW, y1);

	for (int i = 0; i < kPaletteCols * kPaletteRows; ++i)
	{
		int row = i / kPaletteCols;
		int col = i % kPaletteCols;
		float x = x0 + 6.0f + (float)col * (swatch + gap);
		float y = y1 - 30.0f - (float)(row + 1) * (swatch + gap);

		unsigned char rgb[3];
		paletteColour(i, rgb);
		ui.quad(x, y, x + swatch, y + swatch, rgb[0] / 255.0f, rgb[1] / 255.0f, rgb[2] / 255.0f,
		        1.0f);

		if (rgb[0] == fixedColour[0] && rgb[1] == fixedColour[1] && rgb[2] == fixedColour[2])
			ui.outline(x - 1, y - 1, x + swatch + 1, y + swatch + 1, 2.0f, 1.0f, 1.0f, 1.0f, 1.0f);

		ui.addHit(ACT_PALETTE_BASE + i, x, y, x + swatch, y + swatch);
	}

	addButton(x0 + panelW - 46.0f, y0 + 6.0f, 40.0f, 20.0f, "Close", false, ACT_PALETTE_CLOSE);

	ui.flush(screenMatrix(), 0);

	ui.begin();
	ui.text(fontLarge, "Palette", x0 + 8, y1 - 16, 1.0f, 1.0f, 1.0f, 1.0f);
	ui.flush(screenMatrix(), fontLarge.ok() ? fontLarge.atlasTexture() : 0);
}

static bool heightRangeDirty = false;

// Applies a horizontal map settings slider from the cursor x position.
static void updateSliderFromMouse(int i)
{
	if (i < 0 || i > 2)
		return;
	float x0 = sliderRect[i][0];
	float x1 = sliderRect[i][2];
	if (x1 <= x0 + 1.0f)
		return;

	float t = ((float)Main->mousex - x0) / (x1 - x0);
	t = std::max(0.0f, std::min(1.0f, t));

	if (i == 0)
	{
		mapMinZ = -200.0f + t * 400.0f;
		if (mapMinZ > mapMaxZ - 1.0f)
			mapMinZ = mapMaxZ - 1.0f;
		heightRangeDirty = true;
	} else if (i == 1)
	{
		mapMaxZ = -200.0f + t * 400.0f;
		if (mapMaxZ < mapMinZ + 1.0f)
			mapMaxZ = mapMinZ + 1.0f;
		heightRangeDirty = true;
	} else
	{
		waterHeight = -100.0f + t * 200.0f;
	}
}

// Height of a map settings row.
static float mapRowY(float y1, int row)
{
	return y1 - 34.0f - (float)(row + 1) * 28.0f;
}

static void drawMapSettings()
{
	if (!mapSettingsVisible)
		return;

	const float panelW = 400.0f;
	const float panelH = 306.0f;
	const float x0 = 10.0f;
	const float y1 = (float)Main->height - 10.0f;
	const float y0 = y1 - panelH;
	const float controlX = x0 + 128.0f;
	const float sliderW = 170.0f;

	ui.begin();
	addPanel(x0, y0, x0 + panelW, y1);

	// Min Z / Max Z / water height sliders.
	const char *labels[3] = {"Min Z", "Max Z", "Water Z"};
	float values[3] = {mapMinZ, mapMaxZ, waterHeight};
	float lo[3] = {-200.0f, -200.0f, -100.0f};
	float hi[3] = {200.0f, 200.0f, 100.0f};

	for (int i = 0; i < 3; ++i)
	{
		float yTop = mapRowY(y1, i);
		float t = (values[i] - lo[i]) / (hi[i] - lo[i]);
		t = std::max(0.0f, std::min(1.0f, t));

		ui.quad(controlX, yTop + 2, controlX + sliderW, yTop + 18, 0.18f, 0.20f, 0.24f, 1.0f);
		ui.quad(controlX, yTop + 2, controlX + sliderW * t, yTop + 18, 0.35f, 0.55f, 0.75f, 1.0f);
		ui.outline(controlX, yTop + 2, controlX + sliderW, yTop + 18, 1.0f, 0.4f, 0.45f, 0.5f,
		           1.0f);
		ui.addHit(ACT_SLIDER_BASE + i, controlX, yTop + 2, controlX + sliderW, yTop + 18);

		sliderRect[i][0] = controlX;
		sliderRect[i][1] = yTop + 2;
		sliderRect[i][2] = controlX + sliderW;
		sliderRect[i][3] = yTop + 18;
	}

	// Water on/off and the map info text fields.
	float waterY = mapRowY(y1, 2);
	addButton(controlX + sliderW + 14.0f, waterY + 1.0f, 44.0f, 18.0f, waterPresent ? "On" : "Off",
	          waterPresent, ACT_WATER_TOGGLE);

	static const char *fieldNames[FIELD_COUNT] = {"Tidal strength", "Max metal", "Min wind",
	                                              "Max wind"};
	for (int i = 0; i < FIELD_COUNT; ++i)
	{
		float yTop = mapRowY(y1, 3 + i);
		ui.quad(controlX, yTop + 2, controlX + 110, yTop + 18, 0.22f, 0.24f, 0.28f, 1.0f);
		ui.outline(controlX, yTop + 2, controlX + 110, yTop + 18, 1.0f, 0.45f, 0.5f, 0.55f, 1.0f);
		ui.addHit(ACT_FIELD_BASE + i, controlX, yTop + 2, controlX + 110, yTop + 18);
		(void)fieldNames;
	}

	addButton(x0 + panelW - 46.0f, y0 + 6.0f, 40.0f, 20.0f, "Close", false, ACT_MAP_CLOSE);

	ui.flush(screenMatrix(), 0);

	// Labels, values and the field text.
	ui.begin();
	ui.text(fontLarge, "Map", x0 + 8, y1 - 16, 1.0f, 1.0f, 1.0f, 1.0f);
	ui.flush(screenMatrix(), fontLarge.ok() ? fontLarge.atlasTexture() : 0);

	ui.begin();
	char text[128];
	for (int i = 0; i < 3; ++i)
	{
		float yTop = mapRowY(y1, i);
		ui.text(fontSmall, labels[i], x0 + 8, yTop + 5, 0.85f, 0.9f, 0.95f, 1.0f);
		snprintf(text, sizeof(text), "%.1f", i == 0 ? mapMinZ : (i == 1 ? mapMaxZ : waterHeight));
		ui.text(fontSmall, text, controlX + sliderW + 12, yTop + 5, 0.85f, 0.9f, 0.95f, 1.0f);
	}

	static const char *fieldNames2[FIELD_COUNT] = {"Tidal strength", "Max metal", "Min wind",
	                                               "Max wind"};
	for (int i = 0; i < FIELD_COUNT; ++i)
	{
		float yTop = mapRowY(y1, 3 + i);
		ui.text(fontSmall, fieldNames2[i], x0 + 8, yTop + 5, 0.85f, 0.9f, 0.95f, 1.0f);
		if (activeField == i)
			snprintf(text, sizeof(text), "%s|", fieldText.c_str());
		else
			snprintf(text, sizeof(text), "%g", infoValues[i]);
		ui.text(fontSmall, text, controlX + 4, yTop + 5, 1.0f, 1.0f, 1.0f, 1.0f);
	}

	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

// Draws every button registered this frame (the bar and the pickers), so the
// buttons are always painted above the panels that own them.
static void drawButtons()
{
	ui.begin();
	for (size_t i = 0; i < buttons.size(); ++i)
	{
		const Button &b = buttons[i];
		float r = b.active ? 0.30f : 0.20f;
		float g = b.active ? 0.48f : 0.24f;
		float bl = b.active ? 0.62f : 0.28f;
		ui.quad(b.x, b.y, b.x + b.w, b.y + b.h, r, g, bl, 1.0f);
		ui.outline(b.x, b.y, b.x + b.w, b.y + b.h, 1.0f, 0.45f, 0.50f, 0.55f, 1.0f);
	}
	ui.flush(screenMatrix(), 0);

	ui.begin();
	for (size_t i = 0; i < buttons.size(); ++i)
	{
		const Button &b = buttons[i];
		if (b.label.empty())
			continue;
		float tw = fontSmall.textWidth(b.label.c_str());
		ui.text(fontSmall, b.label.c_str(), b.x + (b.w - tw) * 0.5f,
		        b.y + (b.h - fontSmall.ascent()) * 0.5f, 1.0f, 1.0f, 1.0f, 1.0f);
	}
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

// ---------------------------------------------------------------- menu and dialogs
static bool writeArchive(const std::string &archivePath, const std::vector<std::string> &files);
static std::string extractArchive(const std::string &archivePath, const std::string &dir);

static void requestQuit()
{
	if (dirty)
	{
		dialog = DLG_EXIT;
		dialogTitle = "Unsaved changes";
		dialogMessage = "Save the project before quitting?";
	} else
	{
		Main->quit = true;
	}
}

static void drawMenuBar()
{
	const float barH = (float)Main->barHeight;
	const float W = (float)Main->width;

	ui.begin();
	ui.quad(0, (float)Main->viewTop(), W, (float)Main->height, 0.16f, 0.17f, 0.20f, 1.0f);
	ui.outline(0, (float)Main->viewTop(), W, (float)Main->height, 1.0f, 0.3f, 0.33f, 0.38f, 1.0f);
	ui.flush(screenMatrix(), 0);

	const float y = (float)Main->viewTop() + 2.0f;
	addButton(6, y, 44, kMenuHeight - 4, "File", openMenu == 0, ACT_MENU_FILE);
	addButton(54, y, 44, kMenuHeight - 4, "Edit", openMenu == 1, ACT_MENU_EDIT);
	addButton(102, y, 44, kMenuHeight - 4, "View", openMenu == 2, ACT_MENU_VIEW);

	// Project and map name on the right.
	ui.begin();
	char text[512];
	snprintf(text, sizeof(text), "%s%s", projectDir.empty() ? "no project" : projectDir.c_str(),
	         dirty ? " *" : "");
	std::string label = fitText(text, W * 0.6f);
	float tw = fontSmall.textWidth(label.c_str());
	ui.text(fontSmall, label.c_str(), W - tw - 10.0f, y + 4.0f, 0.75f, 0.8f, 0.85f, 1.0f);
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
	(void)barH;
}

static void drawMenuDropdown()
{
	if (openMenu < 0)
		return;

	const MenuItem *items = menus[openMenu];
	const int count = menuCounts[openMenu];
	const float itemH = 22.0f;
	const float w = 190.0f;
	const float x = 6.0f + (float)openMenu * 48.0f;
	const float top = (float)Main->viewTop();
	const float y0 = top - (float)count * itemH;

	ui.begin();
	addPanel(x, y0, x + w, top);
	ui.flush(screenMatrix(), 0);

	ui.begin();
	for (int i = 0; i < count; ++i)
	{
		float y = top - (float)(i + 1) * itemH;
		ui.addHit(ACT_MENU_ITEM_BASE + i, x, y, x + w, y + itemH);
	}
	ui.flush(screenMatrix(), 0);

	ui.begin();
	for (int i = 0; i < count; ++i)
	{
		float y = top - (float)(i + 1) * itemH;
		ui.text(fontSmall, items[i].label, x + 8.0f, y + 6.0f, 0.9f, 0.92f, 0.95f, 1.0f);
	}
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

static void drawDialog()
{
	if (dialog == DLG_NONE)
		return;

	const bool small = dialog != DLG_PROGRESS;
	const float w = small ? 420.0f : 460.0f;
	const float h = small ? 170.0f : 130.0f;
	const float x0 = ((float)Main->width - w) * 0.5f;
	const float y1 = (float)Main->viewTop() - 60.0f;
	const float y0 = y1 - h;

	ui.begin();
	// Dim the whole window behind the dialog.
	ui.quad(0, 0, (float)Main->width, (float)Main->height, 0.0f, 0.0f, 0.0f, 0.45f);
	addPanel(x0, y0, x0 + w, y1);
	ui.flush(screenMatrix(), 0);

	ui.begin();
	ui.text(fontLarge, dialogTitle.c_str(), x0 + 12, y1 - 24, 1.0f, 1.0f, 1.0f, 1.0f);
	ui.flush(screenMatrix(), fontLarge.ok() ? fontLarge.atlasTexture() : 0);

	ui.begin();
	ui.text(fontSmall, fitText(dialogMessage, w - 24).c_str(), x0 + 12, y1 - 44, 0.8f, 0.85f, 0.9f,
	        1.0f);
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);

	if (dialog == DLG_PROGRESS)
	{
		ui.begin();
		float bx0 = x0 + 16, bx1 = x0 + w - 16;
		ui.quad(bx0, y0 + 28, bx1, y0 + 48, 0.18f, 0.20f, 0.24f, 1.0f);
		ui.quad(bx0, y0 + 28, bx0 + (bx1 - bx0) * progressValue, y0 + 48, 0.35f, 0.65f, 0.9f, 1.0f);
		ui.outline(bx0, y0 + 28, bx1, y0 + 48, 1.0f, 0.4f, 0.45f, 0.5f, 1.0f);
		ui.flush(screenMatrix(), 0);

		ui.begin();
		char text[128];
		snprintf(text, sizeof(text), "%s  %d%%", progressText.c_str(),
		         (int)(progressValue * 100.0f));
		ui.text(fontSmall, text, bx0, y0 + 14, 0.85f, 0.9f, 0.95f, 1.0f);
		ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
		return;
	}

	const char *buttonLabels[5];
	int buttonCount = 0;
	switch (dialog)
	{
	case DLG_CONFIRM:
		buttonLabels[0] = "Continue";
		buttonLabels[1] = "Cancel";
		buttonCount = 2;
		break;
	case DLG_EXIT:
		buttonLabels[0] = "Save";
		buttonLabels[1] = "Discard";
		buttonLabels[2] = "Cancel";
		buttonCount = 3;
		break;
	case DLG_SAVE_AS:
	case DLG_EXPORT:
	case DLG_NEW:
		buttonLabels[0] = dialog == DLG_EXPORT ? "Export" : (dialog == DLG_NEW ? "Create" : "Save");
		buttonLabels[1] = "Cancel";
		buttonCount = 2;
		break;
	default:
		buttonLabels[0] = "OK";
		buttonCount = 1;
		break;
	}

	// Path (and size) fields for the dialogs that ask for them.
	if (dialog == DLG_SAVE_AS || dialog == DLG_EXPORT || dialog == DLG_NEW)
	{
		float fx0 = x0 + 12, fx1 = x0 + w - 12, fy0 = y1 - 78, fy1 = y1 - 58;
		ui.begin();
		ui.quad(fx0, fy0, fx1, fy1, 0.22f, 0.24f, 0.28f, 1.0f);
		ui.outline(fx0, fy0, fx1, fy1, 1.0f, 0.45f, 0.5f, 0.55f, 1.0f);
		ui.addHit(ACT_DIALOG_BASE + 10, fx0, fy0, fx1, fy1);
		if (dialog == DLG_NEW)
		{
			ui.quad(fx0, fy0 - 26, fx0 + 200, fy0 - 8, 0.22f, 0.24f, 0.28f, 1.0f);
			ui.outline(fx0, fy0 - 26, fx0 + 200, fy0 - 8, 1.0f, 0.45f, 0.5f, 0.55f, 1.0f);
			ui.addHit(ACT_DIALOG_BASE + 11, fx0, fy0 - 26, fx0 + 200, fy0 - 8);
		}
		ui.flush(screenMatrix(), 0);

		ui.begin();
		std::string shown = fieldText;
		ui.text(fontSmall, fitText(shown, fx1 - fx0 - 8).c_str(), fx0 + 4, fy0 + 6, 1.0f, 1.0f,
		        1.0f, 1.0f);
		if (dialog == DLG_NEW)
		{
			ui.text(fontSmall, "size", fx0 + 206, fy0 + 6, 0.8f, 0.85f, 0.9f, 1.0f);
			ui.text(fontSmall, fitText(dialogSizeText, 190).c_str(), x0 + 12, fy0 - 21, 1.0f, 1.0f,
			        1.0f, 1.0f);
		}
		ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
	}

	ui.begin();
	for (int i = 0; i < buttonCount; ++i)
	{
		float bw = 90.0f;
		float bx = x0 + w - 12.0f - (float)(buttonCount - i) * (bw + 6.0f) + 6.0f;
		ui.quad(bx, y0 + 10.0f, bx + bw, y0 + 34.0f, 0.24f, 0.30f, 0.38f, 1.0f);
		ui.outline(bx, y0 + 10.0f, bx + bw, y0 + 34.0f, 1.0f, 0.45f, 0.55f, 0.65f, 1.0f);
		ui.addHit(ACT_DIALOG_BASE + i, bx, y0 + 10.0f, bx + bw, y0 + 34.0f);
	}
	ui.flush(screenMatrix(), 0);

	ui.begin();
	for (int i = 0; i < buttonCount; ++i)
	{
		float bw = 90.0f;
		float bx = x0 + w - 12.0f - (float)(buttonCount - i) * (bw + 6.0f) + 6.0f;
		float tw = fontSmall.textWidth(buttonLabels[i]);
		ui.text(fontSmall, buttonLabels[i], bx + (bw - tw) * 0.5f, y0 + 17.0f, 1.0f, 1.0f, 1.0f,
		        1.0f);
	}
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

static void drawProgressFrame()
{
	glViewport(0, 0, Main->width, Main->height);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	drawDialog();
	SDL_GL_SwapWindow(Main->screen);
}

static void exportProgress(int done, int total, void *)
{
	progressValue = total > 0 ? 0.1f + 0.8f * (float)done / (float)total : 0.1f;
	progressText = "Compressing tiles";
	drawProgressFrame();
}

static std::string smtPathFor(const std::string &smfPath)
{
	return pathWithoutExtension(smfPath) + ".smt";
}

static std::string smdPathFor(const std::string &smfPath)
{
	return pathWithoutExtension(smfPath) + ".smd";
}

static void createNewMap(const std::string &path, const std::string &sizeText)
{
	int width = 1024, height = 1024;
	int w = 0, h = 0;
	if (sscanf(sizeText.c_str(), "%d%*[xX]%d", &w, &h) >= 1 && w > 0)
	{
		width = w;
		height = h > 0 ? h : w;
	}
	width = std::max(128, std::min(8192, (width / 128) * 128));
	height = std::max(128, std::min(8192, (height / 128) * 128));

	dialog = DLG_PROGRESS;
	progressValue = 0.2f;
	progressText = "Creating map";
	drawProgressFrame();

	std::vector<unsigned short> heights((size_t)(width + 1) * (height + 1), 45875); // ~0.7
	std::vector<unsigned char> metal((size_t)(width / 2) * (height / 2), 0);
	std::vector<unsigned char> types((size_t)(width / 2) * (height / 2), 0);

	MapWriteOptions options;
	options.writeMinimap = true;

	bool ok = writeMapFiles(path, width, height, heights, metal, types,
	                        std::vector<unsigned char>(), -20.0f, 20.0f, options);

	if (ok)
	{
		std::string smd = pathWithoutExtension(path) + ".smd";
		FILE *f = fopen(smd.c_str(), "w");
		if (f)
		{
			fprintf(f,
			        "[MAP]\n{\n\tDescription=%s;\n\tTidalStrength=0;\n\tMaxMetal=0.02;\n"
			        "\tGravity=130;\n\tExtractorRadius=500;\n\tMapHardness=100;\n}\n"
			        "[ATMOSPHERE]\n{\n\tMinWind=5;\n\tMaxWind=25;\n}\n",
			        fileStem(path).c_str());
			fclose(f);
		}
		loadMap(path);
		dirty = false;
		dialog = DLG_MESSAGE;
		dialogTitle = "New map";
		dialogMessage = "Created " + path;
	} else
	{
		dialog = DLG_MESSAGE;
		dialogTitle = "New map";
		dialogMessage = "Could not write " + path;
	}
}

static void startExport(const std::string &path)
{
	if (!map || !tex)
		return;

	dialog = DLG_PROGRESS;
	progressValue = 0.02f;
	progressText = "Preparing";
	drawProgressFrame();

	const int width = gs->mapx + 1;
	const int height = gs->mapy + 1;
	std::vector<unsigned short> heights((size_t)width * height);
	for (int i = 0; i < width * height; ++i)
	{
		float v = std::max(0.0f, std::min(1.0f, map->heightmap[i]));
		heights[(size_t)i] = (unsigned short)(v * 65535.0f);
	}

	std::vector<unsigned char> metal((size_t)metalWidth * metalHeight);
	memcpy(metal.data(), metalmap, metal.size());

	std::vector<unsigned char> types((size_t)(gs->mapx / 2) * (gs->mapy / 2), 0);

	progressValue = 0.08f;
	progressText = "Reading ground texture";
	drawProgressFrame();

	std::vector<unsigned char> colormap;
	tex->buildColormap(colormap);

	MapWriteOptions options;
	options.writeMinimap = true;

	const std::string target = path;
	progressText = "Compressing tiles";

	bool ok = writeMapFiles(target, gs->mapx, gs->mapy, heights, metal, types, colormap, mapMinZ,
	                        mapMaxZ, options, [](int done, int total) {
		                        progressValue =
		                            0.10f + 0.85f * (total > 0 ? (float)done / (float)total : 0.0f);
		                        drawProgressFrame();
	                        });

	if (ok && target.size() > 4 &&
	    (target.compare(target.size() - 4, 4, ".sd7") == 0 ||
	     target.compare(target.size() - 4, 4, ".sdz") == 0))
	{
		progressValue = 0.97f;
		progressText = "Packing archive";
		drawProgressFrame();

		std::string base = pathWithoutExtension(target);
		std::string tempSmf = base + ".smf";
		std::string tempSmt = base + ".smt";
		std::string tempSmd = base + ".smd";
		std::string archive = target;

		std::string packed = "/tmp/springmapeditor-export.sd7";
		std::vector<std::string> files;
		files.push_back(tempSmf);
		files.push_back(tempSmt);
		files.push_back(tempSmd);
		writeArchive(packed, files);
		rename(packed.c_str(), archive.c_str());
		remove(tempSmf.c_str());
		remove(tempSmt.c_str());
		remove(tempSmd.c_str());
	}

	progressValue = 1.0f;
	progressText = "Done";
	drawProgressFrame();

	dialog = DLG_MESSAGE;
	dialogTitle = "Export";
	dialogMessage = ok ? ("Exported " + target) : ("Could not write " + target);
}

// ---------------------------------------------------------------- files
static std::string fileStem(const std::string &path)
{
	std::string name = fileName(path);
	size_t dot = name.find_last_of('.');
	return dot == std::string::npos ? name : name.substr(0, dot);
}

static bool makeDirectory(const std::string &path)
{
	struct stat st;
	if (stat(path.c_str(), &st) == 0)
		return S_ISDIR(st.st_mode);
	return mkdir(path.c_str(), 0755) == 0;
}

static bool copyFile(const std::string &from, const std::string &to)
{
	FILE *in = fopen(from.c_str(), "rb");
	if (!in)
		return false;
	FILE *out = fopen(to.c_str(), "wb");
	if (!out)
	{
		fclose(in);
		return false;
	}
	char buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		fwrite(buf, 1, n, out);
	fclose(in);
	fclose(out);
	return true;
}

#ifdef HAVE_FREEIMAGE
static bool savePng16(const std::string &path, int w, int h, const unsigned short *data)
{
	FIBITMAP *bmp = FreeImage_AllocateT(FIT_UINT16, w, h);
	if (!bmp)
		return false;
	BYTE *bits = FreeImage_GetBits(bmp);
	const int pitch = FreeImage_GetPitch(bmp);
	for (int y = 0; y < h; ++y)
		memcpy(bits + (size_t)y * pitch, data + (size_t)y * w, (size_t)w * 2);
	bool ok = FreeImage_Save(FIF_PNG, bmp, path.c_str(), 0) != 0;
	FreeImage_Unload(bmp);
	return ok;
}

static bool loadPng16(const std::string &path, int w, int h, unsigned short *data)
{
	FIBITMAP *bmp = FreeImage_Load(FIF_PNG, path.c_str(), 0);
	if (!bmp)
		return false;
	FIBITMAP *grey = FreeImage_ConvertToType(bmp, FIT_UINT16);
	FreeImage_Unload(bmp);
	if (!grey)
		return false;
	if ((int)FreeImage_GetWidth(grey) != w || (int)FreeImage_GetHeight(grey) != h)
	{
		FIBITMAP *scaled = FreeImage_Rescale(grey, w, h, FILTER_BILINEAR);
		FreeImage_Unload(grey);
		grey = scaled;
		if (!grey)
			return false;
	}
	BYTE *bits = FreeImage_GetBits(grey);
	const int pitch = FreeImage_GetPitch(grey);
	for (int y = 0; y < h; ++y)
		memcpy(data + (size_t)y * w, bits + (size_t)y * pitch, (size_t)w * 2);
	FreeImage_Unload(grey);
	return true;
}

static bool savePng8(const std::string &path, int w, int h, const unsigned char *data)
{
	FIBITMAP *bmp = FreeImage_Allocate(w, h, 8);
	if (!bmp)
		return false;
	RGBQUAD *pal = FreeImage_GetPalette(bmp);
	for (int i = 0; i < 256; ++i)
	{
		pal[i].rgbRed = pal[i].rgbGreen = pal[i].rgbBlue = (BYTE)i;
	}
	BYTE *bits = FreeImage_GetBits(bmp);
	const int pitch = FreeImage_GetPitch(bmp);
	for (int y = 0; y < h; ++y)
		memcpy(bits + (size_t)y * pitch, data + (size_t)y * w, (size_t)w);
	bool ok = FreeImage_Save(FIF_PNG, bmp, path.c_str(), 0) != 0;
	FreeImage_Unload(bmp);
	return ok;
}

static bool loadPng8(const std::string &path, int w, int h, unsigned char *data)
{
	FIBITMAP *bmp = FreeImage_Load(FIF_PNG, path.c_str(), 0);
	if (!bmp)
		return false;
	FIBITMAP *grey = FreeImage_ConvertToGreyscale(bmp);
	FreeImage_Unload(bmp);
	if (!grey)
		return false;
	if ((int)FreeImage_GetWidth(grey) != w || (int)FreeImage_GetHeight(grey) != h)
	{
		FIBITMAP *scaled = FreeImage_Rescale(grey, w, h, FILTER_BILINEAR);
		FreeImage_Unload(grey);
		grey = scaled;
		if (!grey)
			return false;
	}
	BYTE *bits = FreeImage_GetBits(grey);
	const int pitch = FreeImage_GetPitch(grey);
	for (int y = 0; y < h; ++y)
		memcpy(data + (size_t)y * w, bits + (size_t)y * pitch, (size_t)w);
	FreeImage_Unload(grey);
	return true;
}

static bool savePng32(const std::string &path, int w, int h, const unsigned char *rgba)
{
	FIBITMAP *bmp = FreeImage_Allocate(w, h, 32);
	if (!bmp)
		return false;
	BYTE *bits = FreeImage_GetBits(bmp);
	const int pitch = FreeImage_GetPitch(bmp);
	for (int y = 0; y < h; ++y)
	{
		BYTE *dst = bits + (size_t)y * pitch;
		const unsigned char *src = rgba + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x)
		{
			dst[x * 4 + 0] = src[x * 4 + 2];
			dst[x * 4 + 1] = src[x * 4 + 1];
			dst[x * 4 + 2] = src[x * 4 + 0];
			dst[x * 4 + 3] = src[x * 4 + 3];
		}
	}
	bool ok = FreeImage_Save(FIF_PNG, bmp, path.c_str(), 0) != 0;
	FreeImage_Unload(bmp);
	return ok;
}

static bool loadPng32(const std::string &path, int w, int h, unsigned char *rgba)
{
	FIBITMAP *bmp = FreeImage_Load(FIF_PNG, path.c_str(), 0);
	if (!bmp)
		return false;
	FIBITMAP *rgb = FreeImage_ConvertTo32Bits(bmp);
	FreeImage_Unload(bmp);
	if (!rgb)
		return false;
	if ((int)FreeImage_GetWidth(rgb) != w || (int)FreeImage_GetHeight(rgb) != h)
	{
		FIBITMAP *scaled = FreeImage_Rescale(rgb, w, h, FILTER_BILINEAR);
		FreeImage_Unload(rgb);
		rgb = scaled;
		if (!rgb)
			return false;
	}
	BYTE *bits = FreeImage_GetBits(rgb);
	const int pitch = FreeImage_GetPitch(rgb);
	for (int y = 0; y < h; ++h)
		break;
	for (int y = 0; y < h; ++y)
	{
		const BYTE *src = bits + (size_t)y * pitch;
		unsigned char *dst = rgba + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x)
		{
			dst[x * 4 + 0] = src[x * 4 + 2];
			dst[x * 4 + 1] = src[x * 4 + 1];
			dst[x * 4 + 2] = src[x * 4 + 0];
			dst[x * 4 + 3] = src[x * 4 + 3];
		}
	}
	FreeImage_Unload(rgb);
	return true;
}
// Loads any image FreeImage understands as top-down RGB.
static bool loadImageRgb(const std::string &path, std::vector<unsigned char> &rgb, int &outW,
                         int &outH)
{
	FIBITMAP *bmp = FreeImage_Load(FIF_UNKNOWN, path.c_str(), 0);
	if (!bmp)
		return false;
	FIBITMAP *rgbBmp = FreeImage_ConvertTo24Bits(bmp);
	FreeImage_Unload(bmp);
	if (!rgbBmp)
		return false;

	const int w = (int)FreeImage_GetWidth(rgbBmp);
	const int h = (int)FreeImage_GetHeight(rgbBmp);
	const int pitch = (int)FreeImage_GetPitch(rgbBmp);
	const BYTE *bits = FreeImage_GetBits(rgbBmp);

	rgb.assign((size_t)w * h * 3, 0);
	for (int y = 0; y < h; ++y)
	{
		const BYTE *src = bits + (size_t)y * pitch;
		unsigned char *dst = &rgb[(size_t)(h - 1 - y) * w * 3];
		for (int x = 0; x < w; ++x)
		{
			dst[x * 3 + 0] = src[x * 3 + 2];
			dst[x * 3 + 1] = src[x * 3 + 1];
			dst[x * 3 + 2] = src[x * 3 + 0];
		}
	}
	FreeImage_Unload(rgbBmp);
	outW = w;
	outH = h;
	return true;
}

#else
static bool savePng16(const std::string &, int, int, const unsigned short *)
{
	return false;
}
static bool loadPng16(const std::string &, int, int, unsigned short *)
{
	return false;
}
static bool savePng8(const std::string &, int, int, const unsigned char *)
{
	return false;
}
static bool loadPng8(const std::string &, int, int, unsigned char *)
{
	return false;
}
static bool savePng32(const std::string &, int, int, const unsigned char *)
{
	return false;
}
static bool loadPng32(const std::string &, int, int, unsigned char *)
{
	return false;
}
#endif

// ---------------------------------------------------------------- project files
static std::string projectMapPath()
{
	return joinPath(projectDir, "map.smf");
}

static void saveProject(const std::string &dir)
{
	if (mapPath.empty() || !map || !tex)
		return;

	if (!makeDirectory(dir))
	{
		dialog = DLG_MESSAGE;
		dialogTitle = "Save project";
		dialogMessage = "Cannot create " + dir;
		return;
	}

	commitStroke();

	const std::string stem = fileStem(mapPath);
	std::string smtPath = joinPath(mapDir, stem + ".smt");
	std::string smdPath = joinPath(mapDir, stem + ".smd");

	copyFile(mapPath, joinPath(dir, "map.smf"));
	copyFile(smtPath, joinPath(dir, "map.smt"));
	copyFile(smdPath, joinPath(dir, "map.smd"));

	const int w = gs->mapx + 1, h = gs->mapy + 1;
	std::vector<unsigned short> heights((size_t)w * h);
	for (int i = 0; i < w * h; ++i)
	{
		float v = std::max(0.0f, std::min(1.0f, map->heightmap[i]));
		heights[(size_t)i] = (unsigned short)(v * 65535.0f);
	}
	savePng16(joinPath(dir, "height.png"), w, h, heights.data());

	std::vector<unsigned char> metal((size_t)metalWidth * metalHeight);
	memcpy(metal.data(), metalmap, metal.size());
	savePng8(joinPath(dir, "metal.png"), metalWidth, metalHeight, metal.data());

	// Painted colours, one RGBA texel per map square.
	const int pw = gs->mapx, ph = gs->mapy;
	std::vector<unsigned char> paint((size_t)pw * ph * 4, 0);
	for (int by = 0; by < bigSquaresY(); ++by)
	{
		for (int bx = 0; bx < bigSquaresX(); ++bx)
		{
			for (int py = 0; py < 128; ++py)
			{
				int y = by * 128 + py;
				if (y >= ph)
					break;
				for (int px = 0; px < 128; ++px)
				{
					int x = bx * 128 + px;
					if (x >= pw)
						break;
					unsigned char rgba[4] = {0, 0, 0, 0};
					tex->getPaintTexel(tex->paintTexelIndex(bx, by, px, py), rgba);
					if (rgba[3] == 0)
						continue;
					unsigned char *dst = &paint[((size_t)y * pw + x) * 4];
					dst[0] = rgba[0];
					dst[1] = rgba[1];
					dst[2] = rgba[2];
					dst[3] = rgba[3];
				}
			}
		}
	}
	savePng32(joinPath(dir, "paint.png"), pw, ph, paint.data());

	FILE *f = fopen(joinPath(dir, "project.ini").c_str(), "w");
	if (f)
	{
		fprintf(f, "minZ=%g\n", mapMinZ);
		fprintf(f, "maxZ=%g\n", mapMaxZ);
		fprintf(f, "water=%d\n", waterPresent ? 1 : 0);
		fprintf(f, "waterHeight=%g\n", waterHeight);
		fprintf(f, "tidal=%g\n", infoValues[FIELD_TIDAL]);
		fprintf(f, "maxMetal=%g\n", infoValues[FIELD_MAXMETAL]);
		fprintf(f, "minWind=%g\n", infoValues[FIELD_MINWIND]);
		fprintf(f, "maxWind=%g\n", infoValues[FIELD_MAXWIND]);
		fprintf(f, "textures=%s\n", textureDir.c_str());
		fclose(f);
	}

	projectDir = dir;
	dirty = false;
	dialog = DLG_MESSAGE;
	dialogTitle = "Save project";
	dialogMessage = "Saved " + dir;
}

static void openProject(const std::string &dir)
{
	std::string mapFile = joinPath(dir, "map.smf");
	if (access(mapFile.c_str(), R_OK) != 0)
	{
		dialog = DLG_MESSAGE;
		dialogTitle = "Open project";
		dialogMessage = "No map.smf in " + dir;
		return;
	}

	projectDir = dir;
	loadMap(mapFile);
	if (!map || !tex)
		return;

	// Settings.
	FILE *f = fopen(joinPath(dir, "project.ini").c_str(), "r");
	if (f)
	{
		char line[512];
		while (fgets(line, sizeof(line), f))
		{
			char key[128], value[384];
			if (sscanf(line, "%127[^=]=%383[^\n]", key, value) != 2)
				continue;
			if (strcmp(key, "minZ") == 0)
				mapMinZ = (float)atof(value);
			else if (strcmp(key, "maxZ") == 0)
				mapMaxZ = (float)atof(value);
			else if (strcmp(key, "water") == 0)
				waterPresent = atoi(value) != 0;
			else if (strcmp(key, "waterHeight") == 0)
				waterHeight = (float)atof(value);
			else if (strcmp(key, "tidal") == 0)
				infoValues[FIELD_TIDAL] = (float)atof(value);
			else if (strcmp(key, "maxMetal") == 0)
				infoValues[FIELD_MAXMETAL] = (float)atof(value);
			else if (strcmp(key, "minWind") == 0)
				infoValues[FIELD_MINWIND] = (float)atof(value);
			else if (strcmp(key, "maxWind") == 0)
				infoValues[FIELD_MAXWIND] = (float)atof(value);
			else if (strcmp(key, "textures") == 0 && value[0])
			{
				textureDir = value;
				browser.LoadDirectory(textureDir);
			}
		}
		fclose(f);
	}

	// Height and metal.
	const int w = gs->mapx + 1, h = gs->mapy + 1;
	std::vector<unsigned short> heights((size_t)w * h);
	if (loadPng16(joinPath(dir, "height.png"), w, h, heights.data()))
	{
		for (int i = 0; i < w * h; ++i)
			map->heightmap[i] = heights[(size_t)i] / 65535.0f;
	}
	std::vector<unsigned char> metal((size_t)metalWidth * metalHeight);
	if (loadPng8(joinPath(dir, "metal.png"), metalWidth, metalHeight, metal.data()))
		memcpy(metalmap, metal.data(), metal.size());

	// Paint layer.
	const int pw = gs->mapx, ph = gs->mapy;
	std::vector<unsigned char> paint((size_t)pw * ph * 4);
	if (loadPng32(joinPath(dir, "paint.png"), pw, ph, paint.data()))
	{
		for (int by = 0; by < bigSquaresY(); ++by)
		{
			for (int bx = 0; bx < bigSquaresX(); ++bx)
			{
				for (int py = 0; py < 128; ++py)
				{
					int y = by * 128 + py;
					if (y >= ph)
						break;
					for (int px = 0; px < 128; ++px)
					{
						int x = bx * 128 + px;
						if (x >= pw)
							break;
						const unsigned char *src = &paint[((size_t)y * pw + x) * 4];
						if (src[3] == 0)
							continue;
						unsigned char rgba[4] = {src[0], src[1], src[2], src[3]};
						tex->setPaintTexel(tex->paintTexelIndex(bx, by, px, py), rgba);
					}
				}
			}
		}
		tex->uploadPaintLayers();
	}

	terrain.setHeightRange(mapMinZ, mapMaxZ);
	terrain.build(map, tex);
	rebuildMetalTexture();

	dirty = false;
	dialog = DLG_MESSAGE;
	dialogTitle = "Open project";
	dialogMessage = "Loaded " + dir;
}

// Extracts an .sd7/.sdz archive into a directory and returns the .smf found in it.
static std::string extractArchive(const std::string &archivePath, const std::string &dir)
{
	makeDirectory(dir);

	struct archive *a = archive_read_new();
	archive_read_support_filter_all(a);
	archive_read_support_format_all(a);
	if (archive_read_open_filename(a, archivePath.c_str(), 1 << 16) != ARCHIVE_OK)
	{
		archive_read_free(a);
		return std::string();
	}

	std::string found;
	struct archive_entry *entry;
	while (archive_read_next_header(a, &entry) == ARCHIVE_OK)
	{
		std::string name = archive_entry_pathname(entry);
		if (name.empty() || name[name.size() - 1] == '/')
			continue;
		std::string out = joinPath(dir, fileName(name));
		FILE *f = fopen(out.c_str(), "wb");
		if (!f)
			continue;
		char buf[65536];
		la_ssize_t n;
		while ((n = archive_read_data(a, buf, sizeof(buf))) > 0)
			fwrite(buf, 1, (size_t)n, f);
		fclose(f);
		if (name.size() > 4 && name.compare(name.size() - 4, 4, ".smf") == 0 && found.empty())
			found = out;
	}
	archive_read_free(a);
	return found;
}

// Packs the current map files into a .sd7 archive.
static bool writeArchive(const std::string &archivePath, const std::vector<std::string> &files)
{
	struct archive *a = archive_write_new();
	archive_write_set_format_7zip(a);
	archive_write_open_filename(a, archivePath.c_str());

	for (size_t i = 0; i < files.size(); ++i)
	{
		FILE *f = fopen(files[i].c_str(), "rb");
		if (!f)
			continue;

		std::string name = fileName(files[i]);
		fseek(f, 0, SEEK_END);
		long size = ftell(f);
		fseek(f, 0, SEEK_SET);

		struct archive_entry *entry = archive_entry_new();
		archive_entry_set_pathname(entry, name.c_str());
		archive_entry_set_size(entry, size);
		archive_entry_set_filetype(entry, AE_IFREG);
		archive_entry_set_perm(entry, 0644);
		archive_write_header(a, entry);

		char buf[65536];
		size_t n;
		while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
			archive_write_data(a, buf, n);

		archive_entry_free(entry);
		fclose(f);
	}

	archive_write_close(a);
	archive_write_free(a);
	return true;
}

static int texScroll = 0;

static bool mouseOverTexturePanel()
{
	if (browser.empty())
		return false;
	return Main->mousex >= Main->width - 154 && Main->mousey < Main->viewTop() - 2 &&
	       Main->mousey > Main->height - Main->barHeight;
}

static void drawTexturePanel()
{
	if (browser.empty())
		return;

	const float w = 150.0f;
	const float x0 = (float)Main->width - w - 4.0f;
	const float top = (float)Main->viewTop() - 4.0f;
	const float bottom = (float)Main->barHeight + 4.0f;
	const float rowH = 70.0f;
	const int rows = std::max(1, (int)((top - bottom) / rowH));
	const int count = browser.count();

	if (texScroll > count - rows)
		texScroll = std::max(0, count - rows);
	if (texScroll < 0)
		texScroll = 0;

	ui.begin();
	addPanel(x0, bottom, x0 + w, top);
	ui.flush(screenMatrix(), 0);

	for (int i = 0; i < rows; ++i)
	{
		int index = texScroll + i;
		if (index >= count)
			break;
		float y = top - (float)(i + 1) * rowH + 3.0f;
		bool selected = (index == browser.selected);
		ui.begin();
		ui.quad(x0 + 4, y - 2, x0 + w - 4, y + 62, selected ? 0.9f : 0.30f,
		        selected ? 0.75f : 0.34f, selected ? 0.2f : 0.4f, 1.0f);
		ui.imageQuad(x0 + 6, y, x0 + 66, y + 60, 1.0f, 1.0f, 1.0f, 1.0f);
		ui.flush(screenMatrix(), browser.at(index).glTex);

		ui.begin();
		const GroundTexture &t = browser.at(index);
		std::string label = fitText(t.name, w - 78.0f);
		ui.text(fontSmall, label.c_str(), x0 + 72, y + 34, 0.95f, 0.95f, 0.95f, 1.0f);
		ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);

		ui.begin();
		ui.addHit(ACT_TEXTURE_BASE + index, x0 + 4, y - 2, x0 + w - 4, y + 62);
		ui.flush(screenMatrix(), 0);
	}
}

static void drawBottomBar()
{
	const float barH = (float)Main->barHeight;
	const float W = (float)Main->width;
	const float margin = 6.0f;
	const float gap = 5.0f;

	ui.begin();
	ui.quad(0, 0, W, barH, 0.12f, 0.14f, 0.17f, 1.0f);

	// The bar has two strips: the texture thumbnails along the bottom and the
	// control panels above them, both stretched to the full window width.
	const float thumbY = 5.0f;
	const int maxThumb = 0;
	const float thumbsBottom = 5.0f + 8.0f;

	const float panelsTop = barH - 4.0f;
	const float panelsBottom = thumbsBottom;
	const float panelH = std::max(40.0f, panelsTop - panelsBottom);
	const float buttonH = std::max(20.0f, std::min(30.0f, panelH * 0.34f));
	const float contentH = buttonH + 10.0f + 12.0f + 14.0f;
	const float buttonY = panelsBottom + (panelH + contentH) * 0.5f - contentH + 6.0f;
	const float titleY = buttonY - 10.0f;
	const float infoY = titleY - 14.0f;

	// Widths: every panel gets at least the room its controls need, the rest of
	// the window is shared between them.
	float minW[6] = {100.0f, 156.0f, 206.0f, 152.0f, 124.0f, 246.0f};
	float minSum = 0.0f;
	for (int i = 0; i < 6; ++i)
		minSum += minW[i];

	float extra = W - (margin * 2.0f + gap * 5.0f + minSum);
	if (extra < 0.0f)
	{
		// Too narrow for the minimum widths: shrink every panel instead of
		// letting them run off the window.
		float scale = (W - margin * 2.0f - gap * 5.0f) / minSum;
		if (scale < 0.4f)
			scale = 0.4f;
		for (int i = 0; i < 6; ++i)
			minW[i] *= scale;
		minSum = 0.0f;
		for (int i = 0; i < 6; ++i)
			minSum += minW[i];
		extra = 0.0f;
	}

	float panelX[6];
	float panelW[6];
	float x = margin;
	for (int i = 0; i < 6; ++i)
	{
		panelW[i] = minW[i] + extra * minW[i] / minSum;
		panelX[i] = x;
		x += panelW[i] + gap;
	}

	addPanel(panelX[0], panelsBottom, panelX[0] + panelW[0], panelsTop);
	addPanel(panelX[1], panelsBottom, panelX[1] + panelW[1], panelsTop);
	addPanel(panelX[2], panelsBottom, panelX[2] + panelW[2], panelsTop);
	addPanel(panelX[3], panelsBottom, panelX[3] + panelW[3], panelsTop);
	addPanel(panelX[4], panelsBottom, panelX[4] + panelW[4], panelsTop);
	addPanel(panelX[5], panelsBottom, panelX[5] + panelW[5], panelsTop);

	// Map.
	float px = panelX[0] + 6.0f;
	addButton(px, buttonY, 42, buttonH, "Open", false, ACT_OPEN_MAP);
	addButton(px + 46, buttonY, 42, buttonH, "Save", false, ACT_SAVE);

	// Mode.
	px = panelX[1] + 6.0f;
	addButton(px, buttonY, 34, buttonH, "Hgt", mode == MODE_HEIGHT, ACT_MODE_HEIGHT);
	addButton(px + 38, buttonY, 34, buttonH, "Mtl", mode == MODE_METAL, ACT_MODE_METAL);
	addButton(px + 76, buttonY, 34, buttonH, "Tex", mode == MODE_PAINT, ACT_MODE_TEXTURE);
	addButton(px + 114, buttonY, 34, buttonH, "Col", mode == MODE_COLOUR, ACT_MODE_COLOUR);

	// Brush shape, size and falloff.
	px = panelX[2] + 6.0f;
	addButton(px, buttonY, 26, buttonH, "O", brushShape == BRUSH_CIRCLE, ACT_SHAPE_CIRCLE);
	addButton(px + 30, buttonY, 26, buttonH, "[]", brushShape == BRUSH_SQUARE, ACT_SHAPE_SQUARE);
	addButton(px + 60, buttonY, 26, buttonH, "/\\", brushShape == BRUSH_TRIANGLE,
	          ACT_SHAPE_TRIANGLE);
	addButton(px + 94, buttonY, 24, buttonH, "-", false, ACT_SIZE_DOWN);
	addButton(px + 122, buttonY, 24, buttonH, "+", false, ACT_SIZE_UP);
	addButton(px + 150, buttonY, 24, buttonH, "F-", false, ACT_FALL_DOWN);
	addButton(px + 178, buttonY, 24, buttonH, "F+", false, ACT_FALL_UP);

	// Brush strength or set value, and add/subtract.
	px = panelX[3] + 6.0f;
	addButton(px, buttonY, 24, buttonH, "-", false, ACT_VALUE_DOWN);
	addButton(px + 28, buttonY, 24, buttonH, "+", false, ACT_VALUE_UP);
	addButton(px + 58, buttonY, 44, buttonH, brushSetMode ? "Set" : "Add", brushSetMode,
	          ACT_BRUSH_APPLY);
	addButton(px + 106, buttonY, 40, buttonH, brushSubtract ? "-" : "+", brushSubtract,
	          ACT_BRUSH_SIGN);

	// Colour swatch, palette button and the three channel sliders.
	px = panelX[4] + 6.0f;
	ui.quad(px, buttonY, px + 30, buttonY + buttonH, fixedColour[0] / 255.0f,
	        fixedColour[1] / 255.0f, fixedColour[2] / 255.0f, 1.0f);
	ui.outline(px, buttonY, px + 30, buttonY + buttonH, 1.0f, 0.45f, 0.50f, 0.55f, 1.0f);
	addButton(px + 34, buttonY, 40, buttonH, "Pal", paletteVisible, ACT_PALETTE_OPEN);

	{
		float sliderTop = buttonY - 6.0f;
		float sliderBottom = panelsBottom + 4.0f;
		float sliderH = std::max(10.0f, sliderTop - sliderBottom);
		for (int i = 0; i < 3; ++i)
		{
			float sx = px + (float)i * 36.0f;
			if (sx + 30.0f > panelX[4] + panelW[4] - 4.0f)
				break;
			float v = fixedColour[i] / 255.0f;
			ui.quad(sx, sliderBottom, sx + 30, sliderBottom + sliderH, 0.18f, 0.20f, 0.24f, 1.0f);
			ui.quad(sx, sliderBottom, sx + 30, sliderBottom + sliderH * v, i == 0 ? 0.95f : 0.25f,
			        i == 1 ? 0.95f : 0.25f, i == 2 ? 0.95f : 0.25f, 1.0f);
			ui.addHit(ACT_COLOUR_BASE + i, sx, sliderBottom, sx + 30, sliderBottom + sliderH);
		}
	}

	// Actions, right aligned.
	px = panelX[5] + panelW[5] - 6.0f;
	float bw = 36.0f;
	px -= bw;
	addButton(px, buttonY, bw, buttonH, "Quit", false, ACT_QUIT);
	px -= bw + 4.0f;
	addButton(px, buttonY, bw, buttonH, "Dir", false, ACT_OPEN_TEXTURES);
	px -= bw + 4.0f;
	addButton(px, buttonY, bw, buttonH, "Map", mapSettingsVisible, ACT_MAP_SETTINGS);
	px -= bw + 4.0f;
	addButton(px, buttonY, bw, buttonH, "Over", debugView != 0, ACT_DEBUG);
	px -= bw + 4.0f;
	addButton(px, buttonY, bw, buttonH, "Help", helpVisible, ACT_HELP);
	px -= bw + 4.0f;
	addButton(px, buttonY, bw, buttonH, "Rect", rectFill, ACT_RECT_FILL);

	ui.flush(screenMatrix(), 0);

	// Panel titles and the value lines.
	ui.begin();
	ui.text(fontSmall, "MAP", panelX[0] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "MODE", panelX[1] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "BRUSH", panelX[2] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "VALUE", panelX[3] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "COLOUR", panelX[4] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "ACTIONS", panelX[5] + 8, titleY, 0.85f, 0.9f, 0.95f, 1.0f);

	char text[512];
	std::string label = fileName(mapPath.empty() ? std::string("no map") : mapPath);
	label = fitText(label, panelW[0] - 14.0f);
	ui.text(fontSmall, label.c_str(), panelX[0] + 8, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	ui.text(fontSmall, modeNames[mode], panelX[1] + 8, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	snprintf(text, sizeof(text), "size %.1f  falloff %.2f  %s", brushsize, falloff,
	         shapeNames[brushShape]);
	label = fitText(text, panelW[2] - 14.0f);
	ui.text(fontSmall, label.c_str(), panelX[2] + 8, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	if (brushSetMode)
		snprintf(text, sizeof(text), "%s %.2f  LOD %d", mode == MODE_METAL ? "metal" : "height",
		         mode == MODE_METAL ? setMetal : setHeight, detail);
	else
		snprintf(text, sizeof(text), "strength %.2f  LOD %d", brushstrength, detail);
	label = fitText(text, panelW[3] - 14.0f);
	ui.text(fontSmall, label.c_str(), panelX[3] + 8, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	if (!browser.empty())
	{
		const GroundTexture *current = browser.current();
		snprintf(text, sizeof(text), "%s x%.2f", current ? current->name.c_str() : "none",
		         paintScale);
	} else
	{
		snprintf(text, sizeof(text), "no textures");
	}
	label = fitText(text, panelW[4] - 14.0f);
	ui.text(fontSmall, label.c_str(), panelX[4] + 8, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	snprintf(text, sizeof(text), "cursor %d,%d  fps %.1f", (int)(mx * 64.0), (int)(mz * 64.0),
	         Main->getfps());
	label = fitText(text, panelW[5] - 14.0f);
	ui.text(fontSmall, label.c_str(), panelX[5] + 8, infoY, 0.6f, 0.7f, 0.8f, 1.0f);

	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

static void drawHelpPanel()
{
	if (!helpVisible)
		return;

	float w = 240.0f;
	float x0 = 8.0f;
	float y1 = (float)Main->viewTop() - 8.0f;
	float y0 = y1 - 320.0f;

	ui.begin();
	addPanel(x0, y0, x0 + w, y1);
	ui.flush(screenMatrix(), 0);

	ui.begin();
	static const char *keys[][2] = {
	    {"Left drag", "apply brush"},
	    {"Right drag", "orbit camera"},
	    {"Middle drag", "pan map"},
	    {"Wheel/Q/E", "zoom"},
	    {"Arrows", "pan pivot"},
	    {"A / Z", "brush size"},
	    {"C / X", "strength / set value"},
	    {"B", "add / subtract"},
	    {"G", "add / set mode"},
	    {"S", "smooth"},
	    {"P", "brush shape"},
	    {"- / =", "brush falloff"},
	    {"0 1 2 3", "mode"},
	    {"T / Y", "terrain detail"},
	    {"[ ]", "select texture"},
	    {", .", "texture scale"},
	    {"R", "rectangle fill"},
	    {"L", "show metal/height"},
	    {"W", "wireframe"},
	    {"F", "fullscreen"},
	    {"Ctrl+Z", "undo"},
	    {"Ctrl+Shift+Z", "redo"},
	    {"Esc", "quit"},
	};

	float y = y1 - 22.0f;
	for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); ++i)
	{
		char text[256];
		snprintf(text, sizeof(text), "%-13s %s", keys[i][0], keys[i][1]);
		ui.text(fontSmall, text, x0 + 8, y, 0.85f, 0.9f, 0.95f, 1.0f);
		y -= 14.0f;
		if (y < y0 + 10.0f)
			break;
	}
	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);
}

// One point of a brush outline at the given scale, with a in [0, 1) going all
// the way around the shape.
static void brushOutlinePoint(int shape, double cx, double cz, double r, double scale, double a,
                              double &outX, double &outZ)
{
	const double pi = 3.14159265358979323846;

	if (shape == BRUSH_CIRCLE)
	{
		double angle = a * 2.0 * pi;
		outX = cx + sin(angle) * r * scale;
		outZ = cz + cos(angle) * r * scale;
	} else if (shape == BRUSH_SQUARE)
	{
		double half = r * scale;
		double t = a * 4.0;
		int side = (int)t;
		double f = t - (double)side;
		switch (side)
		{
		case 0:
			outX = cx - half + 2.0 * half * f;
			outZ = cz - half;
			break;
		case 1:
			outX = cx + half;
			outZ = cz - half + 2.0 * half * f;
			break;
		case 2:
			outX = cx + half - 2.0 * half * f;
			outZ = cz + half;
			break;
		default:
			outX = cx - half;
			outZ = cz + half - 2.0 * half * f;
			break;
		}
	} else
	{
		// Right triangle with the right angle on the cursor.
		double r2 = r * scale;
		double t = a * 3.0;
		int side = (int)t;
		double f = t - (double)side;
		double px, pz, qx, qz;
		if (side == 0)
		{
			px = cx;
			pz = cz;
			qx = cx - r2;
			qz = cz;
		} else if (side == 1)
		{
			px = cx - r2;
			pz = cz;
			qx = cx;
			qz = cz - r2;
		} else
		{
			px = cx;
			pz = cz - r2;
			qx = cx;
			qz = cz;
		}
		outX = px + (qx - px) * f;
		outZ = pz + (qz - pz) * f;
	}
}

static void drawBrushOutline()
{
	if (brushsize <= 0.0f)
		return;

	bool smooth = Main->getkey(SDL_SCANCODE_S);
	float cr = smooth ? 0.0f : 1.0f;
	float cg = smooth ? 1.0f : 0.0f;
	float sr = brushsize;

	ui.begin();

	if (rectFill)
	{
		if (rectActive)
		{
			float ay = GetHWorld((int)(rectAx * 64.0), (int)(rectAz * 64.0)) + 0.15f;
			float by = GetHWorld((int)(mx * 64.0), (int)(mz * 64.0)) + 0.15f;
			ui.lineSegment((float)rectAx, ay, (float)rectAz, (float)mx, ay, (float)rectAz, cr, cg,
			               0.0f, 1.0f);
			ui.lineSegment((float)mx, ay, (float)rectAz, (float)mx, by, (float)mz, cr, cg, 0.0f,
			               1.0f);
			ui.lineSegment((float)mx, by, (float)mz, (float)rectAx, by, (float)mz, cr, cg, 0.0f,
			               1.0f);
			ui.lineSegment((float)rectAx, by, (float)mz, (float)rectAx, ay, (float)rectAz, cr, cg,
			               0.0f, 1.0f);
		}
	} else
	{
		// The outer ring plus one ring for every strength level of the falloff,
		// so the soft edge of the brush can be seen on the ground.
		static const double strengths[] = {1.0, 0.75, 0.5, 0.25};
		const int segments = 64;
		const int rings = (int)(sizeof(strengths) / sizeof(strengths[0]));

		for (int ring = 0; ring < rings; ++ring)
		{
			double strength = strengths[ring];
			double scale = 1.0;
			if (ring > 0)
				scale = 1.0 - pow(strength, 1.0 / (double)falloff);

			float alpha = ring == 0 ? 1.0f : (0.6f - 0.12f * (float)ring);

			double lx = 0.0, lz = 0.0;
			float lh = 0.0f;
			for (int i = 0; i <= segments; ++i)
			{
				double a = (double)i / (double)segments;
				double px, pz;
				brushOutlinePoint(brushShape, mx, mz, sr, scale, a, px, pz);
				float h = GetHWorld((int)(px * 64.0), (int)(pz * 64.0)) + 0.15f;
				if (i > 0)
					ui.lineSegment((float)lx, lh, (float)lz, (float)px, h, (float)pz, cr, cg, 0.0f,
					               alpha);
				lx = px;
				lz = pz;
				lh = h;
			}
		}
	}

	ui.flushLines(Main->vp);
}

// ---------------------------------------------------------------- main

static std::string findFont()
{
	const char *env = getenv("SPRINGMAPEDITOR_FONT");
	if (env && *env && access(env, R_OK) == 0)
		return env;

	static const char *candidates[] = {
	    "Vera.ttf",
	    "assets/Vera.ttf",
	    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	    "/usr/share/fonts/TTF/DejaVuSans.ttf",
	    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	};
	for (const char *path : candidates)
		if (access(path, R_OK) == 0)
			return path;
	return "";
}

int main(int argc, char **argv)
{
	bool checkOnly = false;
	std::string requestedMap;
	std::string requestedDir;

	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "--check") == 0)
			checkOnly = true;
		else if (strcmp(argv[i], "-textures") == 0 && i + 1 < argc)
			textureDir = argv[++i];
		else if (strncmp(argv[i], "-textures=", 10) == 0)
			textureDir = argv[i] + 10;
		else if (strcmp(argv[i], "-browser") == 0)
			helpVisible = true;
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			printf("Usage: %s [map.smf | project.dir] [map directory] [-textures <dir>]\n",
			       argv[0]);
			printf("       %s --check\n", argv[0]);
			printf("Without a map the editor starts with an empty project.\n");
			return 0;
		} else if (requestedMap.empty())
			requestedMap = argv[i];
		else if (requestedDir.empty())
			requestedDir = argv[i];
	}

	// Starting without a map opens an empty project; use the File menu to import
	// a map or to open a saved project.

	Main = new engine;
	gs = new gst;

	if (!Main->Init3d(1024, 768, 24, false))
	{
		printf("Failed to initialize OpenGL, exiting\n");
		delete Main;
		delete gs;
		return 1;
	}

	printf("SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
	printf("OpenGL vendor:    %s\n", glGetString(GL_VENDOR));
	printf("OpenGL renderer:  %s\n", glGetString(GL_RENDERER));
	printf("OpenGL version:   %s\n", glGetString(GL_VERSION));

	if (checkOnly)
	{
		Main->Terminate();
		delete Main;
		delete gs;
		return 0;
	}

	if (!terrain.init() || !ui.init())
	{
		fprintf(stderr, "Failed to create the renderer, exiting\n");
		Main->Terminate();
		delete Main;
		delete gs;
		return 1;
	}

	std::string fontpath = findFont();
	if (!fontSmall.load(fontpath, 11) || !fontLarge.load(fontpath, 15))
		fprintf(stderr, "Warning: no TrueType font found, on-screen text disabled. "
		                "Set SPRINGMAPEDITOR_FONT to a .ttf file.\n");

	if (!requestedDir.empty())
		mapDir = requestedDir;

	if (!requestedMap.empty())
		loadMap(requestedMap);

	if (!textureDir.empty())
		browser.LoadDirectory(textureDir);

	Main->setBarHeight(Main->height / 5);
	signal(SIGINT, handle_interrupt);

	printf("Entering main loop\n");

	while (!Main->quit && !interrupt_requested)
	{
		glViewport(0, 0, Main->width, Main->height);
		Main->clearscreen(0.22f, 0.23f, 0.26f, 1.0f);
		Main->setBarHeight(Main->height / 5);
		Main->setMenuHeight(kMenuHeight);
		if (Main->closeRequested)
		{
			Main->closeRequested = false;
			requestQuit();
		}

		// --- camera ---
		if (Main->mouseright && !orbiting)
		{
			orbiting = true;
			pivx = (float)mx;
			pivy = (float)my;
			pivz = (float)mz;
		}
		if (!Main->mouseright)
			orbiting = false;

		if (orbiting)
		{
			camRotY += (float)Main->mouserelx * 0.4f;
			camRotX += (float)Main->mouserely * 0.4f;
		}

		// Middle drag pans the map: keep the point that was grabbed under the
		// cursor by moving the pivot by the same world space amount.
		static bool panning = false;
		static float grabbedX = 0.0f, grabbedZ = 0.0f;
		if (Main->mousecenter)
		{
			float gx, gz;
			if (Main->groundPoint(Main->mousex, Main->mousey, &gx, &gz))
			{
				if (!panning)
				{
					panning = true;
					grabbedX = gx;
					grabbedZ = gz;
				} else
				{
					pivx += grabbedX - gx;
					pivz += grabbedZ - gz;
				}
			}
		} else
		{
			panning = false;
		}

		if (Main->getwup())
		{
			if (mouseOverTexturePanel())
				texScroll = std::max(0, texScroll - 1);
			else
				camDist *= 0.85f;
		}
		if (Main->getwdown())
		{
			if (mouseOverTexturePanel())
				++texScroll;
			else
				camDist *= 1.18f;
		}
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

		// --- map settings sliders ---
		if (activeSlider >= 0 && Main->mouseleft)
			updateSliderFromMouse(activeSlider);
		if (!Main->mouseleft)
			activeSlider = -1;

		if (heightRangeDirty)
		{
			static unsigned lastRangeUpdate = 0;
			unsigned now = SDL_GetTicks();
			if (now - lastRangeUpdate > 100 || !Main->mouseleft)
			{
				lastRangeUpdate = now;
				heightRangeDirty = false;
				terrain.setHeightRange(mapMinZ, mapMaxZ);
				if (map && tex)
					terrain.build(map, tex);
			}
		}

		// --- map info text fields ---
		if (activeField >= 0 || dialogTextActive)
		{
			for (size_t i = 0; i < Main->textInput.size(); ++i)
			{
				char c = Main->textInput[i];
				if (dialogTextActive && dialogField == 1)
				{
					if ((c >= '0' && c <= '9') || c == 'x' || c == 'X')
						dialogSizeText += c;
				} else if ((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' || c == '_' ||
				           c == '/')
					fieldText += c;
			}
			if (Main->backspacePressed)
			{
				if (dialogTextActive && dialogField == 1)
				{
					if (!dialogSizeText.empty())
						dialogSizeText.pop_back();
				} else if (!fieldText.empty())
					fieldText.pop_back();
			}
			if (Main->enterPressed)
			{
				if (!fieldText.empty())
				{
					if (activeField >= 0)
					{
						float v = (float)atof(fieldText.c_str());
						infoValues[activeField] = v < 0.0f ? 0.0f : v;
					}
				}
				activeField = -1;
				dialogTextActive = false;
			}
			if (Main->escapePressed)
			{
				activeField = -1;
				dialogTextActive = false;
			}
		}
		Main->textInput.clear();
		Main->backspacePressed = false;
		Main->enterPressed = false;
		Main->escapePressed = false;

		// The shortcuts are disabled while a map info field has the keyboard.
		if (activeField < 0 && !dialogTextActive)
		{
			// --- brush keys ---
			float rate = 5.0f / std::max(1.0, (double)Main->getfps());
			if (Main->getkey(SDL_SCANCODE_A) && brushsize < 100.0f)
				brushsize += rate;
			if (Main->getkey(SDL_SCANCODE_Z) && !Main->getkey(SDL_SCANCODE_LCTRL) &&
			    brushsize > 0.0f)
				brushsize -= rate;
			if (Main->getkey(SDL_SCANCODE_C))
			{
				stepBrushValue(+rate);
			}
			if (Main->getkey(SDL_SCANCODE_X))
			{
				stepBrushValue(-rate);
			}
			if (Main->getkeydown(SDL_SCANCODE_B))
				brushSubtract = !brushSubtract;
			if (Main->getkeydown(SDL_SCANCODE_G))
				brushSetMode = !brushSetMode;
			if (Main->getkey(SDL_SCANCODE_MINUS))
				falloff = std::max(0.2f, falloff - 0.2f);
			if (Main->getkey(SDL_SCANCODE_EQUALS))
				falloff = std::min(4.0f, falloff + 0.2f);

			if (Main->getkeydown(SDL_SCANCODE_T) && detail > 2)
				detail -= 1;
			if (Main->getkeydown(SDL_SCANCODE_Y) && detail < 10)
				detail += 1;

			int newMode = mode;
			if (Main->getkeydown(SDL_SCANCODE_0))
				newMode = MODE_HEIGHT;
			if (Main->getkeydown(SDL_SCANCODE_1))
				newMode = MODE_METAL;
			if (Main->getkeydown(SDL_SCANCODE_2))
				newMode = MODE_PAINT;
			if (Main->getkeydown(SDL_SCANCODE_3))
				newMode = MODE_COLOUR;
			if (newMode != mode)
			{
				commitStroke();
				mode = newMode;
				printf("mode -> %s\n", modeNames[mode]);
			}

			if (Main->getkeydown(SDL_SCANCODE_P))
				brushShape = (brushShape + 1) % 3;

			if (Main->getkeydown(SDL_SCANCODE_R))
				rectFill = !rectFill;

			if (Main->getkeydown(SDL_SCANCODE_L))
			{
				debugView = (debugView + 1) % 3;
				if (debugView == 2)
					rebuildHeightTexture();
			}

			if (Main->getkeydown(SDL_SCANCODE_LEFTBRACKET))
				browser.select(browser.selected - 1);
			if (Main->getkeydown(SDL_SCANCODE_RIGHTBRACKET))
				browser.select(browser.selected + 1);
			if (Main->getkey(SDL_SCANCODE_COMMA))
				paintScale = std::max(0.1f, paintScale * 0.9f);
			if (Main->getkey(SDL_SCANCODE_PERIOD))
				paintScale = std::min(64.0f, paintScale * 1.1f);

			// --- undo / redo ---
			if (Main->getkeydown(SDL_SCANCODE_Z) &&
			    (Main->getkey(SDL_SCANCODE_LCTRL) || Main->getkey(SDL_SCANCODE_RCTRL)))
			{
				commitStroke();
				if (Main->getkey(SDL_SCANCODE_LSHIFT) || Main->getkey(SDL_SCANCODE_RSHIFT))
					doRedo();
				else
					doUndo();
			}

			if (Main->getkeydown(SDL_SCANCODE_ESCAPE))
				requestQuit();
			if (Main->getkeydown(SDL_SCANCODE_F))
				Main->ToggleFullScreen();
		}

		// --- mouse: the UI gets the first claim on a click ---
		const bool haveMap = (map != nullptr && tex != nullptr);
		int hit = ui.hit((float)Main->mousex, (float)(Main->height - Main->mousey));
		bool mousePressed = Main->getmousedown();

		// A dialog takes every click while it is open.
		if (dialog != DLG_NONE)
		{
			if (mousePressed && hit >= ACT_DIALOG_BASE)
			{
				int b = hit - ACT_DIALOG_BASE;
				if (dialog == DLG_EXIT)
				{
					if (b == 0)
					{
						if (projectDir.empty())
						{
							dialog = DLG_SAVE_AS;
							dialogTitle = "Save project as";
							dialogMessage = "Directory to write the project into";
							fieldText = mapPath.empty() ? "project"
							                            : pathWithoutExtension(mapPath) + ".proj";
							dialogTextActive = true;
							quitAfterSave = true;
						} else
						{
							saveProject(projectDir);
							Main->quit = true;
						}
					} else if (b == 1)
					{
						Main->quit = true;
					} else
						dialog = DLG_NONE;
				} else if (dialog == DLG_SAVE_AS)
				{
					if (b == 0)
					{
						std::string target = fieldText;
						dialog = DLG_NONE;
						saveProject(target);
						if (quitAfterSave)
							Main->quit = true;
						quitAfterSave = false;
					} else
						dialog = DLG_NONE;
				} else if (dialog == DLG_EXPORT)
				{
					if (b == 0)
					{
						std::string target = fieldText;
						dialog = DLG_NONE;
						startExport(target);
					} else
						dialog = DLG_NONE;
				} else if (dialog == DLG_CONFIRM)
				{
					dialog = DLG_NONE;
					if (b == 0 && confirmAction == ACT_IMPORT_TEXTURE)
					{
						picker = PICK_MAPTEX;
						pickerDir =
						    textureDir.empty() ? (mapDir.empty() ? "." : mapDir) : textureDir;
						pickerOffset = 0;
					}
				} else if (dialog == DLG_NEW)
				{
					if (b == 0)
					{
						std::string target = fieldText;
						std::string size = dialogSizeText;
						dialog = DLG_NONE;
						createNewMap(target, size);
					} else
						dialog = DLG_NONE;
				} else if (b == 10)
				{
					dialogTextActive = true;
					dialogField = 0;
					fieldText.clear();
				} else if (b == 11)
				{
					dialogTextActive = true;
					dialogField = 1;
					dialogSizeText.clear();
				} else
				{
					dialog = DLG_NONE;
				}
			}
		} else if (mousePressed && hit >= ACT_MENU_ITEM_BASE)
		{
			int item = hit - ACT_MENU_ITEM_BASE;
			if (openMenu >= 0 && item >= 0 && item < menuCounts[openMenu])
			{
				int action = menus[openMenu][item].action;
				openMenu = -1;
				handleAction(action);
			}
		} else if (mousePressed && hit >= 0)
		{
			if (hit >= ACT_FIELD_BASE)
			{
				int index = hit - ACT_FIELD_BASE;
				if (index >= 0 && index < FIELD_COUNT)
				{
					if (activeField >= 0 && activeField != index)
						infoValues[activeField] = (float)atof(fieldText.c_str());
					activeField = index;
					// Start empty so typing replaces the value; Enter with an
					// empty field keeps the old one.
					fieldText.clear();
				}
			} else if (hit >= ACT_SLIDER_BASE)
			{
				activeSlider = hit - ACT_SLIDER_BASE;
				updateSliderFromMouse(activeSlider);
			} else if (hit >= ACT_PALETTE_BASE)
			{
				int index = hit - ACT_PALETTE_BASE;
				if (index >= 0 && index < kPaletteCols * kPaletteRows)
					paletteColour(index, fixedColour);
			} else if (hit >= ACT_PICKER_BASE)
			{
				handlePickerClick(hit - ACT_PICKER_BASE);
			} else if (hit >= ACT_COLOUR_BASE)
			{
				int channel = hit - ACT_COLOUR_BASE;
				float barH = (float)Main->barHeight;
				float bottom = barH - 58.0f;
				float top = barH - 46.0f;
				float t = (float)(Main->height - Main->mousey) - bottom;
				float v = std::max(0.0f, std::min(1.0f, t / (top - bottom)));
				fixedColour[channel] = (unsigned char)(v * 255.0f);
			} else if (hit >= ACT_TEXTURE_BASE)
			{
				browser.select(hit - ACT_TEXTURE_BASE);
				if (!browser.empty())
					printf("texture: %s\n", browser.current()->name.c_str());
			} else
			{
				handleAction(hit);
			}
		}

		bool inView = haveMap && Main->mousey < Main->height - Main->barHeight && activeField < 0 &&
		              dialog == DLG_NONE;
		bool paintingMouse = Main->mouseleft && hit < 0 && inView;

		if (rectFill)
		{
			if (mousePressed && inView)
			{
				commitStroke();
				rectActive = true;
				rectAx = mx;
				rectAz = mz;
			} else if (rectActive && !Main->mouseleft)
			{
				rectActive = false;
				commitStroke();
				fillRectangle(rectAx, rectAz, mx, mz);
				commitStroke();
			}
		} else if (paintingMouse)
		{
			paintStrokeSegment();

			// Push the changes to the GPU right away so the stroke is visible
			// while it is being drawn instead of only when the button is
			// released.
			tex->uploadPaintLayers();
			if (mode == MODE_METAL)
				rebuildMetalTexture();
			if (mode == MODE_HEIGHT && debugView == 2)
				rebuildHeightTexture();
		} else if (painting)
		{
			painting = false;
			commitStroke();
		}

		// --- draw ---
		refreshTerrainLods();
		terrain.flushDirty();
		Main->camrx = camRotX;
		Main->camry = camRotY;
		Main->camrz = camRotZ;
		Main->camdist = camDist;
		Main->pivx = pivx;
		Main->pivy = pivy;
		Main->pivz = pivz;
		Main->updateCamera();

		if (Main->getkey(SDL_SCANCODE_W))
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		Main->viewport3D();

		terrain.draw(tex, metalTexture, Main->vp, mode, metalWidth, metalHeight, 0.0f, 1.0f, 0.0f,
		             Main->frustum);

		// Pick the map square under the cursor from the depth buffer that was just
		// rendered. Painting uses the previous frame's result, which is invisible;
		// the brush outline below uses this one right away.
		if (Main->mousey < Main->height - Main->barHeight)
			Main->pickGround(Main->GetMouseX(), Main->GetMouseY(), &mx, &my, &mz);

		float mapUnitsX = haveMap ? map->width * 8 / 1024.0f * 2.0f : 0.0f;
		float mapUnitsZ = haveMap ? map->height * 8 / 1024.0f * 2.0f : 0.0f;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (waterPresent && haveMap)
		{
			ui.begin();
			float wy = waterHeight / kElmosPerUnit;
			ui.quad3(0.0f, wy, 0.0f, mapUnitsX, wy, 0.0f, mapUnitsX, wy, mapUnitsZ, 0.0f, wy,
			         mapUnitsZ, 0.15f, 0.38f, 0.72f, 0.45f);
			ui.flush(Main->vp, 0);
		}

		if (debugView != 0 && haveMap)
		{
			// Flat metal / height map over the whole map area, drawn on top.
			glDisable(GL_DEPTH_TEST);
			ui.begin();
			ui.quad3(0.0f, 0.05f, 0.0f, mapUnitsX, 0.05f, 0.0f, mapUnitsX, 0.05f, mapUnitsZ, 0.0f,
			         0.05f, mapUnitsZ, 1.0f, 1.0f, 1.0f, 1.0f);
			ui.flush(Main->vp, debugView == 1 ? metalTexture : heightTexture);
			glEnable(GL_DEPTH_TEST);
		}

		drawBrushOutline();

		glDisable(GL_BLEND);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// --- 2D overlays, depth test off so they always sit on top ---
		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glViewport(0, 0, Main->width, Main->height);
		buttons.clear();
		ui.clearHits();
		drawTexturePanel();
		drawMenuBar();
		drawBottomBar();
		drawHelpPanel();
		drawPicker();
		drawPalette();
		drawMapSettings();
		drawButtons();
		drawMenuDropdown();
		drawDialog();
		drawButtons();

		if (!map)
		{
			ui.begin();
			const char *lines[] = {"No map loaded", "File > Import map... to open an .smf or .sd7",
			                       "File > Open project... to load a saved project"};
			float cx = (float)Main->width * 0.5f;
			float cy = (float)(Main->viewTop() + Main->barHeight) * 0.5f;
			ui.quad(cx - 230, cy - 60, cx + 230, cy + 40, 0.10f, 0.11f, 0.14f, 0.9f);
			ui.flush(screenMatrix(), 0);
			ui.begin();
			for (int i = 0; i < 3; ++i)
			{
				float tw = fontLarge.textWidth(lines[i]);
				ui.text(fontLarge, lines[i], cx - tw * 0.5f, cy + 10 - (float)i * 22,
				        i == 0 ? 1.0f : 0.75f, i == 0 ? 1.0f : 0.8f, i == 0 ? 1.0f : 0.85f, 1.0f);
			}
			ui.flush(screenMatrix(), fontLarge.ok() ? fontLarge.atlasTexture() : 0);
		}
		glEnable(GL_DEPTH_TEST);

		if (haveMap)
			cval = GetH(map, (int)(mx * 64.0), (int)(mz * 64.0));

		Main->display();
	}

	commitStroke();

	Main->Terminate();

	if (metalmap)
		map->FreeInfoMap("metal", metalmap);
	if (metalTexture)
		glDeleteTextures(1, &metalTexture);
	if (heightTexture)
		glDeleteTextures(1, &heightTexture);

	delete tex;
	delete map;
	delete Main;
	delete gs;

	printf("Exiting\n");
	return 0;
}
