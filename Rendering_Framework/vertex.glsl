#version 410 core

layout(location = 0) in vec3 iv3vertex;
layout(location = 1) in vec3 iv3tex_coord;
layout(location = 2) in vec3 iv3normal;

uniform mat4 vp;
uniform mat4 m;
uniform vec4 ov_color;

out vec2 texcoord;
out vec3 normal;
flat out vec4 ovc;

void main()
{
	gl_Position = vp * m * vec4(iv3vertex, 1);
	texcoord = iv3tex_coord.xy;
        normal = iv3normal;
	ovc = ov_color;
}
