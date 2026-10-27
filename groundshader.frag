varying vec3 v_V;
varying vec3 v_N;
uniform sampler2D maintex;
uniform sampler2D metaltex;
uniform sampler2D painttex;
uniform int mode;
uniform int hasPaint;

void main() {
	vec3 N = normalize(v_N);
	vec3 V = normalize(v_V);
	vec3 R = reflect(V, N);
	vec3 L = normalize(vec3(gl_LightSource[0].position));

	vec2 uv = gl_TexCoord[0].st;
	vec3 color = texture2D(maintex, uv).rgb;

	// Multitexturing: blend the painted colour over the ground tile texture.
	if (hasPaint == 1) {
		vec4 paint = texture2D(painttex, uv);
		color = mix(color, paint.rgb, paint.a);
	}

	vec3 metalvalue = texture2D(metaltex, uv).rgb;
	vec4 ambient = vec4(0,0,0,1);
	vec4 diffuse;

	if (mode == 1) {
		diffuse = vec4(max(color/3.0, metalvalue * vec3(0.0,0.7,0.0)),1.0) * max(dot(L, N), 0.0);
	} else if (mode == 0) {
		diffuse = vec4(color,1) * max(dot(L, N), 0.0);
	} else {
		diffuse = vec4(color,1) * max(dot(L, N), 0.0);
	}

	gl_FragColor = ambient + diffuse;
}
