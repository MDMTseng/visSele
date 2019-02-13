#version 330 core
uniform sampler2DRect gradient_map;

varying vec3 position;

uniform uvec3 XYRegion;
layout(location=0) out vec2 mesh;

void main()
{
	//gl_Position
	vec2 coorf=floor(gl_FragCoord.xy);
	uvec2 coorui=uvec2(coorf);
	uint idx = (coorui.x+coorui.y*outputDim.y)*outputDim.z;
	vec2 mapCoord = vec2(position.x*XYRegion.x,position.y*XYRegion.y);

	for(int i=-(XYRegion.y-1);i<XYRegion.y;i++)
	{
		for(int j=-(XYRegion.x-1);j<XYRegion.x;j++)
		{
			texture(gradient_map, mapCoord.xy+vec2(j,i))
		}
	}
	mesh =

}
