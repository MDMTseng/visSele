#version 330 core
in vec2 rectPos;
uniform sampler2DRect baseTexture1;
uniform sampler2DRect baseTexture2;
out vec4 color;

void main()
{
	color = vec4(0,0,0,0);
	float alpha = 1-rectPos.x;
	color = texture2DRect(baseTexture1, rectPos*3);
	color = alpha*color+(1-alpha)*texture2DRect(baseTexture2, rectPos*3);
}
