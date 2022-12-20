
#define SOKOL_METAL
#define SOKOL_IMPL
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "shaders.glsl.h"

struct app
{
    struct
    {
        sg_pass_action m_PassAction;
        sg_pass        m_Pass;
        sg_pipeline    m_Pipeline;
        sg_image       m_Image;
        sg_bindings    m_Bindings;
    } m_Offscreen;

    sg_pass_action m_PassAction;
    sg_pipeline    m_Pipeline;
    sg_bindings    m_Bindings;

    struct
    {
        int      m_Width;
        int      m_Height;
        sg_image m_Image;
    } m_Texture;
    const char*    m_InputPath;
    uint8_t        m_IsDone : 1;
} g_app = {};

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
        .render_target = true,
        .width         = g_app.m_Texture.m_Width,
        .height        = g_app.m_Texture.m_Height,
        .pixel_format  = SG_PIXELFORMAT_RGBA8,
        .min_filter    = SG_FILTER_LINEAR,
        .mag_filter    = SG_FILTER_LINEAR,
        .wrap_u        = SG_WRAP_REPEAT,
        .wrap_v        = SG_WRAP_REPEAT,
        .sample_count  = 4,
        .label         = "color-image"
    };

    g_app.m_Offscreen.m_Image = sg_make_image(&img_desc);

    img_desc.pixel_format = SG_PIXELFORMAT_DEPTH;
    img_desc.label        = "depth-image";
    sg_image depth_img    = sg_make_image(&img_desc);

    g_app.m_Offscreen.m_Pass = sg_make_pass(&(sg_pass_desc){
        .color_attachments[0].image = g_app.m_Offscreen.m_Image,
        .depth_stencil_attachment.image = depth_img,
        .label                      = "offscreen-pass"
    });

    g_app.m_Offscreen.m_Pipeline = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = sg_make_shader(pbr_shader_shader_desc(sg_query_backend())),
        .layout = {
            .attrs = {
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .depth = {
            .pixel_format = SG_PIXELFORMAT_DEPTH,
            .compare = SG_COMPAREFUNC_LESS_EQUAL,
            .write_enabled = true,
        },
        .colors[0].pixel_format = SG_PIXELFORMAT_RGBA8,
        .sample_count = 4,
        .cull_mode = SG_CULLMODE_NONE,
        .label = "pipeline_fullscreen"
    });

    float vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,
        -1.0f,  1.0f, 0.0f, 1.0f,
         1.0f,  1.0f, 1.0f, 1.0f,

        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
    };

    g_app.m_Offscreen.m_Bindings.vertex_buffers[0] = sg_make_buffer(&(sg_buffer_desc){
        .data = SG_RANGE(vertices),
        .label = "triangle-vertices",
    });
}

void init(void)
{
    sg_setup(&(sg_desc) {
        .context = sapp_sgcontext()
    });

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
                [ATTR_vs_position].format = SG_VERTEXFORMAT_FLOAT2,
                [ATTR_vs_texcoord].format = SG_VERTEXFORMAT_FLOAT2,
            },
        },
        .cull_mode = SG_CULLMODE_NONE,
        .label = "pbr_pipeline"
    });

    make_image();
    make_offscreen();
}

void frame(void)
{
    if (g_app.m_IsDone)
    {
        return;
    }

    g_app.m_Offscreen.m_Bindings.fs_images[SLOT_tex] = g_app.m_Texture.m_Image;

    // Offscreen pass
    sg_begin_pass(g_app.m_Offscreen.m_Pass, &g_app.m_Offscreen.m_PassAction);
    sg_apply_pipeline(g_app.m_Offscreen.m_Pipeline);
    sg_apply_bindings(&g_app.m_Offscreen.m_Bindings);
    sg_draw(0, 6, 1);
    sg_end_pass();

    // Display pass
    g_app.m_Bindings.fs_images[SLOT_tex] = g_app.m_Offscreen.m_Image;
    sg_begin_default_pass(&g_app.m_PassAction, sapp_width(), sapp_height());
    sg_apply_pipeline(g_app.m_Pipeline);
    sg_apply_bindings(&g_app.m_Bindings);
    sg_draw(0, 6, 1);
    sg_end_pass();

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
