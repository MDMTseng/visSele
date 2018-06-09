#version 330 core
uniform sampler2DRect x1;

uniform uvec3 outputDim;
layout(location=0) out vec4 y1;

void main()
{
	vec2 XYHalfRegion = textureSize(x1) / (vec2(outputDim.xy)-1);
	vec2 mapCoord = gl_FragCoord.xy*XYHalfRegion;

	vec4 sum=vec4(0);
	for(float i=-(XYHalfRegion.y-1);i<XYHalfRegion.y;i++)
	{
		for(float j=-(XYHalfRegion.x-1);j<XYHalfRegion.x;j++)
		{
			vec2 relCoor = vec2(j,i);
			vec2 distance_ratio = 1 - abs(relCoor)/XYHalfRegion;
			sum+=texture(x1, mapCoord.xy+relCoor)*
								(distance_ratio.x * distance_ratio.y);
		}
	}
	y1 =sum/30;

}
