#include "dxt1.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

	inline void unpack565(unsigned short c, int& r, int& g, int& b)
	{
		r = (c >> 11) & 0x1f;
		g = (c >> 5) & 0x1f;
		b = c & 0x1f;
		// Expand five bits to eight.
		r = (r << 3) | (r >> 2);
		g = (g << 3) | (g >> 2);
		b = (b << 3) | (b >> 2);
		}

	inline unsigned short pack565(int r, int g, int b)
	{
		r = std::max(0, std::min(255, r));
		g = std::max(0, std::min(255, g));
		b = std::max(0, std::min(255, b));
		return (unsigned short)(((r >> 3) << 11) | ((g >> 3) << 5) | (b >> 3));
		}

	// Colour table of a DXT1 block: the four candidate colours of a block.
	void block_colors(const unsigned char* block, int color[4][3])
	{
		unsigned short c0 = (unsigned short)(block[0] | (block[1] << 8));
		unsigned short c1 = (unsigned short)(block[2] | (block[3] << 8));

		int r0, g0, b0, r1, g1, b1;
		unpack565(c0, r0, g0, b0);
		unpack565(c1, r1, g1, b1);

		color[0][0] = r0; color[0][1] = g0; color[0][2] = b0;
		color[1][0] = r1; color[1][1] = g1; color[1][2] = b1;

		if (c0 > c1)
		{
			color[2][0] = (2 * r0 + r1) / 3; color[2][1] = (2 * g0 + g1) / 3; color[2][2] = (2 * b0 + b1) / 3;
			color[3][0] = (r0 + 2 * r1) / 3; color[3][1] = (g0 + 2 * g1) / 3; color[3][2] = (b0 + 2 * b1) / 3;
			}
		else
		{
			color[2][0] = (r0 + r1) / 2; color[2][1] = (g0 + g1) / 2; color[2][2] = (b0 + b1) / 2;
			color[3][0] = 0; color[3][1] = 0; color[3][2] = 0;
			}
		}

	// Pick the two endpoints of a block as the extreme colours along its principal
	// axis (a cheap PCA), which is what decent DXT1 encoders do.
	void block_endpoints(const unsigned char* rgba, int w, int bx, int by, int out[2][3])
	{
		int px[16][3];
		long long sr = 0, sg = 0, sb = 0;

		for (int j = 0; j < 4; ++j) {
			for (int i = 0; i < 4; ++i) {
				const unsigned char* p = rgba + ((size_t)(by * 4 + j) * w + (bx * 4 + i)) * 4;
				int k = j * 4 + i;
				px[k][0] = p[0]; px[k][1] = p[1]; px[k][2] = p[2];
				sr += p[0]; sg += p[1]; sb += p[2];
				}
			}

		int mr = (int)(sr / 16), mg = (int)(sg / 16), mb = (int)(sb / 16);

		double cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
		for (int k = 0; k < 16; ++k) {
			int dr = px[k][0] - mr, dg = px[k][1] - mg, db = px[k][2] - mb;
			cov[0][0] += dr * (double)dr; cov[0][1] += dr * (double)dg; cov[0][2] += dr * (double)db;
			cov[1][0] += dg * (double)dr; cov[1][1] += dg * (double)dg; cov[1][2] += dg * (double)db;
			cov[2][0] += db * (double)dr; cov[2][1] += db * (double)dg; cov[2][2] += db * (double)db;
			}

		double v[3] = {cov[0][0], cov[1][0], cov[2][0]};
		if (v[0] == 0 && v[1] == 0 && v[2] == 0)
		v[0] = 1;

		for (int it = 0; it < 24; ++it) {
			double nv[3] = {
				cov[0][0] * v[0] + cov[0][1] * v[1] + cov[0][2] * v[2],
				cov[1][0] * v[0] + cov[1][1] * v[1] + cov[1][2] * v[2],
				cov[2][0] * v[0] + cov[2][1] * v[1] + cov[2][2] * v[2]};
			double len = sqrt(nv[0] * nv[0] + nv[1] * nv[1] + nv[2] * nv[2]);
			if (len < 1e-9)
			break;
			v[0] = nv[0] / len; v[1] = nv[1] / len; v[2] = nv[2] / len;
			}

		int bestMin = 0, bestMax = 0;
		double minProj = 1e18, maxProj = -1e18;
		for (int k = 0; k < 16; ++k) {
			double proj = px[k][0] * v[0] + px[k][1] * v[1] + px[k][2] * v[2];
			if (proj < minProj) { minProj = proj; bestMin = k; }
			if (proj > maxProj) { maxProj = proj; bestMax = k; }
			}

		out[0][0] = px[bestMax][0]; out[0][1] = px[bestMax][1]; out[0][2] = px[bestMax][2];
		out[1][0] = px[bestMin][0]; out[1][1] = px[bestMin][1]; out[1][2] = px[bestMin][2];

		// A single coloured block would fall into the three colour mode, where the
		// fourth entry is transparent, so nudge the second endpoint instead.
		unsigned short c0 = pack565(out[0][0], out[0][1], out[0][2]);
		unsigned short c1 = pack565(out[1][0], out[1][1], out[1][2]);
		if (c0 == c1) {
			if (out[1][2] > 0) --out[1][2];
			else if (out[1][1] > 0) --out[1][1];
			else if (out[1][0] > 0) --out[1][0];
			else ++out[0][0];
			c0 = pack565(out[0][0], out[0][1], out[0][2]);
			c1 = pack565(out[1][0], out[1][1], out[1][2]);
			}
		if (c0 < c1) {
			std::swap(out[0][0], out[1][0]);
			std::swap(out[0][1], out[1][1]);
			std::swap(out[0][2], out[1][2]);
			}
		}

	} // namespace

