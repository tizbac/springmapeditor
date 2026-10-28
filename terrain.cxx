/*
 * terrain.cxx - ROAM terrain mesh.
 *
 * This is a port of RecoilEngine's ROAM patch tessellation. Each 128x128 map
 * square patch keeps a fixed full resolution vertex grid; a binary triangle
 * tree selects the vertices used by the current level of detail and is turned
 * into an index buffer. Changing LOD therefore only rebuilds indices, never
 * vertices.
 */
#include "terrain.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Map/GlobalStuff.h"
#include "Map/SMF/BFGroundTextures.h"
#include "Map/SMF/SmfReadMap.h"

namespace {
const int kDefaultViewRadius = 60;
const int kMinViewRadius = 20;
const int kMaxViewRadius = 120;
// One map square is 1/64 world unit; ROAM's variance values are in elmos.
const float kElmosPerUnit = 512.0f;
const float kWorldPerSquare = 1.0f / 64.0f;

// True when the box is not entirely outside one of the frustum planes.
bool boxInView(const float frustum[6][4], float x0, float y0, float z0, float x1, float y1,
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
		if (best <= -0.01f)
			return false;
	}
	return true;
}

bool viewChanged(const Mat4 &a, const Mat4 &b)
{
	for (int i = 0; i < 16; ++i)
	{
		if (std::fabs(a.m[i] - b.m[i]) > 1e-4f)
			return true;
	}
	return false;
}
} // namespace

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

float Terrain::heightWorld(int gx, int gz) const
{
	if (!map)
		return minZ / kElmosPerUnit;

	gx = std::max(0, std::min(gs->mapx, gx));
	gz = std::max(0, std::min(gs->mapy, gz));

	float f = map->heightmap[(gs->mapx + 1) * gz + gx];
	if (f < 0.0f)
		f = 0.0f;
	if (f > 1.0f)
		f = 1.0f;
	return (minZ + f * (maxZ - minZ)) / kElmosPerUnit;
}

float Terrain::heightElmos(int gx, int gz) const
{
	return heightWorld(gx, gz) * kElmosPerUnit;
}

void Terrain::deleteGpuResources()
{
	for (size_t i = 0; i < patches.size(); ++i)
	{
		if (patches[i].vao)
			glDeleteVertexArrays(1, &patches[i].vao);
		if (patches[i].ebo)
			glDeleteBuffers(1, &patches[i].ebo);
	}
	patches.clear();

	if (sharedVao)
		glDeleteVertexArrays(1, &sharedVao);
	if (sharedVbo)
		glDeleteBuffers(1, &sharedVbo);
	if (heightTexture)
		glDeleteTextures(1, &heightTexture);

	sharedVao = 0;
	sharedVbo = 0;
	heightTexture = 0;
	heightTextureW = 0;
	heightTextureH = 0;
}

void Terrain::createSharedGrid()
{
	std::vector<float> verts;
	verts.reserve((size_t)kPatchGrid * kPatchGrid * 5);

	for (int y = 0; y < kPatchGrid; ++y)
	{
		for (int x = 0; x < kPatchGrid; ++x)
		{
			verts.push_back((float)x * (2.0f / (float)kPatchSize));
			verts.push_back(0.0f);
			verts.push_back((float)y * (2.0f / (float)kPatchSize));
			verts.push_back((float)x / (float)kPatchSize);
			verts.push_back((float)y / (float)kPatchSize);
		}
	}

	glGenVertexArrays(1, &sharedVao);
	glGenBuffers(1, &sharedVbo);
	glBindVertexArray(sharedVao);
	glBindBuffer(GL_ARRAY_BUFFER, sharedVbo);
	glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)verts.size() * sizeof(float), verts.data(),
	             GL_STATIC_DRAW);

	const GLsizei stride = 5 * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (const void *)(3 * sizeof(float)));
	glBindVertexArray(0);
}

