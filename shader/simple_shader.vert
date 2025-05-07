#version 450 core

// ÿ������ִ��һ�Σ����λ��
//const vec2 positions[3] = vec2[3](
//    vec2( 0.0, -0.5),
//    vec2( 0.5,  0.5),
//   vec2( -0.5, 0.5)
//);

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;

//layout(location = 0) out vec3 fragcolor;

layout(push_constant) uniform Push
{
    mat2 transform;
    vec2 offset;
    vec3 color;
}push;

void main() {
    // gl_VertexIndex: Vulkan ���ö�������
    gl_Position = vec4(push.transform * position + push.offset, 0.0, 1.0);
    //fragcolor = push.color;
}
