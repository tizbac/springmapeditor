/*
 * terrain.cxx - ground mesh built into one vertex buffer per big square.
 */
#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "Map/GlobalStuff.h"
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"

namespace {
const int kDefaultLod = 6;
const int kMinLod = 3;  // one quad per map square
const int kMaxLod = 10; // one quad per big square
// How large a quad should be on screen, in pixels. Used to pick the level of
// detail of each square.
const float kTargetQuadPixels = 3.0f;
// Squares rebuilt per updateLods() call, so a big zoom change does not stall.
const int kRebuildsPerCall = 12;
} // namespace

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
		// A small epsilon so a square that just touches a plane is kept.
		if (best <= -0.01f)
			return false;
	}
	return true;
}

bool Terrain::init()
{
	if (!shader.createFromFiles("ground.vert", "ground.frag"))
		return false;
	return true;
}

void Terrain::buildSquare(int bx, int by, int lod, const int neighbourLod[4],
                          std::vector<float> &out)
{
	int sqsize = 1024 >> lod;
	if (sqsize < 1)
		sqsize = 1;

	const float quadWidth = 2.0f / (float)sqsize;
	const int step = 128 / sqsize < 1 ? 1 : 128 / sqsize;

	// Sample the four borders along the map grid, then let them follow the
	// neighbour when it is coarser: a fine square and a coarse square do not
	// share vertices along their border, so a fine border vertex that sits on
	// the coarse neighbour's straight edge segment is what keeps the surfaces
	// watertight.
	const int baseX = bx * 128;
	const int baseY = by * 128;

	auto sampleFront = [&](int d) { return mapHeight(map, baseX + 1 + d, baseY - 1); };
	auto sampleBack = [&](int d) { return mapHeight(map, baseX + 1 + d, baseY + 127); };
	auto sampleLeft = [&](int d) { return mapHeight(map, baseX + 1, baseY - 1 + d); };
	auto sampleRight = [&](int d) { return mapHeight(map, baseX + 129, baseY - 1 + d); };

	std::vector<float> edgeFront((size_t)sqsize + 1, 0.0f), edgeBack((size_t)sqsize + 1, 0.0f);
	std::vector<float> edgeLeft((size_t)sqsize + 1, 0.0f), edgeRight((size_t)sqsize + 1, 0.0f);
	for (int i = 0; i <= sqsize; ++i)
	{
		int d = i * step;
		edgeFront[(size_t)i] = sampleFront(d);
		edgeBack[(size_t)i] = sampleBack(d);
		edgeLeft[(size_t)i] = sampleLeft(d);
		edgeRight[(size_t)i] = sampleRight(d);
	}

	auto snapEdge = [&](std::vector<float> &edge, int nLod, auto sample) {
		if (nLod <= lod)
			return;
		int nSize = 1024 >> nLod;
		if (nSize < 1)
			nSize = 1;
		int nStep = 128 / nSize;
		if (nStep < 1)
			nStep = 1;

		for (int i = 0; i <= sqsize; ++i)
		{
			int d = i * step;
			int k = d / nStep;
			int frac = d - k * nStep;
			float h0 = (float)sample(k * nStep);
			float h1 = (float)sample((k + 1) * nStep);
			edge[(size_t)i] = h0 + (h1 - h0) * (float)frac / (float)nStep;
		}
	};

	snapEdge(edgeFront, neighbourLod[0], sampleFront);
	snapEdge(edgeBack, neighbourLod[1], sampleBack);
	snapEdge(edgeLeft, neighbourLod[2], sampleLeft);
	snapEdge(edgeRight, neighbourLod[3], sampleRight);

	for (int Y = 0; Y < sqsize; ++Y)
	{
		for (int X = 0; X < sqsize; ++X)
		{
			float z1 = mapHeight(map, baseX + X * step + 1, baseY + Y * step - 1);
			float z2 = mapHeight(map, baseX + (X + 1) * step + 1, baseY + Y * step - 1);
			float z3 = mapHeight(map, baseX + (X + 1) * step + 1, baseY + (Y + 1) * step - 1);
			float z4 = mapHeight(map, baseX + X * step + 1, baseY + (Y + 1) * step - 1);

			if (Y == 0)
			{
				z1 = edgeFront[(size_t)X];
				z2 = edgeFront[(size_t)X + 1];
			}
			if (Y == sqsize - 1)
			{
				z4 = edgeBack[(size_t)X];
				z3 = edgeBack[(size_t)X + 1];
			}
			if (X == 0)
			{
				z1 = edgeLeft[(size_t)Y];
				z4 = edgeLeft[(size_t)Y + 1];
			}
			if (X == sqsize - 1)
			{
				z2 = edgeRight[(size_t)Y];
				z3 = edgeRight[(size_t)Y + 1];
			}

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

void Terrain::uploadSquare(int index, int lod)
{
	BigSquare &sq = squares[(size_t)index];
	int bx = (int)((size_t)index % (size_t)bigX);
	int by = (int)((size_t)index / (size_t)bigX);

	// Levels of the neighbours: front, back, left, right. A square that is finer
	// than a neighbour snaps its border onto that neighbour's edge segments.
	int neighbourLod[4] = {lod, lod, lod, lod};
	if (by > 0)
		neighbourLod[0] = squares[(size_t)(by - 1) * bigX + bx].lod;
	if (by + 1 < bigY)
		neighbourLod[1] = squares[(size_t)(by + 1) * bigX + bx].lod;
	if (bx > 0)
		neighbourLod[2] = squares[(size_t)by * bigX + bx - 1].lod;
	if (bx + 1 < bigX)
		neighbourLod[3] = squares[(size_t)by * bigX + bx + 1].lod;

	std::vector<float> verts;
	buildSquare(bx, by, lod, neighbourLod, verts);

	if (!sq.vao)
	{
		glGenVertexArrays(1, &sq.vao);
		glGenBuffers(1, &sq.vbo);
	}

	glBindVertexArray(sq.vao);
	glBindBuffer(GL_ARRAY_BUFFER, sq.vbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)verts.size() * sizeof(float), verts.data(),
	             GL_DYNAMIC_DRAW);

	const GLsizei stride = 8 * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(6 * sizeof(float)));
	glBindVertexArray(0);

	sq.count = (int)(verts.size() / 8);
	sq.lod = lod;

	sq.cx0 = (float)(bx * 2);
	sq.cz0 = (float)(by * 2);
	sq.cx1 = (float)(bx * 2 + 2);
	sq.cz1 = (float)(by * 2 + 2);
	sq.cy0 = 0.0f;
	sq.cy1 = 0.0f;
	for (size_t k = 0; k < verts.size(); k += 8)
	{
		if (verts[k + 1] < sq.cy0)
			sq.cy0 = verts[k + 1];
		if (verts[k + 1] > sq.cy1)
			sq.cy1 = verts[k + 1];
	}
}

void Terrain::build(CSmfReadMap *map_, CBFGroundTextures *textures_)
{
	map = map_;
	textures = textures_;
	bigX = textures->numBigSquaresX();
	bigY = textures->numBigSquaresY();

	for (size_t i = 0; i < squares.size(); ++i)
	{
		if (squares[i].vao)
			glDeleteVertexArrays(1, &squares[i].vao);
		if (squares[i].vbo)
			glDeleteBuffers(1, &squares[i].vbo);
	}

	squares.assign((size_t)bigX * bigY, BigSquare());
	dirty.assign((size_t)bigX * bigY, 0);

	for (int i = 0; i < (int)squares.size(); ++i)
		uploadSquare(i, kDefaultLod);

	printf("Terrain: %dx%d big squares, level %d\n", bigX, bigY, kDefaultLod);
}

int Terrain::lodOf(int bx, int by) const
{
	if (bx < 0 || by < 0 || bx >= bigX || by >= bigY)
		return kDefaultLod;
	return squares[(size_t)by * bigX + bx].lod;
}

bool Terrain::updateLods(const Mat4 &view, int viewportHeight, float fovDegrees, int bias)
{
	if (squares.empty() || viewportHeight < 1)
		return false;

	const float tanHalfFov = std::tan(fovDegrees * 3.14159265358979f / 360.0f);
	const float pixelsPerUnitAtOne = (0.5f * (float)viewportHeight) / tanHalfFov;

	struct Change {
		float size;
		int index;
		int lod;
	};
	std::vector<Change> changes;

	for (int by = 0; by < bigY; ++by)
	{
		for (int bx = 0; bx < bigX; ++bx)
		{
			size_t i = (size_t)by * bigX + bx;
			BigSquare &sq = squares[i];

			// Depth along the view direction, not the distance to the eye and
			// not the projected extent: both make the mesh change while the
			// camera only rotates.
			float cx = bx * 2.0f + 1.0f;
			float cz = by * 2.0f + 1.0f;
			float camZ = view.m[2] * cx + view.m[6] * 0.0f + view.m[10] * cz + view.m[14];
			float depth = -camZ;

			float size = 0.0f;
			int want = kMaxLod;
			if (depth > 0.01f)
			{
				size = 2.0f * pixelsPerUnitAtOne / depth;
				float levelF = 10.0f + std::log2(kTargetQuadPixels) - std::log2(size) + (float)bias;
				want = (int)std::floor(levelF);
				if (want < kMinLod)
					want = kMinLod;
				if (want > kMaxLod)
					want = kMaxLod;
			}

			sq.screenSize = size;
			if (want != sq.lod)
				changes.push_back({size, (int)i, want});
		}
	}

	if (changes.empty())
		return false;

	// Refine the squares that cover the most screen first, and only a few per
	// call so a zoom does not stall the frame.
	std::sort(changes.begin(), changes.end(),
	          [](const Change &a, const Change &b) { return a.size > b.size; });

	int done = 0;
	for (size_t c = 0; c < changes.size(); ++c)
	{
		if (done >= kRebuildsPerCall)
			break;
		int index = changes[c].index;
		int bx = index % bigX;
		int by = index / bigX;

		// Neighbours snap their border onto this square, so they have to be
		// rebuilt as well when its level changes.
		markDirty(bx - 1, by);
		markDirty(bx + 1, by);
		markDirty(bx, by - 1);
		markDirty(bx, by + 1);

		squares[(size_t)index].lod = changes[c].lod;
		uploadSquare(index, changes[c].lod);
		++done;
	}
	return true;
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

	for (size_t i = 0; i < squares.size(); ++i)
	{
		if (!dirty[i])
			continue;
		uploadSquare((int)i, squares[i].lod);
		dirty[i] = 0;
	}
}

void Terrain::draw(CBFGroundTextures *ground, GLuint metalTexture, const Mat4 &vp, int mode,
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

	for (size_t i = 0; i < squares.size(); ++i)
	{
		const BigSquare &sq = squares[i];
		int bx = (int)(i % (size_t)bigX);
		int by = (int)(i / (size_t)bigX);

		if (!boxInView(frustum, sq.cx0, sq.cy0, sq.cz0, sq.cx1, sq.cy1, sq.cz1))
			continue;

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, ground->squares[by * bigX + bx].texture);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, metalTexture);

		GLuint paint = ground->paintTexture(bx, by);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, paint);
		shader.set("hasPaint", paint ? 1 : 0);

		shader.set("uMetalOffset", (float)(bx * 64) / (float)metalWidth,
		           (float)(by * 64) / (float)metalHeight);
		shader.set("uMetalScale", 128.0f / (float)metalWidth, 128.0f / (float)metalHeight);

		glBindVertexArray(sq.vao);
		glDrawArrays(GL_TRIANGLES, 0, sq.count);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(0);
}
