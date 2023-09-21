#include "editor.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

float logerp(float a, float b, float t);
float inv_logerp(float a, float b, float r);

static void _input(Editor *const editor, float dt, LW_Context *const context);
static void _recalcViewMatrices(Editor *const editor, float dt, LW_Context *const context);

static bool _pointsAreEqual(lw_vec2 const a, lw_vec2 const b, float epsilon) {
    lw_vec2 difference;
    // check
    difference[0] = b[0] - a[0];
    difference[1] = b[1] - a[1];
    float sqdst   = lw_dot2d(difference, difference);

    return sqdst <= epsilon * epsilon;
}

static bool _validPortalOverlap(LW_LineDef const *const line0, LW_LineDef const *const line1) {
    lw_vec2 a, b;
    a[0] = line1->start[0], a[1] = line1->start[1];
    b[0] = line1->sector->walls[line1->next].start[0], b[1] = line1->sector->walls[line1->next].start[1];

    // // check
    // difference[0] = b[0] - line0->start[0];
    // difference[1] = b[1] - line0->start[1];
    // sqdst         = lw_dot2d(difference, difference);

    if (!_pointsAreEqual(b, line0->start, AUTO_PORTAL_EPSILON)) return false;

    // difference[0] = a[0] - line0->sector->walls[line0->next].start[0];
    // difference[1] = a[1] - line0->sector->walls[line0->next].start[1];
    // sqdst         = lw_dot2d(difference, difference);

    return _pointsAreEqual(a, line0->sector->walls[line0->next].start, AUTO_PORTAL_EPSILON);
}

static bool _linesAreEqual(LW_LineDef const *const line0, LW_LineDef const *const line1) {
    lw_vec2 a, b, difference;
    float sqdst;
    a[0] = line1->start[0], a[1] = line1->start[1];
    b[0] = line1->sector->walls[line1->next].start[0], b[1] = line1->sector->walls[line1->next].start[1];

    // check
    difference[0] = a[0] - line0->start[0];
    difference[1] = a[1] - line0->start[1];
    sqdst         = lw_dot2d(difference, difference);

    if (sqdst > AUTO_PORTAL_EPSILON) return false;

    difference[0] = b[0] - line0->sector->walls[line0->next].start[0];
    difference[1] = b[1] - line0->sector->walls[line0->next].start[1];
    sqdst         = lw_dot2d(difference, difference);

    return sqdst <= AUTO_PORTAL_EPSILON;
}

