#version 330 core
in vec3 position;
in vec3 color;
out vec2 frag_position_0_5;
uniform uvec3 outputDim;
void main()
{
    gl_Position = vec4(position, 1.0f);
    vec2 scaleXY=vec2(outputDim);
    scaleXY/=(scaleXY-1);
    frag_position_0_5 = (position.xy*scaleXY)*0.5;
}
