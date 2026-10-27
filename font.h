/*
 * font.h - FreeType glyph atlas rendered through the core profile flat shader.
 *
 * FTGL is gone: the fixed function pipeline it relies on does not exist in the
 * core profile, and FreeType is already a dependency.
 */
#ifndef FONT_H
#define FONT_H

#include <GL/glew.h>

#include <string>
#include <vector>

class Font {
  public:
	~Font();

	bool load(const std::string &path, int pixelSize);
	bool ok() const
	{
		return texture != 0;
	}
	GLuint atlasTexture() const
	{
		return texture;
	}
	int ascent() const
	{
		return lineheight;
	}

	// Width of the string in pixels.
	float textWidth(const char *text) const;

	// Appends glyph quads (pos3, uv2, rgba4) to `verts`.
	// x is the left edge, baselineY the text baseline, in screen pixels with y
	// growing upwards.
	void appendText(std::vector<float> &verts, const char *text, float x, float baselineY, float r,
	                float g, float b, float a) const;

  private:
	struct Glyph {
		float u0, v0, u1, v1; // atlas coordinates
		float left, width;    // box relative to the text start
		float top, bottom;    // box relative to the baseline
		float advance;
		bool used;
	};

	Glyph glyphs[128];
	GLuint texture = 0;
	int lineheight = 0;
};

#endif // FONT_H
