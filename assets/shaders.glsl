@vs vs
in vec2 position;
in vec2 texcoord;

out vec2 v_texcoord;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    v_texcoord = texcoord;
}
@end

@fs fs
out vec4 fragColor;

in vec2 v_texcoord;

uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, v_texcoord);
}
@end

@fs display
out vec4 fragColor;

in vec2 v_texcoord;

uniform sampler2D tex;

void main()
{
    fragColor = texture(tex, v_texcoord);
}
@end

@program pbr_shader vs fs
@program pbr_display vs display
