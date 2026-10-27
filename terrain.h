/*
 * terrain.h - the ground mesh as a vertex buffer, drawn with the ground shader.
 *
 * The map is made of "big squares" of 128x128 map squares, each of which has
 * its own ground texture and paint layer, so the mesh is stored as one range of
 * triangles per big square and only the squares whose heights changed are
 * regenerated.
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

	// detail is the exponent the old renderer used: a big square is subdivided
	// into 1024 / 2^detail quads per side, so a bigger exponent is a coarser mesh.
	void build(CSmfReadMap *map, CBFGroundTextures *textures, int detail);

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

  private:
	struct BigSquare {
		int first;           // index of the first vertex in `vertices`
		int count;           // number of vertices
		float cx0, cy0, cz0; // bounding box, world space
		float cx1, cy1, cz1;
	};

	void buildSquare(int bx, int by, std::vector<float> &out);

	Shader shader;
	CSmfReadMap *map = nullptr;
	int detail = 6;
	int bigX = 0, bigY = 0;
	int quadsPerSquare = 0;

	std::vector<float> vertices;
	std::vector<BigSquare> squares;
	std::vector<char> dirty;
	GLuint vao = 0;
	GLuint vbo = 0;
};

#endif // TERRAIN_H
