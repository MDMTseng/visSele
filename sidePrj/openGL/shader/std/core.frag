#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
uniform uvec3 outputDim;


layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	vec2 tmp = coorf/(vec2(outputDim.xy)-1);//0~1
	tmp=tmp*(vec2(outputDim.xy)-1)+0.5;//0.5~W-0.5
	y1.xy=tmp;
}
