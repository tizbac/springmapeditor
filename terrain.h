/*
 * terrain.h - ROAM terrain mesh.
 *
 * The mesh follows the same approach as RecoilEngine's ROAM drawer: every big
 * square is a patch with a fixed, full resolution vertex grid, while only the
 * triangle-tree indices are regenerated when the level of detail changes. A
 * shared vertex buffer and the height-map texture are reused by every patch.
 */
#ifndef TERRAIN_H
#define TERRAIN_H

#include <GL/glew.h>

#include <array>
#include <cstdint>
#include <vector>

#include "mat4.h"
#include "shader.h"

class CSmfReadMap;
class CBFGroundTextures;

struct TerrainLight {
	float dir[3] = {0.0f, 1.0f, 0.0f};
	float ambient[3] = {0.5f, 0.5f, 0.5f};
	float diffuse[3] = {0.5f, 0.5f, 0.5f};
};

class Terrain {
  public:
	bool init();

	// Builds the shared grid and the per-patch ROAM state.
	void build(CSmfReadMap *map, CBFGroundTextures *textures);

	// The world height range the normalised height map is mapped onto.
	void setHeightRange(float minZ, float maxZ);

	// Retessellates the visible patches from the current camera. The frustum is
	// used to skip patches that cannot be seen.
	bool updateLods(const Mat4 &view, int viewportHeight, float fovDegrees, int bias,
	                const float frustum[6][4]);

	void markDirty(int bx, int by);
	// Uploads changed height-map regions and recomputes affected variances.
	void flushDirty();

	void draw(CBFGroundTextures *textures, GLuint metalTexture, const Mat4 &vp, int mode,
	          int metalWidth, int metalHeight, const TerrainLight &light,
	          const float frustum[6][4]);

	bool built() const
	{
		return !patches.empty();
	}

  private:
	static constexpr int kPatchSize = 128;
	static constexpr int kPatchGrid = kPatchSize + 1;
	static constexpr int kNodePoolSize = 1 << 20;
	static constexpr int kVarianceDepth = 12;
	static constexpr int kVarianceSize = 1 << kVarianceDepth;

	struct IPoint {
		int x;
		int y;
	};

	struct Patch;

	struct TriNode {
		TriNode *left = nullptr;
		TriNode *right = nullptr;
		TriNode *base = nullptr;
		TriNode *leftNeighbor = nullptr;
		TriNode *rightNeighbor = nullptr;
		Patch *parentPatch = nullptr;

		bool isLeaf() const
		{
			return left == nullptr;
		}
	};

	struct Patch {
		int bx = 0;
		int by = 0;
		GLuint vao = 0;
		GLuint ebo = 0;
		int indexCount = 0;
		bool changed = true;
		bool tessDirty = true;
		bool visible = false;

		// Bounds in world space, used for frustum culling.
		float cx0 = 0.0f;
		float cy0 = 0.0f;
		float cz0 = 0.0f;
		float cx1 = 0.0f;
		float cy1 = 0.0f;
		float cz1 = 0.0f;

		// Centre and camera distance in elmos, matching ROAM's variance scale.
		float midX = 0.0f;
		float midY = 0.0f;
		float midZ = 0.0f;
		float lastCamDistance = 0.0f;
		float camDistLODFactor = 1.0f;
		float varianceMaxLimit = 1.0f;

		std::array<float, kVarianceSize> variance[2];
		TriNode baseLeft;
		TriNode baseRight;
		std::vector<uint32_t> indices;
	};

	void deleteGpuResources();
	void createSharedGrid();
	void createHeightTexture();
	void updateHeightTexture(Patch &patch);
	void updatePatchBounds(Patch &patch);

	void resetPatches();
	void linkPatchNeighbors();

	float heightElmos(int gx, int gz) const;
	float heightWorld(int gx, int gz) const;

	void computeVariance(Patch &patch, int tree);
	float recursComputeVariance(Patch &patch, int tree, int nodeIndex, IPoint left, IPoint right,
	                            IPoint apex, float hLeft, float hRight, float hApex);
	bool allocateNode(TriNode *&left, TriNode *&right);
	bool splitNode(Patch &patch, TriNode *node);
	void tessellate(Patch &patch, const float camPos[3], int viewRadius);
	void recursTessellate(Patch &patch, TriNode *node, IPoint left, IPoint right, IPoint apex,
	                      int tree, int nodeIndex);
	void generateIndices(Patch &patch);
	void recursRender(Patch &patch, TriNode *node, IPoint left, IPoint right, IPoint apex);
	void uploadIndices(Patch &patch);

	Shader shader;
	CSmfReadMap *map = nullptr;
	CBFGroundTextures *textures = nullptr;
	int bigX = 0;
	int bigY = 0;
	float minZ = 0.0f;
	float maxZ = 1.0f;

	GLuint sharedVao = 0;
	GLuint sharedVbo = 0;
	GLuint heightTexture = 0;
	int heightTextureW = 0;
	int heightTextureH = 0;

	std::vector<Patch> patches;
	std::vector<TriNode> nodePool;
	size_t nextNode = 0;
	std::vector<char> textureDirty;
	bool tessellationDirty = false;
	Mat4 lastView;
	bool haveLastView = false;
	int lastBias = 0;
};

#endif // TERRAIN_H
