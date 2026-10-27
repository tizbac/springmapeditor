
#include <GL/glew.h>
#include "BFGroundTextures.h"

#include "dxt1.h"
#include "mapfile.h"
#include "GlobalStuff.h"
#include "SmfReadMap.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

using std::string;

CBFGroundTextures::CBFGroundTextures(CSmfReadMap* rm)
{
	usePBO = false;
	if (GLEW_EXT_pixel_buffer_object && rm->usePBO) {
		glGenBuffers(10, pboIDs);
		currentPBO = 0;
		usePBO = true;
	}

	fakevfs* ifs = rm->mapf;
	map = rm;

	bigSquareSize = 128;
	numBigTexX = gs->mapx / bigSquareSize;
	numBigTexY = gs->mapy / bigSquareSize;

	SMFHeader* header = &map->header;
	ifs->Seek(header->tilesPtr);

	tileSize = header->tilesize;

	MapTileHeader tileHeader;
	READPTR_MAPTILEHEADER(tileHeader, rm->mapf);

	int count = (header->mapx * header->mapy) / 16;
	tileMap.assign(count, 0);

	int totalTiles = tileHeader.numTiles;
	if (totalTiles < 0)
		totalTiles = 0;
	tiles.assign((size_t)totalTiles * SMALL_TILE_SIZE, 0);
	scratch.assign(32 * 32 * 4, 0);

	int curTile = 0;

	for (int a = 0; a < tileHeader.numTileFiles; ++a) {
		printf("Loading tile file\n");

		long sizeOffset = ifs->GetPos();
		int size;
		ifs->Read(&size, 4);
		size = swabdword(size);
		string name;

		while (true) {
			char ch;
			ifs->Read(&ch, 1);
			/* char, no swab */
			if (ch == 0)
				break;

			name += ch;
		}

		// The map directory is normally passed with a trailing slash, but accept
		// both cases and make sure the separator is present.
		string dir = rm->mapf->cpath;
		if (!dir.empty() && dir[dir.size() - 1] != '/' && dir[dir.size() - 1] != '\\')
			dir += '/';
		string path = dir + name;

		fakevfs* tileFile = new fakevfs;
		tileFile->f = fopen(path.c_str(), "rb");
		tileFile->Seek(0);

		if (!tileFile->f) {
			printf("Couldnt find tile file %s\n", path.c_str());
			memset(&tiles[(size_t)curTile * SMALL_TILE_SIZE], 0xaa, (size_t)size * SMALL_TILE_SIZE);
			curTile += size;
			delete tileFile;
			tileFiles.push_back({path, size, sizeOffset});
			continue;
		}

		printf("Reading tiles\n");

		TileFileHeader tfh;
		READ_TILEFILEHEADER(tfh, tileFile);

		if (strcmp(tfh.magic, "spring tilefile") != 0 || tfh.version != 1 || tfh.tileSize != 32 || tfh.compressionType != 1) {
			printf("Error couldnt open tile file %s\n", path.c_str());
			printf("tfh.magic=%s tfh.version = %i tfh.tileSize = %i, tfh.compressionType = %i\n",
			       tfh.magic, tfh.version, tfh.tileSize, tfh.compressionType);
			exit(0);
		}

		for (int b = 0; b < size; ++b) {
			tileFile->Read(&tiles[(size_t)curTile * SMALL_TILE_SIZE], SMALL_TILE_SIZE);
			curTile++;
		}

		delete tileFile;
		tileFiles.push_back({path, size, sizeOffset});
	}

	printf("Reading tile map\n");

	tileMapOffset = ifs->GetPos();
	ifs->Read(tileMap.data(), count * sizeof(int));

	for (int i = 0; i < count; i++) {
		tileMap[i] = swabdword(tileMap[i]);
	}

	tileMapXSize = header->mapx / 4;
	tileMapYSize = header->mapy / 4;

	paint.assign(numBigTexX * numBigTexY, PaintLayer());
	paintDirty.assign(numBigTexX * numBigTexY, 0);

	squares = new GroundSquare[numBigTexX * numBigTexY];

	for (int y = 0; y < numBigTexY; ++y) {
		for (int x = 0; x < numBigTexX; ++x) {
			GroundSquare* square = &squares[y * numBigTexX + x];
			square->texLevel = 1;
			square->lastUsed = -100;
			LoadSquare(x, y, 0);
		}
	}
}

