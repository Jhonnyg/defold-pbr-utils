
#include <math.h>
#include <stdint.h>

#include <dirent.h>
#include <errno.h>

#include "linmath.h"

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

// Logging
#define LOG_VERBOSE(...) if (g_app.m_Params.m_Verbose) printf("[VERBOSE]: "); printf(__VA_ARGS__);
#define LOG_INFO(...)                                  printf("[INFO]: ");    printf(__VA_ARGS__);
#define LOG_ERROR(...)                                 printf("[ERROR]: ");   printf(__VA_ARGS__);

// Windows needs an OpenGL loader for these extra functions
#if defined(_WIN32)
    typedef PROC (WINAPI * PFN_WGLGETPROCADDRESSPROC)(LPCSTR);
    typedef void (WINAPI * PFN_GLGETTEXIMAGEPROC)    (GLenum, GLint, GLenum, GLenum, void*);
    typedef void (WINAPI * PFN_GLGENERATEMIPMAPPROC) (GLenum);

    // OpenGL DLL functions
    static HINSTANCE g_opengl32_dll                      = 0;
    static PFN_WGLGETPROCADDRESSPROC g_wglGetProcAddress = 0;

    // OpenGL Function ptrs
    static PFN_GLGETTEXIMAGEPROC    glGetTexImage    = NULL;
    static PFN_GLGENERATEMIPMAPPROC glGenerateMipmap = NULL;

    // OpenGL Defines
    #define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif

// Error message numbers
static const int PARAMS_RESULT_OK                         =  1;
static const int PARAMS_RESULT_INCORRECT_INPUT            = -1;
static const int PARAMS_RESULT_INCORRECT_OUTPUT_DIRECTORY = -2;
static const int PARAMS_RESULT_SHOW_HELP                  = -3;
static const int PARAMS_RESULT_INVALID_GENERATION_MASK    = -4;

static const int GENERATE_NONE                     = 0;
static const int GENERATE_BRDF_LUT                 = 1;
static const int GENERATE_DIFFUSE_IRRADIANCE       = 2;
static const int GENERATE_PREFILTERED_ENVIRONMENT  = 4;
static const int GENERATE_ALL                      = GENERATE_BRDF_LUT | GENERATE_DIFFUSE_IRRADIANCE | GENERATE_PREFILTERED_ENVIRONMENT;

typedef struct
{
    sg_buffer vbuf;
    sg_buffer ibuf;
    int num_elements;
} mesh_t;

typedef struct
{
    float pos[3];
} vertex_t;

typedef struct
{
    const char* m_PathInput;
    const char* m_PathDirectory;
    int         m_GenerateMask;
    bool        m_GenerateMetaData;
    bool        m_Verbose;
    bool        m_Preview;
} app_params;

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
        sg_image    m_Image;
        int         m_Width;
        int         m_Height;
        int         m_MipmapCount;
    } m_EnvironmentTexture;

    app_params m_Params;

    uint8_t m_IsDone : 1;
} g_app = {};


// Extra hooks for sokol because there's some functions missing
static void sg_update_texture_filter(sg_image img_id, sg_filter min_filter, sg_filter mag_filter)
{
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
}

static void sg_query_image_pixels(sg_image img_id, void* pixels, int target, int data_type, int mipmap)
{
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);
    glGetTexImage(target, mipmap, GL_RGBA, data_type, pixels);
    _SG_GL_CHECK_ERROR();
    _sg_gl_cache_restore_texture_binding(0);
}