void Terrain::createHeightTexture()
{
	heightTextureW = gs->mapx + 1;
	heightTextureH = gs->mapy + 1;

	glGenTextures(1, &heightTexture);
	glBindTexture(GL_TEXTURE_2D, heightTexture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, heightTextureW, heightTextureH, 0, GL_RED, GL_FLOAT,
	             map->heightmap);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Terrain::updateHeightTexture(Patch &patch)
{
	int x0 = patch.bx * kPatchSize;
	int y0 = patch.by * kPatchSize;
	int w = std::min(kPatchGrid, heightTextureW - x0);
	int h = std::min(kPatchGrid, heightTextureH - y0);
	if (w <= 0 || h <= 0)
		return;

	std::vector<float> region((size_t)w * h);
	for (int row = 0; row < h; ++row)
	{
		memcpy(&region[(size_t)row * w], &map->heightmap[(size_t)(y0 + row) * heightTextureW + x0],
		       (size_t)w * sizeof(float));
	}

	glBindTexture(GL_TEXTURE_2D, heightTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, w, h, GL_RED, GL_FLOAT, region.data());
	glBindTexture(GL_TEXTURE_2D, 0);
}

void Terrain::updatePatchBounds(Patch &patch)
{
	float ymin = 1e30f;
	float ymax = -1e30f;

	for (int z = 0; z < kPatchGrid; ++z)
	{
		for (int x = 0; x < kPatchGrid; ++x)
		{
			float h = heightWorld(patch.bx * kPatchSize + x, patch.by * kPatchSize + z);
			ymin = std::min(ymin, h);
			ymax = std::max(ymax, h);
		}
	}

	patch.cx0 = (float)patch.bx * 2.0f;
	patch.cz0 = (float)patch.by * 2.0f;
	patch.cx1 = patch.cx0 + 2.0f;
	patch.cz1 = patch.cz0 + 2.0f;
	patch.cy0 = ymin;
	patch.cy1 = ymax;

	patch.midX = (patch.cx0 + patch.cx1) * 0.5f * kElmosPerUnit;
	patch.midZ = (patch.cz0 + patch.cz1) * 0.5f * kElmosPerUnit;
	patch.midY = (ymin + ymax) * 0.5f * kElmosPerUnit;
}

void Terrain::resetPatches()
{
	nextNode = 0;

	for (size_t i = 0; i < patches.size(); ++i)
	{
		Patch &patch = patches[i];
		patch.baseLeft = TriNode();
		patch.baseRight = TriNode();
		patch.baseLeft.base = &patch.baseRight;
		patch.baseRight.base = &patch.baseLeft;
		patch.baseLeft.parentPatch = &patch;
		patch.baseRight.parentPatch = &patch;
		patch.changed = true;
		// Keep the previous index buffer for hidden patches. If one enters view
		// before the next retessellation, it still draws its last valid mesh
		// instead of leaving a hole.
	}

	linkPatchNeighbors();
}

void Terrain::linkPatchNeighbors()
{
	for (int by = 0; by < bigY; ++by)
	{
		for (int bx = 0; bx < bigX; ++bx)
		{
			size_t index = (size_t)by * bigX + bx;
			TriNode *baseLeft = &patches[index].baseLeft;
			TriNode *baseRight = &patches[index].baseRight;

			if (bx > 0)
				baseLeft->leftNeighbor = &patches[index - 1].baseRight;
			if (bx + 1 < bigX)
				baseRight->leftNeighbor = &patches[index + 1].baseLeft;
			if (by > 0)
				baseLeft->rightNeighbor = &patches[index - bigX].baseRight;
			if (by + 1 < bigY)
				baseRight->rightNeighbor = &patches[index + bigX].baseLeft;
		}
	}
}

bool Terrain::allocateNode(TriNode *&left, TriNode *&right)
{
	if (nextNode + 2 > nodePool.size())
		return false;

	left = &nodePool[nextNode++];
	right = &nodePool[nextNode++];
	*left = TriNode();
	*right = TriNode();
	return true;
}

// Splits one triangle and maintains the diamond/neighbour links, exactly like
// Patch::Split in RecoilEngine.
bool Terrain::splitNode(Patch &patch, TriNode *node)
{
	if (!node)
		return false;
	if (!node->isLeaf())
		return true;

	Patch &owner = node->parentPatch ? *node->parentPatch : patch;

	if (node->base && node->base->base != node)
	{
		Patch &neighbourOwner = node->base->parentPatch ? *node->base->parentPatch : owner;
		splitNode(neighbourOwner, node->base);
		if (node->base->parentPatch != &owner)
			owner.changed = true;
	}

	TriNode *left = nullptr;
	TriNode *right = nullptr;
	if (!allocateNode(left, right))
		return false;

	node->left = left;
	node->right = right;
	left->parentPatch = &owner;
	right->parentPatch = &owner;
	owner.changed = true;

	TriNode *leftNeighbor = node->leftNeighbor;
	TriNode *rightNeighbor = node->rightNeighbor;
	TriNode *baseNeighbor = node->base;

	left->base = leftNeighbor;
	left->leftNeighbor = right;
	right->base = rightNeighbor;
	right->rightNeighbor = left;

	if (leftNeighbor)
	{
		if (leftNeighbor->base == node)
			leftNeighbor->base = left;
		else if (leftNeighbor->leftNeighbor == node)
			leftNeighbor->leftNeighbor = left;
		else if (leftNeighbor->rightNeighbor == node)
			leftNeighbor->rightNeighbor = left;
	}
	if (rightNeighbor)
	{
		if (rightNeighbor->base == node)
			rightNeighbor->base = right;
		else if (rightNeighbor->rightNeighbor == node)
			rightNeighbor->rightNeighbor = right;
		else if (rightNeighbor->leftNeighbor == node)
			rightNeighbor->leftNeighbor = right;
	}

	if (baseNeighbor)
	{
		if (!baseNeighbor->isLeaf())
		{
			left->rightNeighbor = baseNeighbor->right;
			if (left->rightNeighbor)
				left->rightNeighbor->leftNeighbor = left;
			right->leftNeighbor = baseNeighbor->left;
			if (right->leftNeighbor)
				right->leftNeighbor->rightNeighbor = right;
		} else
		{
			Patch &neighbourOwner = baseNeighbor->parentPatch ? *baseNeighbor->parentPatch : owner;
			splitNode(neighbourOwner, baseNeighbor);
			if (baseNeighbor->parentPatch != &owner)
				owner.changed = true;
		}
	} else
	{
		left->rightNeighbor = nullptr;
		right->leftNeighbor = nullptr;
	}

	return true;
}

float Terrain::recursComputeVariance(Patch &patch, int tree, int nodeIndex, IPoint left,
                                     IPoint right, IPoint apex, float hLeft, float hRight,
                                     float hApex)
{
	IPoint mid = {(left.x + right.x) >> 1, (left.y + right.y) >> 1};
	float midHeight = heightElmos(patch.bx * kPatchSize + mid.x, patch.by * kPatchSize + mid.y);

	float variance = std::fabs(midHeight - (hLeft + hRight) * 0.5f);
	if ((hLeft * hRight) < 0.0f || (hLeft * midHeight) < 0.0f || (hRight * midHeight) < 0.0f)
	{
		variance = std::max(variance * 1.5f, 20.0f);
	}

	if (nodeIndex < kVarianceSize &&
	    (std::abs(left.x - right.x) >= 4 || std::abs(left.y - right.y) >= 4))
	{
		variance = std::max(variance, recursComputeVariance(patch, tree, nodeIndex * 2, apex, left,
		                                                    mid, hApex, hLeft, midHeight));
		variance = std::max(variance, recursComputeVariance(patch, tree, nodeIndex * 2 + 1, right,
		                                                    apex, mid, hRight, hApex, midHeight));
	}

	variance = std::max(0.001f, variance);
	if (nodeIndex < kVarianceSize)
		patch.variance[tree][nodeIndex] = variance;
	return variance;
}

void Terrain::computeVariance(Patch &patch, int tree)
{
	if (tree == 0)
	{
		IPoint left = {0, kPatchSize};
		IPoint right = {kPatchSize, 0};
		IPoint apex = {0, 0};
		float hLeft = heightElmos(patch.bx * kPatchSize + left.x, patch.by * kPatchSize + left.y);
		float hRight =
		    heightElmos(patch.bx * kPatchSize + right.x, patch.by * kPatchSize + right.y);
		float hApex = heightElmos(patch.bx * kPatchSize + apex.x, patch.by * kPatchSize + apex.y);
		recursComputeVariance(patch, tree, 1, left, right, apex, hLeft, hRight, hApex);
	} else
	{
		IPoint left = {kPatchSize, 0};
		IPoint right = {0, kPatchSize};
		IPoint apex = {kPatchSize, kPatchSize};
		float hLeft = heightElmos(patch.bx * kPatchSize + left.x, patch.by * kPatchSize + left.y);
		float hRight =
		    heightElmos(patch.bx * kPatchSize + right.x, patch.by * kPatchSize + right.y);
		float hApex = heightElmos(patch.bx * kPatchSize + apex.x, patch.by * kPatchSize + apex.y);
		recursComputeVariance(patch, tree, 1, left, right, apex, hLeft, hRight, hApex);
	}
}

void Terrain::tessellate(Patch &patch, const float camPos[3], int viewRadius)
{
	if (viewRadius < 1)
		viewRadius = 1;

	float dx = patch.midX - camPos[0];
	float dy = patch.midY - camPos[1];
	float dz = patch.midZ - camPos[2];
	float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
	patch.lastCamDistance = distance;

	float factor = distance * (300.0f / (float)viewRadius);
	factor = std::max(1.0f, factor);
	patch.camDistLODFactor = 1.0f / factor;
	patch.varianceMaxLimit = (float)viewRadius * 0.35f;

	{
		IPoint left = {0, kPatchSize};
		IPoint right = {kPatchSize, 0};
		IPoint apex = {0, 0};
		recursTessellate(patch, &patch.baseLeft, left, right, apex, 0, 1);
	}
	{
		IPoint left = {kPatchSize, 0};
		IPoint right = {0, kPatchSize};
		IPoint apex = {kPatchSize, kPatchSize};
		recursTessellate(patch, &patch.baseRight, left, right, apex, 1, 1);
	}

	if (patch.baseLeft.isLeaf() && patch.baseRight.isLeaf())
		patch.changed = true;
}

void Terrain::recursTessellate(Patch &patch, TriNode *node, IPoint left, IPoint right, IPoint apex,
                               int tree, int nodeIndex)
{
	if (!node)
		return;
	if (std::abs(left.x - right.x) <= 1 && std::abs(left.y - right.y) <= 1)
		return;
	if (!node->isLeaf())
	{
		IPoint center = {(left.x + right.x) >> 1, (left.y + right.y) >> 1};
		recursTessellate(patch, node->left, apex, left, center, tree, nodeIndex * 2);
		recursTessellate(patch, node->right, right, apex, center, tree, nodeIndex * 2 + 1);
		return;
	}

	float variance = 10.0f;
	if (nodeIndex < kVarianceSize)
	{
		int sizeX = std::abs(left.x - right.x);
		int sizeY = std::abs(left.y - right.y);
		int size = std::max(sizeX, sizeY);
		float stored = patch.variance[tree][nodeIndex];
		variance = std::min(stored, patch.varianceMaxLimit) * (float)kPatchSize * (float)size *
		           patch.camDistLODFactor;
	}

	if (variance <= 1.0f)
		return;

	if (!splitNode(patch, node))
		return;
	if (node->isLeaf())
		return;

	IPoint center = {(left.x + right.x) >> 1, (left.y + right.y) >> 1};
	recursTessellate(patch, node->left, apex, left, center, tree, nodeIndex * 2);
	recursTessellate(patch, node->right, right, apex, center, tree, nodeIndex * 2 + 1);
}

void Terrain::recursRender(Patch &patch, TriNode *node, IPoint left, IPoint right, IPoint apex)
{
	if (!node)
		return;
	if (node->isLeaf())
	{
		int apexIndex = apex.x + apex.y * kPatchGrid;
		int leftIndex = left.x + left.y * kPatchGrid;
		int rightIndex = right.x + right.y * kPatchGrid;

		if (node->base && node->base->base == node)
		{
			IPoint baseApex = {left.x + right.x - apex.x, left.y + right.y - apex.y};
			if (baseApex.x >= 0 && baseApex.y >= 0 && baseApex.x < kPatchGrid &&
			    baseApex.y < kPatchGrid)
			{
				float apexHeight =
				    heightElmos(patch.bx * kPatchSize + apex.x, patch.by * kPatchSize + apex.y);
				float leftHeight =
				    heightElmos(patch.bx * kPatchSize + left.x, patch.by * kPatchSize + left.y);
				float rightHeight =
				    heightElmos(patch.bx * kPatchSize + right.x, patch.by * kPatchSize + right.y);
				float baseApexHeight = heightElmos(patch.bx * kPatchSize + baseApex.x,
				                                   patch.by * kPatchSize + baseApex.y);

				float heightDiff = std::fabs(leftHeight - rightHeight);
				float heightDiff2 = std::fabs(apexHeight - baseApexHeight);
				if (heightDiff2 < heightDiff - 0.0001f)
				{
					int baseApexIndex = baseApex.x + baseApex.y * kPatchGrid;
					patch.indices.push_back((uint32_t)apexIndex);
					patch.indices.push_back((uint32_t)leftIndex);
					patch.indices.push_back((uint32_t)baseApexIndex);
					return;
				}
			}
		}

		patch.indices.push_back((uint32_t)apexIndex);
		patch.indices.push_back((uint32_t)leftIndex);
		patch.indices.push_back((uint32_t)rightIndex);
		return;
	}

	IPoint center = {(left.x + right.x) >> 1, (left.y + right.y) >> 1};
	recursRender(patch, node->left, apex, left, center);
	recursRender(patch, node->right, right, apex, center);
}

void Terrain::generateIndices(Patch &patch)
{
	patch.indices.clear();

	IPoint left = {0, kPatchSize};
	IPoint right = {kPatchSize, 0};
	IPoint apex = {0, 0};
	recursRender(patch, &patch.baseLeft, left, right, apex);

	left = {kPatchSize, 0};
	right = {0, kPatchSize};
	apex = {kPatchSize, kPatchSize};
	recursRender(patch, &patch.baseRight, left, right, apex);
}

void Terrain::uploadIndices(Patch &patch)
{
	if (patch.indices.empty())
	{
		patch.indexCount = 0;
		return;
	}

	glBindVertexArray(patch.vao);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, patch.ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)patch.indices.size() * sizeof(uint32_t),
	             patch.indices.data(), GL_DYNAMIC_DRAW);
	glBindVertexArray(0);
	patch.indexCount = (int)patch.indices.size();
}

