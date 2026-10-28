#version 330 core

in vec2 vTexCoord;
in vec2 vMetalCoord;
in vec3 vNormal;

uniform sampler2D maintex;   // the ground tile texture
uniform sampler2D metaltex;  // whole map metal map
uniform sampler2D painttex;  // painted colour + coverage for this big square
uniform int mode;            // 0 height, 1 metal, 2 texture, 3 colour
uniform int hasPaint;
uniform vec3 sunDir;
uniform vec3 groundAmbientColor;
uniform vec3 groundDiffuseColor;

out vec4 outColor;

void main()
{
	vec3 N = normalize(vNormal);
	vec3 L = normalize(sunDir);
	float ndl = max(dot(L, N), 0.0);

	// Same ground lighting model as Spring's SMF shader.
	vec3 light = min((groundAmbientColor + groundDiffuseColor * ndl) * (210.0 / 255.0),
	                 vec3(1.0));

	vec3 color = texture(maintex, vTexCoord).rgb;

	// Multitexturing: blend the painted colour over the ground tile texture.
	if (hasPaint == 1) {
		vec4 paint = texture(painttex, vTexCoord);
		color = mix(color, paint.rgb, paint.a);
	}

	vec3 metal = texture(metaltex, vMetalCoord).rgb;

	vec3 result;
	if (mode == 1)
		result = max(color / 3.0, metal * vec3(0.0, 0.7, 0.0)) * light;
	else
		result = color * light;

	outColor = vec4(result, 1.0);
}
