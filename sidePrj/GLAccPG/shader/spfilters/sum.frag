#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=gl_FragCoord.xy;
	vec4 x1_=texture(x1, coorf);
	vec4 x2_=texture(x2, coorf);
	y1=x1_+x2_/4;
}
