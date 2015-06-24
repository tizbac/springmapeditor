varying vec3 v_V;
varying vec3 v_N;
uniform sampler2D maintex;
uniform sampler2D metaltex;
uniform int mode;
void main() {
	vec3 N = normalize(v_N);
	vec3 V = normalize(v_V);
	vec3 R = reflect(V, N);
	vec3 L = normalize(vec3(gl_LightSource[0].position));
	vec3 color = texture2D(maintex,gl_TexCoord[0].st).rgb;
	vec3 metalvalue = texture2D(metaltex,gl_TexCoord[0].st).rgb;
	vec4 ambient = vec4(0,0,0,1);
	vec4 diffuse;
	if  (mode == 1 )
	{
		//diffuse = vec4(metalvalue,1.0);
		diffuse = vec4(max(vec3(color)/3.0,metalvalue*vec4(0.0,0.7,0,0)),1) * max(dot(L, N), 0.0);
	}else if ( mode == 0){
		diffuse = vec4(color,1) * max(dot(L, N), 0.0);
	}else{
  	diffuse = vec4(1.0,1.0,1.0,1.0);}
	//vec4 specular = gl_FrontMaterial.specular * pow(max(dot(R, L), 0.0), gl_FrontMaterial.shininess);

	gl_FragColor = ambient + diffuse ;//+ specular;
}
