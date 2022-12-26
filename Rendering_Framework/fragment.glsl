#version 410 core

layout(location = 0) out vec4 fragColor;

flat in vec4 ovc;
in vec2 texcoord;

uniform sampler2D tex;

void main()
{
    vec4 texel = texture(tex, texcoord);
    if (texel.a < 0.3)
            discard;
    fragColor = vec4((texel.rgb * (1.0 - ovc.a)) + ovc.rgb, 1.0);
}
