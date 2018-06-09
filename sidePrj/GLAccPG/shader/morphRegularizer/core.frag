#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;
uniform uvec3 outputDim;

layout(location=0) out vec4 y1;

void main()
{
	float W=0.5;
	vec2 meshCoord=textureSize(x1)*gl_FragCoord.xy/outputDim.xy;

	//Avoid changing edge
	/*vec2 oD=vec2(outputDim.xy);
	if(meshCoord.x<1||meshCoord.y<1 || meshCoord.x>(oD.x-1)||meshCoord.y>(oD.y-1))
	{
		return;
	}*/
	//We can use viewport(1,1,W-2,H-2) to achieve same effect as well

	vec2 moffset_c = texture(x1, meshCoord).xy;
	vec2 moffset_l = texture(x1, meshCoord+vec2(-1,0)).xy;
	vec2 moffset_r = texture(x1, meshCoord+vec2(+1,0)).xy;
	vec2 moffset_t = texture(x1, meshCoord+vec2(0,+1)).xy;
	vec2 moffset_b = texture(x1, meshCoord+vec2(0,-1)).xy;
	y1.xy=W*moffset_c+(1-W)*(+moffset_l+moffset_r+moffset_t+moffset_b)/4;
}
