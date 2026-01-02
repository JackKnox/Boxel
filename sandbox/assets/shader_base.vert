#version 450

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_colour;
layout(location = 2) in vec2 in_texcoord;

layout(location = 0) out vec3 v_colour;
layout(location = 1) out vec2 v_texcoord;

void main() {
    v_colour = in_colour;
    v_texcoord = in_texcoord;
    gl_Position = vec4(in_pos, 0.0, 1.0);
}
