#include "texturebrowser.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

#ifdef HAVE_FREEIMAGE
#include <FreeImage.h>
#endif

namespace {

bool has_image_extension(const std::string &name)
{
	size_t dot = name.find_last_of('.');
	if (dot == std::string::npos)
		return false;
	std::string ext = name.substr(dot);
	for (char &c : ext)
		c = (char)tolower(c);
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif" ||
	       ext == ".tga" || ext == ".dds" || ext == ".webp" || ext == ".tif" || ext == ".tif";
}

} // namespace

#ifdef HAVE_FREEIMAGE

bool load_image_rgba(const std::string &path, std::vector<unsigned char> &rgba, int &w, int &h)
{
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(path.c_str());
	if (fif == FIF_UNKNOWN)
		return false;

	FIBITMAP *dib = FreeImage_Load(fif, path.c_str(), 0);
	if (!dib)
		return false;

	// Keep the browser and the paint layers reasonably small.
	const int MAX_DIM = 1024;
	int iw = (int)FreeImage_GetWidth(dib);
	int ih = (int)FreeImage_GetHeight(dib);
	if (iw > MAX_DIM || ih > MAX_DIM)
	{
		float s = (float)MAX_DIM / (float)std::max(iw, ih);
		FIBITMAP *scaled = FreeImage_Rescale(dib, std::max(1, (int)(iw * s)),
		                                     std::max(1, (int)(ih * s)), FILTER_BILINEAR);
		if (scaled)
		{
			FreeImage_Unload(dib);
			dib = scaled;
		}
	}

	FIBITMAP *rgb = FreeImage_ConvertTo32Bits(dib);
	if (!rgb)
		rgb = FreeImage_ConvertTo24Bits(dib);
	if (!rgb)
	{
		FIBITMAP *grey = FreeImage_ConvertToGreyscale(dib);
		if (grey)
			rgb = FreeImage_ConvertTo32Bits(grey);
		if (grey)
			FreeImage_Unload(grey);
	}
	if (!rgb)
	{
		FreeImage_Unload(dib);
		return false;
	}

	w = (int)FreeImage_GetWidth(rgb);
	h = (int)FreeImage_GetHeight(rgb);
	rgba.resize((size_t)w * h * 4);

	const int bpp = (int)FreeImage_GetBPP(rgb);
	const BYTE *bits = FreeImage_GetBits(rgb);
	unsigned pitch = FreeImage_GetPitch(rgb);

	// FreeImage stores rows bottom up, we want the first row to be the top.
	for (int y = 0; y < h; ++y)
	{
		const BYTE *src = bits + (size_t)(h - 1 - y) * pitch;
		unsigned char *dst = rgba.data() + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x)
		{
			if (bpp == 32)
			{
				dst[x * 4 + 0] = src[x * 4 + 0];
				dst[x * 4 + 1] = src[x * 4 + 1];
				dst[x * 4 + 2] = src[x * 4 + 2];
				dst[x * 4 + 3] = src[x * 4 + 3];
			} else
			{
				dst[x * 4 + 0] = src[x * 3 + 0];
				dst[x * 4 + 1] = src[x * 3 + 1];
				dst[x * 4 + 2] = src[x * 3 + 2];
				dst[x * 4 + 3] = 255;
			}
		}
	}

	FreeImage_Unload(rgb);
	FreeImage_Unload(dib);
	return w > 0 && h > 0;
}

#else

#include <SDL3/SDL.h>

bool load_image_rgba(const std::string &path, std::vector<unsigned char> &rgba, int &w, int &h)
{
	SDL_Surface *surf = SDL_LoadBMP(path.c_str());
	if (!surf)
		return false;

	w = surf->w;
	h = surf->h;
	rgba.resize((size_t)w * h * 4);

	const int srcBpp = surf->format->BitsPerPixel;
	const BYTE *pixels = (const BYTE *)surf->pixels;
	for (int y = 0; y < h; ++y)
	{
		const BYTE *src = pixels + (size_t)y * surf->pitch;
		unsigned char *dst = rgba.data() + (size_t)y * w * 4;
		for (int x = 0; x < w; ++x)
		{
			if (srcBpp == 32)
			{
				dst[x * 4 + 0] = src[x * 4 + 0];
				dst[x * 4 + 1] = src[x * 4 + 1];
				dst[x * 4 + 2] = src[x * 4 + 2];
				dst[x * 4 + 3] = src[x * 4 + 3];
			} else if (srcBpp == 24)
			{
				dst[x * 4 + 0] = src[x * 3 + 0];
				dst[x * 4 + 1] = src[x * 3 + 1];
				dst[x * 4 + 2] = src[x * 3 + 2];
				dst[x * 4 + 3] = 255;
			} else
			{
				SDL_Color c;
				SDL_GetPaletteColor(surf, src[x], &c);
				dst[x * 4 + 0] = c.r;
				dst[x * 4 + 1] = c.g;
				dst[x * 4 + 2] = c.b;
				dst[x * 4 + 3] = 255;
			}
		}
	}

	SDL_DestroySurface(surf);
	return w > 0 && h > 0;
}

