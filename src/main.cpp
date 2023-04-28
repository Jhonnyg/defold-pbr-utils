
#include <math.h>
#include <stdint.h>

#include "linmath.h"

#define SJSON_IMPLEMENT
#include "sjson.h"

//#define SOKOL_METAL
#define SOKOL_GLCORE33
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(SOKOL_METAL)
    #include "shaders.metal.h"
#elif defined(SOKOL_GLCORE33)
    #include "shaders.glsl.h"
#else
    #error "Unsupported platform"
#endif

#if defined(_WIN32)
    typedef void (__stdcall * PFN_GLGETTEXIMAGEPROC)    (GLenum, GLint, GLenum, GLenum, void*);
    typedef void (__stdcall * PFN_GLGENERATEMIPMAPPROC) (GLenum);
    PFN_GLGETTEXIMAGEPROC    glGetTexImage    = NULL;
    PFN_GLGENERATEMIPMAPPROC glGenerateMipmap = NULL;
#endif

typedef struct
{
    sg_buffer vbuf;
    sg_buffer ibuf;
    int num_elements;
} mesh_t;

typedef struct {
    float pos[3];
} vertex_t;

struct app
{
    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass[6];
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
        int            m_Size;
    } m_EnvironmentPass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass[6];
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
        int            m_Size;
    } m_DiffuseIrradiancePass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass*       m_Pass;
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
        int            m_Size;
        int            m_MipmapCount;
    } m_PrefilterPass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass;
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
        int            m_Size;
    } m_BRDFLutPass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pipeline    m_Pipeline;
        sg_bindings    m_Bindings;
    } m_DisplayPass;

    mesh_t m_Cube;
    mat4x4 m_CubeViewMatrices[6];

    struct
    {
        const char* m_InputPath;
        sg_image    m_Image;
        int         m_Width;
        int         m_Height;
        int         m_MipmapCount;
    } m_EnvironmentTexture;

    uint8_t m_IsDone : 1;
} g_app = {};

static void sg_update_texture_filter(sg_image img_id, sg_filter min_filter, sg_filter mag_filter)
{
#if defined(SOKOL_GLCORE33)
    SOKOL_ASSERT(img_id.id != SG_INVALID_ID);
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    SOKOL_ASSERT(img);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);
    img->cmn.min_filter = min_filter;
    img->cmn.mag_filter = mag_filter;
    GLenum gl_min_filter = _sg_gl_filter(img->cmn.min_filter);
    GLenum gl_mag_filter = _sg_gl_filter(img->cmn.mag_filter);
    glTexParameteri(img->gl.target, GL_TEXTURE_MIN_FILTER, (GLint)gl_min_filter);
    _SG_GL_CHECK_ERROR();
    glTexParameteri(img->gl.target, GL_TEXTURE_MAG_FILTER, (GLint)gl_mag_filter);
    _SG_GL_CHECK_ERROR();
    _sg_gl_cache_restore_texture_binding(0);
#endif
}

static void sg_query_image_pixels(sg_image img_id, void* pixels, int target, int data_type, int mipmap)
{
#if defined(SOKOL_GLCORE33)
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);
    glGetTexImage(target, mipmap, GL_RGBA, data_type, pixels);
    _SG_GL_CHECK_ERROR();
    _sg_gl_cache_restore_texture_binding(0);
#endif
}

static void sg_generate_mipmaps(sg_image img_id)
{
#if defined(SOKOL_GLCORE33)
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    img->cmn.num_mipmaps = 1 + floor(log2(fmax(img->cmn.width, img->cmn.height)));

    _SG_GL_CHECK_ERROR();
    _sg_gl_cache_restore_texture_binding(0);
#endif
}