CBFGroundTextures::~CBFGroundTextures(void)
{
	for (int i = 0; i < numBigTexX * numBigTexY; ++i) {
		glDeleteTextures(1, &squares[i].texture);
		if (paint[i].texture)
			glDeleteTextures(1, &paint[i].texture);
	}

	delete[] squares;

	if (usePBO) {
		glDeleteBuffers(10, pboIDs);
	}
}

void CBFGroundTextures::SetTexture(int x, int y)
{
	GroundSquare* square = &squares[y * numBigTexX + x];
	glBindTexture(GL_TEXTURE_2D, square->texture);
	square->lastUsed = gs->frameNum;
}

inline bool CBFGroundTextures::TexSquareInView(int btx, int bty)
{
	return false;
}

void CBFGroundTextures::DrawUpdate(void)
{
}

// ---------------------------------------------------------------- painting

CBFGroundTextures::PaintLayer* CBFGroundTextures::paintLayer(int bx, int by)
{
	if (bx < 0 || by < 0 || bx >= numBigTexX || by >= numBigTexY)
		return NULL;

	PaintLayer& layer = paint[by * numBigTexX + bx];
	if (layer.data.empty()) {
		layer.data.assign((size_t)bigSquareSize * bigSquareSize * 4, 0);
	 glGenTextures(1, &layer.texture);
		glBindTexture(GL_TEXTURE_2D, layer.texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bigSquareSize, bigSquareSize, 0,
		            GL_RGBA, GL_UNSIGNED_BYTE, layer.data.data());
	}
	return &layer;
}

CBFGroundTextures::PaintLayer* CBFGroundTextures::existingPaintLayer(int bx, int by)
{
	if (bx < 0 || by < 0 || bx >= numBigTexX || by >= numBigTexY)
		return NULL;
	PaintLayer& layer = paint[by * numBigTexX + bx];
	return layer.data.empty() ? NULL : &layer;
}

GLuint CBFGroundTextures::paintTexture(int bx, int by)
{
	PaintLayer* layer = existingPaintLayer(bx, by);
	return layer ? layer->texture : 0;
}

void CBFGroundTextures::uploadPaintLayers()
{
	for (int i = 0; i < (int)paint.size(); ++i) {
		if (!paintDirty[i] || paint[i].data.empty())
			continue;
		glBindTexture(GL_TEXTURE_2D, paint[i].texture);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, bigSquareSize, bigSquareSize,
                        GL_RGBA, GL_UNSIGNED_BYTE, paint[i].data.data());
		paintDirty[i] = 0;
	}
}

void CBFGroundTextures::getPaintTexel(int index, unsigned char out[4])
{
	int square = bigSquareOfPaintIndex(index);
	int local = index - square * bigSquareSize * bigSquareSize;
	const PaintLayer& layer = paint[square];
	if (layer.data.empty() || local < 0 || local >= bigSquareSize * bigSquareSize) {
		out[0] = out[1] = out[2] = out[3] = 0;
		return;
	}
	const unsigned char* p = &layer.data[(size_t)local * 4];
	out[0] = p[0]; out[1] = p[1]; out[2] = p[2]; out[3] = p[3];
}

void CBFGroundTextures::setPaintTexel(int index, const unsigned char rgba[4])
{
	int square = bigSquareOfPaintIndex(index);
	int bx = square % numBigTexX;
	int by = square / numBigTexX;

	PaintLayer* layer = paintLayer(bx, by);
	if (!layer)
		return;

	int local = index - square * bigSquareSize * bigSquareSize;
	if (local < 0 || local >= bigSquareSize * bigSquareSize)
		return;

	unsigned char* p = &layer->data[(size_t)local * 4];
	p[0] = rgba[0];
	p[1] = rgba[1];
	p[2] = rgba[2];
	p[3] = rgba[3];
	paintDirty[square] = 1;
}

// ---------------------------------------------------------------- tiles

void CBFGroundTextures::decodeTile(int index, unsigned char* rgba32x32) const
{
	const unsigned char* data = &tiles[(size_t)index * SMALL_TILE_SIZE];
	dxt1_decode(data, 32, 32, rgba32x32);
}

int CBFGroundTextures::addTile(const unsigned char* rgba32x32)
{
	static std::unordered_map<unsigned long long, int> seen;

	unsigned long long hash = 14695981254224661567ull;
	for (int i = 0; i < 32 * 32 * 4; ++i) {
		hash ^= rgba32x32[i];
		hash *= 1099511628211ull;
	}

	auto it = seen.find(hash);
	if (it != seen.end())
		return it->second;

	unsigned char tile[SMALL_TILE_SIZE];
	dxt1_encode_tile(rgba32x32, tile);

	int index = numTiles();
	tiles.insert(tiles.end(), tile, tile + SMALL_TILE_SIZE);
	seen[hash] = index;
	return index;
}

