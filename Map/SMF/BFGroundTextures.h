// BFGroundTextures.h
///////////////////////////////////////////////////////////////////////////

#ifndef __BF_GROUND_TEXTURES_H__
#define __BF_GROUND_TEXTURES_H__

#include <GL/glew.h>

#include <string>
#include <vector>

#include "mapfile.h"

class CSmfReadMap;

class CBFGroundTextures {
  public:
	struct GroundSquare {
		int texLevel;
		GLuint texture;
		int lastUsed;
	};

	// One paint layer per big texture square. It holds one texel per map
	// square: RGB is the painted colour and A is how much of the ground texture
	// it replaces. The layer is uploaded as a second texture unit and blended
	// with the ground tile texture in the shader (multitexturing).
	struct PaintLayer {
		GLuint texture = 0;
		std::vector<unsigned char> data; // bigSquareSize * bigSquareSize * 4
	};

	CBFGroundTextures(CSmfReadMap *srm);
	~CBFGroundTextures(void);

	void SetTexture(int x, int y);
	void DrawUpdate(void);
	void LoadSquare(int x, int y, int level);

	GroundSquare *squares;

	int bigSquareSize;
	int numBigTexX;
	int numBigTexY;
	std::vector<int> tileMap;
	std::vector<unsigned char> tiles; // numTiles * SMALL_TILE_SIZE DXT1 records

	int tilesPerBigSquare() const
	{
		return bigSquareSize / 4;
	}
	int numBigSquaresX() const
	{
		return numBigTexX;
	}
	int numBigSquaresY() const
	{
		return numBigTexY;
	}

	// --- painting ---

	PaintLayer *paintLayer(int bx, int by);
	PaintLayer *existingPaintLayer(int bx, int by);
	GLuint paintTexture(int bx, int by);

	// Decodes the whole ground texture to RGB (mapx*8 by mapy*8 texels) with the
	// paint layers blended in. Used when the map is exported.
	bool buildColormap(std::vector<unsigned char> &rgb);

	// Replaces every ground texture with the given RGB image (any size, it is
	// rescaled to mapx*8 by mapy*8). Export and the colormap use it instead of
	// the tiles from the .smt.
	bool setColormap(const unsigned char *rgb, int width, int height);
	void uploadPaintLayers();

	int paintTexelIndex(int bx, int by, int px, int py) const
	{
		return (by * numBigTexX + bx) * bigSquareSize * bigSquareSize + py * bigSquareSize + px;
	}
	int bigSquareOfPaintIndex(int index) const
	{
		return index / (bigSquareSize * bigSquareSize);
	}

	// Read/write a paint texel by its global index, creating the layer when
	// needed. Used by the undo/redo machinery.
	void getPaintTexel(int index, unsigned char out[4]);
	void setPaintTexel(int index, const unsigned char rgba[4]);

	// --- tiles ---

	int numTiles() const
	{
		return (int)tiles.size() / SMALL_TILE_SIZE;
	}

	void decodeTile(int index, unsigned char *rgba32x32) const;
	int addTile(const unsigned char *rgba32x32);

	// Fold the paint layers into the tile map, appending the newly created
	// tiles to the tile storage. Returns true if anything changed.
	// Called after every big square is baked, so the editor can show progress.
	typedef void (*BakeProgressFn)(int done, int total, void *user);
	bool bakePaint(BakeProgressFn progress = nullptr, void *user = nullptr);
	// Write the tile storage back to the .smt file(s) and update the tile map
	// inside the .smf.
	bool writeTileFiles(const std::string &smfPath);

  protected:
	CSmfReadMap *map;
	int tileSize;
	int tileMapXSize;
	int tileMapYSize;

	std::vector<PaintLayer> paint; // numBigTexX * numBigTexY, empty until painted
	std::vector<unsigned char> paintDirty;
	std::vector<unsigned char> importedColormap; // RGB, empty until a texture is imported

	struct TileFile {
		std::string path;
		int count;
		long sizeOffset; // offset of this file's tile count inside the .smf
	};
	std::vector<TileFile> tileFiles;
	long tileMapOffset = 0;

	std::vector<unsigned char> scratch; // 32*32*4 scratch buffer

	//! use Pixel Buffer Objects for async. uploading (DMA)?
	bool usePBO;
	GLuint pboIDs[10];
	int currentPBO;

	float anisotropy;
};

#endif // __BF_GROUND_TEXTURES_H__
