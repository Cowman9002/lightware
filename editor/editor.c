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

int render(LW_Context *const context, LW_Framebuffer const *const main_frame_buffer) {
    EditorData *data = (EditorData *)lw_getUserData(context);

    for (unsigned y = 0; y < main_frame_buffer->height; ++y) {
        for (unsigned x = 0; x < main_frame_buffer->width; ++x) {
            main_frame_buffer->pixels[x + y * main_frame_buffer->width] = (LW_Color){
                .a = 255,
                .r = (x + data->frame) & 0xff,
                .g = (y) & 0xff,
                .b = data->frame / 8,
            };
        }
    }
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