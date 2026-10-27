/*
 * terrain.h - the ground mesh as a vertex buffer, drawn with the ground shader.
 *
 * The map is made of "big squares" of 128x128 map squares, each of which has
 * its own ground texture and paint layer, so the mesh is stored as one range of
 * triangles per big square and only the squares whose heights changed are
 * regenerated. Every big square picks its own level of detail from how large it
 * is on screen, see Terrain::updateLods().
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

	// (Re)builds the whole mesh. `lod` holds one level of detail per big square,
	// row by row. The level is the exponent the old renderer used: a big square
	// is subdivided into 1024 / 2^lod quads per side, so a bigger exponent is a
	// coarser mesh.
	void build(CSmfReadMap *map, CBFGroundTextures *textures, const std::vector<int> &lod);

	// Fills `lod` with the level of detail each square should use, derived from
	// how large the square is on screen. Levels are clamped to [4, 10] and offset
	// by `bias`, so a lower bias gives a finer mesh. Returns true when any level
	// differs from the mesh that is currently built.
	bool updateLods(const Mat4 &vp, int viewportWidth, int viewportHeight, int bias,
	                std::vector<int> &lod);

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

	// Level of detail of the square, for the HUD/debug output.
	int lodOf(int bx, int by) const;

  private:
	struct BigSquare {
		int first;           // index of the first vertex in `vertices`
		int count;           // number of vertices
		int lod;             // 1024 / 2^lod quads per side
		float cx0, cy0, cz0; // bounding box, world space
		float cx1, cy1, cz1;
	};

	void buildSquare(int bx, int by, int lod, std::vector<float> &out);

	Shader shader;
	CSmfReadMap *map = nullptr;
	int bigX = 0, bigY = 0;

	std::vector<float> vertices;
	std::vector<BigSquare> squares;
	std::vector<char> dirty;
	GLuint vao = 0;
	GLuint vbo = 0;
};

#endif // TERRAIN_H
