#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	y1.x=texture(x1, coorui).x+texture(x2, coorui).x;



}
