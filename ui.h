/*
 * ui.h - flat geometry renderer used for the bottom bar, the overlays and the
 * 3D helper geometry (map base plane, brush outline).
 */
#ifndef UI_H
#define UI_H

#include <GL/glew.h>

#include <vector>

#include "mat4.h"
#include "shader.h"

class Font;

struct HitArea {
	int id;
	float x0, y0, x1, y1;
};

class FlatRenderer {
  public:
	bool init();

	// Starts a new batch of geometry. Hit areas are not cleared here.
	void begin();
	void clearHits();

	// Solid axis aligned quad in the current matrix space.
	void quad(float x0, float y0, float x1, float y1, float r, float g, float b, float a);

	// Solid quad given by four corners in 3D.
	void quad3(float x0, float y0, float z0, float x1, float y1, float z1, float x2, float y2,
	           float z2, float x3, float y3, float z3, float r, float g, float b, float a);

	// Quad that samples a whole texture, with the image stored top down.
	void imageQuad(float x0, float y0, float x1, float y1, float r, float g, float b, float a);

	// Rectangle outline built from four thin quads.
	void outline(float x0, float y0, float x1, float y1, float thickness, float r, float g, float b,
	             float a);

	// Text using a font atlas; the batch must be flushed with that atlas.
	void text(const Font &font, const char *str, float x, float baselineY, float r, float g,
	          float b, float a);

	// Adds a point to the closed polyline drawn by flushLines.
	void linePoint(float x, float y, float z, float r, float g, float b, float a);

	// Uploads and draws the collected quads.
	void flush(const Mat4 &matrix, GLuint texture);

	// Draws the collected polyline as GL_LINE_LOOP.
	void flushLines(const Mat4 &matrix);

	void addHit(int id, float x0, float y0, float x1, float y1);
	int hit(float x, float y) const;

  private:
	Shader shader;
	GLuint vao = 0;
	GLuint vbo = 0;
	std::vector<float> quads;
	std::vector<float> lines;
	std::vector<HitArea> hits;
};

#endif // UI_H
