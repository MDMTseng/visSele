#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
uniform sampler2DRect x3;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;
layout(location=1) out vec4 y2;

void main()
{
	vec2 coorf=gl_FragCoord.xy;
	vec4 x1_=texture(x1, coorf);
	vec4 x2_=texture(x2, coorf);
	vec4 x3_=texture(x3, coorf);
	y1.xy=x1_.xy+0.5*x2_.xy/(x3_.xy+1);
	y2.xy=x3_.xy+x2_.xy*x2_.xy*200;
}
