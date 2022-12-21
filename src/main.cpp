
#include <math.h>

#include "linmath.h"

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

#include "shaders.glsl.h"

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
    } m_EnvironmentPass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass[6];
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
    } m_DiffuseIrradiancePass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass*       m_Pass;
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
    } m_PrefilterPass;

    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass;
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
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

void sg_update_texture_filter(sg_image img_id, sg_filter min_filter, sg_filter mag_filter)
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

static void sg_query_image_pixels(sg_image img_id, void* pixels, int size, int type, int mipmap)
{
#if defined(SOKOL_GLCORE33)
    SOKOL_ASSERT(pixels);
    SOKOL_ASSERT(img_id.id != SG_INVALID_ID);
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    SOKOL_ASSERT(img);
    //SOKOL_ASSERT(size >= (img->cmn.width * img->cmn.height * 4));
    _SOKOL_UNUSED(size);
    SOKOL_ASSERT(0 != img->gl.tex[img->cmn.active_slot]);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);
    glGetTexImage(type, mipmap, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
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

void make_cube()
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
    mesh_t cube = {
        .vbuf = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(vertices),
            .label = "cube-vertices"
        }),
        .num_elements = 6 * 6,
    };

    g_app.m_Cube = cube;
}

void make_environment_image()
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
    }
    else
    {
        pixel_data   = (uint8_t*) stbi_load(g_app.m_EnvironmentTexture.m_InputPath, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA8;
        pixel_size   = x * y * 4 * sizeof(uint8_t);
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

void make_brdf_lut_pass()
{
    g_app.m_BRDFLutPass.m_Image = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = 512,
        .height        = 512,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_CLAMP_TO_EDGE,
        .wrap_v        = SG_WRAP_CLAMP_TO_EDGE,
        .label         = "color-image"
    });

    sg_image depth_img = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = 512,
        .height        = 512,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .label         = "cubemap-depth-rt"
    });

    g_app.m_BRDFLutPass.m_Pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0] = {
            .image = g_app.m_BRDFLutPass.m_Image,
            .slice = 0
        },
        .depth_stencil_attachment.image = depth_img,
        .label                          = "offscreen-pass"
    });

    g_app.m_BRDFLutPass.m_PassAction = (sg_pass_action) {
        .colors[0] = {
            .action=SG_ACTION_CLEAR,
            .value={0.0f, 0.0f, 0.0f, 1.0f}
        }
    };

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };

    g_app.m_BRDFLutPass.m_Bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "triangle-vertices",
    });

    g_app.m_BRDFLutPass.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_brdf_lut_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_brdf_lut_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_brdf_lut_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    });
}

void make_prefilter_pass()
{
    g_app.m_PrefilterPass.m_PassAction = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    uint8_t num_mipmaps = 1 + floor(log2(256));

    g_app.m_PrefilterPass.m_Image = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = 256,
        .height        = 256,
        .num_mipmaps   = num_mipmaps,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .label         = "color-image"
    });

    g_app.m_PrefilterPass.m_Pass = (sg_pass*) malloc(sizeof(sg_pass) * num_mipmaps * 6);

    int pass_index = 0;
    for (int mipmap = 0; mipmap < num_mipmaps; ++mipmap)
    {
        sg_image depth_img = sg_make_image(&(sg_image_desc) {
            .type          = SG_IMAGETYPE_2D,
            .render_target = true,
            .width         = 256 >> mipmap,
            .height        = 256 >> mipmap,
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .label         = "cubemap-depth-rt"
        });

        for (int i = 0; i < 6; ++i)
        {
            g_app.m_PrefilterPass.m_Pass[pass_index] = sg_make_pass(&(sg_pass_desc){
                .color_attachments[0] = {
                    .image     = g_app.m_PrefilterPass.m_Image,
                    .slice     = i,
                    .mip_level = mipmap,
                },
                .depth_stencil_attachment.image = depth_img,
                .label                          = "offscreen-pass"
            });

            pass_index++;
        }
    }

    g_app.m_PrefilterPass.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_prefilter_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
            },
        },
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    });

    g_app.m_PrefilterPass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