static void sg_generate_mipmaps(sg_image img_id)
{
    _sg_image_t* img = _sg_lookup_image(&_sg.pools, img_id.id);
    _sg_gl_cache_store_texture_binding(0);
    _sg_gl_cache_bind_texture(0, img->gl.target, img->gl.tex[img->cmn.active_slot]);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    img->cmn.num_mipmaps = 1 + floor(log2(fmax(img->cmn.width, img->cmn.height)));

    _SG_GL_CHECK_ERROR();
    _sg_gl_cache_restore_texture_binding(0);
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

static bool make_environment_image()
{
    /// Load image
    sg_pixel_format pixel_format;
    uint8_t* pixel_data;
    int x,y,ch;
    int pixel_size;

    const char* input_path = g_app.m_Params.m_PathInput;

    if (stbi_is_hdr(g_app.m_Params.m_PathInput))
    {
        pixel_data   = (uint8_t*) stbi_loadf(g_app.m_Params.m_PathInput, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA32F;
        pixel_size   = x * y * 4 * sizeof(float);
    }
    else
    {
        pixel_data   = (uint8_t*) stbi_load(g_app.m_Params.m_PathInput, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA8;
        pixel_size   = x * y * 4 * sizeof(uint8_t);
    }

    if (pixel_data == 0)
    {
        printf("Unable to load image from %s\n", g_app.m_Params.m_PathInput);
        return false;
    }

    if (pixel_format == SG_PIXELFORMAT_RGBA32F)
    {
        LOG_VERBOSE("Input environment: HDR\n");
    }
    else
    {
        LOG_VERBOSE("Input environment: RGBA8\n");
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
    return true;
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
    brdf_lut_pass_desc.color_attachments[0].image = g_app.m_BRDFLutPass.m_Image;
    brdf_lut_pass_desc.color_attachments[0].slice = 0;

    g_app.m_BRDFLutPass.m_Pass = sg_make_pass(&brdf_lut_pass_desc);

    g_app.m_BRDFLutPass.m_PassAction.colors[0].action  = SG_ACTION_CLEAR;
    g_app.m_BRDFLutPass.m_PassAction.colors[0].value.r = 0.0f;
    g_app.m_BRDFLutPass.m_PassAction.colors[0].value.g = 0.0f;
    g_app.m_BRDFLutPass.m_PassAction.colors[0].value.b = 0.0f;
    g_app.m_BRDFLutPass.m_PassAction.colors[0].value.a = 1.0f;

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

    g_app.m_PrefilterPass.m_PassAction.colors[0].action  = SG_ACTION_CLEAR;
    g_app.m_PrefilterPass.m_PassAction.colors[0].value.r = 0.0f;
    g_app.m_PrefilterPass.m_PassAction.colors[0].value.g = 0.0f;
    g_app.m_PrefilterPass.m_PassAction.colors[0].value.b = 0.0f;
    g_app.m_PrefilterPass.m_PassAction.colors[0].value.a = 1.0f;

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
            prefilter_pass_desc.color_attachments[0].image     = g_app.m_PrefilterPass.m_Image,
            prefilter_pass_desc.color_attachments[0].mip_level = mipmap,
            prefilter_pass_desc.color_attachments[0].slice     = i,

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
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0].action  = SG_ACTION_CLEAR;
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0].value.r = 0.25f;
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0].value.g = 0.25f;
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0].value.b = 0.25f;
    g_app.m_DiffuseIrradiancePass.m_PassAction.colors[0].value.a = 1.0f;

    sg_image_desc diffuse_irridance_img_desc = {
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
        diffuse_irradiance_pass_desc.color_attachments[0].image = g_app.m_DiffuseIrradiancePass.m_Image;
        diffuse_irradiance_pass_desc.color_attachments[0].slice = i;

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
    g_app.m_EnvironmentPass.m_Size                         = 1024;
    g_app.m_EnvironmentPass.m_PassAction.colors[0].action  = SG_ACTION_CLEAR;
    g_app.m_EnvironmentPass.m_PassAction.colors[0].value.r = 0.25f;
    g_app.m_EnvironmentPass.m_PassAction.colors[0].value.g = 0.25f;
    g_app.m_EnvironmentPass.m_PassAction.colors[0].value.b = 0.25f;
    g_app.m_EnvironmentPass.m_PassAction.colors[0].value.a = 1.0f;

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
        environment_pass_desc.color_attachments[0].image = g_app.m_EnvironmentPass.m_Image;
        environment_pass_desc.color_attachments[0].slice = i;

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
    g_app.m_DisplayPass.m_PassAction.colors[0].action = SG_ACTION_CLEAR;
    g_app.m_DisplayPass.m_PassAction.colors[0].value.r = 0.0f;
    g_app.m_DisplayPass.m_PassAction.colors[0].value.g = 0.0f;
    g_app.m_DisplayPass.m_PassAction.colors[0].value.b = 0.0f;
    g_app.m_DisplayPass.m_PassAction.colors[0].value.a = 1.0f;

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
}

void write_side(int side)
{
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
}

void write_brdf_lut()
{
    uint32_t pixel_count = 512 * 512 * 4;
    uint8_t* pixels = (uint8_t*) malloc(pixel_count * sizeof(uint8_t));

    sg_query_image_pixels(g_app.m_BRDFLutPass.m_Image, pixels, GL_TEXTURE_2D, GL_UNSIGNED_BYTE, 0);

    if (!stbi_write_png("brdf_lut.png", 512, 512, 4, pixels, 512 * 4))
    {
        printf("Failed to write debug texture\n");
    }
    free(pixels);
}

static void write_bytes_to_file(const char* output_path, uint8_t* data, uint32_t data_size)
{
    FILE* f              = fopen(output_path, "wb");
    size_t bytes_written = fwrite(data, data_size, 1, f);
    fclose(f);
}


///////////////////////////////////////////////////////////////////////////////////////////////
// From: https://stackoverflow.com/questions/1659440/32-bit-to-16-bit-floating-point-conversion
typedef uint16_t ushort;
typedef uint32_t uint;

uint as_uint(const float x) {
    return *(uint*)&x;
}
float as_float(const uint x) {
    return *(float*)&x;
}

float half_to_float(const ushort x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint e = (x&0x7C00)>>10; // exponent
    const uint m = (x&0x03FF)<<13; // mantissa
    const uint v = as_uint((float)m)>>23; // evil log2 bit hack to count leading zeros in denormalized format
    return as_float((x&0x8000)<<16 | (e!=0)*((e+112)<<23|m) | ((e==0)&(m!=0))*((v-37)<<23|((m<<(150-v))&0x007FE000))); // sign : normalized : denormalized
}
ushort float_to_half(const float x) { // IEEE-754 16-bit floating-point format (without infinity): 1-5-10, exp-15, +-131008.0, +-6.1035156E-5, +-5.9604645E-8, 3.311 digits
    const uint b = as_uint(x)+0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
    const uint e = (b&0x7F800000)>>23; // exponent
    const uint m = b&0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
    return (b&0x80000000)>>16 | (e>112)*((((e-112)<<10)&0x7C00)|m>>13) | ((e<113)&(e>101))*((((0x007FF000+m)>>(125-e))+1)>>1) | (e>143)*0x7FFF; // sign : normalized : denormalized : saturate
}
///////////////////////////////////////////////////////////////////////////////////////////////

static void float32_to_float16(float* floats_in, uint32_t num_floats_in, uint16_t* half_floats_out)
{
    for (int i = 0; i < num_floats_in; ++i)
    {
        half_floats_out[i] = float_to_half(floats_in[i]);
    }
}

static void fill_base_name(const char* file_path, char* buf)
{
    size_t path_len = strlen(file_path);
    uint32_t last_slash = 0;

    for (int i = path_len; i >= 0; --i)
    {
        if (file_path[i] == '/')
        {
            last_slash = i;
            break;
        }
    }

    const char* ptr = file_path + last_slash + 1;
    memcpy(buf, ptr, path_len - last_slash);
    buf[path_len] = 0;
}

static void fill_base_directory(const char* directory_path, char* buf)
{
    uint32_t start_copy = 0;
    size_t path_len = strlen(directory_path);

    if (directory_path[0] == '/')
    {
        start_copy++;
    }

    buf[0]        = '/';
    buf[path_len] = 0;

    memcpy(buf + 1, directory_path + start_copy, path_len - start_copy);
}

void write_meta_data(const char* output_path)
{
    FILE* f = fopen(output_path, "wb");

    const char* meta_data_template =
        "return\n"
        "{\n"
        "    name            = \"%s\",\n"
        "    path            = \"%s\",\n"
        "    irradiance_size = %d,\n"
        "    prefilter_size  = %d,\n"
        "    brdf_lut_size   = %d,\n"
        "}\n";

    char name_buffer[256];
    memset(name_buffer, 0, sizeof(name_buffer));
    fill_base_name(g_app.m_Params.m_PathInput, name_buffer);

    char path_buffer[256];
    memset(path_buffer, 0, sizeof(path_buffer));
    fill_base_directory(g_app.m_Params.m_PathDirectory, path_buffer);

    char data_buffer[512];
    memset(data_buffer, 0, sizeof(data_buffer));
    sprintf(data_buffer, meta_data_template, name_buffer, path_buffer,
        g_app.m_DiffuseIrradiancePass.m_Size,
        g_app.m_PrefilterPass.m_Size,
        g_app.m_BRDFLutPass.m_Size);

    fwrite(data_buffer, strlen(data_buffer), 1, f);

    fclose(f);
}

void write_output_data()
{
    int gl_to_defold_side_mapping[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
    };

    // Generate diffuse irradiance buffers
    if (g_app.m_Params.m_GenerateMask & GENERATE_DIFFUSE_IRRADIANCE)
    {
        char output_path_irridance[256];
        sprintf(output_path_irridance, "%s/irradiance.bin", g_app.m_Params.m_PathDirectory);

        LOG_INFO("Writing irradiance images to %s* with type (float16)\n", output_path_irridance);

        // TODO: Output type should be configurable by arguments

        // buffer for each individual side
        uint32_t data_size_side = g_app.m_DiffuseIrradiancePass.m_Size * g_app.m_DiffuseIrradiancePass.m_Size * 4 * sizeof(float);
        float* pixels_side      = (float*) malloc(data_size_side);

        // pixel buffer for entire cubemap
        uint32_t data_size = data_size_side * 6;
        float* pixels      = (float*) malloc(data_size);

        for (int side = 0; side < 6; ++side)
        {
            sg_query_image_pixels(g_app.m_DiffuseIrradiancePass.m_Image, pixels_side, gl_to_defold_side_mapping[side], GL_FLOAT, 0);
            flip_image_y(pixels_side, g_app.m_DiffuseIrradiancePass.m_Size, g_app.m_DiffuseIrradiancePass.m_Size * 4 * sizeof(float));

            uint8_t* write_ptr = ((uint8_t*) pixels) + side * data_size_side;
            memcpy(write_ptr, pixels_side, data_size_side);
        }

        uint32_t half_float_buffer_data_size = data_size / 2;
        uint16_t* half_float_buffer          = (uint16_t*) malloc(data_size / 2);

        float32_to_float16(pixels, data_size / sizeof(float), half_float_buffer);
        write_bytes_to_file(output_path_irridance, (uint8_t*) half_float_buffer, half_float_buffer_data_size);

        free(pixels_side);
        free(pixels);
        free(half_float_buffer);
    }

    // Generate prefilter buffers
    if (g_app.m_Params.m_GenerateMask & GENERATE_PREFILTERED_ENVIRONMENT)
    {
        char output_path_prefiter_base[256];
        sprintf(output_path_prefiter_base, "%s/prefilter", g_app.m_Params.m_PathDirectory);

        LOG_INFO("Writing prefilter images to %s*\n", output_path_prefiter_base);

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

            char output_path_prefite_slice[128];
            sprintf(output_path_prefite_slice, "%s_mm_%d.bin", output_path_prefiter_base, mip);

            uint32_t half_float_buffer_data_size = mipmap_data_size / 2;
            uint16_t* half_float_buffer          = (uint16_t*) malloc(mipmap_data_size / 2);

            float32_to_float16(pixels, mipmap_data_size / sizeof(float), half_float_buffer);

            write_bytes_to_file(output_path_prefite_slice, (uint8_t*) half_float_buffer, half_float_buffer_data_size);

            free(half_float_buffer);
        }

        free(pixels);
        free(pixels_side);
    }

    // Generate BRDF buffer
    if (g_app.m_Params.m_GenerateMask & GENERATE_BRDF_LUT)
    {
        char output_path_brdf_lut[256];
        sprintf(output_path_brdf_lut, "%s/brdf_lut.bin", g_app.m_Params.m_PathDirectory);
        LOG_INFO("Writing BRDF Lut to %s\n", output_path_brdf_lut);

        uint32_t pixel_count    = g_app.m_BRDFLutPass.m_Size * g_app.m_BRDFLutPass.m_Size * 4;
        uint32_t data_size      = pixel_count * sizeof(float);
        float* pixels           = (float*) malloc(data_size);
        sg_query_image_pixels(g_app.m_BRDFLutPass.m_Image, pixels, GL_TEXTURE_2D, GL_FLOAT, 0);

        uint32_t half_float_buffer_data_size = data_size / 2;
        uint16_t* half_float_buffer          = (uint16_t*) malloc(half_float_buffer_data_size);

        float32_to_float16(pixels, pixel_count, half_float_buffer);

        write_bytes_to_file(output_path_brdf_lut, (uint8_t*) half_float_buffer, half_float_buffer_data_size);

        free(half_float_buffer);
        free(pixels);
    }

    // Generate lua table
    if (g_app.m_Params.m_GenerateMetaData)
    {
        char output_path_meta_data[256];
        sprintf(output_path_meta_data, "%s/meta.lua", g_app.m_Params.m_PathDirectory);
        write_meta_data(output_path_meta_data);
    }

    LOG_VERBOSE("Writing complete!\n");
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

        if (g_app.m_Params.m_GenerateMask & GENERATE_DIFFUSE_IRRADIANCE)
        {
            LOG_INFO("Generating diffuse irradiance\n");
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
        }

        //////////////////////////////////////////////////////////////////////
        // BRDF Lookup table pass
        //////////////////////////////////////////////////////////////////////
        if (g_app.m_Params.m_GenerateMask & GENERATE_BRDF_LUT)
        {
            LOG_INFO("Generating BRDF Lut\n");
            sg_begin_pass(g_app.m_BRDFLutPass.m_Pass, &g_app.m_BRDFLutPass.m_PassAction);
            sg_apply_pipeline(g_app.m_BRDFLutPass.m_Pipeline);
            sg_apply_bindings(&g_app.m_BRDFLutPass.m_Bindings);
            sg_draw(0, 6, 1);
            sg_end_pass();
        }

        //////////////////////////////////////////////////////////////////////
        // Light prefilter pass
        //////////////////////////////////////////////////////////////////////
        if (g_app.m_Params.m_GenerateMask & GENERATE_PREFILTERED_ENVIRONMENT)
        {
            LOG_INFO("Generating prefiltered environment\n");

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
        }

        //////////////////////////////////////////////////////////////////////
        // Finally, write output data from generation
        //////////////////////////////////////////////////////////////////////
        write_output_data();

        LOG_INFO("Finished generating!\n");

    #if 0
        /*
        for (int mip = 0; mip < num_mipmaps; ++mip)
        {
            for (int i = 0; i < 6; ++i)
            {
                write_prefilter(i, mip);
            }
        }
        */

        write_side(0);
        write_side(1);
        write_side(2);
        write_side(3);
        write_side(4);
        write_side(5);
        write_brdf_lut();
    #endif
    }

    if (!g_app.m_Params.m_Preview)
    {
        exit(0);
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

void cleanup_platform()
{
#ifdef _WIN32
    FreeLibrary(g_opengl32_dll);
#endif
}

void cleanup(void)
{
    cleanup_platform();
    sg_shutdown();
}

bool init_platform()
{
#if defined(_WIN32)

    #define GET_PROC_ADDRESS(function, name, type) \
        function = (type) g_wglGetProcAddress(name);\
        if (function == 0x0)\
        {\
            function = (type) g_wglGetProcAddress(name "ARB");\
        }\
        if (function == 0x0)\
        {\
            function = (type) g_wglGetProcAddress(name "EXT");\
        }\
        if (function == 0x0)\
        {\
            function = (type) GetProcAddress(g_opengl32_dll, name);\
        }\
        if (function == 0x0)\
        {\
            printf("Could not find gl function '%s'.\n", name);\
        }

    g_opengl32_dll      = LoadLibraryA("opengl32.dll");
    g_wglGetProcAddress = (PFN_WGLGETPROCADDRESSPROC) GetProcAddress(g_opengl32_dll, "wglGetProcAddress");

    GET_PROC_ADDRESS(glGetTexImage,    "glGetTexImage",    PFN_GLGETTEXIMAGEPROC);
    GET_PROC_ADDRESS(glGenerateMipmap, "glGenerateMipmap", PFN_GLGENERATEMIPMAPPROC);
    #undef GET_PROC_ADDRESS

    return glGetTexImage != 0x0 && glGenerateMipmap != 0x0;
#else
    return true;
#endif
}

void init(void)
{
    sg_desc app_desc = {
        .pass_pool_size = 1024,
        .context        = sapp_sgcontext(),
    };

    sg_setup(&app_desc);

    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    if (init_platform())
    {
        make_display_pass();
        make_cube();
        if (!make_environment_image())
        {
            exit(-1);
        }
        make_environment_pass();
        make_diffuse_irradiance_pass();
        make_prefilter_pass();
        make_brdf_lut_pass();
        make_uniforms();
    }
    else
    {
        printf("Unable to initialize, check console for errors!\n");
    }
}

app_params get_default_app_params()
{
    app_params params;
    params.m_Verbose       = false;
    params.m_PathInput     = NULL; // required
    params.m_PathDirectory = NULL; // required
    params.m_GenerateMask  = GENERATE_ALL;

    return params;
}

void generation_mask_to_str(int mask_val, char* mask)
{
    if (mask_val == GENERATE_ALL)
    {
        sprintf(mask, "all");
        return;
    }

    char* _mask = mask;

#define WRITE_MASK(name) \
    char* mask_str = name " "; \
    size_t mask_len = strlen(mask_str); \
    memcpy(_mask, mask_str, mask_len); \
    _mask += mask_len;

    if (mask_val & GENERATE_BRDF_LUT)
    {
        WRITE_MASK("brdf");
    }
    if (mask_val & GENERATE_DIFFUSE_IRRADIANCE)
    {
        WRITE_MASK("irradiance");
    }
    if (mask_val & GENERATE_PREFILTERED_ENVIRONMENT)
    {
        WRITE_MASK("prefilter");
    }
#undef WRITE_MASK

    mask[_mask - mask] = 0;
}

void print_app_params(app_params params)
{
    if (!params.m_Verbose)
    {
        return;
    }

    char mask_str[128];
    generation_mask_to_str(params.m_GenerateMask, mask_str);

#define TRUE_FALSE_LABEL(cond) (cond?"TRUE":"FALSE")
    printf("----------- Configuration -----------\n");
    printf("Input path         : %s\n", params.m_PathInput);
    printf("Output directory   : %s\n", params.m_PathDirectory);
    printf("Generate           : %s\n", mask_str);
    printf("Generate meta-data : %s\n", TRUE_FALSE_LABEL(params.m_GenerateMetaData));
    printf("Preview            : %s\n", TRUE_FALSE_LABEL(params.m_Preview));
    printf("-------------------------------------\n");
#undef TRUE_FALSE_LABEL
}

void show_usage()
{
    printf("--------------- Help ---------------\n");
    printf("Usage: pbr-utils <input-file> <output-file> [options]\n");
    printf("Options:\n");
    printf("  --generate <value> : What to generate (can be multiple), where value is:\n");
    printf("      all            : Generate BRDF lut, diffuse irradiance, prefiltered environment (default)\n");
    printf("      brdf           : Generate BRDF lut map\n");
    printf("      irradiance     : Generate diffuse irradiance map\n");
    printf("      prefilter      : Generate prefiltered environment map\n");
    printf("  --meta-data        : Generate meta-data about generation (in lua format)\n");
    printf("  --verbose          : Enable verbose logging\n");
    printf("  --preview          : Enable preview rendering\n");
    printf("  --help             : Show this help screen\n");
    printf("-------------------------------------\n");
}

int str_case_cmp(const char *s1, const char *s2)
{
#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

bool is_app_arg(const char* arg)
{
    size_t str_len = strlen(arg);
    return str_len > 2 && arg[0] == '-' && arg[1] == '-';
}

bool directory_exists(const char* path)
{
    DIR* dir = opendir(path);
    if (dir)
    {
        closedir(dir);
        return true;
    }
    return false;
}

int validate_app_arguments(app_params* params)
{
    if (!params->m_PathInput || is_app_arg(params->m_PathInput))
    {
        return PARAMS_RESULT_INCORRECT_INPUT;
    }
    else if (!params->m_PathDirectory || is_app_arg(params->m_PathDirectory) || !directory_exists(params->m_PathDirectory))
    {
        return PARAMS_RESULT_INCORRECT_OUTPUT_DIRECTORY;
    }

    return PARAMS_RESULT_OK;
}

int parse_arguments(int argc, char* argv[], app_params* params)
{
    params->m_PathInput     = argc > 1 ? argv[1] : 0;
    params->m_PathDirectory = argc > 2 ? argv[2] : 0;

    int generation_mask = GENERATE_NONE;

    for (int i = 2; i < argc; ++i)
    {
        if (is_app_arg(argv[i]))
        {
            #define CMP_ARG(name)      (str_case_cmp(argv[i] + 2, name) == 0)
            #define CMP_VAL(name)      (str_case_cmp(argv[i],     name) == 0)
            #define CMP_ARG_1_OP(name) (CMP_ARG(name) && (i+1) < argc)

            if (CMP_ARG("verbose"))
            {
                params->m_Verbose = 1;
            }
            else if (CMP_ARG("help"))
            {
                show_usage();
                return PARAMS_RESULT_SHOW_HELP;
            }
            else if (CMP_ARG("preview"))
            {
                params->m_Preview = true;
            }
            else if (CMP_ARG("meta-data"))
            {
                params->m_GenerateMetaData = true;
            }
            else if (CMP_ARG_1_OP("generate"))
            {
                i++;
                if (CMP_VAL("brdf"))
                {
                    generation_mask |= GENERATE_BRDF_LUT;
                }
                else if (CMP_VAL("prefilter"))
                {
                    generation_mask |= GENERATE_PREFILTERED_ENVIRONMENT;
                }
                else if (CMP_VAL("irradiance"))
                {
                    generation_mask |= GENERATE_DIFFUSE_IRRADIANCE;
                }
            }
            else
            {
                LOG_INFO("Argument '%s' is unsupported", argv[i]);
            }

            #undef CMP_ARG_1_OP
            #undef CMP_ARG
        }
    }

    if (generation_mask != GENERATE_NONE)
    {
        params->m_GenerateMask = generation_mask;
    }

    return validate_app_arguments(params);
}

void handle_parse_result(int res, app_params* params)
{
#define GET_STRING_OR_NULL(str) (str ? str : "<null>")

    if (res == PARAMS_RESULT_OK)
    {
        return;
    }

    printf("ERROR! ");
    if (res == PARAMS_RESULT_INCORRECT_INPUT)
    {
        printf("Incorrect input passed (arg=1) '%s'\n", GET_STRING_OR_NULL(params->m_PathInput));
    }
    else if (res == PARAMS_RESULT_INCORRECT_OUTPUT_DIRECTORY)
    {
        printf("Incorrect directory passed (arg=2) '%s'\n", GET_STRING_OR_NULL(params->m_PathDirectory));
    }

    show_usage();

    exit(-1);
#undef GET_STRING_OR_NULL
}

sapp_desc sokol_main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        LOG_ERROR("At least one argument required\n");
        exit(-1);
    }

    g_app.m_Params = get_default_app_params();

    handle_parse_result(parse_arguments(argc, argv, &g_app.m_Params), &g_app.m_Params);

    print_app_params(g_app.m_Params);

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
