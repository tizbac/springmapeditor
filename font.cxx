/*
 * font.cxx - FreeType glyph atlas rendered through the core profile flat shader.
 *
 * FTGL is gone: the fixed function pipeline it relies on does not exist in the
 * core profile, and FreeType is already a dependency.
 */
#include "font.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdio>
#include <vector>

Font::~Font()
{
	if (texture)
		glDeleteTextures(1, &texture);
}

bool Font::load(const std::string &path, int pixelSize)
{
	if (path.empty() || texture)
		return false;

	FT_Library library;
	if (FT_Init_FreeType(&library))
	{
		fprintf(stderr, "FreeType init failed\n");
		return false;
	}

	FT_Face face;
	if (FT_New_Face(library, path.c_str(), 0, &face))
	{
		fprintf(stderr, "Could not load font %s\n", path.c_str());
		FT_Done_FreeType(library);
		return false;
	}

	FT_Set_Pixel_Sizes(face, 0, (unsigned)pixelSize);

	const int pad = 2;
	int atlasWidth = pad;
	int maxRows = 0;
	int ascent = 0;

	for (int c = 32; c < 127; ++c)
	{
		if (FT_Load_Char(face, (FT_UInt)c, FT_LOAD_RENDER))
			continue;
		FT_GlyphSlot glyph = face->glyph;
		if (!glyph->bitmap.width || !glyph->bitmap.rows)
			continue;
		atlasWidth += glyph->bitmap.width + pad;
		if ((int)glyph->bitmap.rows > maxRows)
			maxRows = glyph->bitmap.rows;
		if (glyph->bitmap_top > ascent)
			ascent = glyph->bitmap_top;
	}

	const int atlasHeight = maxRows + pad * 2;

	// RGBA texels, stored bottom up: texel row 0 is the first byte of the buffer
	// and becomes v = 0 in the texture.
	std::vector<unsigned char> atlas((size_t)atlasWidth * atlasHeight * 4, 0);

	int cursorX = pad;
	for (int c = 32; c < 127; ++c)
	{
		glyphs[c].used = false;
		if (FT_Load_Char(face, (FT_UInt)c, FT_LOAD_RENDER))
			continue;

		FT_GlyphSlot glyph = face->glyph;
		int w = glyph->bitmap.width, h = glyph->bitmap.rows;
		if (!w || !h)
			continue;

		int baseCol = cursorX;
		int baseRow = pad;

		for (int row = 0; row < h; ++row)
		{
			// FreeType bitmaps start at the top row, the atlas is stored bottom up.
			int destRow = baseRow + (h - 1 - row);
			const unsigned char *src = glyph->bitmap.buffer + (size_t)row * glyph->bitmap.pitch;
			for (int col = 0; col < w; ++col)
			{
				unsigned char *texel =
				    &atlas[((size_t)destRow * atlasWidth + (size_t)(baseCol + col)) * 4];
				texel[0] = texel[1] = texel[2] = 255;
				texel[3] = src[col];
			}
		}

		glyphs[c].used = true;
		glyphs[c].u0 = (float)baseCol / atlasWidth;
		glyphs[c].u1 = (float)(baseCol + w) / atlasWidth;
		glyphs[c].v0 = (float)baseRow / atlasHeight;
		glyphs[c].v1 = (float)(baseRow + h) / atlasHeight;
		glyphs[c].left = (float)glyph->bitmap_left;
		glyphs[c].width = (float)w;
		glyphs[c].top = (float)glyph->bitmap_top;
		glyphs[c].bottom = (float)(glyph->bitmap_top - h);
		glyphs[c].advance = (float)(glyph->advance.x >> 6);

		cursorX += w + pad;
	}

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
	             atlas.data());

	lineheight = ascent;

	FT_Done_Face(face);
	FT_Done_FreeType(library);
	return true;
}

float Font::textWidth(const char *text) const
{
	float width = 0.0f;
	for (const char *p = text; *p; ++p)
	{
		unsigned char c = (unsigned char)*p;
		if (c < 128 && glyphs[c].used)
			width += glyphs[c].advance;
	}
	return width;
}

void Font::appendText(std::vector<float> &verts, const char *text, float x, float baselineY,
                      float r, float g, float b, float a) const
{
	for (const char *p = text; *p; ++p)
	{
		unsigned char c = (unsigned char)*p;
		if (c >= 128 || !glyphs[c].used)
		{
			x += 6.0f;
			continue;
		}

		const Glyph &glyph = glyphs[c];
		float x0 = x + glyph.left;
		float x1 = x0 + glyph.width;
		float y0 = baselineY + glyph.bottom;
		float y1 = baselineY + glyph.top;

		// Two triangles, same winding as FlatRenderer::quad3.
		const float uv[4][2] = {
		    {glyph.u0, glyph.v0}, {glyph.u1, glyph.v0}, {glyph.u1, glyph.v1}, {glyph.u0, glyph.v1}};
		const float pos[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
		const int order[6] = {0, 1, 2, 0, 2, 3};

		for (int i = 0; i < 6; ++i)
		{
			int corner = order[i];
			verts.push_back(pos[corner][0]);
			verts.push_back(pos[corner][1]);
			verts.push_back(0.0f);
			verts.push_back(uv[corner][0]);
			verts.push_back(uv[corner][1]);
			verts.push_back(r);
			verts.push_back(g);
			verts.push_back(b);
			verts.push_back(a);
		}

		x += glyph.advance;
	}
}