void dxt1_decode(const unsigned char* data, int w, int h, unsigned char* rgba)
{
	for (int by = 0; by < h / 4; ++by) {
		for (int bx = 0; bx < w / 4; ++bx) {
			const unsigned char* block = data + (size_t)(by * (w / 4) + bx) * 8;
			int color[4][3];
			block_colors(block, color);

			for (int j = 0; j < 4; ++j) {
				for (int i = 0; i < 4; ++i) {
					int index = (block[4 + j] >> (i * 2)) & 3;
					unsigned char* out = rgba + ((size_t)(by * 4 + j) * w + (bx * 4 + i)) * 4;
					out[0] = (unsigned char)color[index][0];
					out[1] = (unsigned char)color[index][1];
					out[2] = (unsigned char)color[index][2];
					out[3] = 255;
					}
				}
			}
		}
	}

void dxt1_encode(const unsigned char* rgba, int w, int h, unsigned char* out)
{
	for (int by = 0; by < h / 4; ++by) {
		for (int bx = 0; bx < w / 4; ++bx) {
			int end[2][3];
			block_endpoints(rgba, w, bx, by, end);

			unsigned short c0 = pack565(end[0][0], end[0][1], end[0][2]);
			unsigned short c1 = pack565(end[1][0], end[1][1], end[1][2]);

			unsigned char block[8];
			block[0] = (unsigned char)(c0 & 0xff);
			block[1] = (unsigned char)(c0 >> 8);
			block[2] = (unsigned char)(c1 & 0xff);
			block[3] = (unsigned char)(c1 >> 8);

			int color[4][3];
			block_colors(block, color);

			for (int j = 0; j < 4; ++j) {
				unsigned char bits = 0;
				for (int i = 0; i < 4; ++i) {
					const unsigned char* p = rgba + ((size_t)(by * 4 + j) * w + (bx * 4 + i)) * 4;
					int best = 0, bestd = 1 << 30;
					for (int k = 0; k < 4; ++k) {
						int dr = p[0] - color[k][0];
						int dg = p[1] - color[k][1];
						int db = p[2] - color[k][2];
						int d = dr * dr + dg * dg + db * db;
						if (d < bestd) { bestd = d; best = k; }
						}
					bits |= (unsigned char)(best << (i * 2));
					}
				block[4 + j] = bits;
				}

			memcpy(out + (size_t)(by * (w / 4) + bx) * 8, block, 8);
			}
		}
	}

void dxt1_downsample(const unsigned char* rgba, int w, int h, unsigned char* out)
{
	int nw = w / 2, nh = h / 2;
	for (int y = 0; y < nh; ++y) {
		for (int x = 0; x < nw; ++x) {
			int r = 0, g = 0, b = 0;
			for (int j = 0; j < 2; ++j)
			for (int i = 0; i < 2; ++i) {
				const unsigned char* p = rgba + ((size_t)(y * 2 + j) * w + (x * 2 + i)) * 4;
				r += p[0]; g += p[1]; b += p[2];
				}
			unsigned char* o = out + (size_t)(y * nw + x) * 4;
			o[0] = (unsigned char)(r / 4);
			o[1] = (unsigned char)(g / 4);
			o[2] = (unsigned char)(b / 4);
			o[3] = 255;
			}
		}
	}

// A tile is the 32x32 image followed by its 16x16, 8x8 and 4x4 mipmaps.
void dxt1_encode_tile(const unsigned char* rgba32x32, unsigned char* tile680)
{
	unsigned char mip[16 * 16 * 4];

	dxt1_encode(rgba32x32, 32, 32, tile680);

	dxt1_downsample(rgba32x32, 32, 32, mip);
	dxt1_encode(mip, 16, 16, tile680 + 512);

	dxt1_downsample(mip, 16, 16, mip);
	dxt1_encode(mip, 8, 8, tile680 + 640);

	dxt1_downsample(mip, 8, 8, mip);
	dxt1_encode(mip, 4, 4, tile680 + 672);
	}

