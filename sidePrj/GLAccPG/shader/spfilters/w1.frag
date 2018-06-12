#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=gl_FragCoord.xy;
	y1=texture(x1, coorf);
	y1.w=1;
}
