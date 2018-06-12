#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
layout(location=0) out vec4 y1;

uniform uvec3 outputDim;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	vec2 texRect = coorf /outputDim.xy* textureSize(x1);
	y1=(texture(x1, texRect)*500)+0.5;



}
