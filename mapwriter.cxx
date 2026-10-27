/*
 * mapwriter.cxx - writes a complete .smf + .smt pair from the editor's data.
 */
#include "mapwriter.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>

#include "dxt1.h"

namespace {

const int kSmallTileSize = 680;  // 32x32 DXT1 + 16x16 + 8x8 + 4x4
const int kMinimapSize = 699048; // 1024x1024 DXT1 + 8 mip levels
const int kTileTexels = 32;      // texels per tile side
const int kTexelsPerSquare = 8;  // texels per map square

struct SMFHeaderOut {
	char magic[16];
	int version;
	int mapid;
	int mapx;
	int mapy;
	int squareSize;
	int texelPerSquare;
	int tilesize;
	float minHeight;
	float maxHeight;
	int heightmapPtr;
	int typeMapPtr;
	int tilesPtr;
	int minimapPtr;
	int metalmapPtr;
	int featurePtr;
	int numExtraHeaders;
};

static const char kTileMagic[16] = "spring tilefile";

struct TileFileHeaderOut {
	char magic[16];
	int version;
	int numTiles;
	int tileSize;
	int compressionType;
};

static void putBytes(FILE *f, const void *data, size_t size)
{
	if (size)
		fwrite(data, 1, size, f);
}

static void fillSolidTile(std::vector<unsigned char> &rgba, int w, int h, unsigned char r,
                          unsigned char g, unsigned char b)
{
	rgba.resize((size_t)w * h * 4);
	for (int i = 0; i < w * h; ++i)
	{
		rgba[(size_t)i * 4 + 0] = r;
		rgba[(size_t)i * 4 + 1] = g;
		rgba[(size_t)i * 4 + 2] = b;
		rgba[(size_t)i * 4 + 3] = 255;
	}
}

} // namespace

