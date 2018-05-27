#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform sampler2DRect x2;
uniform uvec3 outputDim;


layout(location=0) out vec4 y1;

void func0(float x1,float x2,inout float  y1,float idx)
{
	y1 = sin((((((x1+ x2) + idx))))) + x1 + x2;
}
void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	uint idx = (coorui.x+coorui.y*outputDim.x)*outputDim.z;
	vec4 id_v= vec4(0,1,2,3)+idx;
	vec4 x1_v=texture(x1, coorf);
	vec4 x2_v=texture(x2, coorf);

	{
		func0(x1_v.x,x2_v.x,y1.x,id_v.x);
	}



}