#endif

TextureBrowser::TextureBrowser() {}

TextureBrowser::~TextureBrowser()
{
	DestroyGLTextures();
}

void TextureBrowser::DestroyGLTextures()
{
	for (GroundTexture &t : textures)
	{
		if (t.glTex)
		{
			glDeleteTextures(1, &t.glTex);
			t.glTex = 0;
		}
	}
}

int TextureBrowser::LoadDirectory(const std::string &dir)
{
#ifdef HAVE_FREEIMAGE
	FreeImage_Initialise(0);
#endif

	DIR *d = opendir(dir.c_str());
	if (!d)
	{
		fprintf(stderr, "Could not open texture directory '%s'\n", dir.c_str());
		return 0;
	}

	struct Entry {
		std::string name;
	};
	std::vector<std::string> names;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL)
	{
		std::string name = ent->d_name;
		if (name == "." || name == "..")
			continue;
		if (has_image_extension(name))
			names.push_back(name);
	}
	closedir(d);

	std::sort(names.begin(), names.end());

	for (const std::string &name : names)
	{
		std::vector<unsigned char> rgba;
		int w = 0, h = 0;
		if (!load_image_rgba(dir + "/" + name, rgba, w, h))
		{
			printf("Skipping %s (unsupported image)\n", name.c_str());
			continue;
		}

		GroundTexture t;
		t.name = name;
		t.w = w;
		t.h = h;
		t.rgba = std::move(rgba);

		glGenTextures(1, &t.glTex);
		glBindTexture(GL_TEXTURE_2D, t.glTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t.rgba.data());

		textures.push_back(std::move(t));
	}

	if (textures.empty())
	{
		// A single neutral colour is better than nothing: it keeps the colour
		// painting usable when the directory has no readable images.
		GroundTexture t;
		t.name = "solid colour";
		t.w = t.h = 4;
		t.rgba.assign(4 * 4 * 4, 255);
		for (int i = 3; i < (int)t.rgba.size(); i += 4)
			t.rgba[i] = 255;

		glGenTextures(1, &t.glTex);
		glBindTexture(GL_TEXTURE_2D, t.glTex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		             t.rgba.data());
		textures.push_back(std::move(t));
	}

	selected = 0;
	return (int)textures.size();
}

void TextureBrowser::select(int index)
{
	if (textures.empty())
		return;
	if (index < 0)
		index = 0;
	if (index >= (int)textures.size())
		index = (int)textures.size() - 1;
	selected = index;
}

const GroundTexture *TextureBrowser::current() const
{
	if (textures.empty())
		return NULL;
	return &textures[selected];
}

void TextureBrowser::sample(int index, double mapTexelX, double mapTexelY, double scale,
                            unsigned char out[4]) const
{
	if (textures.empty())
	{
		out[0] = out[1] = out[2] = out[3] = 255;
		return;
	}

	const GroundTexture &t = textures[index];
	double ux = mapTexelX / scale;
	double uy = mapTexelY / scale;

	// C++ remainder is signed, so wrap manually.
	ux = ux - floor(ux / t.w) * t.w;
	uy = uy - floor(uy / t.h) * t.h;
	if (ux < 0)
		ux += t.w;
	if (uy < 0)
		uy += t.h;

	int x = (int)ux % t.w;
	int y = (int)uy % t.h;

	const unsigned char *p = &t.rgba[((size_t)y * t.w + x) * 4];
	out[0] = p[0];
	out[1] = p[1];
	out[2] = p[2];
	out[3] = p[3];
}
