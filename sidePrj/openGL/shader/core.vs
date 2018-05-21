#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;

out vec2 rectPos;

void main()
{
    gl_Position = vec4(position, 1.0f);
    rectPos = vec2(position.x+1,position.y+1)*0.5;
}
