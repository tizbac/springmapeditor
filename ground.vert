#version 330 core

layout(location = 0) in vec3 aLocalPos;   // local grid position inside a patch
layout(location = 1) in vec2 aTexCoord;  // uv inside the big square's ground texture

uniform mat4 uVP;
uniform vec2 uSquareOffset;              // patch origin in world units
uniform vec2 uMapSize;                   // corner heightmap size
uniform float uHeightMin;
uniform float uHeightScale;
uniform sampler2D heightMapTex;

uniform vec2 uMetalOffset;
uniform vec2 uMetalScale;

out vec2 vTexCoord;
out vec2 vMetalCoord;
out vec3 vWorldPos;
out vec3 vNormal;

float heightAt(ivec2 p)
{
	p = clamp(p, ivec2(0), ivec2(uMapSize) - 1);
	return uHeightMin + texelFetch(heightMapTex, p, 0).r * uHeightScale;
}

void main()
{
	vec2 world = aLocalPos.xz + uSquareOffset;

	// The heightmap has one texel per map square; 64 map squares make one
	// world unit. texelFetch keeps the grid vertices exactly on texel centres.
	ivec2 mapPos = ivec2(round(world * 64.0));
	mapPos = clamp(mapPos, ivec2(0), ivec2(uMapSize) - 1);
	float f = texelFetch(heightMapTex, mapPos, 0).r;

	float hL = heightAt(mapPos + ivec2(-1, 0));
	float hR = heightAt(mapPos + ivec2( 1, 0));
	float hD = heightAt(mapPos + ivec2(0, -1));
	float hU = heightAt(mapPos + ivec2(0,  1));

	vWorldPos = vec3(world.x, uHeightMin + f * uHeightScale, world.y);
	vNormal = normalize(vec3(-(hR - hL) * 32.0, 1.0, -(hU - hD) * 32.0));
	vTexCoord = aTexCoord;
	vMetalCoord = aTexCoord * uMetalScale + uMetalOffset;

	gl_Position = uVP * vec4(vWorldPos, 1.0);
}