int editor2dUpdate(Editor *const editor, float dt, LW_Context *const context) {
    _input(editor, dt, context);
    _recalcViewMatrices(editor, dt, context);

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };
    lw_mat4MulVec4(editor->data2d.to_world_mat, mouse_screen_posv4, editor->data2d.mouse_world_pos);

    // swap views
    if (isInputActionDown(context, InputName_swapViews)) {
        editor->view_3d              = true;
        editor->data3d.camera.pos[0] = editor->data2d.mouse_world_pos[0];
        editor->data3d.camera.pos[1] = editor->data2d.mouse_world_pos[1];

        switch (editor->data2d.cam_rot) {
            case 0: editor->data3d.camera.yaw = 0.0f; break;
            case 1: editor->data3d.camera.yaw = M_PI * 0.5; break;
            case 2: editor->data3d.camera.yaw = M_PI; break;
            case 3: editor->data3d.camera.yaw = -M_PI * 0.5F; break;
        }

        editor->data3d.camera.sector = lw_getSector(editor->world, editor->data3d.camera.pos);
        if (editor->data3d.camera.sector != NULL) {
            editor->data3d.camera.pos[2] = editor->data3d.camera.sector->subsectors[0].floor_height + CAMERA_3D_HEIGHT;
        }

        return LW_EXIT_OK;
    }

    // calculate snapped position
    if (editor->data2d.grid_active) {
        editor->data2d.mouse_snapped_pos[0] = roundf(editor->data2d.mouse_world_pos[0] / editor->data2d.grid_size) * editor->data2d.grid_size;
        editor->data2d.mouse_snapped_pos[1] = roundf(editor->data2d.mouse_world_pos[1] / editor->data2d.grid_size) * editor->data2d.grid_size;
    } else {
        editor->data2d.mouse_snapped_pos[0] = editor->data2d.mouse_world_pos[0];
        editor->data2d.mouse_snapped_pos[1] = editor->data2d.mouse_world_pos[1];
    }

    // state machine
    switch (editor->data2d.state) {
        case StateIdle:
            if (isInputActionDown(context, InputName_newSector)) {
                editor->data2d.selected_points_len = 0; // deselect all points

                editor->data2d.new_sector_capacity = 16;

                editor->data2d.new_sector.num_subsectors               = 1;
                editor->data2d.new_sector.subsectors                   = malloc(editor->data2d.new_sector.num_subsectors * sizeof(*editor->data2d.new_sector.subsectors));
                editor->data2d.new_sector.subsectors[0].ceiling_height = 3.0f;
                editor->data2d.new_sector.subsectors[0].floor_height   = 0.0f;

                editor->data2d.new_sector.walls                  = malloc(editor->data2d.new_sector_capacity * sizeof(*editor->data2d.new_sector.walls));
                editor->data2d.new_sector.walls[0].start[0]      = editor->data2d.mouse_snapped_pos[0];
                editor->data2d.new_sector.walls[0].start[1]      = editor->data2d.mouse_snapped_pos[1];
                editor->data2d.new_sector.walls[0].next          = UNDEFINED;
                editor->data2d.new_sector.walls[0].prev          = UNDEFINED;
                editor->data2d.new_sector.walls[0].portal_sector = NULL;
                editor->data2d.new_sector.walls[0].portal_wall   = NULL;

                editor->data2d.new_sector.num_walls = 1;

                editor->data2d.state = StateCreateSector;

            } else if (isInputActionDown(context, InputName_deletePoints)) {
                // delete selected points
                for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                    LW_LineDef *line  = editor->data2d.selected_points[i];
                    LW_Sector *sector = line->sector;

                    // early out and remove sector
                    if (sector->num_walls <= 3) {
                        // remove all selected points from that sector
                        unsigned n = editor->data2d.selected_points_len;
                        for (unsigned j = i + 1; j < n; ++j) {
                            LW_LineDef *jline = editor->data2d.selected_points[j];
                            if (jline->sector == sector) {
                                // swap remove
                                --editor->data2d.selected_points_len;
                                editor->data2d.selected_points[j] = editor->data2d.selected_points[editor->data2d.selected_points_len];
                            }
                        }

                        // unset portals
                        for (unsigned j = 0; j < sector->num_walls; ++j) {
                            if (sector->walls[j].portal_wall != NULL) {
                                sector->walls[j].portal_wall->portal_sector = NULL;
                                sector->walls[j].portal_wall->portal_wall   = NULL;
                            }
                        }

                        // remove sector
                        LW_SectorList_remove(&editor->world.sectors, sector);
                        continue;
                    }

                    // fill in missing wall
                    LW_LineDef *prev = &sector->walls[line->prev];
                    LW_LineDef *next = &sector->walls[line->next];
                    unsigned index   = prev->next;

                    prev->next = line->next;
                    next->prev = line->prev;
                    lw_recalcLinePlane(prev);

                    // remove portal if needed
                    if (line->portal_sector != NULL) {
                        line->portal_wall->portal_wall   = NULL;
                        line->portal_wall->portal_sector = NULL;
                        line->portal_wall                = NULL;
                        line->portal_sector              = NULL;
                    }

                    if (prev->portal_sector != NULL) {
                        prev->portal_wall->portal_wall   = NULL;
                        prev->portal_wall->portal_sector = NULL;
                        prev->portal_wall                = NULL;
                        prev->portal_sector              = NULL;
                    }

                    // swap remove
                    --sector->num_walls;
                    if (&sector->walls[sector->num_walls] != line) {
                        memcpy(line, &sector->walls[sector->num_walls], sizeof(*line));
                        sector->walls[line->prev].next = index;
                        sector->walls[line->next].prev = index;
                    }
                }

                editor->data2d.selected_points_len = 0;

            } else if (isInputActionDown(context, InputName_splitLine)) {
                // Insert point in line

                // if specter select on:
                // for every line in range, add point at mouse pos
                // if not specter select:
                // add point at mouse pos for first line

                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
                lw_vec2 seg[2];
                lw_vec4 closest_point = { 0.0f, 0.0f, 0.0f, 1.0f };
                lw_vec2 difference;

                LW_SectorListNode *node = editor->world.sectors.head;
                for (; node != NULL; node = node->next) {
                    LW_Sector *sec = &node->item;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        b[0] = sec->walls[line->next].start[0], b[1] = sec->walls[line->next].start[1];

                        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, c);
                        lw_mat4MulVec4(editor->data2d.to_screen_mat, b, d);

                        seg[0][0] = c[0], seg[0][1] = c[1];
                        seg[1][0] = d[0], seg[1][1] = d[1];

                        lw_closestPointOnSegment(seg, mouse_screen_posv4, closest_point);
                        difference[0] = closest_point[0] - mouse_screen_posv4[0];
                        difference[1] = closest_point[1] - mouse_screen_posv4[1];

                        float sqdst = lw_dot2d(difference, difference);

                        if (sqdst < LINE_SELECTION_RADIUS * LINE_SELECTION_RADIUS) {
                            // remove portal
                            if (line->portal_sector != NULL) {
                                line->portal_wall->portal_sector = NULL;
                                line->portal_wall->portal_wall   = NULL;
                                line->portal_sector              = NULL;
                                line->portal_wall                = NULL;
                            }

                            // new point in world space
                            // lw_mat4MulVec4(editor->data2d.to_world_mat, closest_point, c);

                            // save old pointer for later
                            LW_LineDef *old_wall_ptr = sec->walls;

                            // add new point
                            ++sec->num_walls;
                            sec->walls                 = realloc(sec->walls, sec->num_walls * sizeof(*sec->walls));
                            line                       = &sec->walls[i];
                            LW_LineDef *const new_line = &sec->walls[sec->num_walls - 1];

                            // update portal pointers if needed
                            if (old_wall_ptr != sec->walls) {
                                for (unsigned j = 0; j < sec->num_walls - 1; ++j) {
                                    if (sec->walls[j].portal_wall != NULL) {
                                        intptr_t ptr_offset                    = (intptr_t)sec->walls[j].portal_wall->portal_wall - (intptr_t)old_wall_ptr;
                                        sec->walls[j].portal_wall->portal_wall = (LW_LineDef *)((intptr_t)sec->walls + ptr_offset);
                                    }
                                }
                            }

                            // initialize new point
                            // new_line->start[0] = c[0], new_line->start[1] = c[1];
                            new_line->sector   = sec;
                            new_line->start[0] = editor->data2d.mouse_snapped_pos[0], new_line->start[1] = editor->data2d.mouse_snapped_pos[1];
                            // memcpy(new_line->plane, line->plane, sizeof(new_line->plane)); // point added on line means point added on plane
                            new_line->portal_sector = NULL;
                            new_line->portal_wall   = NULL;

                            // insert into polygon
                            new_line->next              = line->next;
                            new_line->prev              = i;
                            sec->walls[line->next].prev = sec->num_walls - 1;
                            line->next                  = sec->num_walls - 1;

                            lw_recalcLinePlane(new_line);
                            lw_recalcLinePlane(&sec->walls[new_line->prev]);

                            if (!editor->data2d.specter_select) goto _end_sector_loop;
                            break;
                        }
                    }
                }
            _end_sector_loop:

            } else if (isInputActionDown(context, InputName_autoPortal)) {
                // auto portal
                // find closest line
                // check for any other lines with endpoints in same location
                // set both to portals

                LW_Sector *closest_sector = NULL;
                LW_LineDef *closest       = NULL;
                float min_dist            = LINE_SELECTION_RADIUS * LINE_SELECTION_RADIUS;

                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
                lw_vec2 seg[2];
                lw_vec2 closest_point;
                lw_vec2 difference;

                LW_SectorListNode *node = editor->world.sectors.head;
                for (; node != NULL; node = node->next) {
                    LW_Sector *sec = &node->item;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        b[0] = sec->walls[line->next].start[0], b[1] = sec->walls[line->next].start[1];

                        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, c);
                        lw_mat4MulVec4(editor->data2d.to_screen_mat, b, d);

                        seg[0][0] = c[0], seg[0][1] = c[1];
                        seg[1][0] = d[0], seg[1][1] = d[1];

                        lw_closestPointOnSegment(seg, mouse_screen_posv4, closest_point);
                        difference[0] = closest_point[0] - mouse_screen_posv4[0];
                        difference[1] = closest_point[1] - mouse_screen_posv4[1];

                        float sqdst = lw_dot2d(difference, difference);

                        if (sqdst < min_dist) {
                            closest        = line;
                            closest_sector = sec;
                            min_dist       = sqdst;
                            break;
                        }
                    }
                }

                if (closest != NULL) {
                    if (closest->portal_sector == NULL) {
                        // add portal
                        node = editor->world.sectors.head;
                        for (; node != NULL; node = node->next) {
                            LW_Sector *sec = &node->item;
                            if (sec == closest_sector) continue; // can't be in the same sector

                            for (unsigned i = 0; i < sec->num_walls; ++i) {
                                LW_LineDef *const line = &sec->walls[i];
                                if (_validPortalOverlap(closest, line)) {
                                    // do the portal stuff
                                    closest->portal_sector = sec;
                                    closest->portal_wall   = line;

                                    line->portal_sector = closest_sector;
                                    line->portal_wall   = closest;

                                    goto _end_outer;
                                }
                            }
                        }
                    _end_outer:
                    } else {
                        // remove portal
                        closest->portal_wall->portal_sector = NULL;
                        closest->portal_wall->portal_wall   = NULL;
                        closest->portal_sector              = NULL;
                        closest->portal_wall                = NULL;
                    }
                }

            } else if (isInputActionDown(context, InputName_selectionBox)) {
                editor->data2d.state = StateSelectionBox;
                for (unsigned i = 0; i < 2; ++i) {
                    editor->data2d.selection_box.low[i]   = mouse_screen_posv4[i];
                    editor->data2d.selection_box.high[i]  = mouse_screen_posv4[i];
                    editor->data2d.selection_box_pivot[i] = mouse_screen_posv4[i];
                }
            } else if (isInputActionDown(context, InputName_multiSelect)) {
                // if selecting already selected point, remove point
                LW_Circle circle;
                circle.radius = POINT_RENDER_RADIUS;

                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;

                bool deselected = false;
                for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                    LW_LineDef *const line = editor->data2d.selected_points[i];

                    a[0] = line->start[0], a[1] = line->start[1];
                    lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                    circle.pos[0] = b[0];
                    circle.pos[1] = b[1];

                    if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                        deselected                        = true;
                        editor->data2d.selected_points[i] = editor->data2d.selected_points[editor->data2d.selected_points_len - 1];
                        --editor->data2d.selected_points_len;

                        if (!editor->data2d.specter_select) break;
                    }
                }
                if (deselected) break;

                // select points

                if (editor->data2d.selected_points_len >= editor->data2d.selected_points_capacity) {
                    editor->data2d.selected_points_capacity = editor->data2d.selected_points_len + 16;
                    editor->data2d.selected_points          = realloc(editor->data2d.selected_points, editor->data2d.selected_points_capacity * sizeof(*editor->data2d.selected_points));
                }

                LW_SectorListNode *node = editor->world.sectors.head;
                for (; node != NULL; node = node->next) {
                    LW_Sector const *sec = &node->item;
                    bool do_break        = false;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                        circle.pos[0] = b[0];
                        circle.pos[1] = b[1];

                        if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                            editor->data2d.selected_points[editor->data2d.selected_points_len] = line;
                            ++editor->data2d.selected_points_len;
                            do_break = true;
                            break;
                        }
                    }
                    if (!editor->data2d.specter_select && do_break) break;
                }

            } else if (isInputActionDown(context, InputName_selectPoint)) {
                // if selecting already selected point, change to move points
                LW_Circle circle;
                circle.radius = POINT_RENDER_RADIUS;

                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;

                bool do_break = false;
                for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                    LW_LineDef *const line = editor->data2d.selected_points[i];

                    a[0] = line->start[0], a[1] = line->start[1];
                    lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                    circle.pos[0] = b[0];
                    circle.pos[1] = b[1];

                    if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                        // move
                        editor->data2d.state              = StateMovePoints;
                        editor->data2d.select_point_index = i;
                        editor->data2d.move_origin[0]     = editor->data2d.selected_points[editor->data2d.select_point_index]->start[0];
                        editor->data2d.move_origin[1]     = editor->data2d.selected_points[editor->data2d.select_point_index]->start[1];
                        do_break                          = true;
                        break;
                    }
                }
                if (do_break) break;

                // select point
                editor->data2d.selected_points_len = 0;

                if (editor->data2d.selected_points_len >= editor->data2d.selected_points_capacity) {
                    editor->data2d.selected_points_capacity = editor->data2d.selected_points_len + 16;
                    editor->data2d.selected_points          = realloc(editor->data2d.selected_points, editor->data2d.selected_points_capacity * sizeof(*editor->data2d.selected_points));
                }

                LW_SectorListNode *node = editor->world.sectors.head;
                for (; node != NULL; node = node->next) {
                    LW_Sector const *sec = &node->item;
                    bool do_break        = false;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                        circle.pos[0] = b[0];
                        circle.pos[1] = b[1];

                        if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                            editor->data2d.selected_points[editor->data2d.selected_points_len] = line;
                            ++editor->data2d.selected_points_len;
                            do_break = true;
                            break;
                        }
                    }
                    if (!editor->data2d.specter_select && do_break) break;
                }

                if (editor->data2d.selected_points_len > 0) {
                    // move point if selected
                    editor->data2d.select_point_index = 0;
                    editor->data2d.state              = StateMovePoints;
                    editor->data2d.move_origin[0]     = editor->data2d.selected_points[editor->data2d.select_point_index]->start[0];
                    editor->data2d.move_origin[1]     = editor->data2d.selected_points[editor->data2d.select_point_index]->start[1];
                }
            } else if (isInputActionDown(context, InputName_selectSector)) {
                LW_Sector *sector = lw_getSector(editor->world, editor->data2d.mouse_world_pos);
                if (sector != NULL) {
                    editor->data2d.selected_points_len = sector->num_walls;

                    if (editor->data2d.selected_points_len >= editor->data2d.selected_points_capacity) {
                        editor->data2d.selected_points_capacity = editor->data2d.selected_points_len;
                        editor->data2d.selected_points          = realloc(editor->data2d.selected_points, editor->data2d.selected_points_capacity * sizeof(*editor->data2d.selected_points));
                    }

                    for (unsigned i = 0; i < sector->num_walls; ++i) {
                        editor->data2d.selected_points[i] = &sector->walls[i];
                    }
                }
            } else if (isInputActionDown(context, InputName_multiSelectSector)) {
                LW_Sector *sector = lw_getSector(editor->world, editor->data2d.mouse_world_pos);
                if (sector != NULL) {
                    unsigned start_offset = editor->data2d.selected_points_len;
                    editor->data2d.selected_points_len += sector->num_walls;

                    if (editor->data2d.selected_points_len >= editor->data2d.selected_points_capacity) {
                        editor->data2d.selected_points_capacity = editor->data2d.selected_points_len;
                        editor->data2d.selected_points          = realloc(editor->data2d.selected_points, editor->data2d.selected_points_capacity * sizeof(*editor->data2d.selected_points));
                    }

                    for (unsigned i = 0; i < sector->num_walls; ++i) {
                        editor->data2d.selected_points[start_offset + i] = &sector->walls[i];
                    }
                }
            }
            break;
            /////////////////////////////////////////////////////////////
        case StateMovePoints:
            if (isInputActionDown(context, InputName_cancel)) {
                // cancel
                lw_vec2 delta;
                delta[0] = editor->data2d.move_origin[0] - editor->data2d.selected_points[editor->data2d.select_point_index]->start[0];
                delta[1] = editor->data2d.move_origin[1] - editor->data2d.selected_points[editor->data2d.select_point_index]->start[1];

                for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                    LW_LineDef *const line = editor->data2d.selected_points[i];
                    line->start[0] += delta[0];
                    line->start[1] += delta[1];
                }

                editor->data2d.state = StateIdle;
                break;

            } else if (isInputActionUp(context, InputName_selectPoint)) {
                for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                    // If moving portal, check if portal should be disconnected
                    // recalc line plane

                    LW_LineDef *line = editor->data2d.selected_points[i];
                    if (line->portal_wall != NULL) {
                        if (!_validPortalOverlap(line->portal_wall, line)) {
                            // remove portal
                            line->portal_wall->portal_wall   = NULL;
                            line->portal_wall->portal_sector = NULL;
                            line->portal_wall                = NULL;
                            line->portal_sector              = NULL;
                        }
                    }
                    lw_recalcLinePlane(line);

                    line = &line->sector->walls[line->prev];
                    if (line->portal_wall != NULL) {
                        if (!_validPortalOverlap(line->portal_wall, line)) {
                            // remove portal
                            line->portal_wall->portal_wall   = NULL;
                            line->portal_wall->portal_sector = NULL;
                            line->portal_wall                = NULL;
                            line->portal_sector              = NULL;
                        }
                    }
                    lw_recalcLinePlane(line);
                }


                editor->data2d.state = StateIdle;
                break;
            }

            lw_vec2 delta;
            delta[0] = editor->data2d.mouse_snapped_pos[0] - editor->data2d.selected_points[editor->data2d.select_point_index]->start[0];
            delta[1] = editor->data2d.mouse_snapped_pos[1] - editor->data2d.selected_points[editor->data2d.select_point_index]->start[1];

            for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
                LW_LineDef *const line = editor->data2d.selected_points[i];
                line->start[0] += delta[0];
                line->start[1] += delta[1];
            }
            break;
            ///////////////////////////////////////////////
        case StateSelectionBox:
            if (isInputActionDown(context, InputName_cancel)) {
                // cancel
                editor->data2d.state = StateIdle;
            } else if (isInputActionDown(context, InputName_selectPoint)) {
                // do selection
                lw_vec4 a               = { 0.0f, 0.0f, 0.0f, 1.0f }, b;
                LW_SectorListNode *node = editor->world.sectors.head;

                // pseudo minkowski's difference
                editor->data2d.selection_box.low[0] -= POINT_RENDER_RADIUS;
                editor->data2d.selection_box.low[1] -= POINT_RENDER_RADIUS;
                editor->data2d.selection_box.high[0] += POINT_RENDER_RADIUS;
                editor->data2d.selection_box.high[1] += POINT_RENDER_RADIUS;

                for (; node != NULL; node = node->next) {
                    LW_Sector const *sec = &node->item;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        // check that point is not already selected
                        bool already_selected = false;
                        for (unsigned j = 0; j < editor->data2d.selected_points_len; ++j) {
                            if (editor->data2d.selected_points[j] == line) {
                                already_selected = true;
                                break;
                            }
                        }
                        if (already_selected) continue;

                        a[0] = line->start[0], a[1] = line->start[1];
                        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                        if (lw_pointInAabb(editor->data2d.selection_box, b)) {
                            if (editor->data2d.selected_points_len >= editor->data2d.selected_points_capacity) {
                                editor->data2d.selected_points_capacity = editor->data2d.selected_points_len + 16;
                                editor->data2d.selected_points          = realloc(editor->data2d.selected_points, editor->data2d.selected_points_capacity * sizeof(*editor->data2d.selected_points));
                            }
                            editor->data2d.selected_points[editor->data2d.selected_points_len] = line;
                            ++editor->data2d.selected_points_len;
                        }
                    }
                }

                editor->data2d.state = StateIdle;
            } else {
                for (unsigned i = 0; i < 2; ++i) {
                    if (mouse_screen_posv4[i] < editor->data2d.selection_box_pivot[i]) {
                        editor->data2d.selection_box.low[i] = mouse_screen_posv4[i];
                    } else if (mouse_screen_posv4[i] > editor->data2d.selection_box_pivot[i]) {
                        editor->data2d.selection_box.high[i] = mouse_screen_posv4[i];
                    }
                }
            }
            break;
            ///////////////////////////////////////////////
        case StateCreateSector:
            if (isInputActionDown(context, InputName_cancel)) {
                // cancel
                editor->data2d.state = StateIdle;

            } else if (isInputActionDown(context, InputName_newSector)) {
                LW_Circle circle;
                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;

                lw_vec4 snapped_screen_pos;
                lw_mat4MulVec4(editor->data2d.to_screen_mat, editor->data2d.mouse_snapped_pos, snapped_screen_pos);

                circle.radius = POINT_RENDER_RADIUS * 2.0f; // overlapping the vertex at the mouse position

                // if we are in range of first point, finish sector
                // if we are in range of prev point, do nothing
                // else, add new wall

                a[0] = editor->data2d.new_sector.walls[0].start[0];
                a[1] = editor->data2d.new_sector.walls[0].start[1];
                lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                circle.pos[0] = b[0];
                circle.pos[1] = b[1];

                if (lw_pointInCircle(circle, snapped_screen_pos)) {
                    if (editor->data2d.new_sector.num_walls > 2) {
                        // finish sector
                        LW_Sector *new_sector = &editor->data2d.new_sector;

                        new_sector->walls[new_sector->num_walls - 1].next = 0;
                        new_sector->walls[0].prev                         = new_sector->num_walls - 1;

                        {
                            // ensure points are counter-clockwise
                            // https://stackoverflow.com/questions/1165647/how-to-determine-if-a-list-of-polygon-points-are-in-clockwise-order

                            float sum;
                            for (unsigned i = 0; i < new_sector->num_walls; ++i) {
                                sum += (new_sector->walls[(new_sector->walls[i].next)].start[0] - new_sector->walls[i].start[0]) *
                                       (new_sector->walls[(new_sector->walls[i].next)].start[1] + new_sector->walls[i].start[1]);
                            }

                            if (sum > 1.0f) {
                                // reverse winding
                                for (unsigned i = 0; i < editor->data2d.new_sector.num_walls; ++i) {
                                    swap(unsigned, editor->data2d.new_sector.walls[i].prev, editor->data2d.new_sector.walls[i].next);
                                }
                            }
                        }

                        // add to world
                        LW_SectorList_push_back(&editor->world.sectors, editor->data2d.new_sector);
                        new_sector                               = &editor->world.sectors.tail->item;
                        new_sector->subsectors[0].floor_height   = INFINITY;
                        new_sector->subsectors[0].ceiling_height = -INFINITY;

                        // finish lines
                        for (unsigned i = 0; i < new_sector->num_walls; ++i) {
                            // set sector
                            new_sector->walls[i].sector = new_sector;
                            // calculate wall planes
                            lw_recalcLinePlane(&new_sector->walls[i]);
                        }

                        // clip world around new sector

                        LW_SectorListNode *node = editor->world.sectors.head;
                        // tail is the just added node, and if we xor with that the entire sector gets removed
                        for (; node != editor->world.sectors.tail; node = node->next) {
                            LW_Sector *sector = &node->item;

                            // check if new sector is inside or outside of sector
                            bool all_inside = true;
                            bool all_outside = true;
                            for (unsigned i = 0; i < new_sector->num_walls; ++i) {
                                if (!lw_pointInSector(*sector, new_sector->walls[i].start, 0.003f)) {
                                    all_inside = false;
                                } else if (lw_pointInSector(*sector, new_sector->walls[i].start, 0.0f)) {
                                    all_outside = false;
                                }

                                if(!all_inside && !all_outside) break;
                            }

                            if (all_inside) {
                                // add entire sector as portals
                                new_sector->subsectors[0].floor_height   = sector->subsectors[0].floor_height;
                                new_sector->subsectors[0].ceiling_height = sector->subsectors[sector->num_subsectors - 1].ceiling_height;

                                LW_LineDef *old_walls = sector->walls;
                                sector->walls         = realloc(sector->walls, (sector->num_walls + new_sector->num_walls) * sizeof(*sector->walls));

                                // update portals
                                if (old_walls != sector->walls) {
                                    for (unsigned i = 0; i < sector->num_walls; ++i) {
                                        if (sector->walls[i].portal_wall != NULL) {
                                            sector->walls[i].portal_wall->portal_wall = &sector->walls[i];
                                        }
                                    }
                                }

                                unsigned i = 0, j = sector->num_walls, k = 0;
                                do {
                                    new_sector->walls[i].portal_sector = sector;
                                    new_sector->walls[i].portal_wall   = &sector->walls[j];

                                    // create portal
                                    sector->walls[j].portal_sector = new_sector;
                                    sector->walls[j].portal_wall   = &new_sector->walls[i];
                                    // init wall
                                    sector->walls[j].sector   = sector;
                                    sector->walls[j].start[0] = new_sector->walls[new_sector->walls[i].next].start[0];
                                    sector->walls[j].start[1] = new_sector->walls[new_sector->walls[i].next].start[1];
                                    sector->walls[j].next     = sector->num_walls + (k - 1 + new_sector->num_walls) % new_sector->num_walls;
                                    sector->walls[j].prev     = sector->num_walls + (k + 1) % new_sector->num_walls;

                                    if (k != 0) {
                                        lw_recalcLinePlane(&sector->walls[j]);
                                    }

                                    i = new_sector->walls[i].next;
                                    ++j;
                                    ++k;
                                } while (i != 0);

                                // recalc last line because not enough information was available the first time
                                lw_recalcLinePlane(&sector->walls[sector->num_walls]);

                                sector->num_walls += new_sector->num_walls;
                            } else if(!all_outside) {
                                // Weiler-Atherton clipping
                                // generate intersections
                                // construct subject and clipper arrays
                                // put all leaving intersections into an array
                                // grab and mark first leaving as used
                                // start at first leaving on clipper and walk backwards
                                // if another intersection is found, move to the corresponding element in subject array
                                // walk forwards until another intersection is found, then move back to clipper
                                // repeat until back at beginning

                                if (new_sector->subsectors[0].floor_height > sector->subsectors[0].floor_height)
                                    new_sector->subsectors[0].floor_height = sector->subsectors[0].floor_height;
                                if (new_sector->subsectors[0].ceiling_height < sector->subsectors[sector->num_subsectors - 1].ceiling_height)
                                    new_sector->subsectors[0].ceiling_height = sector->subsectors[sector->num_subsectors - 1].ceiling_height;

                                // TODO: Save portal information

                                struct WaItem {
                                    lw_vec2 point;
                                    float t_val;
                                    unsigned intersection_index;
                                };

                                struct WaIntersection {
                                    lw_vec2 point;
                                    float t_val, u_val;
                                    unsigned subject_a, subject_b;
                                    unsigned clipper_a, clipper_b;
                                    bool entering;
                                };


                                unsigned buffer_size = sector->num_walls * new_sector->num_walls;
                                // make maximum possible buffer
                                struct WaIntersection *intersections = calloc(buffer_size, sizeof(*intersections));
                                unsigned intersection_len            = 0;

                                // set true if this line overlaps a line in the other sector
                                bool *subject_overlap_list = calloc(sector->num_walls, sizeof(*subject_overlap_list));
                                bool *clipper_overlap_list = calloc(new_sector->num_walls, sizeof(*clipper_overlap_list));

                                {
                                    // generate diff lists
                                    for (unsigned i = 0; i < sector->num_walls; ++i) {
                                        lw_vec2 line0[2] = {
                                            { sector->walls[i].start[0], sector->walls[i].start[1] },
                                            { sector->walls[sector->walls[i].next].start[0], sector->walls[sector->walls[i].next].start[1] },
                                        };
                                        lw_vec2 d0 = { line0[1][0] - line0[0][0], line0[1][1] - line0[0][1] };

                                        for (unsigned j = 0; j < new_sector->num_walls; ++j) {
                                            lw_vec2 line1[2] = {
                                                { new_sector->walls[j].start[0], new_sector->walls[j].start[1] },
                                                { new_sector->walls[new_sector->walls[j].next].start[0], new_sector->walls[new_sector->walls[j].next].start[1] },
                                            };
                                            lw_vec2 d1 = { line1[1][0] - line1[0][0], line1[1][1] - line1[0][1] };

                                            // if lines are parallel and the distance of either point to the line < epsilon they overlap
                                            if (fabsf(lw_cross2d(d0, d1)) < 0.003) {
                                                lw_vec2 p0, p1;
                                                lw_closestPointOnSegment(line0, line1[0], p0);
                                                lw_closestPointOnSegment(line0, line1[1], p1);

                                                if (lw_sqrDist2d(line1[0], p0) < 0.003 * 0.003 || lw_sqrDist2d(line1[1], p1) < 0.003 * 0.003) {
                                                    subject_overlap_list[i] = true;
                                                    clipper_overlap_list[j] = true;
                                                }
                                            }
                                        }
                                    }
                                }

                                // generate intersections
                                for (unsigned i = 0; i < sector->num_walls; ++i) {
                                    lw_vec2 line0[2] = {
                                        { sector->walls[i].start[0], sector->walls[i].start[1] },
                                        { sector->walls[sector->walls[i].next].start[0], sector->walls[sector->walls[i].next].start[1] },
                                    };

                                    for (unsigned j = 0; j < new_sector->num_walls; ++j) {
                                        if (subject_overlap_list[i]) {
                                            // this line overlaps, only test against the unique lines of clipper
                                            if (clipper_overlap_list[j]) continue;
                                        }

                                        lw_vec2 line1[2] = {
                                            { new_sector->walls[j].start[0], new_sector->walls[j].start[1] },
                                            { new_sector->walls[new_sector->walls[j].next].start[0], new_sector->walls[new_sector->walls[j].next].start[1] },
                                        };

                                        float t, u;
                                        if (lw_intersectSegmentSegment(line0, line1, &t, &u)) {
                                            intersections[intersection_len].point[0] = lerp(line0[0][0], line0[1][0], t);
                                            intersections[intersection_len].point[1] = lerp(line0[0][1], line0[1][1], t);

                                            // check that point does not already exist
                                            for (unsigned k = 0; k < intersection_len; ++k) {
                                                if (_pointsAreEqual(intersections[k].point, intersections[intersection_len].point, 0.003)) {
                                                    goto _skip_adding_intersection;
                                                }
                                            }

                                            intersections[intersection_len].t_val     = t;
                                            intersections[intersection_len].u_val     = u;
                                            intersections[intersection_len].clipper_a = j;
                                            intersections[intersection_len].clipper_b = new_sector->walls[j].next;
                                            intersections[intersection_len].subject_a = i;
                                            intersections[intersection_len].subject_b = sector->walls[i].next;
                                            ++intersection_len;

                                        _skip_adding_intersection:
                                        }
                                    }
                                }

                                unsigned subject_len = 0, clipper_len = 0;
                                struct WaItem *subject = calloc(sector->num_walls + intersection_len, sizeof(*subject));
                                struct WaItem *clipper = calloc(new_sector->num_walls + intersection_len, sizeof(*clipper));

                                {
                                    // generate subject
                                    unsigned i = 0;
                                    do {
                                        unsigned base_index                     = subject_len;
                                        subject[subject_len].point[0]           = sector->walls[i].start[0];
                                        subject[subject_len].point[1]           = sector->walls[i].start[1];
                                        subject[subject_len].intersection_index = UNDEFINED;
                                        ++subject_len;

                                        // add all intersecting points
                                        for (unsigned j = 0; j < intersection_len; ++j) {
                                            if (_pointsAreEqual(intersections[j].point, subject[base_index].point, 0.0003f)) {
                                                subject[base_index].intersection_index = j;

                                            } else if (intersections[j].subject_a == i && intersections[j].subject_b == sector->walls[i].next) {
                                                if (intersections[j].t_val < 0.0003f) {
                                                    // this means vertex added was the intersection point, don't re-add it, just change to intersection
                                                    subject[base_index].intersection_index = j;
                                                    continue;
                                                }

                                                if (intersections[j].t_val > 0.9997f) {
                                                    // this means the next vertex will be an intersection point
                                                    continue;
                                                }

                                                subject[subject_len].point[0]           = intersections[j].point[0];
                                                subject[subject_len].point[1]           = intersections[j].point[1];
                                                subject[subject_len].intersection_index = j;
                                                subject[subject_len].t_val              = intersections[j].t_val;
                                                ++subject_len;
                                            }
                                        }

                                        // sort by t
                                        for (unsigned i = base_index + 2; i < subject_len; ++i) {
                                            struct WaItem x = subject[i];
                                            unsigned j      = i;
                                            while (j > 0 && subject[j - 1].t_val > x.t_val) {
                                                subject[j] = subject[j - 1];
                                                --j;
                                            }
                                            subject[j] = x;
                                        }

                                        i = sector->walls[i].next;
                                    } while (i != 0);
                                }

                                {
                                    // generate clipper
                                    unsigned i = 0;
                                    do {
                                        unsigned base_index                     = clipper_len;
                                        clipper[clipper_len].point[0]           = new_sector->walls[i].start[0];
                                        clipper[clipper_len].point[1]           = new_sector->walls[i].start[1];
                                        clipper[clipper_len].intersection_index = UNDEFINED;
                                        ++clipper_len;

                                        // add all intersecting points
                                        for (unsigned j = 0; j < intersection_len; ++j) {
                                            if (_pointsAreEqual(intersections[j].point, clipper[base_index].point, 0.0003f)) {
                                                clipper[base_index].intersection_index = j;

                                            } else if (intersections[j].clipper_a == i && intersections[j].clipper_b == new_sector->walls[i].next) {
                                                if (intersections[j].u_val < 0.0003f) {
                                                    // this means vertex added was the intersection point, don't re-add it, just change to intersection
                                                    clipper[base_index].intersection_index = j;
                                                    continue;
                                                }

                                                if (intersections[j].u_val > 0.9997f) {
                                                    // this means the next vertex will be an intersection point
                                                    continue;
                                                }

                                                clipper[clipper_len].point[0]           = intersections[j].point[0];
                                                clipper[clipper_len].point[1]           = intersections[j].point[1];
                                                clipper[clipper_len].intersection_index = j;
                                                clipper[clipper_len].t_val              = intersections[j].u_val;
                                                ++clipper_len;
                                            }
                                        }

                                        // sort by t
                                        for (unsigned i = base_index + 2; i < clipper_len; ++i) {
                                            struct WaItem x = clipper[i];
                                            unsigned j      = i;
                                            while (j > 0 && clipper[j - 1].t_val > x.t_val) {
                                                clipper[j] = clipper[j - 1];
                                                --j;
                                            }
                                            clipper[j] = x;
                                        }

                                        i = new_sector->walls[i].next;
                                    } while (i != 0);
                                }

                                // set leaving/entering and generate leaving list
                                // rules:
                                //  1. a point is leaving if last point was inside
                                //  2. a point is entering if last point was outside
                                //  3. a point is inside if it is inside and not on an edge
                                //  4. an intersection is inside if it is entering

                                // find first intersection point in clipper where the prev point is not an intersection
                                // go around following rules until we reach the beginning

                                unsigned *leaving_list = calloc(intersection_len, sizeof(*leaving_list));
                                unsigned num_leaving   = 0;

                                {
                                    unsigned i = 0;
                                    for (; i < clipper_len; ++i) {
                                        // special modulo case for i == 0
                                        unsigned j = i != 0 ? i - 1 : clipper_len - 1;

                                        if (clipper[j].intersection_index == UNDEFINED && clipper[i].intersection_index != UNDEFINED) {
                                            break; // this is the start index
                                        }
                                    }

                                    if (i == clipper_len) {
                                        // special case where all points are intersections / no intersection???
                                        // need to solve for the first point, then the rest all works as expected
                                        // if a point is the start of a unique line, it is enterering, otherwise it is leaving
                                        // although overlap lists are stored in the same order as the raw array and not the same as the clipper array,
                                        //  because we added to the clipper array starting with index 0, those two should always be the same
                                        if(clipper[0].intersection_index != UNDEFINED) {
                                            intersections[clipper[0].intersection_index].entering = !clipper_overlap_list[0];
                                        }

                                        i = 0;
                                    }

                                    unsigned start = i;
                                    do {
                                        if (clipper[i].intersection_index != UNDEFINED) {
                                            unsigned j = i != 0 ? i - 1 : clipper_len - 1;
                                            if (clipper[j].intersection_index == UNDEFINED) {
                                                // point in poly
                                                intersections[clipper[i].intersection_index].entering = !lw_pointInSector(*sector, clipper[j].point, 0.004f);
                                            } else {
                                                // use enter state
                                                intersections[clipper[i].intersection_index].entering = !intersections[clipper[j].intersection_index].entering;
                                            }

                                            if (!intersections[clipper[i].intersection_index].entering) {
                                                // add to leaving list
                                                leaving_list[num_leaving] = clipper[i].intersection_index;
                                                ++num_leaving;
                                            }
                                        }

                                        i = (i + 1) % clipper_len;
                                    } while (i != start);
                                }

                                // Construct the new polygons!!!!!!!

                                // reconstruct new sector to match clipping list
                                new_sector->num_walls = clipper_len;
                                new_sector->walls     = realloc(new_sector->walls, new_sector->num_walls * sizeof(*new_sector->walls));
                                for (unsigned i = new_sector->num_walls; i > 0;) {
                                    --i;

                                    new_sector->walls[i].sector        = new_sector;
                                    new_sector->walls[i].start[0]      = clipper[i].point[0];
                                    new_sector->walls[i].start[1]      = clipper[i].point[1];
                                    new_sector->walls[i].next          = (i + 1) % new_sector->num_walls;
                                    new_sector->walls[i].prev          = (i - 1 + new_sector->num_walls) % new_sector->num_walls;
                                    new_sector->walls[i].portal_sector = NULL;
                                    new_sector->walls[i].portal_wall   = NULL;

                                    lw_recalcLinePlane(&new_sector->walls[i]);
                                }
                                lw_recalcLinePlane(&new_sector->walls[new_sector->num_walls - 1]);

                                // while leaving list is not empty
                                // remove item from leaving list
                                // start at that item in clipper
                                // walk backwards along clipper and jump to subject when reaching an intersection
                                // walk forwards along subject, jumping when reaching an intersection
                                // remove item from leaving list if passed over

                                // required for splitting sector into multiple sectors
                                LW_Sector *construction_sector = sector;

                                while (true) {
                                    --num_leaving;
                                    unsigned start_intersection = leaving_list[num_leaving];

                                    struct WaItem *active_list = clipper;
                                    struct WaItem *other_list  = subject;
                                    unsigned active_len        = clipper_len;
                                    unsigned other_len         = subject_len;

                                    unsigned i = 0;
                                    // find start index for clipper
                                    for (; i < active_len; ++i) {
                                        if (active_list[i].intersection_index == start_intersection) break;
                                    }

                                    LW_LineDef *new_walls  = calloc(clipper_len + subject_len, sizeof(*new_walls));
                                    unsigned num_new_walls = 0;

                                    do {
                                        // push point
                                        // walk
                                        // jump if intersection
                                        // remove from leaving_list if needed

                                        new_walls[num_new_walls].start[0]      = active_list[i].point[0];
                                        new_walls[num_new_walls].start[1]      = active_list[i].point[1];
                                        new_walls[num_new_walls].sector        = construction_sector;
                                        new_walls[num_new_walls].next          = num_new_walls + 1;
                                        new_walls[num_new_walls].prev          = num_new_walls - 1;
                                        new_walls[num_new_walls].portal_sector = NULL;
                                        new_walls[num_new_walls].portal_wall   = NULL;

                                        ++num_new_walls;

                                        if (active_list == clipper)
                                            i = (i - 1 + active_len) % active_len;
                                        else
                                            i = (i + 1) % active_len;

                                        if (active_list[i].intersection_index != UNDEFINED) {
                                            unsigned index = active_list[i].intersection_index;
                                            // remove from leaving list
                                            for (unsigned j = 0; j < num_leaving; ++j) {
                                                if (leaving_list[j] == index) {
                                                    --num_leaving;
                                                    leaving_list[j] = leaving_list[num_leaving];
                                                }
                                            }

                                            // jump to other list
                                            swap(struct WaItem *, active_list, other_list);
                                            swap(unsigned, active_len, other_len);

                                            // find index
                                            for (i = 0; i < active_len; ++i) {
                                                if (active_list[i].intersection_index == index) break;
                                            }
                                        }

                                    } while (active_list[i].intersection_index != start_intersection);

                                    // fix last and first wall prev and next
                                    new_walls[0].prev                 = num_new_walls - 1;
                                    new_walls[num_new_walls - 1].next = 0;

                                    // finish and update sector
                                    new_walls = realloc(new_walls, num_new_walls * sizeof(*new_walls)); // free unused memory
                                    free(construction_sector->walls);
                                    construction_sector->walls     = new_walls;
                                    construction_sector->num_walls = num_new_walls;

                                    for (unsigned i = 0; i < construction_sector->num_walls; ++i) {
                                        lw_recalcLinePlane(&construction_sector->walls[i]);
                                    }

                                    if (num_leaving == 0) break;

                                    // create sector for next loop
                                    LW_Sector tmp_sector;

                                    tmp_sector.num_subsectors = sector->num_subsectors;
                                    tmp_sector.subsectors     = malloc(sector->num_subsectors * sizeof(*tmp_sector.subsectors));
                                    memcpy(tmp_sector.subsectors, sector->subsectors, sector->num_subsectors * sizeof(*sector->subsectors));
                                    tmp_sector.walls = NULL;

                                    LW_SectorList_push_back(&editor->world.sectors, tmp_sector);
                                    construction_sector = &editor->world.sectors.tail->item;
                                }

                                // cleanup
                                free(leaving_list);
                                free(subject);
                                free(clipper);
                                free(subject_overlap_list);
                                free(clipper_overlap_list);
                                free(intersections);
                            }
                        }

                        node = editor->world.sectors.head;
                        // tail is the just added node, and if we xor with that the entire sector gets removed
                        for (; node != editor->world.sectors.tail; node = node->next) {
                            LW_Sector *sector = &node->item;

                            // add portals
                            for (unsigned i = 0; i < new_sector->num_walls; ++i) {
                                for (unsigned j = 0; j < sector->num_walls; ++j) {
                                    if (_validPortalOverlap(&new_sector->walls[i], &sector->walls[j])) {
                                        // TODO: This could be somewhere else
                                        if (new_sector->subsectors[0].floor_height > sector->subsectors[0].floor_height)
                                            new_sector->subsectors[0].floor_height = sector->subsectors[0].floor_height;
                                        if (new_sector->subsectors[0].ceiling_height < sector->subsectors[sector->num_subsectors - 1].ceiling_height)
                                            new_sector->subsectors[0].ceiling_height = sector->subsectors[sector->num_subsectors - 1].ceiling_height;

                                        new_sector->walls[i].portal_sector = sector;
                                        sector->walls[j].portal_sector     = new_sector;
                                        new_sector->walls[i].portal_wall   = &sector->walls[j];
                                        sector->walls[j].portal_wall       = &new_sector->walls[i];
                                    }
                                }
                            }
                        }

                        if (new_sector->subsectors[0].floor_height == INFINITY) new_sector->subsectors[0].floor_height = 0.0f;
                        if (new_sector->subsectors[0].ceiling_height == -INFINITY) new_sector->subsectors[0].ceiling_height = 3.0f;
                        editor->data2d.state = StateIdle;
                    } else {
                        // TODO: Play sound
                    }
                    break;
                }

                a[0] = editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls - 1].start[0];
                a[1] = editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls - 1].start[1];
                lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);

                circle.pos[0] = b[0];
                circle.pos[1] = b[1];

                if (!lw_pointInCircle(circle, snapped_screen_pos)) {
                    // add new wall at mouse point
                    // connect last wall to this wall
                    if (editor->data2d.new_sector.num_walls >= editor->data2d.new_sector_capacity) {
                        editor->data2d.new_sector_capacity = editor->data2d.new_sector.num_walls + 16;
                        editor->data2d.new_sector.walls    = realloc(editor->data2d.new_sector.walls, editor->data2d.new_sector_capacity * sizeof(*editor->data2d.new_sector.walls));
                    }

                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].start[0]      = editor->data2d.mouse_snapped_pos[0];
                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].start[1]      = editor->data2d.mouse_snapped_pos[1];
                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].next          = UNDEFINED;
                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].portal_sector = NULL;
                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].portal_wall   = NULL;

                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls].prev     = editor->data2d.new_sector.num_walls - 1;
                    editor->data2d.new_sector.walls[editor->data2d.new_sector.num_walls - 1].next = editor->data2d.new_sector.num_walls;

                    ++editor->data2d.new_sector.num_walls;
                }
            }
            break;
    }

    return LW_EXIT_OK;
}

