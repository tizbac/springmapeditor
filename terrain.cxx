/*
 * terrain.cxx - ground mesh built into a vertex buffer.
 */
#include "terrain.h"

#include <cmath>
#include <cstdio>

#include "Map/GlobalStuff.h"
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"

static float mapHeight(CSmfReadMap *map, int x, int y)
{
	if (x < 0 || y < 0)
		return 0.0f;
	if (x > gs->mapx || y > gs->mapy)
		return 0.0f;
	float f = map->heightmap[(gs->mapx + 1) * y + x];
	if (f > 20)
		f = 0.0f;
	return f;
}

static void pushVertex(std::vector<float> &v, float x, float y, float z, float nx, float ny,
                       float nz, float u, float vv)
{
	v.push_back(x);
	v.push_back(y);
	v.push_back(z);
	v.push_back(nx);
	v.push_back(ny);
	v.push_back(nz);
	v.push_back(u);
	v.push_back(vv);
}

// True when the box is not entirely outside one of the frustum planes.
static bool boxInView(const float frustum[6][4], float x0, float y0, float z0, float x1, float y1,
                      float z1)
{
	for (int p = 0; p < 6; ++p)
	{
		float best = -1e30f;
		for (int i = 0; i < 8; ++i)
		{
			float x = (i & 1) ? x1 : x0;
			float y = (i & 2) ? y1 : y0;
			float z = (i & 4) ? z1 : z0;
			float d = frustum[p][0] * x + frustum[p][1] * y + frustum[p][2] * z + frustum[p][3];
			if (d > best)
				best = d;
		}
		if (best <= 0.0f)
			return false;
	}
	return true;
}

bool Terrain::init()
{
	if (!shader.createFromFiles("ground.vert", "ground.frag"))
		return false;

	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);

	glBindVertexArray(vao);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);

	const GLsizei stride = 8 * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(6 * sizeof(float)));

	glBindVertexArray(0);
	return true;
}

void Terrain::buildSquare(int bx, int by, std::vector<float> &out)
{
	int sqsize = quadsPerSquare;
	const float quadWidth = 2.0f / (float)sqsize;
	double tilesize = pow(2.0, (double)detail);
	const int step = (int)(tilesize / 8.0) < 1 ? 1 : (int)(tilesize / 8.0);

	for (int Y = 0; Y < sqsize; ++Y)
	{
		for (int X = 0; X < sqsize; ++X)
		{
			float z1 = mapHeight(map, bx * 128 + X * step + 1, by * 128 + Y * step - 1);
			float z2 = mapHeight(map, bx * 128 + (X + 1) * step + 1, by * 128 + Y * step - 1);
			float z3 = mapHeight(map, bx * 128 + (X + 1) * step + 1, by * 128 + (Y + 1) * step - 1);
			float z4 = mapHeight(map, bx * 128 + X * step + 1, by * 128 + (Y + 1) * step - 1);

			float x1 = (float)X * quadWidth;
			float x2 = (float)(X + 1) * quadWidth;
			float y1 = (float)Y * quadWidth;
			float y2 = (float)(Y + 1) * quadWidth;

			float u1 = (float)X / (float)sqsize;
			float v1 = (float)Y / (float)sqsize;
			float u2 = (float)(X + 1) / (float)sqsize;
			float v2 = (float)(Y + 1) / (float)sqsize;

			// The local frame is (x, y, height); the world frame keeps x and y
			// as the ground plane and turns the height axis into +y.
			float ax = x2 - x1, ay = 0.0f, az = z2 - z1;
			float bxv = x2 - x1, byv = y2 - y1, bzv = z3 - z1;
			float nx = ay * bzv - az * byv;
			float ny = az * bxv - ax * bzv;
			float nz = ax * byv - ay * bxv;

			pushVertex(out, bx * 2 + x1, z1, by * 2 + y1, nx, nz, ny, u1, v1);
			pushVertex(out, bx * 2 + x2, z2, by * 2 + y1, nx, nz, ny, u2, v1);
			pushVertex(out, bx * 2 + x2, z3, by * 2 + y2, nx, nz, ny, u2, v2);

			ax = x1 - x2;
			ay = 0.0f;
			az = z4 - z3;
			bxv = x1 - x2;
			byv = y1 - y2;
			bzv = z1 - z3;
			nx = ay * bzv - az * byv;
			ny = az * bxv - ax * bzv;
			nz = ax * byv - ay * bxv;

			pushVertex(out, bx * 2 + x2, z3, by * 2 + y2, nx, nz, ny, u2, v2);
			pushVertex(out, bx * 2 + x1, z4, by * 2 + y2, nx, nz, ny, u1, v2);
			pushVertex(out, bx * 2 + x1, z1, by * 2 + y1, nx, nz, ny, u1, v1);
		}
	}
}

