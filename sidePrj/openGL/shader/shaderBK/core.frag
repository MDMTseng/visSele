#version 330 core
#extension GL_ARB_explicit_uniform_location : require

layout(location = 2) uniform vec3 initialUniform = vec3(1.0, 2.0, 3.0);
/*
float vec_dat[]={5,6,7,8};
glUniform3fv(2, 1,vec_dat);*/

layout(location = 500) uniform vec3 vecArr[3];
/*
float vec_datArr[]={5,6,7, 15,16,17, 25,26,27};
glUniform3fv(500, 3,vec_datArr);*/

layout(location = 3) uniform float fdat;
/*
float f_dat=85;
glUniform1f(3, f_dat);*/


layout(location = 55) uniform sampler2DRect baseTexture1;
layout(location = 66) uniform sampler2DRect baseTexture2;
layout(location=0) out vec4 color1;
layout(location=1) out vec4 color2;

void main()
{
	vec2 coor=floor(gl_FragCoord.xy);
	uint _x=uint(coor.x);

	color2=texture(baseTexture1, coor);
	color2.x = 35;
	color1=texture(baseTexture2, coor);
	color1.y = 50;
	/*color.r = texture(baseTexture1, coor).r+1;
	color.gb = texture(baseTexture1, coor).gb;
	color.a = texture(baseTexture2, coor).a;
	for(int i=0;i<5;i++)
	{
		color.gb += texture(baseTexture1, coor+1).xw;
	}*/

}
