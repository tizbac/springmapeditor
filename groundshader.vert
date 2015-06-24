varying vec3 v_V;
varying vec3 v_N;
uniform float scale;
uniform float tx;
uniform float ty;
void main() {
	gl_Position = ftransform();
	gl_TexCoord[0] = gl_MultiTexCoord0 * scale + vec4(tx,ty,0.0,0.0);
	
	v_V = (gl_ModelViewMatrix * gl_Vertex).xyz;
	v_N = gl_NormalMatrix * gl_Normal;
}
