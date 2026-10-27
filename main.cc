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

#include <GL/glew.h>
#include <SDL3/SDL.h>

#include "Map/GlobalStuff.h"
#include "Map/ReadMap.h"
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"
#include "Map/SMF/mapfile.h"
#include "engine.h"
#include "font.h"
#include "mat4.h"
#include "terrain.h"
#include "texturebrowser.h"
#include "ui.h"
#include "undostack.h"

enum BrushShape { BRUSH_CIRCLE = 0, BRUSH_SQUARE = 1, BRUSH_TRIANGLE = 2 };
enum PaintMode { MODE_HEIGHT = 0, MODE_METAL = 1, MODE_PAINT = 2, MODE_COLOUR = 3 };
enum Picker { PICK_NONE = 0, PICK_MAP = 1, PICK_TEXTURES = 2 };

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
	ACT_VALUE_PICK,
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

	ACT_TEXTURE_BASE = 1000, // + index in the texture list
	ACT_COLOUR_BASE = 2000,  // + 0 red, 1 green, 2 blue
	ACT_PICKER_BASE = 3000   // + index in the current directory listing
};

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
static int detail = 6;
static float brushsize = 1.0f; // 1.0 = 64 map squares radius
static float hval = 0.0f;      // height/metal value the brush applies
static float falloff = 1.0f;   // 1 = hard edge, 3 = very soft
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
	if (f > 20)
		f = 0.0;
	return f;
}

