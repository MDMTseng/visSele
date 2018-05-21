#version 330 core
in vec2 rectPos;
uniform sampler2DRect baseTexture;
out vec4 color;

void main()
{
	color = vec4(0,0,0,0);
	float margin=0.01;
	float sec=1.0/2;
	//if(rectPos.x>(sec-margin) && rectPos.x< (sec+margin) )
		color = texture2DRect(baseTexture, rectPos*3);
}
