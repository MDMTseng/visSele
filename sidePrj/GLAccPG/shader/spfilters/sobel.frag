#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	//vec2 mesh_offset = texture(x1, coorf).xy;
	/*
	012
	7 3
	654
	*/
	float p0=texture(x1, coorf+vec2(-1,1)).x;
	float p1=texture(x1, coorf+vec2( 0,1)).x;
	float p2=texture(x1, coorf+vec2( 1,1)).x;

	float p3=texture(x1, coorf+vec2(+1,0)).x;

	float p6=texture(x1, coorf+vec2(-1,-1)).x;
	float p5=texture(x1, coorf+vec2( 0,-1)).x;
	float p4=texture(x1, coorf+vec2( 1,-1)).x;

	float p7=texture(x1, coorf+vec2(-1,0)).x;
	y1.x=(p2+2*p3+p4)-(p0+2*p7+p6);
	y1.y=(p0+2*p1+p2)-(p6+2*p5+p4);
	//y1=y1/2+0.5;

}
