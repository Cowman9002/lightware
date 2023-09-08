#include "../lightware/lightware.h"

#include <stdint.h>
#include <stdio.h>

typedef struct EditorData {
    LW_PortalWorld pod;
    LW_Camera cam;
} EditorData;

int update(LW_Context *const context, float delta) {
    // EditorData *data = (EditorData *)lw_getUserData(context);
    return 0;
}

int render(LW_Context *const context, LW_Framebuffer *const main_frame_buffer) {
    EditorData *editor = (EditorData *)lw_getUserData(context);
    lw_ivec2 fb_dims;
    lw_getFramebufferDimentions(main_frame_buffer, fb_dims);

    lw_fillBuffer(main_frame_buffer, LW_COLOR_BLUE);

    LW_SectorListNode *node = editor->pod.sectors.head;
    for(;node != NULL; node = node->next) {
        LW_Sector sector = node->item;

        for(unsigned i = 0; i < sector.num_walls; ++i) {
            lw_ivec2 a = {
                sector.points[i * 2][0] + (fb_dims[0] / 2),
                sector.points[i * 2][1] + (fb_dims[1] / 2),
            };

            lw_ivec2 b = {
                sector.points[i * 2 + 1][0] + (fb_dims[0] / 2),
                sector.points[i * 2 + 1][1] + (fb_dims[1] / 2),
            };

            lw_drawLine(main_frame_buffer, a, b, LW_COLOR_WHITE);
        }
    }

    // lw_setPixel(main_frame_buffer, (lw_uvec2){20, 30}, LW_COLOR_PURPLE);
    // lw_drawLine(main_frame_buffer, (lw_ivec2){50, 50}, (lw_ivec2){100, 120}, LW_COLOR_RED);
    // lw_fillRect(main_frame_buffer, (LW_Rect){{200, 200}, {40, 70}}, LW_COLOR_GREEN);

    return 0;
}

int main() {
    EditorData editor;

    LW_ContextInit init = {
        .title          = "LightWare Editor",
        .logical_width  = 640,
        .logical_height = 480,
        .scale          = 2,
        .user_data      = &editor,
        .update_fn      = update,
        .render_fn      = render,
    };

    LW_Context *context = lw_init(init);
    if (context == NULL) {
        printf("Failed to create LW_Context\n");
        return -1;
    }

    if(!lw_loadPortalWorld("res/maps/map0.map", 5.0f, &editor.pod)) return -1;
    editor.cam.pos[0] = 0.0f;
    editor.cam.pos[1] = 0.0f;
    editor.cam.pos[2] = 0.0f;
    editor.cam.yaw = 0.0f;
    editor.cam.pitch = 0.0f;

    editor.cam.fov = 80 * TO_RADS;
    editor.cam.aspect_ratio = 640 / 480;
    editor.cam.near_plane = 0.03f;
    editor.cam.far_plane = 500.0f;
    lw_mat4Perspective(editor.cam.fov, editor.cam.aspect_ratio, editor.cam.near_plane, editor.cam.far_plane, editor.cam.proj_mat);

    int result = lw_start(context);

    lw_deinit(context);
    return result;
}