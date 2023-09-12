#include "editor.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

float logerp(float a, float b, float t) {
    return b * powf(a / b, t);
}

// solve for t based on desired r
float inv_logerp(float a, float b, float r) {
    return logf(r / b) / log(a / b);
}

bool editorInit(Editor *const editor) {
    if (!lw_loadTexture("res/fonts/small.png", &editor->font.texture)) return false;
    editor->font.char_width = editor->font.texture.width / 95;
    editor->text_buffer     = calloc(1, TEXT_BUFFER_SIZE);

    editor->cam3d.view_frustum.num_planes = 6;
    editor->cam3d.view_frustum.planes     = malloc(editor->cam3d.view_frustum.num_planes * sizeof(*editor->cam3d.view_frustum.planes));

    editor->mouse_snapped_pos[3] = 1.0f;

    editor->c_background = RGB(10, 10, 40);
    editor->c_grid       = RGB(60, 80, 120);
    editor->c_font       = RGB(240, 240, 210);

    editor->c_walls    = RGB(200, 200, 200);
    editor->c_vertices = RGB(30, 140, 240);
    editor->c_portal   = RGB(200, 40, 80);

    editor->c_new_walls    = RGB(128, 128, 128);
    editor->c_new_vertices = RGB(80, 30, 240);
    editor->c_start_vertex = RGB(250, 190, 100);

    editor->c_sel_vertex           = RGB(250, 230, 80);
    editor->c_selection_box        = RGB(245, 245, 125);
    editor->c_highlighted_vertices = RGB(240, 212, 30);

    editor->grid_active = true;
    editor->grid_size   = 1.0f;

    editor->specter_select = true;

    editor->zoom   = 0.03;
    editor->zoom_t = inv_logerp(MIN_ZOOM, MAX_ZOOM, editor->zoom);

    editor->cam3d.aspect_ratio = (float)editor->width / (float)editor->height;
    editor->cam3d.far_plane    = 500.0f;
    editor->cam3d.fov          = 80 * TO_RADS;
    editor->cam3d.near_plane   = 0.3;
    lw_calcCameraProjection(&editor->cam3d);

    LW_SectorList_init(&editor->world.sectors);

    return true;
}

static void _input(Editor *const editor, float dt, LW_Context *const context);
static void _recalcViewMatrices(Editor *const editor, float dt, LW_Context *const context);
static bool _validPortalOverlap(LW_LineDef *const line0, LW_LineDef *const line1) {
    lw_vec2 a, b, difference;
    float sqdst;
    a[0] = line1->start[0], a[1] = line1->start[1];
    b[0] = line1->sector->walls[line1->next].start[0], b[1] = line1->sector->walls[line1->next].start[1];

    // check
    difference[0] = b[0] - line0->start[0];
    difference[1] = b[1] - line0->start[1];
    sqdst         = lw_dot2d(difference, difference);

    if (sqdst > AUTO_PORTAL_EPSILON) return false;

    difference[0] = a[0] - line0->sector->walls[line0->next].start[0];
    difference[1] = a[1] - line0->sector->walls[line0->next].start[1];
    sqdst         = lw_dot2d(difference, difference);

    return sqdst <= AUTO_PORTAL_EPSILON;
}