void make_diffuse_irradiance_pass()
{
    g_app.m_DiffuseIrradiancePass.m_PassAction = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    g_app.m_DiffuseIrradiancePass.m_Image = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = 64,
        .height        = 64,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .label         = "color-image"
    });

    sg_image depth_img = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = 64,
        .height        = 64,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .label         = "cubemap-depth-rt"
    });

    for (int i = 0; i < 6; ++i)
    {
        g_app.m_DiffuseIrradiancePass.m_Pass[i] = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0] = {
                .image = g_app.m_DiffuseIrradiancePass.m_Image,
                .slice = i
            },
            .depth_stencil_attachment.image = depth_img,
            .label                          = "offscreen-pass"
        });
    }

    g_app.m_DiffuseIrradiancePass.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_diffuse_irradiance_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
            },
        },
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    });

    g_app.m_DiffuseIrradiancePass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

void make_environment_pass()
{
    g_app.m_EnvironmentPass.m_PassAction = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    g_app.m_EnvironmentPass.m_Image = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = 1024, // g_app.m_EnvironmentTexture.m_Width,
        .height        = 1024, // g_app.m_EnvironmentTexture.m_Height,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR_MIPMAP_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .sample_count  = 4,
        .label         = "color-image"
    });

    sg_image depth_img = sg_make_image(&(sg_image_desc) {
        .type          = SG_IMAGETYPE_2D,
        .render_target = true,
        .width         = 1024,
        .height        = 1024,
        .pixel_format  = SG_PIXELFORMAT_DEPTH,
        .sample_count  = 4,
        .label         = "cubemap-depth-rt"
    });

    for (int i = 0; i < 6; ++i)
    {
        g_app.m_EnvironmentPass.m_Pass[i] = sg_make_pass(&(sg_pass_desc) {
            .color_attachments[0] = {
                .image = g_app.m_EnvironmentPass.m_Image,
                .slice = i
            },
            .depth_stencil_attachment.image = depth_img,
            .label                          = "offscreen-pass"
        });
    }

    g_app.m_EnvironmentPass.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc) {
        .shader = sg_make_shader(pbr_shader_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
            },
        },
        .depth = {
            .pixel_format  = SG_PIXELFORMAT_DEPTH,
            .compare       = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count           = 4,
        .cull_mode              = SG_CULLMODE_NONE,
        .label                  = "pipeline_fullscreen"
    });

    g_app.m_EnvironmentPass.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
}

void make_display_pass(void)
{
    g_app.m_DisplayPass.m_PassAction = (sg_pass_action) {
        .colors[0] = {
            .action=SG_ACTION_CLEAR,
            .value={0.0f, 0.0f, 0.0f, 1.0f}
        }
    };

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };

    g_app.m_DisplayPass.m_Bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "triangle-vertices",
    });

    g_app.m_DisplayPass.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_display_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_display_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_display_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .cull_mode = SG_CULLMODE_NONE,
        .label = "pbr_pipeline"
    });
}

void make_uniforms()
{
    vec3 eye, center, up;
    #define SET_VIEW_MATRIX(ix, cx, cy, cz, ux, uy, uz) \
        eye[0] = 0.0f; eye[1] = 0.0f; eye[2] = 0.0f; \
        center[0] = cx; center[1] = cy; center[2] = cz; \
        up[0] = ux; up[1] = uy; up[2] = uz; \
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
    uint8_t* tmp_row = malloc(pitch * rows);
    memcpy(tmp_row, pixels, pitch * rows);
    for (int i = 0; i < rows; ++i)
    {
        memcpy(pixels + pitch * i, tmp_row + (rows-i-1) * pitch, pitch);
    }
    free(tmp_row);
}

void write_prefilter(int side, int mipmap)
{
#if defined(SOKOL_GLCORE33)
    uint32_t size        = 256 >> mipmap;
    uint32_t pixel_count = size * size * 4;
    uint8_t* pixels      = malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_PrefilterPass.m_Image, pixels, pixel_count, GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, mipmap);

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
    uint8_t* pixels = malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_DiffuseIrradiancePass.m_Image, pixels, pixel_count, GL_TEXTURE_CUBE_MAP_POSITIVE_X + side, 0);

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
    uint8_t* pixels = malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_BRDFLutPass.m_Image, pixels, pixel_count, GL_TEXTURE_2D, 0);

    if (!stbi_write_png("brdf_lut.png", 512, 512, 4, pixels, 512 * 4))
    {
        printf("Failed to write debug texture\n");
    }
    free(pixels);
