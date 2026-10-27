#version 330 core

in vec2 vTexCoord;
in vec4 vColor;

uniform sampler2D tex;
uniform int useTex;

out vec4 outColor;

void main()
{
	vec4 color = vColor;
	if (useTex == 1)
		color *= texture(tex, vTexCoord);
	outColor = color;
}
