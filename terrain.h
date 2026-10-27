/*
 * terrain.h - the ground mesh, one vertex buffer per big square.
 *
 * The map is made of "big squares" of 128x128 map squares, each of which has
 * its own ground texture and paint layer. Giving every square its own buffer
 * means a height edit or a level of detail change only re-uploads the squares
 * that are actually affected. A square's level of detail follows how large it
 * is on screen, which is derived from its distance along the view direction so
 * that rotating the camera does not change the mesh.
 */
#ifndef TERRAIN_H
#define TERRAIN_H

#include <GL/glew.h>

#include <vector>

#include "mat4.h"
#include "shader.h"

class CSmfReadMap;
class CBFGroundTextures;

class Terrain {
  public:
	bool init();

	// Builds the mesh for the whole map at a middle level of detail.
	void build(CSmfReadMap *map, CBFGroundTextures *textures);

	// The world height range the normalised height map is mapped onto. Must be
	// set before build() and again whenever the range changes.
	void setHeightRange(float minZ, float maxZ);

	// Refines the level of detail of the squares from their apparent size.
	// Levels are clamped to [3, 10] (level 3 is one quad per map square) and
	// shifted by `bias`, where a negative bias is finer. A few squares are
	// rebuilt per call, so this can be called every frame. Returns true when
	// anything was rebuilt.
	bool updateLods(const Mat4 &view, int viewportHeight, float fovDegrees, int bias);

	void markDirty(int bx, int by);
	// Regenerates the dirty big squares and uploads them to the GPU.
	void flushDirty();

	void draw(CBFGroundTextures *textures, GLuint metalTexture, const Mat4 &vp, int mode,
	          int metalWidth, int metalHeight, float lightX, float lightY, float lightZ,
	          const float frustum[6][4]);

	bool built() const
	{
		return !squares.empty();
	}

	int lodOf(int bx, int by) const;

  private:
	struct BigSquare {
		GLuint vao = 0;
		GLuint vbo = 0;
		int count = 0;
		int lod = 6;         // 1024 / 2^lod quads per side
		float cx0, cy0, cz0; // bounding box, world space
		float cx1, cy1, cz1;
		float screenSize = 0.0f; // approximate width on screen, in pixels
	};

	// Height of a map square in world units.
	float worldHeight(int x, int y) const;

	void buildSquare(int bx, int by, int lod, const int neighbourLod[4], std::vector<float> &out);
	void uploadSquare(int index, int lod);

	Shader shader;
	CSmfReadMap *map = nullptr;
	CBFGroundTextures *textures = nullptr;
	int bigX = 0, bigY = 0;
	float minZ = 0.0f, maxZ = 1.0f;

	std::vector<BigSquare> squares;
	std::vector<char> dirty;
};

#endif // TERRAIN_H