static void _drawGrid(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context);

int editor2dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {

    lw_fillBuffer(framebuffer, editor->c_background);

    _drawGrid(editor, framebuffer, context);

    {
        // draw origin

        lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;
        lw_mat4MulVec4(editor->data2d.to_screen_mat, a, b);
        LW_Color color = RGB(255 - editor->c_background.r, 255 - editor->c_background.g, 255 - editor->c_background.b);

        for (int i = -5; i <= 5; ++i) {
            lw_setPixel(framebuffer, (lw_uvec2){ b[0] + i, b[1] + i }, color);
            lw_setPixel(framebuffer, (lw_uvec2){ b[0] + i, b[1] - i }, color);
        }
    }

    {
        LW_Circlei circle;

        lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
        lw_ivec2 x, y;

        LW_Aabb selection_box;
        if (editor->data2d.state == StateSelectionBox) {
            selection_box.low[0]  = editor->data2d.selection_box.low[0] - POINT_RENDER_RADIUS;
            selection_box.low[1]  = editor->data2d.selection_box.low[1] - POINT_RENDER_RADIUS;
            selection_box.high[0] = editor->data2d.selection_box.high[0] + POINT_RENDER_RADIUS;
            selection_box.high[1] = editor->data2d.selection_box.high[1] + POINT_RENDER_RADIUS;
        }

        // draw all selected points
        circle.radius = POINT_RENDER_RADIUS + 1;
        for (unsigned i = 0; i < editor->data2d.selected_points_len; ++i) {
            LW_LineDef const *const line = editor->data2d.selected_points[i];

            a[0] = line->start[0], a[1] = line->start[1];
            lw_mat4MulVec4(editor->data2d.to_screen_mat, a, c);

            circle.pos[0] = c[0];
            circle.pos[1] = c[1];

            lw_fillCircle(framebuffer, circle, editor->c_sel_vertex);
        }

        // draw all sectors
        circle.radius           = POINT_RENDER_RADIUS;
        LW_SectorListNode *node = editor->world.sectors.head;
        for (; node != NULL; node = node->next) {
            LW_Sector const *sec = &node->item;
            for (unsigned i = 0; i < sec->num_walls; ++i) {
                LW_LineDef line = sec->walls[i];

                a[0] = line.start[0], a[1] = line.start[1];
                lw_mat4MulVec4(editor->data2d.to_screen_mat, a, c);

                if (line.next != UNDEFINED) {
                    // draw line that exits
                    b[0] = sec->walls[line.next].start[0];
                    b[1] = sec->walls[line.next].start[1];
                    lw_mat4MulVec4(editor->data2d.to_screen_mat, b, d);

                    x[0] = c[0], x[1] = c[1];
                    y[0] = d[0], y[1] = d[1];

                    LW_Color color;
                    if (line.portal_sector != NULL)
                        color = editor->c_portal;
                    else
                        color = editor->c_walls;

                    lw_drawLine(framebuffer, x, y, color);

                    // draw normal
                    x[0] = (d[0] + c[0]) * 0.5f;
                    x[1] = (d[1] + c[1]) * 0.5f;
                    y[0] = x[0] + line.plane[0] * 8.0f;
                    y[1] = x[1] - line.plane[1] * 8.0f;

                    lw_drawLine(framebuffer, x, y, color);
                }

                circle.pos[0] = c[0];
                circle.pos[1] = c[1];

                LW_Color color;
                if (editor->data2d.state == StateSelectionBox && lw_pointInAabb(selection_box, c)) {
                    color = editor->c_highlighted_vertices;
                } else {
                    color = editor->c_vertices;
                }

                lw_fillCircle(framebuffer, circle, color);
            }
        }
    }

    if (editor->data2d.state == StateSelectionBox) {
        LW_Recti rect;
        rect.pos[0] = editor->data2d.selection_box.low[0];
        rect.pos[1] = editor->data2d.selection_box.low[1];

        rect.size[0] = editor->data2d.selection_box.high[0] - editor->data2d.selection_box.low[0];
        rect.size[1] = editor->data2d.selection_box.high[1] - editor->data2d.selection_box.low[1];

        lw_drawRect(framebuffer, rect, editor->c_selection_box);
    }

    if (editor->data2d.state == StateCreateSector) {
        LW_Circlei circle;
        circle.radius = POINT_RENDER_RADIUS;

        lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
        lw_ivec2 x, y;

        // draw new sector
        for (unsigned i = 0; i < editor->data2d.new_sector.num_walls; ++i) {
            LW_LineDef line = editor->data2d.new_sector.walls[i];

            a[0] = line.start[0], a[1] = line.start[1];
            lw_mat4MulVec4(editor->data2d.to_screen_mat, a, c);

            if (line.next != UNDEFINED) {
                // draw line that exits
                b[0] = editor->data2d.new_sector.walls[line.next].start[0];
                b[1] = editor->data2d.new_sector.walls[line.next].start[1];
                lw_mat4MulVec4(editor->data2d.to_screen_mat, b, d);

                x[0] = c[0], x[1] = c[1];
                y[0] = d[0], y[1] = d[1];

                lw_drawLine(framebuffer, x, y, editor->c_new_walls);
            } else {
                // draw line to mouse
                b[0] = editor->data2d.mouse_snapped_pos[0];
                b[1] = editor->data2d.mouse_snapped_pos[1];
                lw_mat4MulVec4(editor->data2d.to_screen_mat, b, d);

                x[0] = c[0], x[1] = c[1];
                y[0] = d[0], y[1] = d[1];

                lw_drawLine(framebuffer, x, y, editor->c_new_walls);
            }

            circle.pos[0] = c[0];
            circle.pos[1] = c[1];

            if (i != 0) {
                lw_fillCircle(framebuffer, circle, editor->c_new_vertices);
            } else {
                ++circle.radius;
                lw_fillCircle(framebuffer, circle, editor->c_start_vertex);
                --circle.radius;
                lw_fillCircle(framebuffer, circle, editor->c_new_vertices);
            }
        }

        // draw point at mouse
        circle.pos[0] = d[0];
        circle.pos[1] = d[1];

        ++circle.radius;
        lw_fillCircle(framebuffer, circle, editor->c_start_vertex);
        --circle.radius;
        lw_fillCircle(framebuffer, circle, editor->c_new_vertices);
    }

    // TOP LEFT

    switch (editor->data2d.state) {
        case StateIdle:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Map view");
            break;
        case StateCreateSector:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Add sector");
            break;
        case StateMovePoints:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Move points");
            break;
        case StateSelectionBox:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Box select");
            break;
        default:
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "UNDEFINED STATE");
    }
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 }, editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Sectors: %zu", editor->world.sectors.num_nodes);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 1 }, editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Selected: %u", editor->data2d.selected_points_len);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 2 }, editor->c_font, editor->font, editor->text_buffer);

    // TOP RIGHT

    {
        int rot_val = editor->data2d.cam_rot == 3 ? -90 : editor->data2d.cam_rot * 90.0f;
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ROT: %4d Deg", rot_val);
    }
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ZOOM: %6.2f%%", editor->data2d.zoom * 100.0f);
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 1 },
                  editor->c_font, editor->font, editor->text_buffer);

    if (editor->data2d.grid_active) {
        if (editor->data2d.grid_size >= 1.0f) {
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID: %7.0f", editor->data2d.grid_size);
        } else {
            float d = 1.0f / editor->data2d.grid_size;
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID: %*s1/%.0f", d < 10 ? 4 : 3, "", d);
        }
    } else {
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID:     OFF");
    }
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 2 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, editor->data2d.specter_select ? "SPECTER:   ON" : "SPECTER:  OFF");
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 3 },
                  editor->c_font, editor->font, editor->text_buffer);

    return LW_EXIT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