#endif
}

void frame(void)
{
    mat4x4 projection;
#define DEG_TO_RAD(d) (d * (3.14159265359/180.0))
    mat4x4_perspective(projection, DEG_TO_RAD(90), 1.0f, 0.1f, 10.0f);
    cubemap_uniforms_t cubemap_uniforms = {};
    memcpy(&cubemap_uniforms.projection, projection, sizeof(mat4x4));
#undef DEG_TO_RAD

    if (!g_app.m_IsDone)
    {
        g_app.m_EnvironmentPass.m_Bindings.fs_images[SLOT_tex] = g_app.m_EnvironmentTexture.m_Image;

        // Generate cubemap sides
        for (int i = 0; i < 6; ++i)
        {
            memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

            sg_begin_pass(g_app.m_EnvironmentPass.m_Pass[i], &g_app.m_EnvironmentPass.m_PassAction);
            sg_apply_pipeline(g_app.m_EnvironmentPass.m_Pipeline);
            sg_apply_bindings(&g_app.m_EnvironmentPass.m_Bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms, &SG_RANGE(cubemap_uniforms));

            sg_draw(0, g_app.m_Cube.num_elements, 1);
            sg_end_pass();
        }

        sg_generate_mipmaps(g_app.m_EnvironmentPass.m_Image);

        g_app.m_DiffuseIrradiancePass.m_Bindings.fs_images[SLOT_env_map] = g_app.m_EnvironmentPass.m_Image;

        // Generate cubemap sides
        for (int i = 0; i < 6; ++i)
        {
            memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

            sg_begin_pass(g_app.m_DiffuseIrradiancePass.m_Pass[i], &g_app.m_DiffuseIrradiancePass.m_PassAction);
            sg_apply_pipeline(g_app.m_DiffuseIrradiancePass.m_Pipeline);
            sg_apply_bindings(&g_app.m_DiffuseIrradiancePass.m_Bindings);
            sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms, &SG_RANGE(cubemap_uniforms));

            sg_draw(0, g_app.m_Cube.num_elements, 1);
            sg_end_pass();
        }

        // BRDF Lut pass
        sg_begin_pass(g_app.m_BRDFLutPass.m_Pass, &g_app.m_BRDFLutPass.m_PassAction);
        sg_apply_pipeline(g_app.m_BRDFLutPass.m_Pipeline);
        sg_apply_bindings(&g_app.m_BRDFLutPass.m_Bindings);
        sg_draw(0, 6, 1);
        sg_end_pass();

        prefilter_uniforms_t prefilter_uniforms = {};

        g_app.m_PrefilterPass.m_Bindings.fs_images[SLOT_tex_cube] = g_app.m_EnvironmentPass.m_Image;

        uint8_t num_mipmaps = 1 + floor(log2(256));
        int pass_index = 0;
        for (int mip = 0; mip < num_mipmaps; ++mip)
        {
            prefilter_uniforms.roughness = (float) mip / (float) (num_mipmaps-1);

            for (int i = 0; i < 6; ++i)
            {
                memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

                sg_begin_pass(g_app.m_PrefilterPass.m_Pass[pass_index], &g_app.m_PrefilterPass.m_PassAction);
                sg_apply_pipeline(g_app.m_PrefilterPass.m_Pipeline);
                sg_apply_bindings(&g_app.m_PrefilterPass.m_Bindings);
                sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms,   &SG_RANGE(cubemap_uniforms));
                sg_apply_uniforms(SG_SHADERSTAGE_FS, SLOT_prefilter_uniforms, &SG_RANGE(prefilter_uniforms));

                sg_draw(0, g_app.m_Cube.num_elements, 1);
                sg_end_pass();

                pass_index++;
            }
        }

        for (int mip = 0; mip < num_mipmaps; ++mip)
        {
            for (int i = 0; i < 6; ++i)
            {
                write_prefilter(i, mip);
            }
        }

    #if 0
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
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext(),
        .pass_pool_size = 1024,
    });

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
