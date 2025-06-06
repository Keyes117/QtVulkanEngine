#version 450 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

layout(location = 0) out vec3 fragColor;

layout(set =0, binding = 0) uniform GlobalUbo
{
    mat4 projectionViewMatirx;
    vec3 globalcolor;
} ubo;

layout(push_constant) uniform Push
{
    mat4 modelMatrix;
    vec3 color;
}push;

void main() {
   gl_Position = ubo.projectionViewMatirx * vec4(position , 1.0); 
   fragColor = ubo.globalcolor
;}
