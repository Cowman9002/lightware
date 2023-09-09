#include "../lightware/lightware.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

// TODO: GUI using larger framebuffer overlay

#define DYNARRAY_CHECK_CAP(arr)                                 \
    do {                                                        \
        if (arr##_len >= arr##_cap) {                           \
            arr##_cap = arr##_len + 16;                         \
            arr       = realloc(arr, arr##_cap * sizeof(*arr)); \
        }                                                       \
    } while (0)

LW_FontTexture g_font;
char g_text_buffer[64];

typedef struct EditorData {
    int screen_width, screen_height;
    lw_mat4 world_to_screen_mat, screen_to_world_mat;

    unsigned point_grab_index;
    unsigned point_hover_index;
    unsigned hoverable_index;

    lw_vec2 *points;
    unsigned points_len, points_cap;

    lw_uvec3 *lines; // [2] is information about line
    unsigned lines_len, lines_cap;

    // New sector variables
    bool creating_sector;
    lw_uvec2 *new_lines;
    unsigned new_lines_len, new_lines_cap;
    unsigned new_sec_first_point, prev_point_index;

} EditorData;

int update(LW_Context *const context, float delta) {
    EditorData *editor = (EditorData *)lw_getUserData(context);

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    {
        lw_mat4 scale, translate;
        lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
        lw_mat4Translate((lw_vec3){ -editor->screen_width * 0.5f, -editor->screen_height * 0.5f, 0.0f }, translate);
        lw_mat4Mul(scale, translate, editor->screen_to_world_mat);
    }

    {
        lw_mat4 scale, translate;
        lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
        lw_mat4Translate((lw_vec3){ editor->screen_width * 0.5f, editor->screen_height * 0.5f, 0.0f }, translate);
        lw_mat4Mul(translate, scale, editor->world_to_screen_mat);
    }

    lw_vec4 mouse_world_pos;
    lw_mat4MulVec4(editor->screen_to_world_mat, mouse_screen_posv4, mouse_world_pos);

    {
        // calc hover index
        editor->point_hover_index = ~0;
        lw_vec4 a                 = { 0, 0, 0, 1 }, c;
        for (unsigned i = 0; i < editor->hoverable_index; ++i) {
            if (i == editor->point_grab_index) continue;
            a[0] = editor->points[i][0], a[1] = editor->points[i][1];
            lw_mat4MulVec4(editor->world_to_screen_mat, a, c);

            LW_Aabb aabb = {
                .low  = { c[0] - 3, c[1] - 3 },
                .high = { c[0] + 3, c[1] + 3 },
            };

            if (lw_pointInAabb(aabb, mouse_screen_posv4)) {
                editor->point_hover_index = i;
                break;
            }
        }
    }

    // Point grabbing

    if (editor->point_grab_index != ~0) {
        if (lw_isMouseButtonUp(context, 0)) {
            if (editor->point_hover_index != editor->point_grab_index && editor->point_hover_index != ~0) {
                // combine points

                for (unsigned i = 0; i < editor->lines_len; ++i) {
                    // remove all lines that connect the two points
                    if ((editor->lines[i][0] == editor->point_grab_index && editor->lines[i][1] == editor->point_hover_index) ||
                        (editor->lines[i][0] == editor->point_hover_index && editor->lines[i][1] == editor->point_grab_index)) {
                        --editor->lines_len;
                        editor->lines[i][0] = editor->lines[editor->lines_len][0];
                        editor->lines[i][1] = editor->lines[editor->lines_len][1];
                        editor->lines[i][2] = editor->lines[editor->lines_len][2];
                    }
                }

                for (unsigned i = 0; i < editor->lines_len; ++i) {
                    // update all lines that use grab to use hover instead
                    if (editor->lines[i][0] == editor->point_grab_index || editor->lines[i][1] == editor->point_grab_index) {
                        unsigned other_index = editor->lines[i][0] == editor->point_grab_index ? editor->lines[i][1] : editor->lines[i][0];

                        // check if line exists
                        bool exists = false;
                        for (unsigned j = 0; j < editor->lines_len; ++j) {
                            if(j == i) continue;
                            // if so, remove this line

                            bool match0 = editor->lines[j][0] == editor->point_grab_index || editor->lines[j][0] == editor->point_hover_index;
                            bool match1 = editor->lines[j][1] == editor->point_grab_index || editor->lines[j][1] == editor->point_hover_index;

                            if (match0 && editor->lines[j][1] == other_index || match1 && editor->lines[j][0] == other_index) {
                                --editor->lines_len;
                                editor->lines[i][0] = editor->lines[editor->lines_len][0];
                                editor->lines[i][1] = editor->lines[editor->lines_len][1];
                                editor->lines[i][2] = editor->lines[editor->lines_len][2];

                                exists = true;
                                break;
                            }
                        }

                        if (!exists) {
                            // else, just update it
                            if (editor->lines[i][0] == editor->point_grab_index) editor->lines[i][0] = editor->point_hover_index;
                            if (editor->lines[i][1] == editor->point_grab_index) editor->lines[i][1] = editor->point_hover_index;
                        }
                    }
                }

                // remove grabbed point
                --editor->points_len;
                editor->points[editor->point_grab_index][0] = editor->points[editor->points_len][0];
                editor->points[editor->point_grab_index][1] = editor->points[editor->points_len][1];
                // fix swap remove indices
                for (unsigned i = 0; i < editor->lines_len; ++i) {
                    if (editor->lines[i][0] == editor->points_len) {
                        editor->lines[i][0] = editor->point_grab_index;
                    }
                    if (editor->lines[i][1] == editor->points_len) {
                        editor->lines[i][1] = editor->point_grab_index;
                    }
                }
            }
            editor->point_grab_index = ~0;
        } else {
            editor->points[editor->point_grab_index][0] = mouse_world_pos[0];
            editor->points[editor->point_grab_index][1] = mouse_world_pos[1];
        }
    } else {
        if (lw_isMouseButtonDown(context, 0)) {
            if (editor->point_hover_index != ~0) {
                editor->point_grab_index = editor->point_hover_index;
            }
        }
    }

    // create new sector

    if (!editor->creating_sector) {
        if (lw_isKeyDown(context, LW_KeySpace)) {
            if (editor->point_hover_index == ~0) {
                // not hovering, make new point
                DYNARRAY_CHECK_CAP(editor->points);
                editor->points[editor->points_len][0] = mouse_world_pos[0];
                editor->points[editor->points_len][1] = mouse_world_pos[1];
                editor->new_sec_first_point           = editor->points_len;

                ++editor->points_len;
                ++editor->hoverable_index;
            } else {
                // create new sector connected to point
                editor->new_sec_first_point = editor->point_hover_index;
            }

            editor->prev_point_index = editor->new_sec_first_point;
            editor->creating_sector  = true;
        }
    } else {
        if (lw_isKeyDown(context, LW_KeySpace)) {
            if (editor->point_hover_index == ~0) {
                // not hovering, make new point
                DYNARRAY_CHECK_CAP(editor->new_lines);
                editor->new_lines[editor->new_lines_len][0] = editor->prev_point_index;
                editor->new_lines[editor->new_lines_len][1] = editor->points_len;
                editor->prev_point_index                    = editor->points_len;
                ++editor->new_lines_len;

                DYNARRAY_CHECK_CAP(editor->points);
                editor->points[editor->points_len][0] = mouse_world_pos[0];
                editor->points[editor->points_len][1] = mouse_world_pos[1];
                ++editor->points_len;

            } else if (editor->point_hover_index != editor->new_sec_first_point) {
                // make new line using exiting point
                DYNARRAY_CHECK_CAP(editor->new_lines);
                editor->new_lines[editor->new_lines_len][0] = editor->prev_point_index;
                editor->new_lines[editor->new_lines_len][1] = editor->point_hover_index;
                editor->prev_point_index                    = editor->point_hover_index;
                ++editor->new_lines_len;

            } else if (editor->new_lines_len >= 2) {
                // finish sector
                DYNARRAY_CHECK_CAP(editor->new_lines);
                editor->new_lines[editor->new_lines_len][0] = editor->prev_point_index;
                editor->new_lines[editor->new_lines_len][1] = editor->new_lines[0][0];
                ++editor->new_lines_len;

                for (unsigned i = 0; i < editor->new_lines_len; ++i) {

                    for (unsigned j = 0; j < editor->lines_len; ++j) {
                        // already exists
                        if ((editor->lines[j][0] == editor->new_lines[i][0] && editor->lines[j][1] == editor->new_lines[i][1]) ||
                            (editor->lines[j][0] == editor->new_lines[i][1] && editor->lines[j][1] == editor->new_lines[i][0])) {
                            // editor->lines[j][2] = 1;
                            // TODO: Check if this should be a portal
                            goto _check_next;
                        }
                    }

                    DYNARRAY_CHECK_CAP(editor->lines);
                    editor->lines[editor->lines_len][0] = editor->new_lines[i][0];
                    editor->lines[editor->lines_len][1] = editor->new_lines[i][1];
                    editor->lines[editor->lines_len][2] = 0;
                    ++editor->lines_len;

                _check_next:
                }
                editor->new_lines_len       = 0;
                editor->new_sec_first_point = ~0;
                editor->hoverable_index     = editor->points_len;
                editor->creating_sector     = false;
            }
        }
    }

    return 0;
}

int render(LW_Context *const context, LW_Framebuffer *const main_frame_buffer) {
    EditorData *editor = (EditorData *)lw_getUserData(context);

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    lw_fillBuffer(main_frame_buffer, RGB(0, 0, 40));

    lw_vec4 a = { 0, 0, 0, 1 }, b = { 0, 0, 0, 1 }, c, d;
    lw_uvec2 n;
    lw_ivec2 p, q;
    for (unsigned i = 0; i < editor->lines_len; ++i) {
        n[0] = editor->lines[i][0], n[1] = editor->lines[i][1];
        a[0] = editor->points[n[0]][0], a[1] = editor->points[n[0]][1];
        b[0] = editor->points[n[1]][0], b[1] = editor->points[n[1]][1];

        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);
        lw_mat4MulVec4(editor->world_to_screen_mat, b, d);

        p[0] = roundf(c[0]), p[1] = roundf(c[1]);
        q[0] = roundf(d[0]), q[1] = roundf(d[1]);

        if (editor->lines[i][2] == 0) {
            lw_drawLine(main_frame_buffer, p, q, LW_COLOR_WHITE);
        } else if (editor->lines[i][2] == 1) {
            lw_drawLine(main_frame_buffer, p, q, LW_COLOR_RED);
        }
    }

    for (unsigned i = 0; i < editor->new_lines_len; ++i) {
        n[0] = editor->new_lines[i][0], n[1] = editor->new_lines[i][1];
        a[0] = editor->points[n[0]][0], a[1] = editor->points[n[0]][1];
        b[0] = editor->points[n[1]][0], b[1] = editor->points[n[1]][1];

        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);
        lw_mat4MulVec4(editor->world_to_screen_mat, b, d);

        p[0] = roundf(c[0]), p[1] = roundf(c[1]);
        q[0] = roundf(d[0]), q[1] = roundf(d[1]);

        lw_drawLine(main_frame_buffer, p, q, LW_COLOR_GREY);
    }

    for (unsigned i = 0; i < editor->points_len; ++i) {
        a[0] = editor->points[i][0], a[1] = editor->points[i][1];
        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);

        LW_Recti rect = {
            .pos  = { c[0] - 3, c[1] - 3 },
            .size = { 6, 6 },
        };

        LW_Color c;
        if (i == editor->new_sec_first_point)
            c = LW_COLOR_RED;
        else if (i == editor->point_grab_index)
            c = LW_COLOR_GREEN;
        else
            c = LW_COLOR_YELLOW;

        if (i == editor->point_hover_index || i == editor->point_grab_index) {
            lw_fillRect(main_frame_buffer, rect, c);
        } else {
            lw_drawRect(main_frame_buffer, rect, lw_shadeColor(c, 180));
        }
    }

    snprintf(g_text_buffer, sizeof(g_text_buffer), "Points: %u", editor->points_len);
    lw_drawString(main_frame_buffer, (lw_ivec2){ 5, 5 }, LW_COLOR_WHITE, g_font, g_text_buffer);
    snprintf(g_text_buffer, sizeof(g_text_buffer), "Lines: %u", editor->lines_len);
    lw_drawString(main_frame_buffer, (lw_ivec2){ 5, 5 + 1 * 16 }, LW_COLOR_WHITE, g_font, g_text_buffer);

    return 0;
}

int main() {
    EditorData editor;
    memset(&editor, 0, sizeof(editor));

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

    if (!lw_loadTexture("res/fonts/small.png", &g_font.texture)) return -1;
    g_font.char_width = g_font.texture.width / 95;

    editor.screen_width      = 640;
    editor.screen_height     = 480;
    editor.point_grab_index  = ~0;
    editor.point_hover_index = ~0;

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