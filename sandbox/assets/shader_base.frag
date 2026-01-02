#version 450

layout(binding = 0) uniform sampler2D u_texSampler;

layout(location = 0) in vec3 v_colour;
layout(location = 1) in vec2 v_texcoord;

layout(location = 0) out vec4 o_colour;

void main() {
	vec4 tex_colour = texture(u_texSampler, v_texcoord);
	o_colour = vec4(v_colour * tex_colour.rgb, tex_colour.a);
}