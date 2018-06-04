#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
uniform sampler2DRect mesh;
uniform uvec3 outputDim;

in vec2 frag_position_0_5;//-0.5~0.5

layout(location=0) out vec4 y1;

vec2 getTexCoord(sampler2DRect tex,vec2 fragPos)
{
	vec2 coorScale=textureSize(tex);
	coorScale=(coorScale-1)/coorScale;
	return textureSize(tex)*((coorScale*fragPos)+0.5);
}

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	uint idx = (coorui.x+coorui.y*outputDim.x)*outputDim.z;
	vec4 id_v= vec4(0,1,2,3)+idx;
	vec4 x1_v=texture(x1, coorf);
	vec4 x2_v=texture(x2, coorf);

	vec2 mesh_coor = texture(mesh, getTexCoord(mesh, frag_position_0_5)).xy;
	y1=texture(x2, textureSize(x2)*(gl_FragCoord.xy/outputDim.xy+mesh_coor));



}