static void _input(Editor *const editor, float dt, LW_Context *const context) {
    if (isInputActionDown(context, InputName_toggleGrid)) {
        editor->data2d.grid_active = !editor->data2d.grid_active;
    }

    if (editor->data2d.grid_active) {
        if (isInputActionDown(context, InputName_increaseGrid) && editor->data2d.grid_size < MAX_GRID) {
            editor->data2d.grid_size *= 2;
        }

        if (isInputActionDown(context, InputName_decreaseGrid) && editor->data2d.grid_size > MIN_GRID) {
            editor->data2d.grid_size /= 2;
        }
    }

    if (isInputActionDown(context, InputName_specterSelect)) {
        editor->data2d.specter_select = !editor->data2d.specter_select;
    }

    if (lw_isKeyDown(context, LW_KeyO)) {
        // TODO: Enable vertex snapping
    }

    if (isInputActionDown(context, InputName_rotateLeft)) {
        editor->data2d.cam_rot = (editor->data2d.cam_rot + 1) % 4;
    }

    if (isInputActionDown(context, InputName_rotateRight)) {
        editor->data2d.cam_rot = (editor->data2d.cam_rot - 1 + 4) % 4;
    }

    lw_vec2 movement, movement_rot;
    movement[0] = isInputAction(context, InputName_moveRight) - isInputAction(context, InputName_moveLeft);
    movement[1] = isInputAction(context, InputName_moveForwards) - isInputAction(context, InputName_moveBackwards);
    lw_normalize2d(movement);

    switch (editor->data2d.cam_rot) {
        case 0: movement_rot[0] = movement[0], movement_rot[1] = movement[1]; break;
        case 1: movement_rot[0] = -movement[1], movement_rot[1] = -movement[0]; break;
        case 2: movement_rot[0] = -movement[0], movement_rot[1] = -movement[1]; break;
        case 3: movement_rot[0] = movement[1], movement_rot[1] = movement[0]; break;
    }

    editor->data2d.cam_pos[0] += movement_rot[0] * dt * editor->data2d.zoom * 100.0f * 3.0f;
    editor->data2d.cam_pos[1] += movement_rot[1] * dt * editor->data2d.zoom * 100.0f * 3.0f;

    float z = lw_getMouseScroll(context);
    if (z != 0.0f) {
        editor->data2d.zoom_t = clamp(editor->data2d.zoom_t + (z * 2.0f * dt), 0.0f, 1.0f);
        editor->data2d.zoom   = logerp(MIN_ZOOM, MAX_ZOOM, editor->data2d.zoom_t);
    }
}

