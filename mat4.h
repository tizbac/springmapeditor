/*
 * mat4.h - small column-major 4x4 matrix helper for the core profile renderer.
 *
 * Matrices are stored the way OpenGL passes them to shaders: the element at
 * row r, column c is m[c * 4 + r].
 */
#ifndef MAT4_H
#define MAT4_H

#include <cmath>

struct Mat4 {
	float m[16];

	Mat4()
	{
		for (int i = 0; i < 16; ++i)
			m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
	}

	static Mat4 identity()
	{
		return Mat4();
	}

	static Mat4 multiply(const Mat4 &a, const Mat4 &b)
	{
		Mat4 r;
		for (int c = 0; c < 4; ++c)
			for (int row = 0; row < 4; ++row)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k)
					sum += a.m[k * 4 + row] * b.m[c * 4 + k];
				r.m[c * 4 + row] = sum;
			}
		return r;
	}

	static Mat4 translate(float x, float y, float z)
	{
		Mat4 r;
		r.m[12] = x;
		r.m[13] = y;
		r.m[14] = z;
		return r;
	}

	static Mat4 rotateDeg(float degrees, float axisX, float axisY, float axisZ)
	{
		const float pi = 3.14159265358979323846f;
		float a = degrees * pi / 180.0f;
		float s = sin(a), c = cos(a);
		float x = axisX, y = axisY, z = axisZ;

		Mat4 r;
		r.m[0] = c + x * x * (1.0f - c);
		r.m[1] = y * x * (1.0f - c) + z * s;
		r.m[2] = z * x * (1.0f - c) - y * s;

		r.m[4] = x * y * (1.0f - c) - z * s;
		r.m[5] = c + y * y * (1.0f - c);
		r.m[6] = z * y * (1.0f - c) + x * s;

		r.m[8] = x * z * (1.0f - c) + y * s;
		r.m[9] = y * z * (1.0f - c) - x * s;
		r.m[10] = c + z * z * (1.0f - c);
		return r;
	}

	static Mat4 perspective(float fovDegrees, float aspect, float nearPlane, float farPlane)
	{
		const float pi = 3.14159265358979323846f;
		float f = 1.0f / tan(fovDegrees * pi / 360.0f);

		Mat4 r;
		for (int i = 0; i < 16; ++i)
			r.m[i] = 0.0f;
		r.m[0] = f / aspect;
		r.m[5] = f;
		r.m[10] = (farPlane + nearPlane) / (nearPlane - farPlane);
		r.m[11] = -1.0f;
		r.m[14] = (2.0f * farPlane * nearPlane) / (nearPlane - farPlane);
		return r;
	}

	static Mat4 ortho(float left, float right, float bottom, float top, float nearPlane,
	                  float farPlane)
	{
		Mat4 r;
		for (int i = 0; i < 16; ++i)
			r.m[i] = 0.0f;
		r.m[0] = 2.0f / (right - left);
		r.m[5] = 2.0f / (top - bottom);
		r.m[10] = -2.0f / (farPlane - nearPlane);
		r.m[12] = -(right + left) / (right - left);
		r.m[13] = -(top + bottom) / (top - bottom);
		r.m[14] = -(farPlane + nearPlane) / (farPlane - nearPlane);
		r.m[15] = 1.0f;
		return r;
	}

	// Returns the inverse, or the identity when the matrix is singular.
	Mat4 inverse() const
	{
		// Row major copy: t[r * 4 + c] == element at row r, column c.
		float t[16];
		for (int r = 0; r < 4; ++r)
			for (int c = 0; c < 4; ++c)
				t[r * 4 + c] = m[c * 4 + r];

		float inv[16];
		inv[0] = t[5] * t[10] * t[15] - t[5] * t[11] * t[14] - t[9] * t[6] * t[15] +
		         t[9] * t[7] * t[14] + t[13] * t[6] * t[11] - t[13] * t[7] * t[10];
		inv[4] = -t[4] * t[10] * t[15] + t[4] * t[11] * t[14] + t[8] * t[6] * t[15] -
		         t[8] * t[7] * t[14] - t[12] * t[6] * t[11] + t[12] * t[7] * t[10];
		inv[8] = t[4] * t[9] * t[15] - t[4] * t[11] * t[13] - t[8] * t[5] * t[15] +
		         t[8] * t[7] * t[13] + t[12] * t[5] * t[11] - t[12] * t[7] * t[9];
		inv[12] = -t[4] * t[9] * t[14] + t[4] * t[10] * t[13] + t[8] * t[5] * t[14] -
		          t[8] * t[6] * t[13] - t[12] * t[5] * t[10] + t[12] * t[6] * t[9];
		inv[1] = -t[1] * t[10] * t[15] + t[1] * t[11] * t[14] + t[9] * t[2] * t[15] -
		         t[9] * t[3] * t[14] - t[13] * t[2] * t[11] + t[13] * t[3] * t[10];
		inv[5] = t[0] * t[10] * t[15] - t[0] * t[11] * t[14] - t[8] * t[2] * t[15] +
		         t[8] * t[3] * t[14] + t[12] * t[2] * t[11] - t[12] * t[3] * t[10];
		inv[9] = -t[0] * t[9] * t[15] + t[0] * t[11] * t[13] + t[8] * t[1] * t[15] -
		         t[8] * t[3] * t[13] - t[12] * t[1] * t[11] + t[12] * t[3] * t[9];
		inv[13] = t[0] * t[9] * t[14] - t[0] * t[10] * t[13] - t[8] * t[1] * t[14] +
		          t[8] * t[2] * t[13] + t[12] * t[1] * t[10] - t[12] * t[2] * t[9];
		inv[2] = t[1] * t[6] * t[15] - t[1] * t[7] * t[14] - t[5] * t[2] * t[15] +
		         t[5] * t[3] * t[14] + t[13] * t[2] * t[7] - t[13] * t[3] * t[6];
		inv[6] = -t[0] * t[6] * t[15] + t[0] * t[7] * t[14] + t[4] * t[2] * t[15] -
		         t[4] * t[3] * t[14] - t[12] * t[2] * t[7] + t[12] * t[3] * t[6];
		inv[10] = t[0] * t[5] * t[15] - t[0] * t[7] * t[13] - t[4] * t[1] * t[15] +
		          t[4] * t[3] * t[13] + t[12] * t[1] * t[7] - t[12] * t[3] * t[5];
		inv[14] = -t[0] * t[5] * t[14] + t[0] * t[6] * t[13] + t[4] * t[1] * t[14] -
		          t[4] * t[2] * t[13] - t[12] * t[1] * t[6] + t[12] * t[2] * t[5];
		inv[3] = -t[1] * t[6] * t[11] + t[1] * t[7] * t[10] + t[5] * t[2] * t[11] -
		         t[5] * t[3] * t[10] - t[9] * t[2] * t[7] + t[9] * t[3] * t[6];
		inv[7] = t[0] * t[6] * t[11] - t[0] * t[7] * t[10] - t[4] * t[2] * t[11] +
		         t[4] * t[3] * t[10] + t[8] * t[2] * t[7] - t[8] * t[3] * t[6];
		inv[11] = -t[0] * t[5] * t[11] + t[0] * t[7] * t[9] + t[4] * t[1] * t[11] -
		          t[4] * t[3] * t[9] - t[8] * t[1] * t[7] + t[8] * t[3] * t[5];
		inv[15] = t[0] * t[5] * t[10] - t[0] * t[6] * t[9] - t[4] * t[1] * t[10] +
		          t[4] * t[2] * t[9] + t[8] * t[1] * t[6] - t[8] * t[2] * t[5];

		float det = t[0] * inv[0] + t[1] * inv[4] + t[2] * inv[8] + t[3] * inv[12];
		if (std::fabs(det) < 1e-12f)
			return Mat4::identity();

		Mat4 r;
		for (int row = 0; row < 4; ++row)
			for (int col = 0; col < 4; ++col)
				r.m[col * 4 + row] = inv[row * 4 + col] / det;
		return r;
	}

	// Transforms a point, dividing by w.
	void transformPoint(float x, float y, float z, float &ox, float &oy, float &oz) const
	{
		float w = m[3] * x + m[7] * y + m[11] * z + m[15];
		if (std::fabs(w) < 1e-12f)
			w = 1e-12f;
		ox = (m[0] * x + m[4] * y + m[8] * z + m[12]) / w;
		oy = (m[1] * x + m[5] * y + m[9] * z + m[13]) / w;
		oz = (m[2] * x + m[6] * y + m[10] * z + m[14]) / w;
	}
};

#endif // MAT4_H
