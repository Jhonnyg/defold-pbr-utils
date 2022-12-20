
#include "linmath.h"

#define SOKOL_METAL
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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
    } m_Offscreen;

    sg_pass_action m_PassAction;
    sg_pipeline    m_Pipeline;
    sg_bindings    m_Bindings;

    mesh_t m_Cube;
    mat4x4 m_CubeViewMatrices[6];

    struct
    {
        int      m_Width;
        int      m_Height;
        sg_image m_Image;
    } m_Texture;
    const char*    m_InputPath;
    uint8_t        m_IsDone : 1;
} g_app = {};

void make_cube()
{
    vertex_t vertices[] =  {
        { { -1.0, -1.0, -1.0 } }, // { 0.0, 0.0, -1.0 } },
        { {  1.0, -1.0, -1.0 } }, // { 0.0, 0.0, -1.0 } },
        { {  1.0,  1.0, -1.0 } }, // { 0.0, 0.0, -1.0 } },
        { { -1.0,  1.0, -1.0 } }, // { 0.0, 0.0, -1.0 } },

        { { -1.0, -1.0,  1.0 } }, // { 0.0, 0.0, 1.0 } },
        { {  1.0, -1.0,  1.0 } }, // { 0.0, 0.0, 1.0 } },
        { {  1.0,  1.0,  1.0 } }, // { 0.0, 0.0, 1.0 } },
        { { -1.0,  1.0,  1.0 } }, // { 0.0, 0.0, 1.0 } },

        { { -1.0, -1.0, -1.0 } }, // { -1.0, 0.0, 0.0 } },
        { { -1.0,  1.0, -1.0 } }, // { -1.0, 0.0, 0.0 } },
        { { -1.0,  1.0,  1.0 } }, // { -1.0, 0.0, 0.0 } },
        { { -1.0, -1.0,  1.0 } }, // { -1.0, 0.0, 0.0 } },

        { { 1.0, -1.0, -1.0, } }, // { 1.0, 0.0, 0.0 } },
        { { 1.0,  1.0, -1.0, } }, // { 1.0, 0.0, 0.0 } },
        { { 1.0,  1.0,  1.0, } }, // { 1.0, 0.0, 0.0 } },
        { { 1.0, -1.0,  1.0, } }, // { 1.0, 0.0, 0.0 } },

        { { -1.0, -1.0, -1.0 } }, // { 0.0, -1.0, 0.0 } },
        { { -1.0, -1.0,  1.0 } }, // { 0.0, -1.0, 0.0 } },
        { {  1.0, -1.0,  1.0 } }, // { 0.0, -1.0, 0.0 } },
        { {  1.0, -1.0, -1.0 } }, // { 0.0, -1.0, 0.0 } },

        { { -1.0,  1.0, -1.0 } }, // { 0.0, 1.0, 0.0 } },
        { { -1.0,  1.0,  1.0 } }, // { 0.0, 1.0, 0.0 } },
        { {  1.0,  1.0,  1.0 } }, // { 0.0, 1.0, 0.0 } },
        { {  1.0,  1.0, -1.0 } }, // { 0.0, 1.0, 0.0 } }
    };
    uint16_t indices[] = {
        0, 1, 2,  0, 2, 3,
        6, 5, 4,  7, 6, 4,
        8, 9, 10,  8, 10, 11,
        14, 13, 12,  15, 14, 12,
        16, 17, 18,  16, 18, 19,
        22, 21, 20,  23, 22, 20
    };
    mesh_t cube = {
        .vbuf = sg_make_buffer(&(sg_buffer_desc){
            .data = SG_RANGE(vertices),
            .label = "cube-vertices"
        }),
        .ibuf = sg_make_buffer(&(sg_buffer_desc){
            .type = SG_BUFFERTYPE_INDEXBUFFER,
            .data = SG_RANGE(indices),
            .label = "cube-indices"
        }),
        .num_elements = sizeof(indices) / sizeof(uint16_t)
    };

    g_app.m_Cube = cube;
}

