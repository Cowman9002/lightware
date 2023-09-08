#include "../lightware/lightware.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>

// TODO: Texture loading / Font rendering
// TODO: GUI using larger framebuffer overlay

enum EditorState {
    EditorStateOpen,
    EditorStateAddSector,
};

typedef struct EditorData {
    int screen_width, screen_height;

    lw_vec2 *points;
    unsigned points_len, points_cap;

    lw_uvec2 *lines;
    unsigned lines_len, lines_cap;

    enum EditorState state;
} EditorData;

int update(LW_Context *const context, float delta) {
    EditorData *editor = (EditorData *)lw_getUserData(context);

    lw_mat4 screen_to_world_mat;
    {
        lw_mat4 scale, translate;
        lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
        lw_mat4Translate((lw_vec3){ -editor->screen_width * 0.5f, -editor->screen_height * 0.5f, 0.0f }, translate);
        lw_mat4Mul(scale, translate, screen_to_world_mat);
    }

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    lw_vec4 mouse_world_pos;
    lw_mat4MulVec4(screen_to_world_mat, mouse_screen_posv4, mouse_world_pos);

    switch (editor->state) {
        case EditorStateOpen:
            if (lw_isKeyDown(context, LW_KeySpace)) {
                if (editor->points_len >= editor->points_cap) {
                    editor->points_cap += 16;
                    editor->points = realloc(editor->points, editor->points_cap * sizeof(*editor->points));
                }

                editor->points[editor->points_len][0] = mouse_world_pos[0];
                editor->points[editor->points_len][1] = mouse_world_pos[1];

                ++editor->points_len;

                editor->state = EditorStateAddSector;
            }
            break;

        case EditorStateAddSector:
            if (lw_isKeyDown(context, LW_KeySpace)) {
                if (editor->lines_len >= editor->lines_cap) {
                    editor->lines_cap += 16;
                    editor->lines = realloc(editor->lines, editor->lines_cap * sizeof(*editor->lines));
                }

                editor->lines[editor->lines_len][0] = editor->points_len - 1;
                editor->lines[editor->lines_len][1] = editor->points_len;

                ++editor->lines_len;

                if (editor->points_len >= editor->points_cap) {
                    editor->points_cap += 16;
                    editor->points = realloc(editor->points, editor->points_cap * sizeof(*editor->points));
                }

                editor->points[editor->points_len][0] = mouse_world_pos[0];
                editor->points[editor->points_len][1] = mouse_world_pos[1];

                ++editor->points_len;
            }
            break;
    }

    return 0;
}

int render(LW_Context *const context, LW_Framebuffer *const main_frame_buffer) {
    EditorData *editor = (EditorData *)lw_getUserData(context);

    lw_mat4 overhead_mat;
    {
        lw_mat4 scale, translate;
        lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
        lw_mat4Translate((lw_vec3){ editor->screen_width * 0.5f, editor->screen_height * 0.5f, 0.0f }, translate);
        lw_mat4Mul(translate, scale, overhead_mat);
    }

    lw_fillBuffer(main_frame_buffer, RGB(0, 0, 60));

    unsigned ai, bi;
    lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f };
    lw_vec4 c, d;
    lw_ivec2 e, f;

    for (unsigned i = 0; i < editor->lines_len; ++i) {
        ai = editor->lines[i][0];
        bi = editor->lines[i][1];

        if (ai >= editor->points_len || bi >= editor->points_len) continue;

        a[0] = editor->points[ai][0], a[1] = editor->points[ai][1];
        b[0] = editor->points[bi][0], b[1] = editor->points[bi][1];

        lw_mat4MulVec4(overhead_mat, a, c);
        lw_mat4MulVec4(overhead_mat, b, d);

        e[0] = roundf(c[0]), e[1] = roundf(c[1]);
        f[0] = roundf(d[0]), f[1] = roundf(d[1]);

        lw_drawLine(main_frame_buffer, e, f, LW_COLOR_WHITE);
    }

    for (unsigned i = 0; i < editor->points_len; ++i) {
        a[0] = editor->points[i][0];
        a[1] = editor->points[i][1];

        lw_mat4MulVec4(overhead_mat, a, c);

        e[0] = roundf(c[0]), e[1] = roundf(c[1]);

        lw_drawRect(
            main_frame_buffer,
            (LW_Rect){
                .pos  = { c[0] - 3, c[1] - 3 },
                .size = { 6, 6 },
            },
            LW_COLOR_YELLOW);
    }

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

    editor.state = EditorStateOpen;

    editor.screen_width  = 640;
    editor.screen_height = 480;

    editor.lines_len  = 0;
    editor.points_len = 0;

    editor.lines_cap  = 16;
    editor.points_cap = 16;

    editor.lines  = malloc(editor.lines_cap * sizeof(*editor.lines));
    editor.points = malloc(editor.points_cap * sizeof(*editor.points));

    // if(!lw_loadPortalWorld("res/maps/map0.map", 5.0f, &editor.pod)) return -1;
    // editor.cam.pos[0] = 0.0f;
    // editor.cam.pos[1] = 0.0f;
    // editor.cam.pos[2] = 0.0f;
    // editor.cam.yaw = 0.0f;
    // editor.cam.pitch = 0.0f;

    // editor.cam.fov = 80 * TO_RADS;
    // editor.cam.aspect_ratio = 640 / 480;
    // editor.cam.near_plane = 0.03f;
    // editor.cam.far_plane = 500.0f;
    // lw_mat4Perspective(editor.cam.fov, editor.cam.aspect_ratio, editor.cam.near_plane, editor.cam.far_plane, editor.cam.proj_mat);

    int result = lw_start(context);

    lw_deinit(context);
    return result;
}