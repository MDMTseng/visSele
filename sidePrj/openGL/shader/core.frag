#version 330 core
uniform sampler2DRect baseTexture1;
uniform sampler2DRect baseTexture2;
out vec4 color;

void main()
{
	vec2 coor=floor(gl_FragCoord.xy);
	uint _x=uint(coor.x);
	color = texture(baseTexture1, coor);
	for(int i=0;i<10;++i)
	{
		color +=  vec4(1,2,3,4);
	}

}