int editor2dUpdate(Editor *const editor, float dt, LW_Context *const context) {
    _input(editor, dt, context);
    _recalcViewMatrices(editor, dt, context);

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    lw_mat4MulVec4(editor->to_world_mat, mouse_screen_posv4, editor->mouse_world_pos);

    if (lw_isKeyDown(context, LW_KeyTab)) {
        editor->view_3d      = true;
        editor->cam3d.pos[0] = editor->mouse_world_pos[0];
        editor->cam3d.pos[1] = editor->mouse_world_pos[1];

        switch (editor->cam_rot) {
            case 0: editor->cam3d.yaw = 0.0f; break;
            case 1: editor->cam3d.yaw = M_PI * 0.5; break;
            case 2: editor->cam3d.yaw = M_PI; break;
            case 3: editor->cam3d.yaw = -M_PI * 0.5F; break;
        }

        editor->cam3d.sector = lw_getSector(editor->world, editor->cam3d.pos);
        if (editor->cam3d.sector != NULL) {
            editor->cam3d.pos[2] = (editor->cam3d.sector->sub_sectors[0].floor_height + editor->cam3d.sector->sub_sectors[0].ceiling_height) * 0.5f;
        }

        return LW_EXIT_OK;
    }


    if (editor->grid_active) {
        editor->mouse_snapped_pos[0] = roundf(editor->mouse_world_pos[0] / editor->grid_size) * editor->grid_size;
        editor->mouse_snapped_pos[1] = roundf(editor->mouse_world_pos[1] / editor->grid_size) * editor->grid_size;
    } else {
        editor->mouse_snapped_pos[0] = editor->mouse_world_pos[0];
        editor->mouse_snapped_pos[1] = editor->mouse_world_pos[1];
    }

    switch (editor->state) {
        case StateIdle:
            if (lw_isKeyDown(context, LW_KeySpace)) {
                editor->selected_points_len = 0; // deselect all points

                editor->new_sector_capacity = 16;

                editor->new_sector.num_sub_sectors               = 1;
                editor->new_sector.sub_sectors                   = malloc(editor->new_sector.num_sub_sectors * sizeof(*editor->new_sector.sub_sectors));
                editor->new_sector.sub_sectors[0].ceiling_height = 3.0f;
                editor->new_sector.sub_sectors[0].floor_height   = 0.0f;

                editor->new_sector.walls                  = malloc(editor->new_sector_capacity * sizeof(*editor->new_sector.walls));
                editor->new_sector.walls[0].start[0]      = editor->mouse_snapped_pos[0];
                editor->new_sector.walls[0].start[1]      = editor->mouse_snapped_pos[1];
                editor->new_sector.walls[0].next          = UNDEFINED;
                editor->new_sector.walls[0].prev          = UNDEFINED;
                editor->new_sector.walls[0].portal_sector = NULL;
                editor->new_sector.walls[0].portal_wall   = NULL;

                editor->new_sector.num_walls = 1;

                editor->state = StateCreateSector;

            } else if (lw_isKeyDown(context, LW_KeyX)) {
                // delete selected points
                for (unsigned i = 0; i < editor->selected_points_len; ++i) {
                    LW_LineDef *line  = editor->selected_points[i];
                    LW_Sector *sector = line->sector;

                    // early out and remove sector
                    if (sector->num_walls <= 3) {
                        // remove all selected points from that sector
                        unsigned n = editor->selected_points_len;
                        for (unsigned j = i + 1; j < n; ++j) {
                            if (sector->num_walls == 0) break;
                            LW_LineDef *jline = editor->selected_points[j];
                            if (jline->sector == sector) {
                                // swap remove
                                --sector->num_walls;
                                --editor->selected_points_len;
                                editor->selected_points[j] = editor->selected_points[editor->selected_points_len];
                            }
                        }

                        // do remove
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

                editor->selected_points_len = 0;

            } else if (lw_isKeyDown(context, LW_KeyC)) {
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
                        LW_LineDef *const line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        b[0] = sec->walls[line->next].start[0], b[1] = sec->walls[line->next].start[1];

                        lw_mat4MulVec4(editor->to_screen_mat, a, c);
                        lw_mat4MulVec4(editor->to_screen_mat, b, d);

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
                            lw_mat4MulVec4(editor->to_world_mat, closest_point, c);

                            // add new point
                            ++sec->num_walls;
                            sec->walls                 = realloc(sec->walls, sec->num_walls * sizeof(*sec->walls));
                            LW_LineDef *const new_line = &sec->walls[sec->num_walls - 1];

                            // initialize new point
                            new_line->start[0] = c[0], new_line->start[1] = c[1];
                            memcpy(new_line->plane, line->plane, sizeof(new_line->plane)); // point added on line means point added on plane
                            new_line->portal_sector = NULL;
                            new_line->portal_wall   = NULL;
                            new_line->sector        = sec;

                            // insert into polygon
                            new_line->next              = line->next;
                            new_line->prev              = i;
                            sec->walls[line->next].prev = sec->num_walls - 1;
                            line->next                  = sec->num_walls - 1;

                            if (!editor->specter_select) goto _end_sector_loop;
                            break;
                        }
                    }
                }
            _end_sector_loop:

            } else if (lw_isKeyDown(context, LW_KeyK)) {
                // TODO: Knife tool

            } else if (lw_isKeyDown(context, LW_KeyF)) {
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

                        lw_mat4MulVec4(editor->to_screen_mat, a, c);
                        lw_mat4MulVec4(editor->to_screen_mat, b, d);

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

            } else if (lw_isMouseButtonDown(context, 0)) {
                bool shift = lw_isKey(context, LW_KeyLShift);
                bool alt   = lw_isKey(context, LW_KeyLAlt);
                bool ctrl  = lw_isKey(context, LW_KeyLCtrl);

                if (alt) {
                    // create selection box
                    editor->state = StateSelectionBox;
                    for (unsigned i = 0; i < 2; ++i) {
                        editor->selection_box.low[i]   = mouse_screen_posv4[i];
                        editor->selection_box.high[i]  = mouse_screen_posv4[i];
                        editor->selection_box_pivot[i] = mouse_screen_posv4[i];
                    }
                    break;
                }

                // if selecting already selected point, change to move points
                LW_Circle circle;
                circle.radius = POINT_RENDER_RADIUS;

                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;

                bool finish_loop = false;
                for (unsigned i = 0; i < editor->selected_points_len; ++i) {
                    LW_LineDef *const line = editor->selected_points[i];

                    a[0] = line->start[0], a[1] = line->start[1];
                    lw_mat4MulVec4(editor->to_screen_mat, a, b);

                    circle.pos[0] = b[0];
                    circle.pos[1] = b[1];

                    if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                        if (shift) {
                            // deselect
                            editor->selected_points[i] = editor->selected_points[editor->selected_points_len - 1];
                            --editor->selected_points_len;
                        } else {
                            // move
                            editor->state              = StateMovePoints;
                            editor->select_point_index = i;
                            editor->move_origin[0]     = editor->selected_points[editor->select_point_index]->start[0];
                            editor->move_origin[1]     = editor->selected_points[editor->select_point_index]->start[1];
                        }
                        finish_loop = true;
                        break;
                    }
                }
                if (finish_loop) break;

                // select points

                if (!shift) {
                    // single point
                    editor->selected_points_len = 0;
                }

                if (editor->selected_points_len >= editor->selected_points_capacity) {
                    editor->selected_points_capacity = editor->selected_points_len + 16;
                    editor->selected_points          = realloc(editor->selected_points, editor->selected_points_capacity * sizeof(*editor->selected_points));
                }

                LW_SectorListNode *node = editor->world.sectors.head;
                for (; node != NULL; node = node->next) {
                    LW_Sector const *sec = &node->item;
                    bool do_break        = false;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        a[0] = line->start[0], a[1] = line->start[1];
                        lw_mat4MulVec4(editor->to_screen_mat, a, b);

                        circle.pos[0] = b[0];
                        circle.pos[1] = b[1];

                        if (lw_pointInCircle(circle, mouse_screen_posv4)) {
                            editor->selected_points[editor->selected_points_len] = line;
                            ++editor->selected_points_len;
                            do_break = true;
                            break;
                        }
                    }
                    if (!editor->specter_select && do_break) break;
                }

                if (editor->selected_points_len > 0 && !shift) {
                    // move point if selected
                    editor->select_point_index = 0;
                    editor->state              = StateMovePoints;
                    editor->move_origin[0]     = editor->selected_points[editor->select_point_index]->start[0];
                    editor->move_origin[1]     = editor->selected_points[editor->select_point_index]->start[1];
                }
            }
            break;
            /////////////////////////////////////////////////////////////
        case StateMovePoints:
            if (lw_isKeyDown(context, LW_KeyEscape)) {
                // cancel
                lw_vec2 delta;
                delta[0] = editor->move_origin[0] - editor->selected_points[editor->select_point_index]->start[0];
                delta[1] = editor->move_origin[1] - editor->selected_points[editor->select_point_index]->start[1];

                for (unsigned i = 0; i < editor->selected_points_len; ++i) {
                    LW_LineDef *const line = editor->selected_points[i];
                    line->start[0] += delta[0];
                    line->start[1] += delta[1];
                }

                editor->state = StateIdle;
                break;

            } else if (lw_isMouseButtonUp(context, 0)) {
                for (unsigned i = 0; i < editor->selected_points_len; ++i) {
                    // If moving portal, check if portal should be disconnected
                    // recalc line plane

                    LW_LineDef *line = editor->selected_points[i];
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


                editor->state = StateIdle;
                break;
            }

            lw_vec2 delta;
            delta[0] = editor->mouse_snapped_pos[0] - editor->selected_points[editor->select_point_index]->start[0];
            delta[1] = editor->mouse_snapped_pos[1] - editor->selected_points[editor->select_point_index]->start[1];

            for (unsigned i = 0; i < editor->selected_points_len; ++i) {
                LW_LineDef *const line = editor->selected_points[i];
                line->start[0] += delta[0];
                line->start[1] += delta[1];
            }
            break;
            ///////////////////////////////////////////////
        case StateSelectionBox:
            if (lw_isKeyDown(context, LW_KeyEscape)) {
                // cancel
                editor->state = StateIdle;
            } else if (lw_isMouseButtonUp(context, 0)) {
                // do selection
                lw_vec4 a               = { 0.0f, 0.0f, 0.0f, 1.0f }, b;
                LW_SectorListNode *node = editor->world.sectors.head;

                // pseudo minkowski's difference
                editor->selection_box.low[0] -= POINT_RENDER_RADIUS;
                editor->selection_box.low[1] -= POINT_RENDER_RADIUS;
                editor->selection_box.high[0] += POINT_RENDER_RADIUS;
                editor->selection_box.high[1] += POINT_RENDER_RADIUS;

                for (; node != NULL; node = node->next) {
                    LW_Sector const *sec = &node->item;
                    for (unsigned i = 0; i < sec->num_walls; ++i) {
                        LW_LineDef *const line = &sec->walls[i];

                        // check that point is not already selected
                        bool already_selected = false;
                        for (unsigned j = 0; j < editor->selected_points_len; ++j) {
                            if (editor->selected_points[j] == line) {
                                already_selected = true;
                                break;
                            }
                        }
                        if (already_selected) continue;

                        a[0] = line->start[0], a[1] = line->start[1];
                        lw_mat4MulVec4(editor->to_screen_mat, a, b);

                        if (lw_pointInAabb(editor->selection_box, b)) {
                            if (editor->selected_points_len >= editor->selected_points_capacity) {
                                editor->selected_points_capacity = editor->selected_points_len + 16;
                                editor->selected_points          = realloc(editor->selected_points, editor->selected_points_capacity * sizeof(*editor->selected_points));
                            }
                            editor->selected_points[editor->selected_points_len] = line;
                            ++editor->selected_points_len;
                        }
                    }
                }

                editor->state = StateIdle;
            } else {
                for (unsigned i = 0; i < 2; ++i) {
                    if (mouse_screen_posv4[i] < editor->selection_box_pivot[i]) {
                        editor->selection_box.low[i] = mouse_screen_posv4[i];
                    } else if (mouse_screen_posv4[i] > editor->selection_box_pivot[i]) {
                        editor->selection_box.high[i] = mouse_screen_posv4[i];
                    }
                }
            }
            break;
            ///////////////////////////////////////////////
        case StateCreateSector:
            if (lw_isKeyDown(context, LW_KeyEscape)) {
                // cancel
                editor->state = StateIdle;

            } else if (lw_isKeyDown(context, LW_KeySpace)) {
                LW_Circle circle;
                lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b;

                lw_vec4 snapped_screen_pos;
                lw_mat4MulVec4(editor->to_screen_mat, editor->mouse_snapped_pos, snapped_screen_pos);

                circle.radius = POINT_RENDER_RADIUS * 2.0f; // overlapping the vertex at the mouse position

                // if we are in range of first point, finish sector
                // if we are in range of prev point, do nothing
                // else, add new wall

                a[0] = editor->new_sector.walls[0].start[0];
                a[1] = editor->new_sector.walls[0].start[1];
                lw_mat4MulVec4(editor->to_screen_mat, a, b);

                circle.pos[0] = b[0];
                circle.pos[1] = b[1];

                if (lw_pointInCircle(circle, snapped_screen_pos)) {
                    if (editor->new_sector.num_walls > 2) {
                        // finish sector

                        editor->new_sector.walls[editor->new_sector.num_walls - 1].next = 0;
                        editor->new_sector.walls[0].prev                                = editor->new_sector.num_walls - 1;

                        {
                            // ensure points are counter-clockwise
                            float angle;
                            lw_vec2 a, b;
                            LW_LineDef c, d, e;
                            c = editor->new_sector.walls[0];
                            d = editor->new_sector.walls[c.next];
                            e = editor->new_sector.walls[d.next];

                            a[0]  = d.start[0] - c.start[0];
                            a[1]  = d.start[1] - c.start[1];
                            b[0]  = e.start[0] - d.start[0];
                            b[1]  = e.start[1] - d.start[1];
                            angle = lw_angleBetween2d(a, b);

                            if (angle > 0.0f) {
                                // reverse winding

                                for (unsigned i = 0; i < editor->new_sector.num_walls; ++i) {
                                    swap(unsigned, editor->new_sector.walls[i].prev, editor->new_sector.walls[i].next);
                                }
                            }
                        }

                        LW_SectorList_push_back(&editor->world.sectors, editor->new_sector);
                        LW_Sector *sector = &editor->world.sectors.tail->item;

                        for (unsigned i = 0; i < sector->num_walls; ++i) {
                            // set sector
                            sector->walls[i].sector = sector;
                            // calculate wall planes
                            lw_recalcLinePlane(&sector->walls[i]);
                        }

                        editor->state = StateIdle;
                    } else {
                        // TODO: Play sound
                    }
                    break;
                }

                a[0] = editor->new_sector.walls[editor->new_sector.num_walls - 1].start[0];
                a[1] = editor->new_sector.walls[editor->new_sector.num_walls - 1].start[1];
                lw_mat4MulVec4(editor->to_screen_mat, a, b);

                circle.pos[0] = b[0];
                circle.pos[1] = b[1];

                if (!lw_pointInCircle(circle, snapped_screen_pos)) {
                    // add new wall at mouse point
                    // connect last wall to this wall
                    if (editor->new_sector.num_walls >= editor->new_sector_capacity) {
                        editor->new_sector_capacity = editor->new_sector.num_walls + 16;
                        editor->new_sector.walls    = realloc(editor->new_sector.walls, editor->new_sector_capacity * sizeof(*editor->new_sector.walls));
                    }

                    editor->new_sector.walls[editor->new_sector.num_walls].start[0]      = editor->mouse_snapped_pos[0];
                    editor->new_sector.walls[editor->new_sector.num_walls].start[1]      = editor->mouse_snapped_pos[1];
                    editor->new_sector.walls[editor->new_sector.num_walls].next          = UNDEFINED;
                    editor->new_sector.walls[editor->new_sector.num_walls].portal_sector = NULL;

                    editor->new_sector.walls[editor->new_sector.num_walls].prev     = editor->new_sector.num_walls - 1;
                    editor->new_sector.walls[editor->new_sector.num_walls - 1].next = editor->new_sector.num_walls;

                    ++editor->new_sector.num_walls;
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
        LW_Circlei circle;

        lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
        lw_ivec2 x, y;

        LW_Aabb selection_box;
        if (editor->state == StateSelectionBox) {
            selection_box.low[0]  = editor->selection_box.low[0] - POINT_RENDER_RADIUS;
            selection_box.low[1]  = editor->selection_box.low[1] - POINT_RENDER_RADIUS;
            selection_box.high[0] = editor->selection_box.high[0] + POINT_RENDER_RADIUS;
            selection_box.high[1] = editor->selection_box.high[1] + POINT_RENDER_RADIUS;
        }

        // draw all selected points
        circle.radius = POINT_RENDER_RADIUS + 1;
        for (unsigned i = 0; i < editor->selected_points_len; ++i) {
            LW_LineDef const *const line = editor->selected_points[i];

            a[0] = line->start[0], a[1] = line->start[1];
            lw_mat4MulVec4(editor->to_screen_mat, a, c);

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
                lw_mat4MulVec4(editor->to_screen_mat, a, c);

                if (line.next != UNDEFINED) {
                    // draw line that exits
                    b[0] = sec->walls[line.next].start[0];
                    b[1] = sec->walls[line.next].start[1];
                    lw_mat4MulVec4(editor->to_screen_mat, b, d);

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
                if (editor->state == StateSelectionBox && lw_pointInAabb(selection_box, c)) {
                    color = editor->c_highlighted_vertices;
                } else {
                    color = editor->c_vertices;
                }

                lw_fillCircle(framebuffer, circle, color);
            }
        }
    }

    if (editor->state == StateSelectionBox) {
        LW_Recti rect;
        rect.pos[0] = editor->selection_box.low[0];
        rect.pos[1] = editor->selection_box.low[1];

        rect.size[0] = editor->selection_box.high[0] - editor->selection_box.low[0];
        rect.size[1] = editor->selection_box.high[1] - editor->selection_box.low[1];

        lw_drawRect(framebuffer, rect, editor->c_selection_box);
    }

    if (editor->state == StateCreateSector) {
        LW_Circlei circle;
        circle.radius = POINT_RENDER_RADIUS;

        lw_vec4 a = { 0.0f, 0.0f, 0.0f, 1.0f }, b = { 0.0f, 0.0f, 0.0f, 1.0f }, c, d;
        lw_ivec2 x, y;

        // draw new sector
        for (unsigned i = 0; i < editor->new_sector.num_walls; ++i) {
            LW_LineDef line = editor->new_sector.walls[i];

            a[0] = line.start[0], a[1] = line.start[1];
            lw_mat4MulVec4(editor->to_screen_mat, a, c);

            if (line.next != UNDEFINED) {
                // draw line that exits
                b[0] = editor->new_sector.walls[line.next].start[0];
                b[1] = editor->new_sector.walls[line.next].start[1];
                lw_mat4MulVec4(editor->to_screen_mat, b, d);

                x[0] = c[0], x[1] = c[1];
                y[0] = d[0], y[1] = d[1];

                lw_drawLine(framebuffer, x, y, editor->c_new_walls);
            } else {
                // draw line to mouse
                b[0] = editor->mouse_snapped_pos[0];
                b[1] = editor->mouse_snapped_pos[1];
                lw_mat4MulVec4(editor->to_screen_mat, b, d);

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

    switch (editor->state) {
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

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "Selected: %u", editor->selected_points_len);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 2 }, editor->c_font, editor->font, editor->text_buffer);

    {
        int rot_val = editor->cam_rot == 3 ? -90 : editor->cam_rot * 90.0f;
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ROT: %4d Deg", rot_val);
    }
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ZOOM: %6.2f%%", editor->zoom * 100.0f);
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 1 },
                  editor->c_font, editor->font, editor->text_buffer);

    if (editor->grid_active) {
        if (editor->grid_size >= 1.0f) {
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID: %7.0f", editor->grid_size);
        } else {
            float d = 1.0f / editor->grid_size;
            snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID: %*s1/%.0f", d < 10 ? 4 : 3, "", d);
        }
    } else {
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "GRID: OFF");
    }
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 2 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, editor->specter_select ? "SPECTER:   ON" : "SPECTER:  OFF");
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 3 },
                  editor->c_font, editor->font, editor->text_buffer);

    return LW_EXIT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