static void _recalcViewMatrices(Editor *const editor, float dt, LW_Context *const context) {
    float inv_zoom = 1.0f / editor->data2d.zoom;
    float cam_rot  = editor->data2d.cam_rot * (90.0f * TO_RADS);
    lw_mat4 proj, view, scale, translate, rotation, tmp;

    // to screen
    lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
    lw_mat4Translate((lw_vec3){ editor->width * 0.5, editor->height * 0.5f, 0.0f }, translate);
    lw_mat4Mul(translate, scale, proj);

    lw_mat4Translate((lw_vec3){ -editor->data2d.cam_pos[0], -editor->data2d.cam_pos[1], 0.0f }, translate);
    lw_mat4Scale((lw_vec3){ inv_zoom, inv_zoom, inv_zoom }, scale);
    lw_mat4RotateZ(cam_rot, rotation);
    lw_mat4Mul(scale, rotation, tmp);
    lw_mat4Mul(tmp, translate, view);
    lw_mat4Mul(proj, view, editor->data2d.to_screen_mat);

    // to world
    lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
    lw_mat4Translate((lw_vec3){ -editor->width * 0.5, -editor->height * 0.5f, 0.0f }, translate);
    lw_mat4Mul(scale, translate, proj);

    lw_mat4Translate((lw_vec3){ editor->data2d.cam_pos[0], editor->data2d.cam_pos[1], 0.0f }, translate);
    lw_mat4Scale((lw_vec3){ editor->data2d.zoom, editor->data2d.zoom, editor->data2d.zoom }, scale);
    lw_mat4RotateZ(-cam_rot, rotation);
    lw_mat4Mul(translate, rotation, tmp);
    lw_mat4Mul(tmp, scale, view);
    lw_mat4Mul(view, proj, editor->data2d.to_world_mat);
}

