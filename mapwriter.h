/*
 * mapwriter.h - writes a complete .smf + .smt pair from the editor's data.
 *
 * The layout follows the SMF/SMT format used by the Spring / Recoil engine:
 * an SMFHeader, the height map, the type map, the metal map, the tile index
 * chunk, the minimap and an empty feature chunk. Tiles are re-encoded as DXT1
 * (with the four mip levels the format stores) and deduplicated.
 */
#ifndef MAPWRITER_H
#define MAPWRITER_H

#include <functional>
#include <string>
#include <vector>

struct MapWriteOptions {
	bool writeMinimap = true;
	std::string tileFileName; // name written into the map's tile list
};

// heights:   (mapx + 1) * (mapy + 1) values, 0..65535 (0 = minZ, 65535 = maxZ)
// metal:     (mapx / 2) * (mapy / 2) bytes
// typemap:   (mapx / 2) * (mapy / 2) bytes, may be empty
// colormap:  (mapx * 8) * (mapy * 8) RGB bytes, may be empty (solid colour)
// progress:  called with (done, total) while the tiles are compressed
bool writeMapFiles(const std::string &smfPath, int mapx, int mapy,
                   const std::vector<unsigned short> &heights,
                   const std::vector<unsigned char> &metal,
                   const std::vector<unsigned char> &typemap,
                   const std::vector<unsigned char> &colormap, float minZ, float maxZ,
                   const MapWriteOptions &options,
                   const std::function<void(int, int)> &progress = nullptr);

#endif // MAPWRITER_H