static void _input(Editor *const editor, float dt, LW_Context *const context) {
    if (lw_isKeyDown(context, LW_KeyG)) {
        editor->grid_active = !editor->grid_active;
    }

    if (editor->grid_active) {
        if (lw_isKeyDown(context, LW_KeyEquals) && editor->grid_size < MAX_GRID) {
            editor->grid_size *= 2;
        }

        if (lw_isKeyDown(context, LW_KeyMinus) && editor->grid_size > MIN_GRID) {
            editor->grid_size /= 2;
        }
    }

    if (lw_isKeyDown(context, LW_KeyP)) {
        editor->specter_select = !editor->specter_select;
    }

    if (lw_isKeyDown(context, LW_KeyE)) {
        editor->cam_rot = (editor->cam_rot + 1) % 4;
    }

    if (lw_isKeyDown(context, LW_KeyR)) {
        editor->cam_rot = (editor->cam_rot - 1 + 4) % 4;
    }

    lw_vec2 movement, movement_rot;
    movement[0] = lw_isKey(context, LW_KeyD) - lw_isKey(context, LW_KeyA);
    movement[1] = lw_isKey(context, LW_KeyW) - lw_isKey(context, LW_KeyS);
    lw_normalize2d(movement);

    switch (editor->cam_rot) {
        case 0: movement_rot[0] = movement[0], movement_rot[1] = movement[1]; break;
        case 1: movement_rot[0] = movement[1], movement_rot[1] = -movement[0]; break;
        case 2: movement_rot[0] = -movement[0], movement_rot[1] = -movement[1]; break;
        case 3: movement_rot[0] = -movement[1], movement_rot[1] = movement[0]; break;
    }

    editor->cam_pos[0] += movement_rot[0] * dt * editor->zoom * 100.0f * 3.0f;
    editor->cam_pos[1] += movement_rot[1] * dt * editor->zoom * 100.0f * 3.0f;

    float z = (lw_isKey(context, LW_KeyQ) - lw_isKey(context, LW_KeyZ)) * 0.3f + lw_getMouseScroll(context) * 2.0f;
    if (z != 0.0f) {
        editor->zoom_t = clamp(editor->zoom_t + (z * dt), 0.0f, 1.0f);
        editor->zoom   = logerp(MIN_ZOOM, MAX_ZOOM, editor->zoom_t);
    }
}