void Terrain::build(CSmfReadMap *map_, CBFGroundTextures *textures_)
{
	map = map_;
	textures = textures_;
	bigX = textures->numBigSquaresX();
	bigY = textures->numBigSquaresY();

	deleteGpuResources();
	createSharedGrid();
	createHeightTexture();

	patches.assign((size_t)bigX * bigY, Patch());
	textureDirty.assign((size_t)bigX * bigY, 0);
	nodePool.assign((size_t)kNodePoolSize, TriNode());
	nextNode = 0;

	for (int by = 0; by < bigY; ++by)
	{
		for (int bx = 0; bx < bigX; ++bx)
		{
			Patch &patch = patches[(size_t)by * bigX + bx];
			patch.bx = bx;
			patch.by = by;

			glGenVertexArrays(1, &patch.vao);
			glGenBuffers(1, &patch.ebo);

			glBindVertexArray(patch.vao);
			glBindBuffer(GL_ARRAY_BUFFER, sharedVbo);
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (const void *)0);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
			                      (const void *)(3 * sizeof(float)));
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, patch.ebo);
			glBindVertexArray(0);

			updatePatchBounds(patch);
			patch.tessDirty = true;
		}
	}

	resetPatches();
	haveLastView = false;
	lastBias = 0;
	tessellationDirty = true;

	printf("Terrain: %dx%d ROAM patches\n", bigX, bigY);
}

