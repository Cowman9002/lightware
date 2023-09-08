#include "../lightware/lightware.h"

#include <stdint.h>
#include <stdio.h>

typedef struct EditorData {
    uint64_t frame;
} EditorData;

int update(LW_Context *const context, float delta) {
    EditorData *data = (EditorData *)lw_getUserData(context);
    data->frame += 1;
    return 0;
}

int render(LW_Context *const context, LW_Framebuffer *const main_frame_buffer) {
    // EditorData *data = (EditorData *)lw_getUserData(context);

    lw_fillBuffer(main_frame_buffer, LW_COLOR_BLUE);

    lw_setPixel(main_frame_buffer, (lw_uvec2){20, 30}, LW_COLOR_PURPLE);
    lw_drawLine(main_frame_buffer, (lw_ivec2){50, 50}, (lw_ivec2){100, 120}, LW_COLOR_RED);
    lw_fillRect(main_frame_buffer, (LW_Rect){{200, 200}, {40, 70}}, LW_COLOR_GREEN);

    return 0;
}

int main() {
    EditorData editor;
    editor.frame = 0;

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

    int result = lw_start(context);

    lw_deinit(context);
    return result;
}