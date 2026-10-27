#version 330 core

layout(location = 0) in vec3 aPosition;   // world position, y = -height
layout(location = 1) in vec3 aNormal;    // world space normal
layout(location = 2) in vec2 aTexCoord;   // uv inside the big square's ground texture

uniform mat4 uVP;
uniform vec2 uMetalOffset;
uniform vec2 uMetalScale;

out vec2 vTexCoord;
out vec2 vMetalCoord;
out vec3 vNormal;

void main()
{
	gl_Position = uVP * vec4(aPosition, 1.0);
	vTexCoord = aTexCoord;
	vMetalCoord = aTexCoord * uMetalScale + uMetalOffset;
	vNormal = aNormal;
}
