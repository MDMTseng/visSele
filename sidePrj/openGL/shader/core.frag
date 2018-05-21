#version 330 core
in vec2 rectPos;
uniform sampler2DRect baseTexture;
out vec4 color;

void main()
{
	if(0==1)
	{
		color = vec4(rectPos,0, 1.0f);
	}
	else
	{
		color = texture2DRect(baseTexture, rectPos*3);
	}
}