static void _recalcViewMatrices(Editor *const editor, float dt, LW_Context *const context) {
    float inv_zoom = 1.0f / editor->zoom;
    float cam_rot  = editor->cam_rot * (90.0f * TO_RADS);
    lw_mat4 proj, view, scale, translate, rotation, tmp;

    // to screen
    lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
    lw_mat4Translate((lw_vec3){ editor->width * 0.5, editor->height * 0.5f, 0.0f }, translate);
    lw_mat4Mul(translate, scale, proj);

    lw_mat4Translate((lw_vec3){ -editor->cam_pos[0], -editor->cam_pos[1], 0.0f }, translate);
    lw_mat4Scale((lw_vec3){ inv_zoom, inv_zoom, inv_zoom }, scale);
    lw_mat4RotateZ(-cam_rot, rotation);
    lw_mat4Mul(scale, rotation, tmp);
    lw_mat4Mul(tmp, translate, view);
    lw_mat4Mul(proj, view, editor->to_screen_mat);

    // to world
    lw_mat4Scale((lw_vec3){ 1.0f, -1.0f, 1.0f }, scale);
    lw_mat4Translate((lw_vec3){ -editor->width * 0.5, -editor->height * 0.5f, 0.0f }, translate);
    lw_mat4Mul(scale, translate, proj);

    lw_mat4Translate((lw_vec3){ editor->cam_pos[0], editor->cam_pos[1], 0.0f }, translate);
    lw_mat4Scale((lw_vec3){ editor->zoom, editor->zoom, editor->zoom }, scale);
    lw_mat4RotateZ(cam_rot, rotation);
    lw_mat4Mul(translate, rotation, tmp);
    lw_mat4Mul(tmp, scale, view);
    lw_mat4Mul(view, proj, editor->to_world_mat);
}

static void _drawGrid(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {
    float zoom_grid_ratio = editor->zoom / editor->grid_size;
    if (zoom_grid_ratio <= 1.0f && editor->grid_active) {
        bool draw_grid_half = zoom_grid_ratio >= 0.5f;
        float grid_size     = editor->grid_size * (draw_grid_half ? 2.0f : 1.0f);

        lw_vec4 screen_tl = { 0, 0, 0, 1 };
        lw_vec4 world_tl;

        int x_index, y_index;

        lw_mat4MulVec4(editor->to_world_mat, screen_tl, world_tl);

        // round up to closest grid mark
        x_index     = ceilf(world_tl[0] / grid_size);
        y_index     = floorf(world_tl[1] / grid_size);
        world_tl[0] = x_index * grid_size;
        world_tl[1] = y_index * grid_size;


        lw_mat4MulVec4(editor->to_screen_mat, world_tl, screen_tl);

        float incr_val = grid_size / editor->zoom;

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