bool writeMapFiles(const std::string &smfPath, int mapx, int mapy,
                   const std::vector<unsigned short> &heights,
                   const std::vector<unsigned char> &metal,
                   const std::vector<unsigned char> &typemap,
                   const std::vector<unsigned char> &colormap, float minZ, float maxZ,
                   const MapWriteOptions &options, const std::function<void(int, int)> &progress)
{
	if (mapx <= 0 || mapy <= 0 || (mapx % 128) != 0 || (mapy % 128) != 0)
		return false;

	const int tilesX = mapx / 4;
	const int tilesY = mapy / 4;
	const int cmW = mapx * kTexelsPerSquare;
	const int cmH = mapy * kTexelsPerSquare;

	// ---------------------------------------------------------------- tiles
	std::vector<unsigned char> tileData;
	tileData.reserve((size_t)tilesX * tilesY * kSmallTileSize / 4);
	std::vector<int> tileMap((size_t)tilesX * tilesY, 0);
	std::unordered_map<unsigned long long, int> seen;

	std::vector<unsigned char> rgba((size_t)kTileTexels * kTileTexels * 4);
	std::vector<unsigned char> flat;
	fillSolidTile(flat, kTileTexels, kTileTexels, 96, 124, 70);

	const int totalTiles = tilesX * tilesY;
	if (colormap.empty())
	{
		// A solid map only needs one tile; the whole tile map keeps index 0.
		unsigned char tile[kSmallTileSize];
		dxt1_encode_tile(flat.data(), tile);
		tileData.insert(tileData.end(), tile, tile + kSmallTileSize);
	} else
		for (int ty = 0; ty < tilesY; ++ty)
		{
			for (int tx = 0; tx < tilesX; ++tx)
			{
				if (!colormap.empty())
				{
					for (int y = 0; y < kTileTexels; ++y)
					{
						const int sy = ty * kTileTexels + y;
						for (int x = 0; x < kTileTexels; ++x)
						{
							const int sx = tx * kTileTexels + x;
							const unsigned char *src = &colormap[((size_t)sy * cmW + sx) * 3];
							unsigned char *dst = &rgba[((size_t)y * kTileTexels + x) * 4];
							dst[0] = src[0];
							dst[1] = src[1];
							dst[2] = src[2];
							dst[3] = 255;
						}
					}
				} else
				{
					rgba = flat;
				}

				unsigned char tile[kSmallTileSize];
				dxt1_encode_tile(rgba.data(), tile);

				// FNV-1a over the encoded tile, then compare the bytes to rule out
				// collisions.
				unsigned long long hash = 1469598103934665603ULL;
				for (int i = 0; i < kSmallTileSize; ++i)
				{
					hash ^= tile[i];
					hash *= 1099511628211ULL;
				}

				int index = -1;
				auto it = seen.find(hash);
				if (it != seen.end())
				{
					const unsigned char *other = &tileData[(size_t)it->second * kSmallTileSize];
					if (memcmp(other, tile, kSmallTileSize) == 0)
						index = it->second;
				}
				if (index < 0)
				{
					index = (int)(tileData.size() / kSmallTileSize);
					tileData.insert(tileData.end(), tile, tile + kSmallTileSize);
					seen[hash] = index;
				}

				tileMap[(size_t)ty * tilesX + tx] = index;
			}

			if (progress && (ty % 8) == 0)
				progress(ty * tilesX, totalTiles);
		}
	if (progress)
		progress(totalTiles, totalTiles);

	// One tile is stored once no matter how many squares use it, like mapconvng
	// does: only distinct tiles are appended to the .smt.
	printf("SMT: %d squares, %d unique tiles (%.1f%% reused)\n", totalTiles,
	       (int)(tileData.size() / kSmallTileSize),
	       totalTiles > 0
	           ? 100.0f * (1.0f - (float)(tileData.size() / kSmallTileSize) / (float)totalTiles)
	           : 0.0f);

	// ---------------------------------------------------------------- minimap
	std::vector<unsigned char> minimap;
	if (options.writeMinimap)
	{
		const int mm = 1024;
		std::vector<unsigned char> mmRgba((size_t)mm * mm * 4, 255);
		if (!colormap.empty())
		{
			for (int y = 0; y < mm; ++y)
			{
				const int sy = (int)((long long)y * cmH / mm);
				for (int x = 0; x < mm; ++x)
				{
					const int sx = (int)((long long)x * cmW / mm);
					const unsigned char *src = &colormap[((size_t)sy * cmW + sx) * 3];
					unsigned char *dst = &mmRgba[((size_t)y * mm + x) * 4];
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = 255;
				}
			}
		} else
		{
			for (int i = 0; i < mm * mm; ++i)
			{
				mmRgba[(size_t)i * 4 + 0] = 96;
				mmRgba[(size_t)i * 4 + 1] = 124;
				mmRgba[(size_t)i * 4 + 2] = 70;
			}
		}

		minimap.resize(kMinimapSize);
		size_t offset = 0;
		int size = mm;
		std::vector<unsigned char> level = mmRgba;
		while (size >= 4 && offset + (size_t)size * size / 2 <= minimap.size())
		{
			dxt1_encode(level.data(), size, size, &minimap[offset]);
			offset += (size_t)size * size / 2;
			if (size <= 4)
				break;
			std::vector<unsigned char> next((size_t)(size / 2) * (size / 2) * 4);
			dxt1_downsample(level.data(), size, size, next.data());
			level.swap(next);
			size /= 2;
		}
		minimap.resize(offset);
	}

	// ---------------------------------------------------------------- write the .smt
	std::string smtPath = smfPath;
	size_t dot = smtPath.find_last_of('.');
	if (dot != std::string::npos)
		smtPath = smtPath.substr(0, dot);
	smtPath += ".smt";

	FILE *smt = fopen(smtPath.c_str(), "wb");
	if (!smt)
		return false;

	TileFileHeaderOut tfh;
	memset(&tfh, 0, sizeof(tfh));
	memcpy(tfh.magic, kTileMagic, sizeof(tfh.magic));
	tfh.version = 1;
	tfh.numTiles = (int)(tileData.size() / kSmallTileSize);
	tfh.tileSize = 32;
	tfh.compressionType = 1; // DXT1
	putBytes(smt, &tfh, sizeof(tfh));
	putBytes(smt, tileData.data(), tileData.size());
	fclose(smt);

	// ---------------------------------------------------------------- write the .smf
	std::string tileName = options.tileFileName;
	if (tileName.empty())
	{
		tileName = smtPath.substr(smtPath.find_last_of('/') + 1);
	}

	const int heightW = mapx + 1;
	const int heightH = mapy + 1;
	const int mapSize = mapx / 2 * (mapy / 2);

	SMFHeaderOut h;
	memset(&h, 0, sizeof(h));
	memcpy(h.magic, "spring map file", sizeof(h.magic) - 1);
	h.version = 1;
	h.mapid = 1234567;
	h.mapx = mapx;
	h.mapy = mapy;
	h.squareSize = 8;
	h.texelPerSquare = kTexelsPerSquare;
	h.tilesize = kTileTexels;
	h.minHeight = minZ;
	h.maxHeight = maxZ;
	h.numExtraHeaders = 0;

	const long headerSize = (long)sizeof(SMFHeaderOut);
	const long heightSize = (long)heightW * heightH * 2;
	const long typeSize = mapSize;
	const long metalSize = mapSize;
	const long tileHeaderSize = 8 + 4 + (long)tileName.size() + 1;
	const long tileMapSize = (long)tilesX * tilesY * 4;
	const long featureSize = 8;

	long offset = headerSize;
	h.heightmapPtr = (int)offset;
	offset += heightSize;
	h.typeMapPtr = (int)offset;
	offset += typeSize;
	h.metalmapPtr = (int)offset;
	offset += metalSize;
	h.tilesPtr = (int)offset;
	offset += tileHeaderSize + tileMapSize;
	h.minimapPtr = (int)offset;
	offset += (long)minimap.size();
	h.featurePtr = (int)offset;
	offset += featureSize;

	FILE *smf = fopen(smfPath.c_str(), "wb");
	if (!smf)
		return false;

	putBytes(smf, &h, sizeof(h));

	// Height map.
	{
		std::vector<unsigned short> data((size_t)heightW * heightH, 0);
		if (heights.size() >= data.size())
			memcpy(data.data(), heights.data(), data.size() * 2);
		putBytes(smf, data.data(), data.size() * 2);
	}

	// Type map.
	{
		std::vector<unsigned char> data((size_t)mapSize, 0);
		if (typemap.size() >= data.size())
			memcpy(data.data(), typemap.data(), data.size());
		putBytes(smf, data.data(), data.size());
	}

	// Metal map.
	{
		std::vector<unsigned char> data((size_t)mapSize, 0);
		if (metal.size() >= data.size())
			memcpy(data.data(), metal.data(), data.size());
		putBytes(smf, data.data(), data.size());
	}

	// Tile chunk: header, one file entry, then the tile index array.
	{
		int numFiles = 1;
		int numTiles = (int)(tileData.size() / kSmallTileSize);
		putBytes(smf, &numFiles, 4);
		putBytes(smf, &numTiles, 4);
		int count = numTiles;
		putBytes(smf, &count, 4);
		putBytes(smf, tileName.c_str(), tileName.size() + 1);
		putBytes(smf, tileMap.data(), tileMap.size() * sizeof(int));
	}

	// Minimap.
	if (!minimap.empty())
		putBytes(smf, minimap.data(), minimap.size());

	// Features (none yet).
	{
		int numFeatureType = 0;
		int numFeatures = 0;
		putBytes(smf, &numFeatureType, 4);
		putBytes(smf, &numFeatures, 4);
	}

	fclose(smf);
	return true;
}