static void make_cube()
{
    vertex_t vertices[] =  {
        -1.0f, -1.0f, -1.0f, // bottom-left
         1.0f,  1.0f, -1.0f, // top-right
         1.0f, -1.0f, -1.0f, // bottom-right
         1.0f,  1.0f, -1.0f, // top-right
        -1.0f, -1.0f, -1.0f, // bottom-left
        -1.0f,  1.0f, -1.0f, // top-left
        // front face
        -1.0f, -1.0f,  1.0f, // bottom-left
         1.0f, -1.0f,  1.0f, // bottom-right
         1.0f,  1.0f,  1.0f, // top-right
         1.0f,  1.0f,  1.0f, // top-right
        -1.0f,  1.0f,  1.0f, // top-left
        -1.0f, -1.0f,  1.0f, // bottom-left
        // left face
        -1.0f,  1.0f,  1.0f, // top-right
        -1.0f,  1.0f, -1.0f, // top-left
        -1.0f, -1.0f, -1.0f, // bottom-left
        -1.0f, -1.0f, -1.0f, // bottom-left
        -1.0f, -1.0f,  1.0f, // bottom-right
        -1.0f,  1.0f,  1.0f, // top-right
        // right face
         1.0f,  1.0f,  1.0f, // top-left
         1.0f, -1.0f, -1.0f, // bottom-right
         1.0f,  1.0f, -1.0f, // top-right
         1.0f, -1.0f, -1.0f, // bottom-right
         1.0f,  1.0f,  1.0f, // top-left
         1.0f, -1.0f,  1.0f, // bottom-left
        // bottom face
        -1.0f, -1.0f, -1.0f, // top-right
         1.0f, -1.0f, -1.0f, // top-left
         1.0f, -1.0f,  1.0f, // bottom-left
         1.0f, -1.0f,  1.0f, // bottom-left
        -1.0f, -1.0f,  1.0f, // bottom-right
        -1.0f, -1.0f, -1.0f, // top-right
        // top face
        -1.0f,  1.0f, -1.0f, // top-left
         1.0f,  1.0f , 1.0f, // bottom-right
         1.0f,  1.0f, -1.0f, // top-right
         1.0f,  1.0f,  1.0f, // bottom-right
        -1.0f,  1.0f, -1.0f, // top-left
        -1.0f,  1.0f,  1.0f, // bottom-left
    };

    sg_buffer_desc cube_buffer_desc = {
        .data  = SG_RANGE(vertices),
        .label = "cube-vertices"
    };

    mesh_t cube = {
        .vbuf = sg_make_buffer(&cube_buffer_desc),
        .num_elements = 6 * 6,
    };

    g_app.m_Cube = cube;
}