static void markTerrainDirty(int squareX, int squareY)
{
	int bx = squareX / 128;
	int by = squareY / 128;
	if (bx < 0 || by < 0 || bx >= bigSquaresX() || by >= bigSquaresY())
		return;
	terrain.markDirty(bx, by);
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
		float target;
		if (smooth)
		{
			float avg = (GetH(map, x - 1, y) + GetH(map, x + 1, y) + GetH(map, x, y - 1) +
			             GetH(map, x, y + 1)) /
			            4.0f;
			target = avg * (1.0f - (float)w) + hval * (float)w;
		} else
		{
			float cur = GetH(map, x, y);
			target = cur * (1.0f - (float)w) + hval * (float)w;
		}
		SetH(map, x, y, target);
		return;
	}

	if (mode == MODE_METAL)
	{
		if (smooth)
		{
			float avg = (GetM(map, x - 2, y) + GetM(map, x + 2, y) + GetM(map, x, y - 2) +
			             GetM(map, x, y + 2)) /
			            4.0;
			SetM(map, x, y, avg * (1.0 - (float)w) + hval * (float)w);
		} else
		{
			float cur = GetM(map, x, y);
			SetM(map, x, y, cur * (1.0 - (float)w) + hval * (float)w);
		}
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
		dest[0] = fixedColour[0];
		dest[1] = fixedColour[1];
		dest[2] = fixedColour[2];
		dest[3] = (unsigned char)std::min(255.0, 255.0 * w);
		if (old[3] > dest[3])
			dest[3] = old[3];
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

static void applyBrush()
{
	int centreX = (int)(mx * 64.0);
	int centreY = (int)(mz * 64.0);
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

	static const char *imageExtensions[] = {".png", ".jpg", ".jpeg", ".bmp",
	                                        ".gif", ".tga", ".webp"};

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

	terrain.build(map, tex, detail);

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

static void saveMap()
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

	fseek(mapfout, maph.heightmapPtr, SEEK_SET);
	fwrite(hmapdata.data(), datasize, 1, mapfout);
	fseek(mapfout, maph.metalmapPtr, SEEK_SET);
	fwrite(metalmap, 1, (size_t)(map->width / 2) * (map->height / 2), mapfout);
	fclose(mapfout);

	printf("Height and metal maps written\n");

	if (tex->bakePaint())
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
		hval -= 0.1f;
		break;
	case ACT_VALUE_UP:
		hval += 0.1f;
		break;
	case ACT_VALUE_PICK:
		hval = (float)cval;
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
		Main->quit = true;
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
	default:
		break;
	}
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

	if (picker == PICK_MAP)
	{
		loadMap(full);
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

static void drawBottomBar()
{
	float barH = (float)Main->barHeight;
	buttons.clear();
	ui.clearHits();

	ui.begin();
	ui.quad(0, 0, (float)Main->width, barH, 0.12f, 0.14f, 0.17f, 1.0f);

	// Panels.
	addPanel(6, barH - 66, 106, barH - 4);                      // map
	addPanel(112, barH - 66, 262, barH - 4);                    // mode
	addPanel(268, barH - 66, 460, barH - 4);                    // brush
	addPanel(466, barH - 66, 620, barH - 4);                    // value
	addPanel(626, barH - 66, 752, barH - 4);                    // colour
	addPanel(758, barH - 66, (float)Main->width - 6, barH - 4); // toggles

	// Map buttons.
	addButton(12, barH - 30, 42, 20, "Open", false, ACT_OPEN_MAP);
	addButton(58, barH - 30, 42, 20, "Save", false, ACT_SAVE);

	// Mode buttons.
	addButton(118, barH - 30, 34, 20, "Hgt", mode == MODE_HEIGHT, ACT_MODE_HEIGHT);
	addButton(156, barH - 30, 34, 20, "Mtl", mode == MODE_METAL, ACT_MODE_METAL);
	addButton(194, barH - 30, 34, 20, "Tex", mode == MODE_PAINT, ACT_MODE_TEXTURE);
	addButton(232, barH - 30, 34, 20, "Col", mode == MODE_COLOUR, ACT_MODE_COLOUR);

	// Brush shape and size.
	addButton(274, barH - 30, 26, 20, "O", brushShape == BRUSH_CIRCLE, ACT_SHAPE_CIRCLE);
	addButton(304, barH - 30, 26, 20, "[]", brushShape == BRUSH_SQUARE, ACT_SHAPE_SQUARE);
	addButton(334, barH - 30, 26, 20, "/\\", brushShape == BRUSH_TRIANGLE, ACT_SHAPE_TRIANGLE);
	addButton(366, barH - 30, 22, 20, "-", false, ACT_SIZE_DOWN);
	addButton(392, barH - 30, 22, 20, "+", false, ACT_SIZE_UP);
	addButton(418, barH - 30, 22, 20, "F-", false, ACT_FALL_DOWN);
	addButton(440, barH - 30, 22, 20, "F+", false, ACT_FALL_UP);

	// Brush value.
	addButton(472, barH - 30, 22, 20, "-", false, ACT_VALUE_DOWN);
	addButton(498, barH - 30, 22, 20, "+", false, ACT_VALUE_UP);
	addButton(524, barH - 30, 40, 20, "Pick", false, ACT_VALUE_PICK);

	// Colour sliders.
	for (int i = 0; i < 3; ++i)
	{
		float x = 632.0f + i * 36.0f;
		float v = fixedColour[i] / 255.0f;
		ui.quad(x, barH - 66 + 8.0f, x + 30, barH - 46, 0.18f, 0.20f, 0.24f, 1.0f);
		ui.quad(x, barH - 66 + 8.0f, x + 30, barH - 66 + 8.0f + 12.0f * v, i == 0 ? 0.95f : 0.25f,
		        i == 1 ? 0.95f : 0.25f, i == 2 ? 0.95f : 0.25f, 1.0f);
		ui.addHit(ACT_COLOUR_BASE + i, x, barH - 66 + 8.0f, x + 30, barH - 46);
	}

	// Toggles.
	addButton(764, barH - 30, 40, 20, "Rect", rectFill, ACT_RECT_FILL);
	addButton(808, barH - 30, 40, 20, "Help", helpVisible, ACT_HELP);
	addButton(852, barH - 30, 40, 20, "Map", debugView != 0, ACT_DEBUG);
	addButton(896, barH - 30, 40, 20, "Quit", false, ACT_QUIT);
	addButton(940, barH - 30, 40, 20, "Dir", false, ACT_OPEN_TEXTURES);

	// Texture thumbnails.
	int thumb = 34;
	int maxThumb = std::min(browser.count(), 12);
	for (int i = 0; i < maxThumb; ++i)
	{
		float x = 6.0f + (float)i * (thumb + 6.0f);
		float y = 8.0f;
		ui.addHit(ACT_TEXTURE_BASE + i, x, y, x + thumb, y + thumb);
	}

	ui.flush(screenMatrix(), 0);

	// Panel titles and the value line below them.
	const float titleY = barH - 42.0f;
	const float infoY = barH - 58.0f;

	ui.begin();
	ui.text(fontSmall, "MAP", 12, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "MODE", 118, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "BRUSH", 274, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "VALUE", 472, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	ui.text(fontSmall, "COLOUR", 632, titleY, 0.85f, 0.9f, 0.95f, 1.0f);

	char text[512];
	std::string label = fileName(mapPath.empty() ? std::string("no map") : mapPath);
	label = fitText(label, 92.0f);
	ui.text(fontSmall, label.c_str(), 12, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	label = fitText(modeNames[mode], 138.0f);
	ui.text(fontSmall, label.c_str(), 118, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	snprintf(text, sizeof(text), "size %.1f  falloff %.2f  %s", brushsize, falloff,
	         shapeNames[brushShape]);
	label = fitText(text, 180.0f);
	ui.text(fontSmall, label.c_str(), 274, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	snprintf(text, sizeof(text), "value %.2f  detail %d", hval, detail);
	label = fitText(text, 146.0f);
	ui.text(fontSmall, label.c_str(), 472, infoY, 0.7f, 0.8f, 0.9f, 1.0f);

	if (!browser.empty())
	{
		const GroundTexture *current = browser.current();
		snprintf(text, sizeof(text), "texture %s scale %.2f",
		         current ? current->name.c_str() : "none", paintScale);
		label = fitText(text, (float)Main->width - 764.0f - 14.0f);
		ui.text(fontSmall, label.c_str(), 764, titleY, 0.85f, 0.9f, 0.95f, 1.0f);
	}

	snprintf(text, sizeof(text), "cursor %d,%d  fps %.1f", (int)(mx * 64.0), (int)(mz * 64.0),
	         Main->getfps());
	label = fitText(text, (float)Main->width - 764.0f - 14.0f);
	ui.text(fontSmall, label.c_str(), 764, infoY, 0.6f, 0.7f, 0.8f, 1.0f);

	ui.flush(screenMatrix(), fontSmall.ok() ? fontSmall.atlasTexture() : 0);

	// Texture thumbnails, one flush per texture.
	for (int i = 0; i < maxThumb; ++i)
	{
		ui.begin();
		float x = 6.0f + (float)i * (thumb + 6.0f);
		float y = 8.0f;
		bool selected = (i == browser.selected);
		ui.quad(x - 2, y - 2, x + thumb + 2, y + thumb + 2, selected ? 1.0f : 0.35f,
		        selected ? 0.9f : 0.40f, selected ? 0.20f : 0.45f, 1.0f);
		ui.imageQuad(x, y, x + thumb, y + thumb, 1.0f, 1.0f, 1.0f, 1.0f);
		ui.flush(screenMatrix(), browser.at(i).glTex);
	}
}

static void drawHelpPanel()
{
	if (!helpVisible)
		return;

	float w = 240.0f;
	float x0 = (float)Main->width - w - 8.0f;
	float y1 = (float)Main->height - (float)Main->barHeight - 8.0f;
	float y0 = y1 - 320.0f;

	ui.begin();
	addPanel(x0, y0, x0 + w, y1);
	ui.flush(screenMatrix(), 0);

	ui.begin();
	static const char *keys[][2] = {
	    {"Left drag", "apply brush"},
	    {"Right drag", "orbit camera"},
	    {"Wheel/Q/E", "zoom"},
	    {"Arrows", "pan pivot"},
	    {"A / Z", "brush size"},
	    {"C / X", "brush value"},
	    {"B", "pick ground value"},
	    {"S", "smooth brush"},
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
			float ay = GetH(map, (int)(rectAx * 64.0), (int)(rectAz * 64.0)) + 0.1f;
			float by = GetH(map, (int)(mx * 64.0), (int)(mz * 64.0)) + 0.1f;
			ui.linePoint((float)rectAx, ay, (float)rectAz, cr, cg, 0.0f, 1.0f);
			ui.linePoint((float)mx, ay, (float)rectAz, cr, cg, 0.0f, 1.0f);
			ui.linePoint((float)mx, by, (float)mz, cr, cg, 0.0f, 1.0f);
			ui.linePoint((float)rectAx, by, (float)mz, cr, cg, 0.0f, 1.0f);
		}
	} else if (brushShape == BRUSH_SQUARE)
	{
		float h0 = GetH(map, (int)((mx - sr) * 64.0), (int)((mz - sr) * 64.0)) + 0.1f;
		float h1 = GetH(map, (int)((mx + sr) * 64.0), (int)((mz - sr) * 64.0)) + 0.1f;
		float h2 = GetH(map, (int)((mx + sr) * 64.0), (int)((mz + sr) * 64.0)) + 0.1f;
		float h3 = GetH(map, (int)((mx - sr) * 64.0), (int)((mz + sr) * 64.0)) + 0.1f;
		ui.linePoint((float)(mx - sr), h0, (float)(mz - sr), cr, cg, 0.0f, 1.0f);
		ui.linePoint((float)(mx + sr), h1, (float)(mz - sr), cr, cg, 0.0f, 1.0f);
		ui.linePoint((float)(mx + sr), h2, (float)(mz + sr), cr, cg, 0.0f, 1.0f);
		ui.linePoint((float)(mx - sr), h3, (float)(mz + sr), cr, cg, 0.0f, 1.0f);
	} else if (brushShape == BRUSH_TRIANGLE)
	{
		float h0 = GetH(map, (int)(mx * 64.0), (int)(mz * 64.0)) + 0.1f;
		float h1 = GetH(map, (int)((mx - sr) * 64.0), (int)(mz * 64.0)) + 0.1f;
		float h2 = GetH(map, (int)(mx * 64.0), (int)((mz - sr) * 64.0)) + 0.1f;
		ui.linePoint((float)mx, h0, (float)mz, cr, cg, 0.0f, 1.0f);
		ui.linePoint((float)(mx - sr), h1, (float)mz, cr, cg, 0.0f, 1.0f);
		ui.linePoint((float)mx, h2, (float)(mz - sr), cr, cg, 0.0f, 1.0f);
	} else
	{
		for (int k = 0; k < 360; k += 360 / 64)
		{
			float angle = (float)k / 360.0f * 2.0f * 3.14159265358979323846f;
			float dx = sin(angle) * sr;
			float dz = cos(angle) * sr;
			float height =
			    GetH(map, (int)(mx * 64.0 + dx * 64.0), (int)(mz * 64.0 + dz * 64.0)) + 0.1f;
			ui.linePoint((float)mx + dx, height, (float)mz + dz, cr, cg, 0.0f, 1.0f);
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
			printf("Usage: %s <map.smf> [map directory] [-textures <dir>]\n", argv[0]);
			printf("       %s --check\n", argv[0]);
			return 0;
		} else if (requestedMap.empty())
			requestedMap = argv[i];
		else if (requestedDir.empty())
			requestedDir = argv[i];
	}

	if (!checkOnly && requestedMap.empty())
	{
		fprintf(stderr, "Usage: %s <map.smf> [map directory] [-textures <dir>]\n", argv[0]);
		fprintf(stderr, "       %s --check    (initialize video/OpenGL and exit)\n", argv[0]);
		return 1;
	}

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

		// --- brush keys ---
		float rate = 5.0f / std::max(1.0, (double)Main->getfps());
		if (Main->getkey(SDL_SCANCODE_A) && brushsize < 100.0f)
			brushsize += rate;
		if (Main->getkey(SDL_SCANCODE_Z) && !Main->getkey(SDL_SCANCODE_LCTRL) && brushsize > 0.0f)
			brushsize -= rate;
		if (Main->getkey(SDL_SCANCODE_C))
			hval += rate / 5.0f;
		if (Main->getkey(SDL_SCANCODE_X))
			hval -= rate / 5.0f;
		if (Main->getkey(SDL_SCANCODE_B))
			hval = (float)cval;
		if (Main->getkey(SDL_SCANCODE_MINUS))
			falloff = std::max(0.2f, falloff - 0.2f);
		if (Main->getkey(SDL_SCANCODE_EQUALS))
			falloff = std::min(4.0f, falloff + 0.2f);

		if (Main->getkeydown(SDL_SCANCODE_T) && detail > 4)
		{
			detail -= 1;
			terrain.build(map, tex, detail);
		}
		if (Main->getkeydown(SDL_SCANCODE_Y) && detail < 10)
		{
			detail += 1;
			terrain.build(map, tex, detail);
		}

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
			Main->quit = true;
		if (Main->getkeydown(SDL_SCANCODE_F))
			Main->ToggleFullScreen();

		// --- mouse: the UI gets the first claim on a click ---
		int hit = ui.hit((float)Main->mousex, (float)(Main->height - Main->mousey));
		bool mousePressed = Main->getmousedown();

		if (mousePressed && hit >= 0)
		{
			if (hit >= ACT_PICKER_BASE)
			{
				handlePickerClick(hit - ACT_PICKER_BASE);
			} else if (hit >= ACT_TEXTURE_BASE)
			{
				browser.select(hit - ACT_TEXTURE_BASE);
				if (!browser.empty())
					printf("texture: %s\n", browser.current()->name.c_str());
			} else if (hit >= ACT_COLOUR_BASE)
			{
				int channel = hit - ACT_COLOUR_BASE;
				float barH = (float)Main->barHeight;
				float bottom = barH - 60.0f;
				float top = barH - 32.0f;
				float t = (float)(Main->height - Main->mousey) - bottom;
				float v = std::max(0.0f, std::min(1.0f, t / (top - bottom)));
				fixedColour[channel] = (unsigned char)(v * 255.0f);
			} else
			{
				handleAction(hit);
			}
		}

		bool inView = Main->mousey < Main->height - Main->barHeight;
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
			if (!painting)
			{
				painting = true;
				commitStroke();
			}
			applyBrush();
		} else if (painting)
		{
			painting = false;
			commitStroke();
		}

		// --- draw ---
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

		float mapUnitsX = map->width * 8 / 1024.0f * 2.0f;
		float mapUnitsZ = map->height * 8 / 1024.0f * 2.0f;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (debugView != 0)
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
		drawBottomBar();
		drawHelpPanel();
		drawPicker();
		drawButtons();
		glEnable(GL_DEPTH_TEST);

		cval = GetH(map, (int)(mx * 64.0), (int)(mz * 64.0));

		Main->display();
	}

	commitStroke();

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
		saveMap();

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