static void _drawGrid(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {
    if (!editor->data2d.grid_active) return;

    float zoom_grid_ratio = editor->data2d.zoom / editor->data2d.grid_size;
    if (zoom_grid_ratio <= 1.0f) {
        bool draw_grid_half = zoom_grid_ratio >= 0.5f;
        float grid_size     = editor->data2d.grid_size * (draw_grid_half ? 2.0f : 1.0f);

        lw_vec4 screen_tl = { 0, 0, 0, 1 };
        lw_vec4 world_tl;

        int x_index, y_index;

        lw_mat4MulVec4(editor->data2d.to_world_mat, screen_tl, world_tl);

        // round up to closest grid mark
        x_index     = ceilf(world_tl[0] / grid_size);
        y_index     = floorf(world_tl[1] / grid_size);
        world_tl[0] = x_index * grid_size;
        world_tl[1] = y_index * grid_size;


        lw_mat4MulVec4(editor->data2d.to_screen_mat, world_tl, screen_tl);

        float incr_val = grid_size / editor->data2d.zoom;

        // verical lines
        for (float x = screen_tl[0]; x < editor->width; x += incr_val) {
            for (int y = 0; y < editor->height; ++y) {
                if ((y / 2) % 3 != 0) {
                    lw_setPixel(framebuffer, (lw_uvec2){ x, y }, editor->c_grid);
                }
            }
        }

        // horizontal lines
        for (float y = screen_tl[1]; y < editor->height; y += incr_val) {
            for (int x = 0; x < editor->width; ++x) {
                if ((x / 2) % 3 != 0) {
                    lw_setPixel(framebuffer, (lw_uvec2){ x, y }, editor->c_grid);
                }
            }
        }
    }
}
