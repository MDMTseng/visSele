#version 330 core
#extension GL_ARB_explicit_uniform_location : require

layout(location = 55) uniform sampler2DRect x1;
layout(location = 66) uniform sampler2DRect x2;
layout(location = 3) uniform uvec3 outputDim;
layout(location=0) out vec4 y1;
layout(location=1) out vec4 y2;

void main()
{
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	uint idx = (coorui.x+coorui.y*outputDim.y)*outputDim.z;
	vec4 idxArr=vec4(0,1,2,3)+idx;
	y1=texture(x1, coorui)+idxArr;

	y2=texture(x2, coorui);
	y2+=y1*0.02;


}
