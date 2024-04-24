#include "editor.h"

#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

static void _input(Editor *const editor, float dt, LW_Context *const context);

int editor3dUpdate(Editor *const editor, float dt, LW_Context *const context) {
    _input(editor, dt, context);

    if (isInputActionDown(context, InputName_swapViews)) {
        editor->view_3d = false;
        return LW_EXIT_OK;
    }

    lw_ivec2 mouse_screen_pos;
    lw_getMousePos(context, mouse_screen_pos);
    lw_vec4 mouse_screen_posv4 = { mouse_screen_pos[0], mouse_screen_pos[1], 0.0f, 1.0f };

    // raycast sector
    bool casting                = false;
    editor->data3d.ray_hit_type = RayHitType_None;
    LW_Sector *sector           = editor->data3d.camera.sector;
    LW_Subsector *subsector     = NULL;
    unsigned ssid               = editor->data3d.camera.subsector;

    float dist_cache = 0;
    lw_vec3 ray[2];
    lw_vec2 ray2d[2];

    // precalc ray direction
    lw_screenPointToRay(editor->data3d.camera, editor->width, editor->height, mouse_screen_posv4, ray[0], ray[1]);
    ray2d[1][0] = ray[1][0];
    ray2d[1][1] = ray[1][1];
    lw_normalize2d(ray2d[1]);
    float cos_theta = lw_dot3d(ray[1], (lw_vec3){ ray2d[1][0], ray2d[1][1], 0.0f });

    if (sector != NULL) {
        subsector = &sector->subsectors[ssid];
        casting   = true;
    }

    while (casting) {
        editor->data3d.intersect_dist = INFINITY;

        // raycast ceiling
        lw_vec4 plane;
        float dist;

        ray2d[0][0] = ray[0][0];
        ray2d[0][1] = ray[0][1];

        // ceiling
        plane[0] = 0.0f, plane[1] = 0.0f, plane[2] = -1.0f, plane[3] = -subsector->ceiling_height;
        // backface culling
        if (lw_dot3d(ray[1], plane) < 0.0f) {
            if (lw_intersectRayPlane(ray, plane, &dist) && dist < editor->data3d.intersect_dist) {
                editor->data3d.intersect_dist = dist;
                editor->data3d.ray_hit_type   = RayHitType_Ceiling;
            }
        }

        // floor
        plane[0] = 0.0f, plane[1] = 0.0f, plane[2] = 1.0f, plane[3] = subsector->floor_height;
        // backface culling
        if (lw_dot3d(ray[1], plane) < 0.0f) {
            if (lw_intersectRayPlane(ray, plane, &dist) && dist < editor->data3d.intersect_dist) {
                editor->data3d.intersect_dist = dist;
                editor->data3d.ray_hit_type   = RayHitType_Floor;
            }
        }

        // walls
        for (unsigned i = 0; i < sector->num_walls; ++i) {
            // backface culling
            if (lw_dot2d(ray2d[1], sector->walls[i].plane) >= 0.0f) continue;

            lw_vec2 line[2];
            line[0][0] = sector->walls[i].start[0];
            line[0][1] = sector->walls[i].start[1];
            line[1][0] = sector->walls[sector->walls[i].next].start[0];
            line[1][1] = sector->walls[sector->walls[i].next].start[1];

            if (lw_intersectSegmentRay(line, ray2d, NULL, &dist)) {
                // dist is flat
                dist = dist / cos_theta;
                if (dist < editor->data3d.intersect_dist) {
                    editor->data3d.intersect_dist = dist;
                    editor->data3d.ray_hit_type   = RayHitType_Wall0 + i;
                }
            }
        }

        if (editor->data3d.ray_hit_type != RayHitType_None) {
            editor->data3d.intersect_point[0] = ray[0][0] + ray[1][0] * editor->data3d.intersect_dist;
            editor->data3d.intersect_point[1] = ray[0][1] + ray[1][1] * editor->data3d.intersect_dist;
            editor->data3d.intersect_point[2] = ray[0][2] + ray[1][2] * editor->data3d.intersect_dist;
        }

        casting = false;
        if (editor->data3d.ray_hit_type >= RayHitType_Wall0) {
            // check if portal
            LW_LineDef const *line = &sector->walls[editor->data3d.ray_hit_type - RayHitType_Wall0];
            if (line->portal_sector != NULL) {
                unsigned s       = lw_getSubSector(line->portal_sector, editor->data3d.intersect_point);
                LW_Subsector *ss = &line->portal_sector->subsectors[s];
                if (editor->data3d.intersect_point[2] > ss->floor_height && editor->data3d.intersect_point[2] < ss->ceiling_height) {
                    // continue casting
                    sector    = line->portal_sector;
                    subsector = ss;
                    ssid      = s;
                    casting   = true;

                    // move ray origin
                    ray[0][0] += ray[1][0] * (editor->data3d.intersect_dist + 0.003f);
                    ray[0][1] += ray[1][1] * (editor->data3d.intersect_dist + 0.003f);
                    ray[0][2] += ray[1][2] * (editor->data3d.intersect_dist + 0.003f);
                    dist_cache += editor->data3d.intersect_dist;
                }
            }
        }
    }
    editor->data3d.intersect_dist += dist_cache;

    // check that we are not changing snap value and then take into account the scroll
    // then add the keyboard values
    float high_lower = inputActionValue(context, InputName_changeHeight) + (isInputActionDown(context, InputName_incrHeight) - isInputActionDown(context, InputName_decrHeight));

    switch (editor->data3d.ray_hit_type) {
        case RayHitType_None: break;
        case RayHitType_Ceiling:
            if (high_lower != 0.0f) {
                subsector->ceiling_height += high_lower * editor->data3d.floor_snap_val;
                subsector->ceiling_height = roundf(subsector->ceiling_height / editor->data3d.floor_snap_val) * editor->data3d.floor_snap_val;
            }

            if (isInputActionDown(context, InputName_copy)) {
                editor->data3d.copied_height_val = subsector->ceiling_height;
            } else if (isInputActionDown(context, InputName_paste)) {
                subsector->ceiling_height = editor->data3d.copied_height_val;
            }

            // prevent clipping within subsector
            if (subsector->ceiling_height < subsector->floor_height)
                subsector->ceiling_height = subsector->floor_height;

            // prevent clipping with neighboring subsectors
            if (ssid + 1 < sector->num_subsectors && subsector->ceiling_height > sector->subsectors[ssid + 1].floor_height)
                subsector->ceiling_height = sector->subsectors[ssid + 1].floor_height;

            break;

        case RayHitType_Floor:
            if (high_lower != 0.0f) {
                subsector->floor_height += high_lower * editor->data3d.floor_snap_val;
                subsector->floor_height = roundf(subsector->floor_height / editor->data3d.floor_snap_val) * editor->data3d.floor_snap_val;
            }

            if (isInputActionDown(context, InputName_copy)) {
                editor->data3d.copied_height_val = subsector->floor_height;
            } else if (isInputActionDown(context, InputName_paste)) {
                subsector->floor_height = editor->data3d.copied_height_val;
            }

            // prevent clipping within subsector
            if (subsector->floor_height > subsector->ceiling_height)
                subsector->floor_height = subsector->ceiling_height;

            // prevent clipping with neighboring subsectors
            if (ssid > 0 && subsector->floor_height < sector->subsectors[ssid - 1].ceiling_height)
                subsector->floor_height = sector->subsectors[ssid - 1].ceiling_height;

            break;

        default:
            // walls
    }

    if (sector != NULL && subsector != NULL) {
        if (isInputActionDown(context, InputName_removeSubsector)) {
            // remove subsector
            if (sector->num_subsectors > 1) {
                // fill in space
                // move subsectors down
                --sector->num_subsectors;
                for (unsigned i = ssid; i < sector->num_subsectors; ++i) {
                    sector->subsectors[i] = sector->subsectors[i + 1];
                }
            }

        } else if (isInputActionDown(context, InputName_addSubsector)) {
            // Add subsector
            unsigned new_ssid = ssid + 1;

            // allocate memory
            ++sector->num_subsectors;
            sector->subsectors = realloc(sector->subsectors, sector->num_subsectors * sizeof(*sector->subsectors));
            subsector          = &sector->subsectors[ssid];

            // move subsectors up
            for (unsigned i = sector->num_subsectors - 1; i > new_ssid; --i) {
                sector->subsectors[i] = sector->subsectors[i - 1];
            }

            // set values
            float new_ceiling = (subsector->ceiling_height + subsector->floor_height) * 0.5f - 4.0f / 32.0f;
            float new_floor   = (subsector->ceiling_height + subsector->floor_height) * 0.5f + 4.0f / 32.0f;

            sector->subsectors[new_ssid].ceiling_height = subsector->ceiling_height;
            sector->subsectors[new_ssid].floor_height   = new_floor;
            subsector->ceiling_height                   = new_ceiling;
        }
    }

    return LW_EXIT_OK;
}

