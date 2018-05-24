#version 330 core
uniform sampler2DRect baseTexture1;
uniform sampler2DRect baseTexture2;
out vec4 color;

void main()
{
	vec2 coor=floor(gl_FragCoord.xy);
	uint _x=uint(coor.x);
	color.rg = texture(baseTexture1, coor).rg;
	color.ba = texture(baseTexture2, coor).ba;

}