bool Terrain::updateLods(const Mat4 &view, int viewportHeight, float fovDegrees, int bias,
                         const float frustum[6][4])
{
	(void)viewportHeight;
	(void)fovDegrees;

	if (patches.empty())
		return false;

	static std::chrono::steady_clock::time_point lastUpdate;
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

	bool force = tessellationDirty;
	if (!force && haveLastView && bias == lastBias && !viewChanged(view, lastView))
	{
		return false;
	}
	if (!force && now - lastUpdate < std::chrono::milliseconds(100))
		return false;

	lastUpdate = now;

	Mat4 invView = view.inverse();
	float camX = 0.0f, camY = 0.0f, camZ = 0.0f;
	invView.transformPoint(0.0f, 0.0f, 0.0f, camX, camY, camZ);
	float camPos[3] = {camX * kElmosPerUnit, camY * kElmosPerUnit, camZ * kElmosPerUnit};

	int viewRadius = kDefaultViewRadius + bias * 10;
	viewRadius = std::max(kMinViewRadius, std::min(kMaxViewRadius, viewRadius));

	for (size_t i = 0; i < patches.size(); ++i)
	{
		Patch &patch = patches[i];
		patch.visible =
		    boxInView(frustum, patch.cx0, patch.cy0, patch.cz0, patch.cx1, patch.cy1, patch.cz1);

		if (patch.tessDirty)
		{
			computeVariance(patch, 0);
			computeVariance(patch, 1);
			patch.tessDirty = false;
		}
	}

	resetPatches();

	for (size_t i = 0; i < patches.size(); ++i)
	{
		Patch &patch = patches[i];
		if (!patch.visible)
			continue;

		tessellate(patch, camPos, viewRadius);
		generateIndices(patch);
		uploadIndices(patch);
	}

	lastView = view;
	haveLastView = true;
	lastBias = bias;
	tessellationDirty = false;
	return true;
}

