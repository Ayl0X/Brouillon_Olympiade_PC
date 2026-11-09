#version 330 core

in vec3 vertexPosition;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 FragPos;
out vec3 Normal;

void main()
{
    FragPos = vec3(matModel * vec4(vertexPosition, 1.0));
    Normal  = mat3(matNormal) * vertexNormal;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}