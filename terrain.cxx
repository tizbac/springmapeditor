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
const float kTargetQuadPixels = 8.0f;
// Squares rebuilt per updateLods() call, so a big zoom change does not stall.
const int kRebuildsPerCall = 12;
// One world unit is 64 map squares, and a map square is 8 elmos, so heights
// (which are stored in elmos) are divided by this to share the ground plane's
// scale.
const float kElmosPerUnit = 512.0f;
} // namespace

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

void Terrain::setHeightRange(float minZ_, float maxZ_)
{
	minZ = minZ_;
	maxZ = maxZ_;
	if (maxZ - minZ < 0.01f)
		maxZ = minZ + 0.01f;
}

float Terrain::worldHeight(int x, int y) const
{
	if (x < 0 || y < 0 || x > gs->mapx || y > gs->mapy)
		return minZ;

	float f = map->heightmap[(gs->mapx + 1) * y + x];
	if (f < 0.0f)
		f = 0.0f;
	if (f > 1.0f)
		f = 1.0f;
	return (minZ + f * (maxZ - minZ)) / kElmosPerUnit;
}

void Terrain::buildSquare(int bx, int by, int lod, const int neighbourLod[4],
                          std::vector<float> &out)
{
	int sqsize = 1024 >> lod;
	if (sqsize < 1)
		sqsize = 1;

	const int step = 128 / sqsize < 1 ? 1 : 128 / sqsize;
	const float quadWidth = 2.0f / (float)sqsize;
	const int n = sqsize + 1;

	const int baseX = bx * 128;
	const int baseY = by * 128;

	// Height of every grid point, in world units.
	std::vector<float> h((size_t)n * n);
	for (int Y = 0; Y < n; ++Y)
	{
		for (int X = 0; X < n; ++X)
			h[(size_t)Y * n + X] = worldHeight(baseX + 1 + X * step, baseY - 1 + Y * step);
	}

	// A fine square and a coarse neighbour do not share the vertices along their
	// border, so let the fine border follow the coarse neighbour's edge segments:
	// that keeps the surfaces watertight.
	auto sampleFront = [&](int d) { return worldHeight(baseX + 1 + d, baseY - 1); };
	auto sampleBack = [&](int d) { return worldHeight(baseX + 1 + d, baseY + 127); };
	auto sampleLeft = [&](int d) { return worldHeight(baseX + 1, baseY - 1 + d); };
	auto sampleRight = [&](int d) { return worldHeight(baseX + 129, baseY - 1 + d); };

	auto snapEdge = [&](float *edge, int nLod, auto sample) {
		if (nLod <= lod)
			return;
		int nSize = 1024 >> nLod;
		if (nSize < 1)
			nSize = 1;
		int nStep = 128 / nSize;
		if (nStep < 1)
			nStep = 1;

		for (int i = 0; i < n; ++i)
		{
			int d = i * step;
			int k = d / nStep;
			int frac = d - k * nStep;
			float h0 = (float)sample(k * nStep);
			float h1 = (float)sample((k + 1) * nStep);
			edge[i] = h0 + (h1 - h0) * (float)frac / (float)nStep;
		}
	};

	if (neighbourLod[0] > lod)
		snapEdge(&h[0], neighbourLod[0], sampleFront);
	if (neighbourLod[1] > lod)
		snapEdge(&h[(size_t)sqsize * n], neighbourLod[1], sampleBack);
	{
		std::vector<float> column((size_t)n);
		if (neighbourLod[2] > lod)
		{
			for (int i = 0; i < n; ++i)
				column[(size_t)i] = h[(size_t)i * n];
			snapEdge(column.data(), neighbourLod[2], sampleLeft);
			for (int i = 0; i < n; ++i)
				h[(size_t)i * n] = column[(size_t)i];
		}
		if (neighbourLod[3] > lod)
		{
			for (int i = 0; i < n; ++i)
				column[(size_t)i] = h[(size_t)i * n + sqsize];
			snapEdge(column.data(), neighbourLod[3], sampleRight);
			for (int i = 0; i < n; ++i)
				h[(size_t)i * n + sqsize] = column[(size_t)i];
		}
	}

	// Smooth normals from the height grid: the central differences give one
	// normal per vertex, so the ground is shaded without visible facets.
	std::vector<float> normals((size_t)n * n * 3);
	const float inv2w = 1.0f / (2.0f * quadWidth);
	for (int Y = 0; Y < n; ++Y)
	{
		for (int X = 0; X < n; ++X)
		{
			float hl = X > 0 ? h[(size_t)Y * n + X - 1]
			                 : worldHeight(baseX + 1 - step, baseY - 1 + Y * step);
			float hr = X < sqsize
			               ? h[(size_t)Y * n + X + 1]
			               : worldHeight(baseX + 1 + (sqsize + 1) * step, baseY - 1 + Y * step);
			float hd = Y > 0 ? h[(size_t)(Y - 1) * n + X]
			                 : worldHeight(baseX + 1 + X * step, baseY - 1 - step);
			float hu = Y < sqsize
			               ? h[(size_t)(Y + 1) * n + X]
			               : worldHeight(baseX + 1 + X * step, baseY - 1 + (sqsize + 1) * step);

			float nx = -(hr - hl) * inv2w;
			float nz = -(hu - hd) * inv2w;
			float len = std::sqrt(nx * nx + 1.0f + nz * nz);
			size_t i = ((size_t)Y * n + X) * 3;
			normals[i + 0] = nx / len;
			normals[i + 1] = 1.0f / len;
			normals[i + 2] = nz / len;
		}
	}

	const float x0 = bx * 2.0f;
	const float z0 = by * 2.0f;
	const float uStep = 1.0f / (float)sqsize;

	for (int Y = 0; Y < sqsize; ++Y)
	{
		for (int X = 0; X < sqsize; ++X)
		{
			float px[4] = {x0 + (float)X * quadWidth, x0 + (float)(X + 1) * quadWidth,
			               x0 + (float)(X + 1) * quadWidth, x0 + (float)X * quadWidth};
			float pz[4] = {z0 + (float)Y * quadWidth, z0 + (float)Y * quadWidth,
			               z0 + (float)(Y + 1) * quadWidth, z0 + (float)(Y + 1) * quadWidth};
			const int gi[4] = {Y * n + X, Y * n + X + 1, (Y + 1) * n + X + 1, (Y + 1) * n + X};
			float uu[4] = {(float)X * uStep, (float)(X + 1) * uStep, (float)(X + 1) * uStep,
			               (float)X * uStep};
			float vv[4] = {(float)Y * uStep, (float)Y * uStep, (float)(Y + 1) * uStep,
			               (float)(Y + 1) * uStep};

			static const int order[6] = {0, 1, 2, 0, 2, 3};
			for (int i = 0; i < 6; ++i)
			{
				int c = order[i];
				pushVertex(out, px[c], h[(size_t)gi[c]], pz[c], normals[(size_t)gi[c] * 3 + 0],
				           normals[(size_t)gi[c] * 3 + 1], normals[(size_t)gi[c] * 3 + 2], uu[c],
				           vv[c]);
			}
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

				// Do not tessellate finer than the heights can use: while the
				// relief of the square is smaller than the world size of one of
				// its quads, the extra vertices only repeat the same height, so
				// the square is coarsened (a perfectly flat square ends up as a
				// single quad, like the engine's flat patches).
				float relief = sq.cy1 - sq.cy0;
				while (want < kMaxLod)
				{
					int quads = 1024 >> want;
					float quadWorld = 2.0f / (float)(quads > 0 ? quads : 1);
					if (relief >= quadWorld * 0.5f)
						break;
					++want;
				}
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

		// A big square is 128 map squares, and the metal map has one texel per
		// two map squares, so it covers 64 by 64 texels of it.
		shader.set("uMetalOffset", (float)(bx * 64) / (float)metalWidth,
		           (float)(by * 64) / (float)metalHeight);
		shader.set("uMetalScale", 64.0f / (float)metalWidth, 64.0f / (float)metalHeight);

		glBindVertexArray(sq.vao);
		glDrawArrays(GL_TRIANGLES, 0, sq.count);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(0);
}
