#version 330 core
in vec3 position;
in vec3 color;

out vec2 rectPos;

void main()
{
    gl_Position = vec4(position, 1.0f);
    rectPos = vec2(position.x+1,position.y+1)*0.5;
}
