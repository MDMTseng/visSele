#version 330 core
uniform sampler2DRect mesh_in;
uniform sampler2DRect ref_img;
uniform sampler2DRect input_img;
uniform uvec3 outputDim;

layout(location=0) out vec4 warp_diff;

void main()
{
	//gl_Position
	vec2 coorf=floor(gl_FragCoord.xy);
	//uvec2 coorui=uvec2(coorf);
	//uint idx = (coorui.x+coorui.y*outputDim.y)*outputDim.z;

	vec2 mesh_coord = texture(mesh_in, coorf);//bicubic interpolation
	warp_diff.x = (texture(ref_img, coorf).x-texture(input_img, mesh_coord).x);
}
