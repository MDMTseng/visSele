#version 330 core
//#extension GL_ARB_explicit_uniform_location : require

uniform sampler2DRect x1;//shiftMap
uniform sampler2DRect x2;//input
uniform sampler2DRect x3;//reference
uniform uvec3 outputDim;
layout(location=0) out vec4 y1;//shiftMap


void main()
{
    vec2 coorf=gl_FragCoord.xy;
    //float reference_=texture(x3, coorf).x;

    vec2 offset=texture(x1, coorf).xy;

    y1=abs(texture(x2, offset+coorf)-texture(x3,coorf))*5;

}