static float bilinear(const std::vector<unsigned char>& data, int size, float u, float v, int channel)
{
	float fu = u - 0.5f;
	if (fu < 0) fu = 0;
	if (fu > size - 1) fu = (float)(size - 1);

	int i0 = (int)fu;
	int i1 = i0 + 1;
	if (i1 >= size) i1 = size - 1;
	float fx = fu - i0;

	float fv = v - 0.5f;
	if (fv < 0) fv = 0;
	if (fv > size - 1) fv = (float)(size - 1);

	int j0 = (int)fv;
	int j1 = j0 + 1;
	if (j1 >= size) j1 = size - 1;
	float fy = fv - j0;

	const int stride = size * 4;
	float a = data[(size_t)j0 * stride + i0 * 4 + channel];
	float b = data[(size_t)j0 * stride + i1 * 4 + channel];
	float c = data[(size_t)j1 * stride + i0 * 4 + channel];
	float d = data[(size_t)j1 * stride + i1 * 4 + channel];

	return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

bool CBFGroundTextures::bakePaint()
{
	int tilesPer = tilesPerBigSquare(); // 32 tiles per big square side
	bool changed = false;

	std::vector<unsigned char> base(32 * 32 * 4);
	std::vector<unsigned char> composited(32 * 32 * 4);

	for (int by = 0; by < numBigTexY; ++by) {
		for (int bx = 0; bx < numBigTexX; ++bx) {
			const PaintLayer& layer = paint[by * numBigTexX + bx];
			if (layer.data.empty())
				continue;

			for (int ty = 0; ty < tilesPer; ++ty) {
				for (int tx = 0; tx < tilesPer; ++tx) {
					int cell = (bx * tilesPer + tx) + (by * tilesPer + ty) * tileMapXSize;

					// Does any paint coverage touch this tile at all?
					bool any = false;
					for (int sy = 0; sy < 4 && !any; ++sy)
						for (int sx = 0; sx < 4; ++sx)
							if (layer.data[(size_t)(ty * 4 + sy) * bigSquareSize * 4 + (tx * 4 + sx) * 4 + 3] > 0) {
								any = true;
								break;
							}
					if (!any)
						continue;

					int original = tileMap[cell];
                    if (original < 0 || original >= numTiles())
                        continue;

					decodeTile(original, base.data());

                    for (int y = 0; y < 32; ++y) {
                        for (int x = 0; x < 32; ++x) {
                            // A tile is 32x32 ground texels; the paint layer has
                            // one texel per map square, i.e. per 8 ground texels.
                            float u = (float)(tx * 32 + x) / 8.0f;
                            float v = (float)(ty * 32 + y) / 8.0f;

                            float alpha = bilinear(layer.data, bigSquareSize, u, v, 3) / 255.0f;
                            unsigned char* dst = &composited[(size_t)(y * 32 + x) * 4];

                            if (alpha <= 0.0f) {
                                dst[0] = base[(size_t)(y * 32 + x) * 4 + 0];
                                dst[1] = base[(size_t)(y * 32 + x) * 4 + 1];
                                dst[2] = base[(size_t)(y * 32 + x) * 4 + 2];
                                dst[3] = 255;
                            } else {
                                float pr = bilinear(layer.data, bigSquareSize, u, v, 0);
                                float pg = bilinear(layer.data, bigSquareSize, u, v, 1);
                                float pb = bilinear(layer.data, bigSquareSize, u, v, 2);

                                dst[0] = (unsigned char)(base[(size_t)(y * 32 + x) * 4 + 0] * (1 - alpha) + pr * alpha);
                                dst[1] = (unsigned char)(base[(size_t)(y * 32 + x) * 4 + 1] * (1 - alpha) + pg * alpha);
                                dst[2] = (unsigned char)(base[(size_t)(y * 32 + x) * 4 + 2] * (1 - alpha) + pb * alpha);
                                dst[3] = 255;
                            }
                        }
                    }

                    int newIndex = addTile(composited.data());
                    if (newIndex != tileMap[cell]) {
                        tileMap[cell] = newIndex;
                        changed = true;
                    }
                }
            }
        }
	}

	return changed;
}

bool CBFGroundTextures::writeTileFiles(const std::string& smfPath)
{
	int originalTotal = 0;
	for (const TileFile& f : tileFiles)
		originalTotal += f.count;

	int extra = numTiles() - originalTotal;
	if (extra < 0)
		extra = 0;

	// Newly created tiles are appended to the last tile file.
	std::vector<int> newCount(tileFiles.size(), 0);
	for (size_t i = 0; i < tileFiles.size(); ++i)
		newCount[i] = tileFiles[i].count;
	if (!tileFiles.empty() && extra > 0)
		newCount.back() += extra;

	for (size_t i = 0; i < tileFiles.size(); ++i) {
		int start = 0;
		for (size_t k = 0; k < i; ++k)
			start += newCount[k];
		int count = newCount[i];

		FILE* f = fopen(tileFiles[i].path.c_str(), "wb");
		if (!f) {
			printf("Cannot write tile file %s\n", tileFiles[i].path.c_str());
			return false;
		}

		char magic[16];
		memset(magic, 0, sizeof(magic));
		strcpy(magic, "spring tilefile");
		int version = 1;
		int tileSz = 32;
		int compressionType = 1;

		fwrite(magic, 1, 16, f);
		fwrite(&version, 4, 1, f);
		fwrite(&count, 4, 1, f);
		fwrite(&tileSz, 4, 1, f);
		fwrite(&compressionType, 4, 1, f);

		if (count > 0)
			fwrite(&tiles[(size_t)start * SMALL_TILE_SIZE], SMALL_TILE_SIZE, count, f);
		fclose(f);
	}

	FILE* smf = fopen(smfPath.c_str(), "rb+");
	if (!smf) {
		printf("Cannot update tile map in %s\n", smfPath.c_str());
		return false;
	}

	// MapTileHeader: numTileFiles, numTiles.
	int total = numTiles();
	fseek(smf, map->header.tilesPtr + 4, SEEK_SET);
	fwrite(&total, 4, 1, smf);

	// The tile count stored for every tile file in the .smf.
	for (size_t i = 0; i < tileFiles.size(); ++i) {
		int le = swabdword(newCount[i]);
		fseek(smf, tileFiles[i].sizeOffset, SEEK_SET);
		fwrite(&le, 4, 1, smf);
	}

	// The tile indices.
	fseek(smf, (long)tileMapOffset, SEEK_SET);
	std::vector<int> le(tileMap.size());
	for (size_t i = 0; i < tileMap.size(); ++i)
		le[i] = swabdword(tileMap[i]);
	fwrite(le.data(), sizeof(int), tileMap.size(), smf);

	fclose(smf);
	return true;
}

int tileoffset[] = {0, 512, 640, 672};

void CBFGroundTextures::LoadSquare(int x, int y, int level)
{
	int size = 1024 >> level;

	GLubyte* buf = NULL; bool usedPBO = false;
	if (usePBO) {
		if (currentPBO > 9) currentPBO = 0;
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pboIDs[currentPBO++]);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, size * size / 2, 0, GL_STREAM_DRAW);

		//map the buffer object into client's memory
		buf = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		usedPBO = true;
	}
	if (buf == NULL) {
		buf = new GLubyte[size * size / 2];
		usedPBO = false;
	}

	GroundSquare* square = &squares[y * numBigTexX + x];
	square->texLevel = level;

	int numblocks = 8 / (1 << level);

	for (int y1 = 0; y1 < 32; y1++) {
		for (int x1 = 0; x1 < 32; x1++) {
			const unsigned char* tile = &tiles[(size_t)tileMap[(x1 + x * 32) + (y1 + y * 32) * tileMapXSize] * SMALL_TILE_SIZE + tileoffset[level]];
			for (int yt = 0; yt < numblocks; yt++) {
				for (int xt = 0; xt < numblocks; xt++) {
					GLint* sbuf = (GLint*)&tile[(xt + yt * numblocks) * 8];
					GLint* dbuf = (GLint*)&buf[(x1 * numblocks + xt + (y1 * numblocks + yt) * (numblocks * 32)) * 8];
					dbuf[0] = sbuf[0];
					dbuf[1] = sbuf[1];
				}
			}
		}
	}

	glGenTextures(1, &square->texture);
	glBindTexture(GL_TEXTURE_2D, square->texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	if (GLEW_EXT_texture_edge_clamp) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	if (map->anisotropy != 0.0f)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, map->anisotropy);

	if (usedPBO) {
		glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, size, size, 0, size * size / 2, 0);
		glBufferData(GL_PIXEL_UNPACK_BUFFER, 0, 0, GL_STREAM_DRAW); //free it
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	} else {
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, size, size, 0, size * size / 2, buf);
		delete[] buf;
	}
}