int editor3dRender(Editor *const editor, LW_Framebuffer *const framebuffer, LW_Context *const context) {

    lw_fillBuffer(framebuffer, editor->c_background);

    lw_renderPortalWorld(framebuffer, editor->world, editor->data3d.camera, lw_getFrame(context));

    // draw interact point
    if (editor->data3d.ray_hit_type != RayHitType_None) {
        lw_vec4 world_point = { editor->data3d.intersect_point[0], editor->data3d.intersect_point[1], editor->data3d.intersect_point[2], 1.0f };
        lw_vec4 vert;
        lw_vec2 ndc_point;
        LW_Circlei circle;

        lw_mat4MulVec4(editor->data3d.camera.vp_mat, world_point, vert);

        float inv_w;
        if (vert[3] > 0.0f) {
            inv_w = 1.0f / vert[3];
        } else {
            inv_w = 0.0f;
        }

        ndc_point[0]  = vert[0] * inv_w;
        ndc_point[1]  = vert[2] * inv_w;
        circle.pos[0] = (ndc_point[0] * 0.5 + 0.5) * (editor->width - 1);
        circle.pos[1] = (-ndc_point[1] * 0.5 + 0.5) * (editor->height - 1);

        float inv_dst = editor->data3d.intersect_dist > 0.0f ? 1.0f / editor->data3d.intersect_dist : 0.0f;

        float inner_radius = 0.005;
        float outer_radius = 0.065;

        LW_Color colors[] = {
            RGB(0xe5, 0x39, 0x35),
            RGB(0xff, 0xb7, 0x4d),
            RGB(0xff, 0xee, 0x58),
            RGB(0x38, 0x8e, 0x5a),
            RGB(0x42, 0xa5, 0xf5),
            RGB(0x87, 0x56, 0xd5),
            RGB(0xea, 0x9b, 0xd7),
        };
        const unsigned num_colors = sizeof(colors) / sizeof(colors[0]);

        unsigned color_i = (unsigned)((sinf(lw_getSeconds(context) * 7.0f) * 0.5f + 0.5f) * num_colors);

        for (unsigned i = 0; i < num_colors; ++i) {
            float lerp_val = (float)i / num_colors;
            circle.radius  = editor->height * lerp(outer_radius, inner_radius, lerp_val) * inv_dst;

            LW_Color color = colors[i];
            if (i < color_i) {
                color = lw_shadeColor(color, 180);
            }
            lw_fillCircle(framebuffer, circle, color);
        }
    }

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "%.2f %.2f %.2f", editor->data3d.intersect_point[0], editor->data3d.intersect_point[1], editor->data3d.intersect_point[2]);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 0 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "POS: %.2f %.2f %.2f", editor->data3d.camera.pos[0], editor->data3d.camera.pos[1], editor->data3d.camera.pos[2]);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 1 },
                  editor->c_font, editor->font, editor->text_buffer);

    snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "ROT: %.2f %.2f", editor->data3d.camera.yaw, editor->data3d.camera.pitch);
    lw_drawString(framebuffer, (lw_ivec2){ 5, 5 + editor->font.texture.height * 2 },
                  editor->c_font, editor->font, editor->text_buffer);

    if (editor->data3d.floor_snap_val >= 1.0f) {
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "SNAP: %4.0f", editor->data3d.floor_snap_val);
    } else {
        float d = 1.0f / editor->data3d.floor_snap_val;
        snprintf(editor->text_buffer, TEXT_BUFFER_SIZE, "SNAP: %*s1/%.0f", d < 10 ? 1 : 0, "", d);
    }
    lw_drawString(framebuffer, (lw_ivec2){ editor->width - editor->font.char_width * strlen(editor->text_buffer) - 5, 5 + editor->font.texture.height * 0 },
                  editor->c_font, editor->font, editor->text_buffer);

    return LW_EXIT_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: Convert to input system
