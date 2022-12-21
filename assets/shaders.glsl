///////////////////////////////
// Environment cube generation
///////////////////////////////
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

///////////////////////////////
// Diffuse irradiance generation
///////////////////////////////

@fs diffuse_irradiance_fs
out vec4 fragColor;

in vec3 localPos;

uniform samplerCube env_map;

#define PI 3.14159265359

vec3 CalculateIrradiance()
{
    vec3 N = normalize(localPos);
    vec3 irradiance = vec3(0.0);

    // tangent space calculation from origin point
    vec3 up    = vec3(0.0, 1.0, 0.0);
    vec3 right = normalize(cross(up, N));
    up         = normalize(cross(N, right));

    float sampleDelta = 0.025;
    float nrSamples = 0.0;
    for(float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta)
        {
            // spherical to cartesian (in tangent space)
            vec3 tangentSample = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
            // tangent space to world
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += texture(env_map, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples++;
        }
    }
    irradiance = PI * irradiance * (1.0 / float(nrSamples));
    return irradiance;
}

void main()
{
    vec3 color = CalculateIrradiance();
    fragColor = vec4(color, 1.0);
}
@end


///////////////////////////////
// Display cube pass
///////////////////////////////
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

@program pbr_shader             cubemap_vs cubemap_fs
@program pbr_diffuse_irradiance cubemap_vs diffuse_irradiance_fs
@program pbr_display            display_vs display_fs
