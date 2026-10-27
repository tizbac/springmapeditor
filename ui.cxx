/*
 * ui.cxx - flat geometry renderer for the bottom bar and the overlays.
 */
#include "ui.h"

#include <cstdio>

#include "font.h"

static const int kFloatsPerVertex = 9; // position3 + uv2 + rgba4

static void pushVertex(std::vector<float> &v, float x, float y, float z, float u, float vv, float r,
                       float g, float b, float a)
{
	v.push_back(x);
	v.push_back(y);
	v.push_back(z);
	v.push_back(u);
	v.push_back(vv);
	v.push_back(r);
	v.push_back(g);
	v.push_back(b);
	v.push_back(a);
}

bool FlatRenderer::init()
{
	if (!shader.createFromFiles("flat.vert", "flat.frag"))
		return false;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

	const GLsizei stride = kFloatsPerVertex * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (const void *)(5 * sizeof(float)));

	glBindVertexArray(0);
	return true;
}

void FlatRenderer::begin()
{
	quads.clear();
	lines.clear();
}

void FlatRenderer::clearHits()
{
	hits.clear();
}

void FlatRenderer::quad(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
	quad3(x0, y0, 0.0f, x1, y0, 0.0f, x1, y1, 0.0f, x0, y1, 0.0f, r, g, b, a);
}

void FlatRenderer::quad3(float x0, float y0, float z0, float x1, float y1, float z1, float x2,
                         float y2, float z2, float x3, float y3, float z3, float r, float g,
                         float b, float a)
{
	pushVertex(quads, x0, y0, z0, 0.0f, 0.0f, r, g, b, a);
	pushVertex(quads, x1, y1, z1, 1.0f, 0.0f, r, g, b, a);
	pushVertex(quads, x2, y2, z2, 1.0f, 1.0f, r, g, b, a);

	pushVertex(quads, x0, y0, z0, 0.0f, 0.0f, r, g, b, a);
	pushVertex(quads, x2, y2, z2, 1.0f, 1.0f, r, g, b, a);
	pushVertex(quads, x3, y3, z3, 0.0f, 1.0f, r, g, b, a);
}

void FlatRenderer::imageQuad(float x0, float y0, float x1, float y1, float r, float g, float b,
                             float a)
{
	// The image data is stored top down, so the bottom of the quad samples the
	// last row of the texture.
	pushVertex(quads, x0, y0, 0.0f, 0.0f, 1.0f, r, g, b, a);
	pushVertex(quads, x1, y0, 0.0f, 1.0f, 1.0f, r, g, b, a);
	pushVertex(quads, x1, y1, 0.0f, 1.0f, 0.0f, r, g, b, a);

	pushVertex(quads, x0, y0, 0.0f, 0.0f, 1.0f, r, g, b, a);
	pushVertex(quads, x1, y1, 0.0f, 1.0f, 0.0f, r, g, b, a);
	pushVertex(quads, x0, y1, 0.0f, 0.0f, 0.0f, r, g, b, a);
}

void FlatRenderer::outline(float x0, float y0, float x1, float y1, float thickness, float r,
                           float g, float b, float a)
{
	quad(x0, y1 - thickness, x1, y1, r, g, b, a);
	quad(x0, y0, x1, y0 + thickness, r, g, b, a);
	quad(x0, y0, x0 + thickness, y1, r, g, b, a);
	quad(x1 - thickness, y0, x1, y1, r, g, b, a);
}

void FlatRenderer::text(const Font &font, const char *str, float x, float baselineY, float r,
                        float g, float b, float a)
{
	font.appendText(quads, str, x, baselineY, r, g, b, a);
}

void FlatRenderer::linePoint(float x, float y, float z, float r, float g, float b, float a)
{
	pushVertex(lines, x, y, z, 0.0f, 0.0f, r, g, b, a);
}

void FlatRenderer::flush(const Mat4 &matrix, GLuint texture)
{
	if (quads.empty())
		return;

	shader.use();
	shader.set("uMatrix", matrix);
	shader.set("useTex", texture ? 1 : 0);
	shader.set("tex", 0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)quads.size() * sizeof(float), quads.data(),
	             GL_DYNAMIC_DRAW);

	glBindVertexArray(vao);
	if (texture)
		glBindTexture(GL_TEXTURE_2D, texture);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(quads.size() / kFloatsPerVertex));
	glBindVertexArray(0);

	quads.clear();
}

void FlatRenderer::flushLines(const Mat4 &matrix)
{
	if (lines.empty())
		return;

	shader.use();
	shader.set("uMatrix", matrix);
	shader.set("useTex", 0);
	shader.set("tex", 0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)lines.size() * sizeof(float), lines.data(),
	             GL_DYNAMIC_DRAW);

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glDisable(GL_DEPTH_TEST);
	glBindVertexArray(vao);
	glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)(lines.size() / kFloatsPerVertex));
	glBindVertexArray(0);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);

	lines.clear();
}

void FlatRenderer::addHit(int id, float x0, float y0, float x1, float y1)
{
	HitArea area;
	area.id = id;
	area.x0 = x0;
	area.y0 = y0;
	area.x1 = x1;
	area.y1 = y1;
	hits.push_back(area);
}

int FlatRenderer::hit(float x, float y) const
{
	for (size_t i = 0; i < hits.size(); ++i)
	{
		const HitArea &area = hits[i];
		if (x >= area.x0 && x <= area.x1 && y >= area.y0 && y <= area.y1)
			return area.id;
	}
	return -1;
}