static void _input(Editor *const editor, float dt, LW_Context *const context) {
    {
        lw_vec2 movement, movement_rot;
        movement[0] = isInputAction(context, InputName_moveRight) - isInputAction(context, InputName_moveLeft);
        movement[1] = isInputAction(context, InputName_moveForwards) - isInputAction(context, InputName_moveBackwards);
        lw_normalize2d(movement);

        float z = isInputAction(context, InputName_moveUp) - isInputAction(context, InputName_moveDown);
        float r = isInputAction(context, InputName_rotateLeft) - isInputAction(context, InputName_rotateRight);
        float s = isInputAction(context, InputName_rotateUp) - isInputAction(context, InputName_rotateDown);

        editor->data3d.camera.yaw += r * dt * 2.0f;
        editor->data3d.camera.pitch += s * dt * 1.5f;

        while (editor->data3d.camera.yaw > M_PI)
            editor->data3d.camera.yaw -= 2 * M_PI;
        while (editor->data3d.camera.yaw < -M_PI)
            editor->data3d.camera.yaw += 2 * M_PI;

        editor->data3d.camera.pitch = clamp(editor->data3d.camera.pitch, -M_PI * 0.5f, M_PI * 0.5f);

        lw_rot2d(movement, -editor->data3d.camera.yaw, movement_rot);

        editor->data3d.camera.pos[0] += movement_rot[0] * dt * 5.0f;
        editor->data3d.camera.pos[1] += movement_rot[1] * dt * 5.0f;
        editor->data3d.camera.pos[2] += z * dt * 5.0f;
    }

    if ((isInputActionUp(context, InputName_changeGrid) || isInputActionDown(context, InputName_increaseGrid)) && editor->data3d.floor_snap_val < MAX_GRID) {
        editor->data3d.floor_snap_val *= 2;
    }

    if ((isInputActionDown(context, InputName_changeGrid) || isInputActionDown(context, InputName_decreaseGrid)) && editor->data3d.floor_snap_val > MIN_GRID) {
        editor->data3d.floor_snap_val /= 2;
    }

    editor->data3d.camera.sector    = lw_getSector(editor->world, editor->data3d.camera.pos);
    editor->data3d.camera.subsector = lw_getSubSector(editor->data3d.camera.sector, editor->data3d.camera.pos);

    lw_mat4 translation, rotation, rot_yaw, rot_pitch;

    lw_mat4RotateZ(-editor->data3d.camera.yaw, rot_yaw);
    lw_mat4RotateX(-editor->data3d.camera.pitch, rot_pitch);
    lw_mat4Mul(rot_yaw, rot_pitch, editor->data3d.camera.rot_mat);

    lw_mat4Translate((lw_vec3){ -editor->data3d.camera.pos[0], -editor->data3d.camera.pos[1], -editor->data3d.camera.pos[2] }, translation);
    lw_mat4RotateZ(editor->data3d.camera.yaw, rot_yaw);
    lw_mat4RotateX(editor->data3d.camera.pitch, rot_pitch);
    lw_mat4Mul(rot_pitch, rot_yaw, rotation);

    lw_mat4Mul(rotation, translation, editor->data3d.camera.view_mat);
    lw_mat4Mul(editor->data3d.camera.proj_mat, editor->data3d.camera.view_mat, editor->data3d.camera.vp_mat);

    lw_calcCameraFrustum(&editor->data3d.camera);
}