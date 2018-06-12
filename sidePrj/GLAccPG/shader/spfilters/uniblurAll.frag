#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

uniform int blur_size=-3;//Positive is for horizontal, negative is for vertical
void main()
{
	vec2 coorf=gl_FragCoord.xy;
	//
	vec2 pSum=vec2(0);
	int _size=blur_size;
	if(_size<0)
	{
		_size=-_size;
		for(int j = -_size; j <= _size; j++)
		{
			pSum+=texture(x1, coorf+vec2(0,j)).xy;
		}
	}
	else
	{
		for(int j = -_size; j <= _size; j++)
		{
			pSum+=texture(x1, coorf+vec2(j,0)).xy;
		}
	}
	y1.xy = pSum/(_size*2+1);
}
