#version 330 core
uniform sampler2DRect baseTexture1;
uniform sampler2DRect baseTexture2;
out vec4 color;

void main()
{
	vec2 coor=floor(gl_FragCoord.xy);
	uint _x=uint(coor.x);
	vec4 incV=vec4(_x,_x,_x,_x);
	color = texture(baseTexture2, coor);
	for(int i=0;i<100;++i)
	{
	  color +=  texture(baseTexture2, coor+i*0.001)/1000000;
	}
	/*float alpha = 1-rectPos.x;
	color = texture2DRect(baseTexture1, rectPos*3);
	color = alpha*color+(1-alpha)*texture2DRect(baseTexture2, rectPos*3);*/
}
