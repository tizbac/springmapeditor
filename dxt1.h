/*
 * Minimal DXT1 (S3TC) codec.
 *
 * Spring tiles and the minimap are stored as DXT1 blocks. A tile is a 32x32
 * image followed by its 16x16, 8x8 and 4x4 mipmaps, which adds up to exactly
 * SMALL_TILE_SIZE (512 + 128 + 32 + 8 = 680) bytes.
 *
 * Rows are stored in the same order OpenGL expects for glCompressedTexImage2D
 * (the first block row is the v = 0 row), so tiles can be decoded and
 * re-encoded without any flipping.
 */
#ifndef DXT1_H
#define DXT1_H

#include <vector>

// Decompress w*h/2 bytes of DXT1 data into w*h RGBA pixels.
void dxt1_decode(const unsigned char *data, int w, int h, unsigned char *rgba);

// Compress w*h RGBA pixels into w*h/2 bytes of DXT1 data.
void dxt1_encode(const unsigned char *rgba, int w, int h, unsigned char *out);

// Average a w*h RGBA image into (w/2)*(h/2) RGBA pixels.
void dxt1_downsample(const unsigned char *rgba, int w, int h, unsigned char *out);

// Encode a 32x32 RGBA tile, including the four DXT1 mipmaps, into 680 bytes.
void dxt1_encode_tile(const unsigned char *rgba32x32, unsigned char *tile680);

#endif // DXT1_H
