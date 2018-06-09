#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect offset_mesh;
uniform uvec3 outputDim;

in vec2 frag_position_0_5;//-0.5~0.5

layout(location=0) out vec4 y1;

//To avoid texture boundary interpolation skipping(the interpolation starts with 0.5~1.5)
vec2 getEdgeInterpTexCoord(sampler2DRect tex,vec2 fragPos)
{
	vec2 coorScale=textureSize(tex);
	coorScale=(coorScale-1)/coorScale;
	return textureSize(tex)*((coorScale*fragPos)+0.5);
}

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	vec2 FragCoord_scaled=gl_FragCoord.xy/outputDim.xy;
	uvec2 coorui=uvec2(coorf);
	uint idx = (coorui.x+coorui.y*outputDim.x)*outputDim.z;
	vec4 id_v= vec4(0,1,2,3)+idx;
	vec4 x1_v=texture(x1, coorf);

	vec2 mesh_offset = texture(offset_mesh, getEdgeInterpTexCoord(offset_mesh,frag_position_0_5)).xy;
	y1=texture(x1, textureSize(x1)*(FragCoord_scaled+mesh_offset));
}
