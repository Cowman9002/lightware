#include "../lightware/lightware.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

// TODO: GUI using larger framebuffer overlay

// [x] TODO: Select multiple points at once
// [x] TODO: Ability to insert a point along an edge
// [ ] TODO: Camera movement
// [ ] TODO: Grid snapping similar to trenchbroom
// [ ] TODO: Add sector group above lines
// [ ] TODO: Allow slicing sectors based on rule if entire new sector is inside one sector and it is finished normally or connected to an already existing point in sector
// [ ] TODO: Detect when line should be portal based on if line is shared by two sectors

#define DYNARRAY_CHECK_CAP(arr)                                 \
    do {                                                        \
        if (arr##_len >= arr##_cap) {                           \
            arr##_cap = arr##_len + 16;                         \
            arr       = realloc(arr, arr##_cap * sizeof(*arr)); \
        }                                                       \
    } while (0)

LW_FontTexture g_font;
char g_text_buffer[64];

typedef struct Editor {
    int screen_width, screen_height;
    lw_mat4 world_to_screen_mat, screen_to_world_mat;

    lw_vec2 *points;
    unsigned points_len, points_cap;

    lw_uvec3 *lines; // [2] is information about line
    unsigned lines_len, lines_cap;

    unsigned point_hover_index;
    unsigned line_hover_index;
    unsigned hoverable_index;

    lw_vec2 line_slice_point;

    // Point selection
    bool is_selecting;
    LW_Aabb selection_aabb;
    unsigned point_grab_index;
    bool is_moving_selection;
    unsigned *points_selected;
    unsigned points_selected_len, points_selected_cap;

    // New sector variables
    bool creating_sector;
    lw_uvec2 *new_lines;
    unsigned new_lines_len, new_lines_cap;
    unsigned new_sec_first_point, prev_point_index;

} Editor;

void initEditor(Editor *const editor) {
    memset(editor, 0, sizeof(*editor));

    editor->screen_width      = 640;
    editor->screen_height     = 480;
    editor->point_grab_index  = ~0;
    editor->point_hover_index = ~0;
}

int update(LW_Context *const context, float delta) {
    Editor *editor = (Editor *)lw_getUserData(context);

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

    // calc line hover index
    editor->line_hover_index = ~0;
    if (editor->point_hover_index == ~0 && editor->point_grab_index == ~0) {
        float min_dist = 6 * 6;
        lw_vec4 a = { 0, 0, 0, 1 }, b = { 0, 0, 0, 1 }, c, d;
        lw_uvec2 n;
        lw_vec2 line[2];
        lw_vec2 p, q;
        float dst;

        lw_vec4 closest = { 0, 0, 0, 1 };

        for (unsigned i = 0; i < editor->lines_len; ++i) {
            n[0] = editor->lines[i][0], n[1] = editor->lines[i][1];
            a[0] = editor->points[n[0]][0], a[1] = editor->points[n[0]][1];
            b[0] = editor->points[n[1]][0], b[1] = editor->points[n[1]][1];

            lw_mat4MulVec4(editor->world_to_screen_mat, a, c);
            lw_mat4MulVec4(editor->world_to_screen_mat, b, d);

            line[0][0] = c[0], line[0][1] = c[1];
            line[1][0] = d[0], line[1][1] = d[1];

            lw_closestPointOnSegment(line, mouse_screen_posv4, p);

            q[0] = mouse_screen_posv4[0] - p[0], q[1] = mouse_screen_posv4[1] - p[1];

            dst = lw_dot2d(q, q);

            if (dst < min_dist) {
                min_dist                 = dst;
                editor->line_hover_index = i;
                closest[0] = p[0], closest[1] = p[1];
            }
        }

        lw_mat4MulVec4(editor->screen_to_world_mat, closest, a);
        editor->line_slice_point[0] = a[0], editor->line_slice_point[1] = a[1];
    }

    // split line
    if (editor->line_hover_index != ~0) {
        if (lw_isKeyDown(context, LW_KeyX)) {
            // add new line
            DYNARRAY_CHECK_CAP(editor->lines);
            editor->lines[editor->lines_len][0] = editor->points_len;
            editor->lines[editor->lines_len][1] = editor->lines[editor->line_hover_index][1];
            ++editor->lines_len;

            // update old line
            editor->lines[editor->line_hover_index][1] = editor->points_len;

            // add new point
            DYNARRAY_CHECK_CAP(editor->points);
            editor->points[editor->points_len][0] = editor->line_slice_point[0];
            editor->points[editor->points_len][1] = editor->line_slice_point[1];
            ++editor->points_len;
            ++editor->hoverable_index;
        }
    }

    // selection box
    if (editor->is_selecting) {
        editor->selection_aabb.high[0] = mouse_world_pos[0];
        editor->selection_aabb.high[1] = mouse_world_pos[1];

        if (lw_isMouseButtonUp(context, 0)) {
            if (editor->selection_aabb.low[0] > editor->selection_aabb.high[0]) swap(float, editor->selection_aabb.low[0], editor->selection_aabb.high[0]);
            if (editor->selection_aabb.low[1] > editor->selection_aabb.high[1]) swap(float, editor->selection_aabb.low[1], editor->selection_aabb.high[1]);

            // Do selection
            editor->points_selected_len = 0;
            for (unsigned i = 0; i < editor->points_len; ++i) {
                if (lw_pointInAabb(editor->selection_aabb, editor->points[i])) {
                    DYNARRAY_CHECK_CAP(editor->points_selected);
                    editor->points_selected[editor->points_selected_len] = i;
                    ++editor->points_selected_len;
                }
            }
            editor->is_selecting = false;
            editor->is_moving_selection = false;
        }
    } else {
        if (lw_isMouseButtonDown(context, 0)) {
            if (editor->point_hover_index == ~0) {
                // if pressing on empty space, start new selection box
                editor->is_selecting           = true;
                editor->selection_aabb.low[0]  = mouse_world_pos[0];
                editor->selection_aabb.low[1]  = mouse_world_pos[1];
                editor->selection_aabb.high[0] = mouse_world_pos[0];
                editor->selection_aabb.high[1] = mouse_world_pos[1];
            } else {
                // grab point
                editor->point_grab_index  = editor->point_hover_index;
                editor->point_hover_index = ~0;

                // check if grabbed point is part of selection
                if (editor->points_selected_len > 0) {
                    editor->is_moving_selection = false;
                    for (unsigned i = 0; i < editor->points_selected_len; ++i) {
                        unsigned j = editor->points_selected[i];
                        if (editor->point_grab_index == j) {
                            editor->is_moving_selection = true;
                            break;
                        }
                    }

                    // deselect if not moving selection
                    if (!editor->is_moving_selection) {
                        editor->points_selected_len = 0;
                    }
                }
            }

        } else if (lw_isMouseButtonUp(context, 0)) {
            if (editor->point_grab_index != ~0) {
                if (!editor->is_moving_selection && editor->point_hover_index != editor->point_grab_index && editor->point_hover_index != ~0) {
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
                                if (j == i) continue;
                                // if so, remove this line

                                bool match0 = editor->lines[j][0] == editor->point_grab_index || editor->lines[j][0] == editor->point_hover_index;
                                bool match1 = editor->lines[j][1] == editor->point_grab_index || editor->lines[j][1] == editor->point_hover_index;

                                if ((match0 && editor->lines[j][1] == other_index) || (match1 && editor->lines[j][0] == other_index)) {
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
            }
        } else if (editor->point_grab_index != ~0) {
            // move selection + grab

            if (editor->is_moving_selection) {
                lw_vec2 move_delta;
                move_delta[0] = mouse_world_pos[0] - editor->points[editor->point_grab_index][0];
                move_delta[1] = mouse_world_pos[1] - editor->points[editor->point_grab_index][1];

                // move selected points
                for (unsigned i = 0; i < editor->points_selected_len; ++i) {
                    unsigned j = editor->points_selected[i];
                    editor->points[j][0] += move_delta[0];
                    editor->points[j][1] += move_delta[1];
                }
            } else {
                editor->points[editor->point_grab_index][0] = mouse_world_pos[0];
                editor->points[editor->point_grab_index][1] = mouse_world_pos[1];
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
    Editor *editor = (Editor *)lw_getUserData(context);

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    lw_fillBuffer(main_frame_buffer, RGB(0, 0, 40));

    lw_vec4 a = { 0, 0, 0, 1 }, b = { 0, 0, 0, 1 }, c, d;
    lw_uvec2 n;
    lw_ivec2 p, q;

    LW_Aabb selection_aabb;
    if (editor->is_selecting) {
        selection_aabb.low[0] = editor->selection_aabb.low[0], selection_aabb.low[1] = editor->selection_aabb.low[1];
        selection_aabb.high[0] = editor->selection_aabb.high[0], selection_aabb.high[1] = editor->selection_aabb.high[1];
        if (selection_aabb.low[0] > selection_aabb.high[0]) swap(float, selection_aabb.low[0], selection_aabb.high[0]);
        if (selection_aabb.low[1] > selection_aabb.high[1]) swap(float, selection_aabb.low[1], selection_aabb.high[1]);
    }

    // draw lines
    for (unsigned i = 0; i < editor->lines_len; ++i) {
        n[0] = editor->lines[i][0], n[1] = editor->lines[i][1];
        a[0] = editor->points[n[0]][0], a[1] = editor->points[n[0]][1];
        b[0] = editor->points[n[1]][0], b[1] = editor->points[n[1]][1];

        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);
        lw_mat4MulVec4(editor->world_to_screen_mat, b, d);

        p[0] = roundf(c[0]), p[1] = roundf(c[1]);
        q[0] = roundf(d[0]), q[1] = roundf(d[1]);

        LW_Color c;
        if (i == editor->line_hover_index) {
            c = LW_COLOR_CYAN;
        } else {
            c = LW_COLOR_WHITE;
        }

        // if (editor->lines[i][2] == 0) {
            lw_drawLine(main_frame_buffer, p, q, c);
        // }
    }

    // draw new sector
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

    // draw points
    for (unsigned i = 0; i < editor->points_len; ++i) {
        a[0] = editor->points[i][0], a[1] = editor->points[i][1];
        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);

        LW_Recti rect = {
            .pos  = { c[0] - 3, c[1] - 3 },
            .size = { 6, 6 },
        };

        LW_Color c;
        if (editor->is_selecting && lw_pointInAabb(selection_aabb, a)) {
            c = LW_COLOR_CYAN;
        } else if (i == editor->new_sec_first_point) {
            c = LW_COLOR_RED;
        } else if (i == editor->point_grab_index) {
            c = LW_COLOR_CYAN;
        } else {
            c = LW_COLOR_YELLOW;
        }

        if (i == editor->point_hover_index || i == editor->point_grab_index) {
            lw_fillRect(main_frame_buffer, rect, c);
        } else {
            lw_drawRect(main_frame_buffer, rect, lw_shadeColor(c, 180));
        }
    }

    // selected points
    for (unsigned i = 0; i < editor->points_selected_len; ++i) {
        unsigned j = editor->points_selected[i];
        a[0] = editor->points[j][0], a[1] = editor->points[j][1];
        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);

        LW_Recti rect = {
            .pos  = { c[0] - 4, c[1] - 4 },
            .size = { 8, 8 },
        };

        LW_Color c = LW_COLOR_CYAN;
        lw_drawRect(main_frame_buffer, rect, lw_shadeColor(c, 180));
    }

    // selection box
    if (editor->is_selecting) {
        a[0] = editor->selection_aabb.low[0], a[1] = editor->selection_aabb.low[1];
        b[0] = editor->selection_aabb.high[0], b[1] = editor->selection_aabb.high[1];

        if (a[0] > b[0]) swap(float, a[0], b[0]);
        if (a[1] < b[1]) swap(float, a[1], b[1]);

        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);
        lw_mat4MulVec4(editor->world_to_screen_mat, b, d);

        LW_Recti rect = {
            .pos  = { c[0], c[1] },
            .size = { d[0] - c[0], d[1] - c[1] },
        };

        lw_drawRect(main_frame_buffer, rect, LW_COLOR_CYAN);
    }

    // new slice point
    if (editor->line_hover_index != ~0) {
        a[0] = editor->line_slice_point[0], a[1] = editor->line_slice_point[1];
        lw_mat4MulVec4(editor->world_to_screen_mat, a, c);

        LW_Recti rect = {
            .pos  = { c[0] - 3, c[1] - 3 },
            .size = { 6, 6 },
        };

        LW_Color c = LW_COLOR_PURPLE;
        lw_fillRect(main_frame_buffer, rect, c);
    }

    snprintf(g_text_buffer, sizeof(g_text_buffer), "Points: %u", editor->points_len);
    lw_drawString(main_frame_buffer, (lw_ivec2){ 5, 5 }, LW_COLOR_WHITE, g_font, g_text_buffer);
    snprintf(g_text_buffer, sizeof(g_text_buffer), "Lines: %u", editor->lines_len);
    lw_drawString(main_frame_buffer, (lw_ivec2){ 5, 5 + 1 * 16 }, LW_COLOR_WHITE, g_font, g_text_buffer);

    return 0;
}

int main() {
    Editor editor;

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

    initEditor(&editor);

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