void Terrain::markDirty(int bx, int by)
{
	if (bx < 0 || by < 0 || bx >= bigX || by >= bigY)
		return;

	size_t index = (size_t)by * bigX + bx;
	textureDirty[index] = 1;
	patches[index].tessDirty = true;
	tessellationDirty = true;
}

void Terrain::flushDirty()
{
	if (patches.empty())
		return;

	for (size_t i = 0; i < patches.size(); ++i)
	{
		if (!textureDirty[i])
			continue;
		updateHeightTexture(patches[i]);
		updatePatchBounds(patches[i]);
		textureDirty[i] = 0;
	}
}

void Terrain::draw(CBFGroundTextures *ground, GLuint metalTexture, const Mat4 &vp, int mode,
                   int metalWidth, int metalHeight, const TerrainLight &light,
                   const float frustum[6][4])
{
	if (patches.empty())
		return;

	shader.use();
	shader.set("uVP", vp);
	shader.set("maintex", 0);
	shader.set("metaltex", 1);
	shader.set("painttex", 2);
	shader.set("heightMapTex", 3);
	shader.set("mode", mode);
	shader.set("sunDir", light.dir[0], light.dir[1], light.dir[2]);
	shader.set("groundAmbientColor", light.ambient[0], light.ambient[1], light.ambient[2]);
	shader.set("groundDiffuseColor", light.diffuse[0], light.diffuse[1], light.diffuse[2]);
	shader.set("uMapSize", (float)heightTextureW, (float)heightTextureH);
	shader.set("uHeightMin", minZ / kElmosPerUnit);
	shader.set("uHeightScale", (maxZ - minZ) / kElmosPerUnit);

	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, heightTexture);

	for (size_t i = 0; i < patches.size(); ++i)
	{
		const Patch &patch = patches[i];
		if (patch.indexCount <= 0)
			continue;
		if (!boxInView(frustum, patch.cx0, patch.cy0, patch.cz0, patch.cx1, patch.cy1, patch.cz1))
			continue;

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, ground->squares[patch.by * bigX + patch.bx].texture);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, metalTexture);

		GLuint paint = ground->paintTexture(patch.bx, patch.by);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, paint);
		shader.set("hasPaint", paint ? 1 : 0);

		shader.set("uMetalOffset", (float)(patch.bx * 64) / (float)metalWidth,
		           (float)(patch.by * 64) / (float)metalHeight);
		shader.set("uMetalScale", 64.0f / (float)metalWidth, 64.0f / (float)metalHeight);
		shader.set("uSquareOffset", (float)patch.bx * 2.0f, (float)patch.by * 2.0f);

		glBindVertexArray(patch.vao);
		glDrawElements(GL_TRIANGLES, patch.indexCount, GL_UNSIGNED_INT, nullptr);
	}

	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
}