void make_image()
{
    /// Load image
    sg_pixel_format pixel_format;
    uint8_t* pixel_data;
    int x,y,ch;
    int pixel_size;
    if (stbi_is_hdr(g_app.m_InputPath))
    {
        pixel_data   = (uint8_t*) stbi_loadf(g_app.m_InputPath, &x, &y, &ch, 4);
        pixel_format = SG_PIXELFORMAT_RGBA32F;
        pixel_size   = x * y * 4 * sizeof(float);
    }
    else
    {
        pixel_data   = (uint8_t*) stbi_load(g_app.m_InputPath, &x, &y, &ch, 4);
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

    g_app.m_Texture.m_Image  = sg_make_image(&img_desc);
    g_app.m_Texture.m_Width  = x;
    g_app.m_Texture.m_Height = y;

    stbi_image_free(pixel_data);
}

void make_offscreen()
{
    g_app.m_Offscreen.m_PassAction = (sg_pass_action) {
        .colors[0] = { .action = SG_ACTION_CLEAR, .value = { 0.25f, 0.25f, 0.25f, 1.0f } }
    };

    sg_image_desc img_desc = {
        .type          = SG_IMAGETYPE_CUBE,
        .render_target = true,
        .width         = 1024, // g_app.m_Texture.m_Width,
        .height        = 1024, // g_app.m_Texture.m_Height,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .sample_count  = 4,
        .label         = "color-image"
    };

    g_app.m_Offscreen.m_Image = sg_make_image(&img_desc);

    sg_image depth_img = sg_make_image(&(sg_image_desc) {
        .type = SG_IMAGETYPE_2D,
        .render_target = true,
        .width = 1024,
        .height = 1024,
        .pixel_format = SG_PIXELFORMAT_DEPTH,
        .sample_count = 4,
        .label = "cubemap-depth-rt"
    });

    for (int i = 0; i < 6; ++i)
    {
        g_app.m_Offscreen.m_Pass[i] = sg_make_pass(&(sg_pass_desc){
            .color_attachments[0] = {
                .image = g_app.m_Offscreen.m_Image,
                .slice = i
            },
            //.color_attachments[0].image     = g_app.m_Offscreen.m_Image,
            .depth_stencil_attachment.image = depth_img,
            .label                          = "offscreen-pass"
        });
    }

    g_app.m_Offscreen.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_shader_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_cubemap_vs_position].format = SG_VERTEXFORMAT_FLOAT3,
                //[ATTR_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .index_type = SG_INDEXTYPE_UINT16,
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 4,
        .cull_mode = SG_CULLMODE_NONE,
        .label = "pipeline_fullscreen"
    });

    g_app.m_Offscreen.m_Bindings.vertex_buffers[0] = g_app.m_Cube.vbuf;
    g_app.m_Offscreen.m_Bindings.index_buffer = g_app.m_Cube.ibuf;
}

void make_display(void)
{
    g_app.m_PassAction = (sg_pass_action) {
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

    g_app.m_Bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "triangle-vertices",
    });

    g_app.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
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

void init(void)
{
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });

    make_display();
    make_cube();
    make_image();
    make_offscreen();
    make_uniforms();
}

void frame(void)
{
    if (g_app.m_IsDone)
    {
        //return;
    }

    g_app.m_Offscreen.m_Bindings.fs_images[SLOT_tex] = g_app.m_Texture.m_Image;

    mat4x4 projection;
#define DEG_TO_RAD(d) (d * (3.14159265359/180.0))
    mat4x4_perspective(projection, DEG_TO_RAD(90), 1.0f, 0.1f, 10.0f);
    cubemap_uniforms_t cubemap_uniforms = {};
    memcpy(&cubemap_uniforms.projection, projection, sizeof(mat4x4));
#undef DEG_TO_RAD

    // Generate cubemap sides
    for (int i = 0; i < 6; ++i)
    {
        memcpy(&cubemap_uniforms.view, g_app.m_CubeViewMatrices[i], sizeof(mat4x4));

        sg_begin_pass(g_app.m_Offscreen.m_Pass[i], &g_app.m_Offscreen.m_PassAction);
        sg_apply_pipeline(g_app.m_Offscreen.m_Pipeline);
        sg_apply_bindings(&g_app.m_Offscreen.m_Bindings);
        sg_apply_uniforms(SG_SHADERSTAGE_VS, SLOT_cubemap_uniforms, &SG_RANGE(cubemap_uniforms));

        sg_draw(0, g_app.m_Cube.num_elements, 1);
        sg_end_pass();
    }

    // Display pass
    g_app.m_Bindings.fs_images[SLOT_tex] = g_app.m_Offscreen.m_Image;
    sg_begin_default_pass(&g_app.m_PassAction, sapp_width(), sapp_height());
    sg_apply_pipeline(g_app.m_Pipeline);
    sg_apply_bindings(&g_app.m_Bindings);
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

sapp_desc sokol_main(int argc, char* argv[])
{
    if (argc <= 1)
    {
        printf("At least one argument required\n");
        exit(-1);
    }

    g_app.m_InputPath = argv[1];

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