static void make_environment_image()
{
    /// Load image
    sg_pixel_format pixel_format;
    uint8_t* pixel_data;
    int x,y,ch;
    int pixel_size;
    if (stbi_is_hdr(g_app.m_EnvironmentTexture.m_InputPath))
    {
        pixel_data   = (uint8_t*) stbi_loadf(g_app.m_EnvironmentTexture.m_InputPath, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA32F;
        pixel_size   = x * y * 4 * sizeof(float);
        printf("Input environment: HDR\n");
    }
    else
    {
        pixel_data   = (uint8_t*) stbi_load(g_app.m_EnvironmentTexture.m_InputPath, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA8;
        pixel_size   = x * y * 4 * sizeof(uint8_t);
        printf("Input environment: RGBA8\n");
    }

    sg_image_data img_data       = {};
    img_data.subimage[0][0].ptr  = pixel_data;
    img_data.subimage[0][0].size = pixel_size;

    sg_image_desc img_desc = {
        .width        = x,
        .height       = y,
        .pixel_format = pixel_format,
        .mag_filter   = SG_FILTER_LINEAR,
        .data         = img_data
    };

    g_app.m_EnvironmentTexture.m_Image       = sg_make_image(&img_desc);
    g_app.m_EnvironmentTexture.m_Width       = x;
    g_app.m_EnvironmentTexture.m_Height      = y;
    g_app.m_EnvironmentTexture.m_MipmapCount = 1 + floor(log2(fmax(x, y)));

    stbi_image_free(pixel_data);
}

static void make_brdf_lut_pass()
{
    g_app.m_BRDFLutPass.m_Size  = 512;

    sg_image_desc brdf_lut_pass_image_desc = {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = g_app.m_BRDFLutPass.m_Size,
        .height        = g_app.m_BRDFLutPass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_RGBA32F,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v        = SG_WRAP_CLAMP_TO_EDGE,
        .label         = "color-image"
    };

    g_app.m_BRDFLutPass.m_Image = sg_make_image(&brdf_lut_pass_image_desc);

    sg_image_desc depth_img_desc = {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = g_app.m_BRDFLutPass.m_Size,
        .height        = g_app.m_BRDFLutPass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .label         = "cubemap-depth-rt"
    };

    sg_image depth_img = sg_make_image(&depth_img_desc);

    sg_pass_desc brdf_lut_pass_desc = {
        .label = "offscreen-pass"
    };

    brdf_lut_pass_desc.depth_stencil_attachment.image = depth_img;

    brdf_lut_pass_desc.color_attachments[0] = {
        .image = g_app.m_BRDFLutPass.m_Image,
        .slice = 0
    };

    g_app.m_BRDFLutPass.m_Pass = sg_make_pass(&brdf_lut_pass_desc);

    g_app.m_BRDFLutPass.m_PassAction           = {};
    g_app.m_BRDFLutPass.m_PassAction.colors[0] = {
        .action = SG_ACTION_CLEAR,
        .value  = {0.0f, 0.0f, 0.0f, 1.0f}
    };

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };

    sg_buffer_desc brdf_lut_pass_buffer_desc = {
        .data  = SG_RANGE(vertices),
        .label = "triangle-vertices",
    };

    g_app.m_BRDFLutPass.m_Bindings.vertex_buffers[0] = sg_make_buffer(&brdf_lut_pass_buffer_desc);

    sg_pipeline_desc brdf_lut_pass_pipeline_desc = {
        .shader = sg_make_shader(pbr_brdf_lut_shader_desc(sg_query_backend())),
        .layout = {},
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode = SG_CULLMODE_NONE,
        .label     = "pipeline_fullscreen"
    };

    brdf_lut_pass_pipeline_desc.colors[0].pixel_format                         = SG_PIXELFORMAT_RGBA32F;
    brdf_lut_pass_pipeline_desc.layout.attrs[ATTR_brdf_lut_vs_position].format = SG_VERTEXFORMAT_FLOAT2;
    brdf_lut_pass_pipeline_desc.layout.attrs[ATTR_brdf_lut_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2;

    g_app.m_BRDFLutPass.m_Pipeline = sg_make_pipeline(&brdf_lut_pass_pipeline_desc);
}

static void make_prefilter_pass()
{
    g_app.m_PrefilterPass.m_Size        = 256;
    g_app.m_PrefilterPass.m_MipmapCount = 1 + floor(log2(g_app.m_PrefilterPass.m_Size));

    g_app.m_PrefilterPass.m_PassAction           = {};
    g_app.m_PrefilterPass.m_PassAction.colors[0] = {
        .action = SG_ACTION_CLEAR,
        .value = { 0.25f, 0.25f, 0.25f, 1.0f }
    };

    sg_image_desc prefilter_pass_img_desc = {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = g_app.m_PrefilterPass.m_Size,
        .height        = g_app.m_PrefilterPass.m_Size,
        .num_mipmaps   = g_app.m_PrefilterPass.m_MipmapCount,
        .pixel_format  = SG_PIXELFORMAT_RGBA32F,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .label         = "color-image"
    };

    g_app.m_PrefilterPass.m_Image = sg_make_image(&prefilter_pass_img_desc);

    g_app.m_PrefilterPass.m_Pass = (sg_pass*) malloc(sizeof(sg_pass) * g_app.m_PrefilterPass.m_MipmapCount * 6);

    int pass_index = 0;
    for (int mipmap = 0; mipmap < g_app.m_PrefilterPass.m_MipmapCount; ++mipmap)
    {

        sg_image_desc depth_img_desc = {
            .type          = SG_IMAGETYPE_2D,
            .render_target = true,
            .width         = g_app.m_PrefilterPass.m_Size >> mipmap,
            .height        = g_app.m_PrefilterPass.m_Size >> mipmap,
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .label         = "cubemap-depth-rt"
        };

        sg_image depth_img = sg_make_image(&depth_img_desc);

        for (int i = 0; i < 6; ++i)
        {
            sg_pass_desc prefilter_pass_desc = {
                .label = "offscreen-pass"
            };
            prefilter_pass_desc.depth_stencil_attachment.image = depth_img;
            prefilter_pass_desc.color_attachments[0] = {
                .image     = g_app.m_PrefilterPass.m_Image,
                .mip_level = mipmap,
                .slice     = i,
            };

            g_app.m_PrefilterPass.m_Pass[pass_index] = sg_make_pass(&prefilter_pass_desc);

            pass_index++;
        }
    }

    sg_pipeline_desc prefilter_pass_pipeline_desc = {
        .shader = sg_make_shader(pbr_prefilter_shader_desc(sg_query_backend())),
        .layout = {
        },
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    };

    prefilter_pass_pipeline_desc.colors[0].pixel_format                         = SG_PIXELFORMAT_RGBA32F;
    prefilter_pass_pipeline_desc.layout.attrs[ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3;

    g_app.m_PrefilterPass.m_Pipeline = sg_make_pipeline(&prefilter_pass_pipeline_desc);

    g_app.m_PrefilterPass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

static void make_diffuse_irradiance_pass()
{
    g_app.m_DiffuseIrradiancePass.m_Size = 64;
    g_app.m_DiffuseIrradiancePass.m_PassAction = {};
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0] = {
        .action = SG_ACTION_CLEAR,
        .value  = { 0.25f, 0.25f, 0.25f, 1.0f }
    };

    sg_image_desc diffuse_irridance_img_desc {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = g_app.m_DiffuseIrradiancePass.m_Size,
        .height        = g_app.m_DiffuseIrradiancePass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_RGBA32F,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .label         = "color-image"
    };

    g_app.m_DiffuseIrradiancePass.m_Image = sg_make_image(&diffuse_irridance_img_desc);

    sg_image_desc diffuse_irridance_depth_img_desc = {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = g_app.m_DiffuseIrradiancePass.m_Size,
        .height        = g_app.m_DiffuseIrradiancePass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .label         = "cubemap-depth-rt"
    };

    sg_image depth_img = sg_make_image(&diffuse_irridance_depth_img_desc);

    for (int i = 0; i < 6; ++i)
    {

        sg_pass_desc diffuse_irradiance_pass_desc = {
            .label = "offscreen-pass"
        };

        diffuse_irradiance_pass_desc.depth_stencil_attachment.image = depth_img;
        diffuse_irradiance_pass_desc.color_attachments[0] = {
            .image = g_app.m_DiffuseIrradiancePass.m_Image,
            .slice = i
        },

        g_app.m_DiffuseIrradiancePass.m_Pass[i] = sg_make_pass(&diffuse_irradiance_pass_desc);
    }

    sg_pipeline_desc diffuse_irradiance_pipeline_desc = {
        .shader = sg_make_shader(pbr_diffuse_irradiance_shader_desc(sg_query_backend())),
        .layout = {},
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    };

    diffuse_irradiance_pipeline_desc.colors[0].pixel_format                        = SG_PIXELFORMAT_RGBA32F;
    diffuse_irradiance_pipeline_desc.layout.attrs[ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3;

    g_app.m_DiffuseIrradiancePass.m_Pipeline = sg_make_pipeline(&diffuse_irradiance_pipeline_desc);

    g_app.m_DiffuseIrradiancePass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

static void make_environment_pass()
{
    g_app.m_EnvironmentPass.m_Size                 = 1024;
    g_app.m_EnvironmentPass.m_PassAction           = {};
    g_app.m_EnvironmentPass.m_PassAction.colors[0] = {
        .action = SG_ACTION_CLEAR,
        .value  = { 0.25f, 0.25f, 0.25f, 1.0f }
    };

    sg_image_desc environment_pass_image_desc = {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = g_app.m_EnvironmentPass.m_Size,
        .height        = g_app.m_EnvironmentPass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_RGBA32F,
        .sample_count  = 4,
        .min_filter    = SG_FILTER_LINEAR_MIPMAP_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .label         = "color-image"
    };

    g_app.m_EnvironmentPass.m_Image = sg_make_image(&environment_pass_image_desc);

    sg_image_desc depth_img_desc = {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = g_app.m_EnvironmentPass.m_Size,
        .height        = g_app.m_EnvironmentPass.m_Size,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .sample_count  = 4,
        .label         = "cubemap-depth-rt"
    };

    sg_image depth_img = sg_make_image(&depth_img_desc);

    for (int i = 0; i < 6; ++i)
    {
        sg_pass_desc environment_pass_desc = {
            .label = "offscreen-pass"
        };

        environment_pass_desc.depth_stencil_attachment.image = depth_img;
        environment_pass_desc.color_attachments[0] = {
                .image = g_app.m_EnvironmentPass.m_Image,
                .slice = i
        };

        g_app.m_EnvironmentPass.m_Pass[i] = sg_make_pass(&environment_pass_desc);
    }

    sg_pipeline_desc environment_pass_pipeline_desc = {
        .shader = sg_make_shader(pbr_shader_shader_desc(sg_query_backend())),
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .cull_mode              = SG_CULLMODE_NONE,
        .sample_count           = 4,
        .label                  = "pipeline_fullscreen"
    };

    environment_pass_pipeline_desc.colors[0].pixel_format                        = SG_PIXELFORMAT_RGBA32F;
    environment_pass_pipeline_desc.layout.attrs[ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3;

    g_app.m_EnvironmentPass.m_Pipeline                   = sg_make_pipeline(&environment_pass_pipeline_desc);
    g_app.m_EnvironmentPass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

static void make_display_pass(void)
{
    g_app.m_DisplayPass.m_PassAction           = {};
    g_app.m_DisplayPass.m_PassAction.colors[0] = {
        .action=SG_ACTION_CLEAR,
        .value={0.0f, 0.0f, 0.0f, 1.0f}
    };

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };

    sg_buffer_desc triangle_buffer_desc = {
        .data  = SG_RANGE(vertices),
        .label = "triangle-vertices",
    };

    g_app.m_DisplayPass.m_Bindings.vertex_buffers[0] = sg_make_buffer(&triangle_buffer_desc);

    sg_pipeline_desc display_pass_pipeline_desc = {
        .shader    = sg_make_shader(pbr_display_shader_desc(sg_query_backend())),
        .cull_mode = SG_CULLMODE_NONE,
        .label     = "pbr_pipeline"
    };

    display_pass_pipeline_desc.layout.attrs[ATTR_display_vs_position].format = SG_VERTEXFORMAT_FLOAT2;
    display_pass_pipeline_desc.layout.attrs[ATTR_display_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2;

    g_app.m_DisplayPass.m_Pipeline = sg_make_pipeline(&display_pass_pipeline_desc);
}

static void make_uniforms()
{
    vec3 eye, center, up;
    #define SET_VIEW_MATRIX(ix, cx, cy, cz, ux, uy, uz) \
        eye[0]    = 0.0f; eye[1]    = 0.0f; eye[2]    = 0.0f; \
        center[0] = cx;   center[1] = cy;   center[2] = cz; \
        up[0]     = ux;   up[1]     = uy;   up[2]     = uz; \
        mat4x4_look_at(g_app.m_CubeViewMatrices[ix], eye, center, up);

    SET_VIEW_MATRIX(0,  1.0f,  0.0f,  0.0f, 0.0f, -1.0f,  0.0f);
    SET_VIEW_MATRIX(1, -1.0f,  0.0f,  0.0f, 0.0f, -1.0f,  0.0f);
    SET_VIEW_MATRIX(2,  0.0f,  1.0f,  0.0f, 0.0f,  0.0f,  1.0f);
    SET_VIEW_MATRIX(3,  0.0f, -1.0f,  0.0f, 0.0f,  0.0f, -1.0f);
    SET_VIEW_MATRIX(4,  0.0f,  0.0f,  1.0f, 0.0f, -1.0f,  0.0f);
    SET_VIEW_MATRIX(5,  0.0f,  0.0f, -1.0f, 0.0f, -1.0f,  0.0f);

    #undef SET_VIEW_MATRIX
}

static void flip_image_y(void* pixels, int rows, int pitch)
{
    uint8_t* tmp_row = (uint8_t*) malloc(pitch * rows);
    memcpy(tmp_row, pixels, pitch * rows);
    for (int i = 0; i < rows; ++i)
    {
        uint8_t* write_ptr = (uint8_t*) pixels + pitch * i;
        memcpy(write_ptr, tmp_row + (rows-i-1) * pitch, pitch);
    }
    free(tmp_row);
}

void write_prefilter(int side, int mipmap)
{
#if defined(SOKOL_GLCORE33)
    uint32_t size        = 256 >> mipmap;
    uint32_t pixel_count = size * size * 4;
    uint8_t* pixels      = (uint8_t*) malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_PrefilterPass.m_Image, pixels, GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_UNSIGNED_BYTE, mipmap);

    flip_image_y(pixels, size, size * 4);

    char path_buffer[128];
    sprintf(path_buffer, "build/debug_prefilter_mm%d_side%d.png", mipmap, side);

    if (!stbi_write_png(path_buffer, size, size, 4, pixels, size * 4))
    {
        printf("Failed to write debug texture\n");
    }
    free(pixels);
#endif
}

void write_side(int side)
{
#if defined(SOKOL_GLCORE33)
    uint32_t pixel_count = 64 * 64 * 4;
    uint8_t* pixels = (uint8_t*) malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_DiffuseIrradiancePass.m_Image, pixels, GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, GL_UNSIGNED_BYTE, 0);

    char path_buffer[128];

    sprintf(path_buffer, "debug_%d.png", side);

    if (!stbi_write_png(path_buffer, 64, 64, 4, pixels, 64 * 4))
    {
        printf("Failed to write debug texture\n");
    }
    free(pixels);
#endif
}

void write_brdf_lut()
{
#if defined(SOKOL_GLCORE33)
    uint32_t pixel_count = 512 * 512 * 4;
    uint8_t* pixels = (uint8_t*) malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_BRDFLutPass.m_Image, pixels, GL_TEXTURE_2D, GL_UNSIGNED_BYTE, 0);

    if (!stbi_write_png("brdf_lut.png", 512, 512, 4, pixels, 512 * 4))
    {
        printf("Failed to write debug texture\n");
    }
    free(pixels);
#endif
}

void write_float_buffer(const char* output_path, float* data, uint32_t data_size)
{
    FILE* f = fopen(output_path, "wb");
    size_t bytes_written = fwrite(data, data_size, 1, f);
    fclose(f);
}

void generate_defold_image_buffer(const char* output_path, float* pixels, uint32_t pixel_count)
{
    /*
    sjson_context* json_ctx = sjson_create_context(0, 0, 0);
    sjson_node* json_root   = sjson_mkarray(json_ctx);
    sjson_node* json_stream = sjson_mkobject(json_ctx);

    sjson_append_element(json_root, json_stream);
    sjson_put_string(json_ctx, json_stream, "name",  "color");
    sjson_put_string(json_ctx, json_stream, "type",  "float32");
    sjson_put_int(json_ctx,    json_stream, "count", 4);
    sjson_put_floats(json_ctx, json_stream, "data",  pixels, pixel_count);

    char* json_src = sjson_encode(json_ctx, json_root);

    FILE* f = fopen(output_path, "wb");
    size_t bytes_written = fwrite(pixels, pixel_count * sizeof(float), 1, f);
    fclose(f);

    sjson_destroy_context(json_ctx);
    */
}

void write_output_data()
{
#if defined(SOKOL_GLCORE33)

    int gl_to_defold_side_mapping[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    // Generate diffuse irradiance buffers
    {
        const char* output_path_base = "build/irradiance.bin";
        printf("Writing irradiance images to %s*\n", output_path_base);

        // buffer for each individual side
        uint32_t data_size_side = g_app.m_DiffuseIrradiancePass.m_Size * g_app.m_DiffuseIrradiancePass.m_Size * 4 * sizeof(float);
        float* pixels_side      = (float*) malloc(data_size_side);

        // pixel buffer for entire cubemap
        uint32_t data_size = data_size_side * 6;
        float* pixels        = (float*) malloc(data_size);

        for (int side = 0; side < 6; ++side)
        {
            sg_query_image_pixels(g_app.m_DiffuseIrradiancePass.m_Image, pixels_side, gl_to_defold_side_mapping[side], GL_FLOAT, 0);
            flip_image_y(pixels_side, g_app.m_DiffuseIrradiancePass.m_Size, g_app.m_DiffuseIrradiancePass.m_Size * 4 * sizeof(float));

            uint8_t* write_ptr = ((uint8_t*) pixels) + side * data_size_side;
            memcpy(write_ptr, pixels_side, data_size_side);
        }

        write_float_buffer(output_path_base, pixels, data_size);

        free(pixels_side);
        free(pixels);
    }

    // Generate prefilter buffers
    {
        const char* output_path_base = "build/prefilter";
        printf("Writing prefilter images to %s*\n", output_path_base);

        // buffer for each individual side
        uint32_t data_size_side = g_app.m_PrefilterPass.m_Size * g_app.m_PrefilterPass.m_Size * 4 * sizeof(float);
        float* pixels_side      = (float*) malloc(data_size_side);

        uint32_t data_size = data_size_side * 6;
        float* pixels      = (float*) malloc(data_size);

        for (int mip = 0; mip < g_app.m_PrefilterPass.m_MipmapCount; ++mip)
        {
            uint32_t mipmap_size           = g_app.m_PrefilterPass.m_Size >> mip;
            uint32_t mipmap_data_size_side = mipmap_size * mipmap_size * 4 * sizeof(float);
            uint32_t mipmap_data_size      = mipmap_data_size_side * 6;

            for (int side = 0; side < 6; ++side)
            {
                sg_query_image_pixels(g_app.m_PrefilterPass.m_Image, pixels_side, gl_to_defold_side_mapping[side], GL_FLOAT, mip);
                flip_image_y(pixels_side, mipmap_size, mipmap_size * 4 * sizeof(float));

                uint8_t* write_ptr = ((uint8_t*) pixels) + side * mipmap_data_size_side;
                memcpy(write_ptr, pixels_side, mipmap_data_size_side);
            }

            char path_buffer[128];
            sprintf(path_buffer, "%s_mm_%d.bin", output_path_base, mip);
            write_float_buffer(path_buffer, pixels, mipmap_data_size);
        }

        free(pixels);
        free(pixels_side);
    }

    // Generate BRDF buffer
    {
        const char* output_path = "build/brdf_lut.bin";
        printf("Writing BRDF Lut to %s\n", output_path);

        uint32_t pixel_count    = g_app.m_BRDFLutPass.m_Size * g_app.m_BRDFLutPass.m_Size * 4;
        float* pixels           = (float*) malloc(pixel_count * sizeof(float));
        sg_query_image_pixels(g_app.m_BRDFLutPass.m_Image, pixels, GL_TEXTURE_2D, GL_FLOAT, 0);
        write_float_buffer(output_path, pixels, pixel_count * sizeof(float));
        free(pixels);
    }

    printf("Writing complete!\n");
#endif
}

void frame(void)
{
    mat4x4 projection;
    mat4x4_perspective(projection, 90 * (3.14159265359/180.0), 1.0f, 0.1f, 10.0f);
    cubemap_uniforms_t cubemap_uniforms = {};
    memcpy(&cubemap_uniforms.projection, projection, sizeof(mat4x4));

    if (!g_app.m_IsDone)
    {
        //////////////////////////////////////////////////////////////////////
        // Generate cubemap environment from environment map
        //////////////////////////////////////////////////////////////////////
        g_app.m_EnvironmentPass.m_Bindings.fs_images[SLOT_tex] = g_app.m_EnvironmentTexture.m_Image;
        for (int i = 0; i < 6; ++i)
        {
            memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

            sg_range cubemap_uniform_data = SG_RANGE(cubemap_uniforms);

            sg_begin_pass(g_app.m_EnvironmentPass.m_Pass[i], &g_app.m_EnvironmentPass.m_PassAction);
            sg_apply_pipeline(g_app.m_EnvironmentPass.m_Pipeline);
            sg_apply_bindings(&g_app.m_EnvironmentPass.m_Bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms, &cubemap_uniform_data);

            sg_draw(0, g_app.m_Cube.num_elements, 1);
            sg_end_pass();
        }

        sg_generate_mipmaps(g_app.m_EnvironmentPass.m_Image);

        //////////////////////////////////////////////////////////////////////
        // Diffuse irradiance pass
        //////////////////////////////////////////////////////////////////////
        g_app.m_DiffuseIrradiancePass.m_Bindings.fs_images[SLOT_env_map] = g_app.m_EnvironmentPass.m_Image;
        for (int i = 0; i < 6; ++i)
        {
            memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

            sg_range cubemap_uniform_data = SG_RANGE(cubemap_uniforms);

            sg_begin_pass(g_app.m_DiffuseIrradiancePass.m_Pass[i], &g_app.m_DiffuseIrradiancePass.m_PassAction);
            sg_apply_pipeline(g_app.m_DiffuseIrradiancePass.m_Pipeline);
            sg_apply_bindings(&g_app.m_DiffuseIrradiancePass.m_Bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms, &cubemap_uniform_data);

            sg_draw(0, g_app.m_Cube.num_elements, 1);
            sg_end_pass();
        }

        //////////////////////////////////////////////////////////////////////
        // BRDF Lookup table pass
        //////////////////////////////////////////////////////////////////////
        sg_begin_pass(g_app.m_BRDFLutPass.m_Pass, &g_app.m_BRDFLutPass.m_PassAction);
        sg_apply_pipeline(g_app.m_BRDFLutPass.m_Pipeline);
        sg_apply_bindings(&g_app.m_BRDFLutPass.m_Bindings);
        sg_draw(0, 6, 1);
        sg_end_pass();


        //////////////////////////////////////////////////////////////////////
        // Light prefilter pass
        //////////////////////////////////////////////////////////////////////
        prefilter_uniforms_t prefilter_uniforms = {};
        g_app.m_PrefilterPass.m_Bindings.fs_images[SLOT_tex_cube] = g_app.m_EnvironmentPass.m_Image;

        int pass_index = 0;
        int mipmap_size = g_app.m_PrefilterPass.m_Size;
        for (int mip = 0; mip < g_app.m_PrefilterPass.m_MipmapCount; ++mip)
        {
            prefilter_uniforms.roughness = (float) mip / (float) (g_app.m_PrefilterPass.m_MipmapCount-1);

            for (int i = 0; i < 6; ++i)
            {
                memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

                sg_range cubemap_uniform_data   = SG_RANGE(cubemap_uniforms);
                sg_range prefilter_uniform_data = SG_RANGE(prefilter_uniforms);

                sg_begin_pass(g_app.m_PrefilterPass.m_Pass[pass_index], &g_app.m_PrefilterPass.m_PassAction);

                sg_apply_viewport(0, 0, mipmap_size, mipmap_size, false);

                sg_apply_pipeline(g_app.m_PrefilterPass.m_Pipeline);
                sg_apply_bindings(&g_app.m_PrefilterPass.m_Bindings);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms,   &cubemap_uniform_data);
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_prefilter_uniforms, &prefilter_uniform_data);

                sg_draw(0, g_app.m_Cube.num_elements, 1);
                sg_end_pass();

                pass_index++;
            }

            mipmap_size /= 2;
        }

        //////////////////////////////////////////////////////////////////////
        // Finally, write output data from generation
        //////////////////////////////////////////////////////////////////////
        write_output_data();

    #if 0
        for (int mip = 0; mip < num_mipmaps; ++mip)
        {
            for (int i = 0; i < 6; ++i)
            {
                write_prefilter(i, mip);
            }
        }

        write_side(0);
        write_side(1);
        write_side(2);
        write_side(3);
        write_side(4);
        write_side(5);
        write_brdf_lut();
    #endif
    }

    // Display pass
    g_app.m_DisplayPass.m_Bindings.fs_images[SLOT_tex] = g_app.m_DiffuseIrradiancePass.m_Image;
    g_app.m_DisplayPass.m_Bindings.fs_images[SLOT_tex] = g_app.m_EnvironmentPass.m_Image;

    sg_begin_default_pass(&g_app.m_DisplayPass.m_PassAction, sapp_width(), sapp_height());
    sg_apply_pipeline(g_app.m_DisplayPass.m_Pipeline);
    sg_apply_bindings(&g_app.m_DisplayPass.m_Bindings);
    sg_draw(0, 6, 1);
    sg_end_pass();

    // Commit
    sg_commit();

    g_app.m_IsDone = 1;
}

void cleanup(void)
{
    sg_shutdown();
}

void init(void)
{
    sg_desc app_desc = {
        .pass_pool_size = 1024,
        .context        = sapp_sgcontext(),
    };

    sg_setup(&app_desc);

    make_display_pass();
    make_cube();
    make_environment_image();
    make_environment_pass();
    make_diffuse_irradiance_pass();
    make_prefilter_pass();
    make_brdf_lut_pass();
    make_uniforms();
}

sapp_desc sokol_main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        printf("At least one argument required\n");
        exit(-1);
    }

    g_app.m_EnvironmentTexture.m_InputPath = argv[1];

    return (sapp_desc)
    {
        .init_cb      = init,
        .frame_cb     = frame,
        .cleanup_cb   = cleanup,
        .width        = 960,
        .height       = 640,
        .window_title = "PBR Utils",
    };
}
