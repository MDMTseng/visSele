#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

uniform int skipP=1;
uniform int blur_size=3;
void main()
{
	vec2 coorf=gl_FragCoord.xy;
	//
	vec4 pSum=vec4(0);
	int _size=blur_size*skipP;
	for(int j = -_size; j <= _size; j+=skipP)
	{
		pSum+=texture(x1, coorf+vec2(0,j));
	}
	for(int j = -_size; j <= _size; j+=skipP)
	{
		pSum+=texture(x1, coorf+vec2(j,0));
	}
	_size/=skipP;
	y1 = pSum/(_size*2+1)/2;
	//y1.zw = texture(x1, coorf).zw;
}
