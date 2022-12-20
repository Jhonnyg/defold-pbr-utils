@vs cubemap_vs
in vec3 position;

uniform cubemap_uniforms
{
    mat4 projection;
    mat4 view;
};

out vec3 localPos;

void main()
{
    localPos = position;
    gl_Position = projection * view * vec4(position, 1.0);
}
@end

@fs cubemap_fs
out vec4 fragColor;

in vec3 localPos;

uniform sampler2D tex;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(localPos));
    vec3 color = texture(tex, uv).rgb;
    fragColor = vec4(color, 1.0);
}
@end

@vs display_vs
in vec2 position;
in vec2 texcoord;

out vec2 v_texcoord;

void main()
{
    gl_Position = vec4(position, 0.0, 1.0);
    v_texcoord = texcoord;
}
@end

@fs display_fs
out vec4 fragColor;

in vec2 v_texcoord;

uniform samplerCube tex_cube;

void main()
{
    fragColor = texture(tex_cube, vec3(v_texcoord, 1.0));
}
@end

@program pbr_shader cubemap_vs cubemap_fs
@program pbr_display display_vs display_fs
