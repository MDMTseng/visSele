#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	vec2 sobel_vec = texture(x1, coorf).xy;

	float len = length(	sobel_vec);
	if(len<0.0000001)
	{
		y1.xy=sobel_vec;
	}
	else
	{
		y1.xy=sobel_vec/len;
	}
	y1.z=1;
}