void Terrain::build(CSmfReadMap *map_, CBFGroundTextures *textures, int detail_)
{
	map = map_;
	detail = detail_;
	bigX = textures->numBigSquaresX();
	bigY = textures->numBigSquaresY();

	int sqsize = (int)(1024.0 / pow(2.0, (double)detail));
	if (sqsize < 1)
		sqsize = 1;
	quadsPerSquare = sqsize;

	vertices.clear();
	squares.clear();
	dirty.assign((size_t)bigX * bigY, 0);
	squares.reserve((size_t)bigX * bigY);

	std::vector<float> square;
	for (int by = 0; by < bigY; ++by)
	{
		for (int bx = 0; bx < bigX; ++bx)
		{
			square.clear();
			buildSquare(bx, by, square);

			BigSquare sq;
			sq.first = (int)(vertices.size() / 8);
			sq.count = (int)(square.size() / 8);
			sq.cx0 = (float)(bx * 2);
			sq.cz0 = (float)(by * 2);
			sq.cx1 = (float)(bx * 2 + 2);
			sq.cz1 = (float)(by * 2 + 2);
			sq.cy0 = -20.0f;
			sq.cy1 = 0.0f;

			// Widen the bounding box with the actual heights of the square.
			for (size_t i = 0; i < square.size(); i += 8)
			{
				if (square[i + 1] < sq.cy0)
					sq.cy0 = square[i + 1];
				if (square[i + 1] > sq.cy1)
					sq.cy1 = square[i + 1];
			}

			vertices.insert(vertices.end(), square.begin(), square.end());
			squares.push_back(sq);
		}
	}

	printf("Terrain mesh: %dx%d big squares, %d quads/square, %d vertices\n", bigX, bigY,
	       quadsPerSquare, (int)(vertices.size() / 8));

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)vertices.size() * sizeof(float), vertices.data(),
	             GL_STATIC_DRAW);
}

void Terrain::markDirty(int bx, int by)
{
	if (bx < 0 || by < 0 || bx >= bigX || by >= bigY)
		return;
	dirty[(size_t)by * bigX + bx] = 1;
}

void Terrain::flushDirty()
{
	if (squares.empty())
		return;

	std::vector<float> square;

	// The array buffer binding is not part of the vertex array object, so bind
	// it explicitly: without this the upload silently targets another buffer and
	// height edits never reach the screen.
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	for (size_t i = 0; i < squares.size(); ++i)
	{
		if (!dirty[i])
			continue;
		int bx = (int)(i % (size_t)bigX);
		int by = (int)(i / (size_t)bigX);

		square.clear();
		buildSquare(bx, by, square);

		BigSquare &sq = squares[i];
		size_t offset = (size_t)sq.first * 8 * sizeof(float);
		glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset,
		                (GLsizeiptr)square.size() * sizeof(float), square.data());

		vertices.resize((size_t)sq.first * 8 + square.size());
		for (size_t k = 0; k < square.size(); ++k)
			vertices[(size_t)sq.first * 8 + k] = square[k];

		sq.cy0 = -20.0f;
		sq.cy1 = 0.0f;
		for (size_t k = 0; k < square.size(); k += 8)
		{
			if (square[k + 1] < sq.cy0)
				sq.cy0 = square[k + 1];
			if (square[k + 1] > sq.cy1)
				sq.cy1 = square[k + 1];
		}

		dirty[i] = 0;
	}
}

void Terrain::draw(CBFGroundTextures *textures, GLuint metalTexture, const Mat4 &vp, int mode,
                   int metalWidth, int metalHeight, float lightX, float lightY, float lightZ,
                   const float frustum[6][4])
{
	if (squares.empty())
		return;

	shader.use();
	shader.set("uVP", vp);
	shader.set("maintex", 0);
	shader.set("metaltex", 1);
	shader.set("painttex", 2);
	shader.set("mode", mode);
	shader.set("lightDir", lightX, lightY, lightZ);

	glBindVertexArray(vao);

	for (size_t i = 0; i < squares.size(); ++i)
	{
		const BigSquare &sq = squares[i];
		int bx = (int)(i % (size_t)bigX);
		int by = (int)(i / (size_t)bigX);

		if (!boxInView(frustum, sq.cx0, sq.cy0, sq.cz0, sq.cx1, sq.cy1, sq.cz1))
			continue;

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textures->squares[by * bigX + bx].texture);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, metalTexture);

		GLuint paint = textures->paintTexture(bx, by);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, paint);
		shader.set("hasPaint", paint ? 1 : 0);

		shader.set("uMetalOffset", (float)(bx * 64) / (float)metalWidth,
		           (float)(by * 64) / (float)metalHeight);
		shader.set("uMetalScale", 128.0f / (float)metalWidth, 128.0f / (float)metalHeight);

		glDrawArrays(GL_TRIANGLES, sq.first, sq.count);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(0);
}
