#include "internal.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#define LIST_TAG LW_SectorList
#define LIST_ITEM_TYPE LW_Sector
#define LIST_ITEM_FREE_FUNC lw_freeSector
#include "list_impl.h"

void lw_calcCameraFrustum(LW_Camera *const cam) {

    const float half_v = cam->far_plane * tanf(cam->fov * .5f);
    const float half_h = half_v * cam->aspect_ratio;

    lw_vec4 cam_front, cam_right, cam_up, cam_front_far;
    lw_mat4MulVec4(cam->rot_mat, (lw_vec4){ 1.0f, 0.0f, 0.0f, 0.0f }, cam_right);
    lw_mat4MulVec4(cam->rot_mat, (lw_vec4){ 0.0f, 1.0f, 0.0f, 0.0f }, cam_front);
    lw_mat4MulVec4(cam->rot_mat, (lw_vec4){ 0.0f, 0.0f, 1.0f, 0.0f }, cam_up);

    for (unsigned _x = 0; _x < 3; ++_x)
        cam_front_far[_x] = cam_front[_x] * cam->far_plane;

    lw_vec3 tmp_vec[2];

    for (unsigned _x = 0; _x < 3; ++_x)
        cam->view_frustum.planes[0][_x] = cam_front[_x];
    for (unsigned _x = 0; _x < 3; ++_x)
        cam->view_frustum.planes[1][_x] = -cam_front[_x];

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_right[_x] * half_h;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
    lw_cross3d(tmp_vec[1], cam_up, cam->view_frustum.planes[2]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_right[_x] * half_h;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
    lw_cross3d(cam_up, tmp_vec[1], cam->view_frustum.planes[3]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_up[_x] * half_v;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
    lw_cross3d(cam_right, tmp_vec[1], cam->view_frustum.planes[4]);

    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[0][_x] = cam_up[_x] * half_v;
    for (unsigned _x = 0; _x < 3; ++_x)
        tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
    lw_cross3d(tmp_vec[1], cam_right, cam->view_frustum.planes[5]);

    for (unsigned i = 0; i < 6; ++i) {
        lw_normalize3d(cam->view_frustum.planes[i]);
        cam->view_frustum.planes[i][3] = lw_dot3d(cam->view_frustum.planes[i], cam->pos);
    }

    cam->view_frustum.planes[0][3] += cam->near_plane;
    cam->view_frustum.planes[1][3] += -cam->far_plane;
}

void lw_calcCameraProjection(LW_Camera *const camera) {
    lw_mat4Perspective(camera->fov, camera->aspect_ratio, camera->near_plane, camera->far_plane, camera->proj_mat);
}

LW_Frustum lw_calcFrustumFromPoly(lw_vec3 *polygon, unsigned num_verts, lw_vec3 view_point) {
    assert(num_verts >= 3);
    LW_Frustum frustum;

    frustum.num_planes = num_verts + 1;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));

    for (unsigned i = 0; i < num_verts; ++i) {
        unsigned j = (i + 1) % num_verts;

        lw_calcPlaneFromPoints(view_point, polygon[j], polygon[i], frustum.planes[i + 1]);
        frustum.planes[i + 1][3] -= 0.003; // small offset to avoid issues when clipping walls that share points with portal
    }

    // near plane
    lw_calcPlaneFromPoints(polygon[0], polygon[2], polygon[1], frustum.planes[0]);
    frustum.planes[0][3] -= 0.003;

    return frustum;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#define LOAD_MIN_SUPPORTED_VERSION 1
#define LOAD_MAX_SUPPORTED_VERSION 1

bool lw_loadPortalWorld(const char *path, float scale, LW_PortalWorld *o_pod) {
    assert(o_pod != NULL);

    //     LW_SectorList_init(&o_pod->sectors);

    //     FILE *file = fopen(path, "r");
    //     if (file == NULL) {
    //         printf("Failed to open %s\n", path);
    //         return false;
    //     }

    //     enum LoadState {
    //         state_version,
    //         state_open,
    //         state_sector,
    //         state_poly,
    //         state_subsec,
    //     } state = state_version;

    //     unsigned file_version;

    //     char line[1024];
    //     unsigned line_index = 0;

    //     char directive[64];
    //     int params_read = 0;
    //     unsigned unsigned_buffer[2];

    //     size_t sector_cache_len = 0, sector_cache_cap = 64;
    //     LW_Sector **sector_cache = malloc(sector_cache_cap * sizeof(*sector_cache));

    //     LW_Sector tmp_sector;
    //     bool polys_defined;
    //     bool discard_subsect_data;
    //     unsigned num_polys_read;
    //     unsigned num_subsecs_read;

    //     memset(&tmp_sector, 0, sizeof(tmp_sector));

    //     while (fgets(line, sizeof(line), file)) {
    //         ++line_index;
    //         params_read = sscanf(line, "%64s", directive);

    //         // empty line
    //         if (params_read < 1) continue;

    //         // comment
    //         if (directive[0] == '/' && directive[1] == '/') continue;

    //         switch (state) {
    //             case state_version:
    //                 // get the version number
    //                 if (strcmp(directive, "VERSION") == 0) {
    //                     params_read = sscanf(line, "%*s %u", &file_version);
    //                     if (params_read != 1) {
    //                         printf("ERROR %s:%u: `VERSION` expects one parameter\n", path, line_index);
    //                         goto _error_return;
    //                     }

    //                     // check version is supported
    //                     if (file_version < LOAD_MIN_SUPPORTED_VERSION ||
    //                         file_version > LOAD_MAX_SUPPORTED_VERSION) {
    //                         printf("ERROR %s:%u: Version %u is not supported\n", path, line_index, file_version);
    //                         goto _error_return;
    //                     }

    //                     state = state_open;

    //                 } else {
    //                     printf("ERROR %s:%u: Expected `VERSION` as first directive\n", path, line_index);
    //                     goto _error_return;
    //                 }
    //                 break;

    //             case state_open:
    //                 if (strcmp(directive, "SECTOR") == 0) {
    //                     params_read = sscanf(line, "%*s %u %u", &unsigned_buffer[0], &unsigned_buffer[1]);
    //                     if (params_read != 2) {
    //                         printf("ERROR %s:%u: `SECTOR` expects two parameters\n", path, line_index);
    //                         goto _error_return;
    //                     }

    //                     // clear items for sector state
    //                     memset(&tmp_sector, 0, sizeof(tmp_sector));
    //                     tmp_sector.num_walls       = unsigned_buffer[0];
    //                     tmp_sector.num_sub_sectors = unsigned_buffer[1];

    //                     polys_defined    = false;
    //                     num_polys_read   = 0;
    //                     num_subsecs_read = 0;

    //                     // init memory
    //                     tmp_sector.points       = malloc(tmp_sector.num_walls * 2 * sizeof(*tmp_sector.points));
    //                     tmp_sector.planes       = malloc(tmp_sector.num_walls * sizeof(*tmp_sector.planes));
    //                     tmp_sector.next_sectors = malloc(tmp_sector.num_walls * sizeof(*tmp_sector.next_sectors));
    //                     tmp_sector.sub_sectors  = malloc(tmp_sector.num_sub_sectors * sizeof(*tmp_sector.sub_sectors));

    //                     state = state_sector;

    //                 } else {
    //                     printf("ERROR %s:%u: Unrecognized directive `%s`\n", path, line_index, directive);
    //                     goto _error_return;
    //                 }

    //                 break;

    //             case state_sector:
    //                 if (strcmp(directive, "END") == 0) {
    //                     state = state_open;

    //                     if (num_subsecs_read < tmp_sector.num_sub_sectors) {
    //                         printf("ERROR %s:%u: Expected %u subsectors, but only %u defined\n", path, line_index, tmp_sector.num_sub_sectors, num_subsecs_read);
    //                         goto _error_return;
    //                     }

    //                     // finish sector
    //                     LW_SectorList_push_back(&o_pod->sectors, tmp_sector);
    //                     memset(&tmp_sector, 0, sizeof(tmp_sector));

    //                     if (sector_cache_len >= sector_cache_cap) {
    //                         // realloc to make room for stuff
    //                         sector_cache_cap += 64;
    //                         sector_cache = realloc(sector_cache, sector_cache_cap * sizeof(*sector_cache));
    //                     }
    //                     sector_cache[sector_cache_len] = &o_pod->sectors.tail->item;
    //                     ++sector_cache_len;

    //                 } else if (strcmp(directive, "POLY") == 0) {
    //                     // ensure poly is only defined once
    //                     if (polys_defined) {
    //                         printf("ERROR %s:%u: `POLY` defined more than once\n", path, line_index);
    //                         goto _error_return;
    //                     }
    //                     polys_defined = true;

    //                     state = state_poly;

    //                 } else if (strcmp(directive, "SUB") == 0) {

    //                     // avoid overflow
    //                     if (num_subsecs_read >= tmp_sector.num_sub_sectors) {
    //                         printf("WARNING:%s:%u: Only %u subsectors designated, extra discarded\n", path, line_index, tmp_sector.num_sub_sectors);
    //                         discard_subsect_data = true;
    //                     } else {
    //                         discard_subsect_data = false;
    //                     }

    //                     params_read = sscanf(line, "%*s %f %f",
    //                                          &tmp_sector.sub_sectors[num_subsecs_read].floor_height,
    //                                          &tmp_sector.sub_sectors[num_subsecs_read].ceiling_height);
    //                     if (params_read != 2) {
    //                         printf("ERROR %s:%u: `SUB` expects two parameters\n", path, line_index);
    //                         goto _error_return;
    //                     }

    //                     // change state
    //                     state = state_subsec;

    //                 } else {
    //                     printf("ERROR %s:%u: Unrecognized directive in `SECTOR`: `%s`\n", path, line_index, directive);
    //                     goto _error_return;
    //                 }
    //                 break;

    //             case state_poly:
    //                 if (strcmp(directive, "END") == 0) {
    //                     if (num_polys_read < tmp_sector.num_walls) {
    //                         printf("ERRROR:%s:%u: Expected %u vertices, but only %u defined\n", path, line_index, tmp_sector.num_walls, num_polys_read);
    //                         goto _error_return;
    //                     }

    //                     // calculate wall planes
    //                     for (unsigned i = 0; i < tmp_sector.num_walls; ++i) {
    //                         unsigned j = (i + 1) % tmp_sector.num_walls;
    //                         lw_vec3 p0 = { 0 }, p1 = { 0 };
    //                         p0[0] = tmp_sector.points[i][0];
    //                         p0[1] = tmp_sector.points[i][1];
    //                         p1[0] = tmp_sector.points[j][0];
    //                         p1[1] = tmp_sector.points[j][1];

    //                         lw_vec2 normal;
    //                         normal[0] = -(p1[1] - p0[1]);
    //                         normal[1] = (p1[0] - p0[0]);
    //                         lw_normalize2d(normal);

    //                         float d = lw_dot2d(normal, p0);

    //                         tmp_sector.planes[i][0] = normal[0];
    //                         tmp_sector.planes[i][1] = normal[1];
    //                         tmp_sector.planes[i][2] = 0.0f;
    //                         tmp_sector.planes[i][3] = d;
    //                     }

    //                     // finish up
    //                     state = state_sector;

    //                 } else {
    //                     // avoid overflow
    //                     if (num_polys_read >= tmp_sector.num_walls) {
    //                         printf("WARNING:%s:%u: Only %u vertices designated, extra discarded\n", path, line_index, tmp_sector.num_walls);
    //                         break;
    //                     }

    //                     // get data from the line
    //                     params_read = sscanf(line, "%f %f %u",
    //                                          &tmp_sector.points[num_polys_read][0],
    //                                          &tmp_sector.points[num_polys_read][1],
    //                                          &unsigned_buffer[0]);
    //                     if (params_read != 3) {
    //                         printf("ERROR %s:%u: `POLY:line` expects three parameters\n", path, line_index);
    //                         goto _error_return;
    //                     }

    //                     tmp_sector.points[num_polys_read][0] *= scale;
    //                     tmp_sector.points[num_polys_read][1] *= scale;

    //                     // pointer, but I can use it temporarily as an int
    //                     tmp_sector.next_sectors[num_polys_read] = (LW_Sector *)(intptr_t)unsigned_buffer[0];

    //                     // keep track of number of polygons read
    //                     ++num_polys_read;
    //                 }
    //                 break;

    //             case state_subsec:
    //                 if (strcmp(directive, "END") == 0) {
    //                     // keep track of number of subsectors read
    //                     ++num_subsecs_read;

    //                     state = state_sector;

    //                 } else {
    //                     // TODO: Read subsector information
    //                     if (!discard_subsect_data) {
    //                     }
    //                 }
    //                 break;
    //         }
    //     }
    //     fclose(file);


    //     LW_SectorListNode *node = o_pod->sectors.head;
    //     while (node != NULL) {
    //         LW_Sector *sector = &node->item;
    //         lw_vec2 point_b = {sector->points[0][0], sector->points[0][1]};

    //         for (unsigned i = sector->num_walls; i > 0;) {
    //             --i;

    //             // convert linear polygon point definition to wall based
    //             // starting from end: last wall == last_point, first_point
    //             // second to last == second_to_last, last_point
    //             sector->points[i * 2][0] = sector->points[i][0];
    //             sector->points[i * 2][1] = sector->points[i][1];
    //             sector->points[i * 2 + 1][0] = point_b[0];
    //             sector->points[i * 2 + 1][1] = point_b[1];

    //             point_b[0] = sector->points[i][0];
    //             point_b[1] = sector->points[i][1];

    //             // convert portal sector indices to pointers
    //             unsigned next = (unsigned)(intptr_t)sector->next_sectors[i];
    //             if (next != 0) {
    //                 --next;
    //                 if (next >= sector_cache_len) {
    //                     printf("ERROR %s: Sector next index out of defined sector bounds: %u\n", path, next);
    //                     goto _error_return;
    //                 }

    //                 sector->next_sectors[i] = sector_cache[(size_t)next];
    //             }
    //         }

    //         node = node->next;
    //     }

    //     free(sector_cache);
    //     return true;

    // _error_return:
    //     free(sector_cache);
    //     lw_freeSector(tmp_sector);
    //     lw_freePortalWorld(*o_pod);
    return false;
}

void lw_recalcLinePlane(LW_LineDef *const linedef) {
    LW_Sector *const sector = linedef->sector;
    lw_vec2 p0 = { 0 }, p1 = { 0 };

    p0[0] = linedef->start[0];
    p0[1] = linedef->start[1];
    p1[0] = sector->walls[linedef->end].start[0];
    p1[1] = sector->walls[linedef->end].start[1];

    lw_vec2 normal;
    normal[0] = -(p1[1] - p0[1]);
    normal[1] = (p1[0] - p0[0]);
    lw_normalize2d(normal);

    float d = lw_dot2d(normal, p0);

    linedef->plane[0] = normal[0];
    linedef->plane[1] = normal[1];
    linedef->plane[2] = 0.0f;
    linedef->plane[3] = d;
}

void lw_freeSubsector(LW_Subsector subsector) {
}

void lw_freeSector(LW_Sector sector) {
    for (unsigned i = 0; i < sector.num_sub_sectors; ++i) {
        lw_freeSubsector(sector.sub_sectors[i]);
    }

    free(sector.walls);
    free(sector.sub_sectors);
}

void lw_freePortalWorld(LW_PortalWorld pod) {
    LW_SectorList_free(pod.sectors);
}

bool lw_pointInSector(LW_Sector sector, lw_vec2 point) {
    lw_vec2 ray[2] = { { point[0], point[1] }, { point[0] - 1.0f, point[1] } };
    lw_vec2 line[2];
    float t;

    unsigned num_intersections = 0;

    // Point in polygon casts ray to left and counts number of intersections, odd inside, even out

    for (unsigned i = 0; i < sector.num_walls; ++i) {
        line[0][0] = sector.walls[i].start[0];
        line[0][1] = sector.walls[i].start[1];
        line[1][0] = sector.walls[sector.walls[i].end].start[0];
        line[1][1] = sector.walls[sector.walls[i].end].start[1];

        if (lw_intersectSegmentRay(line, ray, &t)) {
            // If hitting a vertex exactly, only count if other vertex is above the ray
            if (t != 0.0f && t != 1.0f) {
                ++num_intersections;
            } else if (t == 1.0f && line[0][1] > ray[0][1]) {
                ++num_intersections;
            } else if (line[1][1] > ray[0][1]) {
                ++num_intersections;
            }
        }
    }

    return num_intersections % 2 == 1;
}

LW_Sector *lw_getSector(LW_PortalWorld pod, lw_vec2 point) {
    // TODO: add bias to last known sector
    LW_Sector *sector = NULL;
    for (LW_SectorListNode *node = pod.sectors.head; node != NULL; node = node->next) {
        if (lw_pointInSector(node->item, point)) {
            sector = &node->item;
            break;
        }
    }

    return sector;
}

unsigned lw_getSubSector(LW_Sector *sector, lw_vec3 point) {
    if (sector == NULL) return 0;
    unsigned res = 0;

    for (unsigned i = 0; i < sector->num_sub_sectors; ++i) {
        if (point[2] < sector->sub_sectors[i].floor_height) {
            return res;
        } else {
            res = i;
        }
    }

    return res;
}

void _renderSector(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum Frustum, LW_Sector *default_sector);

void lw_renderPortalWorld(LW_Framebuffer *const framebuffer, LW_PortalWorld pod, LW_Camera camera) {
    if (pod.sectors.head == NULL) return;

    // duplicate frustum
    LW_Frustum frustum;
    frustum.num_planes = camera.view_frustum.num_planes;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));
    memcpy(frustum.planes, camera.view_frustum.planes, frustum.num_planes * sizeof(*frustum.planes));
    _renderSector(framebuffer, camera, frustum, &pod.sectors.head->item);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIP_BUFFER_SIZE 32
#define SECTOR_QUEUE_SIZE 128

lw_vec3 *_clipPolygon(LW_Frustum frustum, bool ignore_near, lw_vec3 *point_list0, lw_vec3 *point_list1, unsigned *io_len);

void _renderSector(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum start_frustum, LW_Sector *default_sector) {
    lw_vec3 clipping_list0[CLIP_BUFFER_SIZE];
    lw_vec3 clipping_list1[CLIP_BUFFER_SIZE];

    lw_vec4 transformed_points[CLIP_BUFFER_SIZE];
    lw_vec2 ndc_points[CLIP_BUFFER_SIZE];
    lw_vec2 screen_points[CLIP_BUFFER_SIZE];

    LW_Sector *sector_queue[SECTOR_QUEUE_SIZE];
    unsigned sub_sector_queue[SECTOR_QUEUE_SIZE];
    LW_Frustum frustum_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    sector_queue[sector_queue_end]     = cam.sector != NULL ? cam.sector : default_sector;
    sub_sector_queue[sector_queue_end] = cam.sector != NULL ? cam.sub_sector : 0;
    frustum_queue[sector_queue_end]    = start_frustum;
    sector_queue_end                   = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    unsigned sector_id = 0;

    while (sector_queue_start != sector_queue_end) {
        ++sector_id;
        LW_Sector *sector   = sector_queue[sector_queue_start];
        unsigned sub_sector = sub_sector_queue[sector_queue_start];
        LW_Frustum frustum  = frustum_queue[sector_queue_start];
        sector_queue_start  = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        LW_Subsector def = sector->sub_sectors[sub_sector];

        for (unsigned index0 = 0; index0 < sector->num_walls; ++index0) {
            lw_vec2 p0 = { 0 }, p1 = { 0 };
            LW_LineDef linedef = sector->walls[index0];

            p0[0] = linedef.start[0];
            p0[1] = linedef.start[1];
            p1[0] = sector->walls[linedef.end].start[0];
            p1[1] = sector->walls[linedef.end].start[1];

            // back face culling
            float back_face_test = lw_dot3d(linedef.plane, cam.pos);
            if (back_face_test < linedef.plane[3]) continue;

            LW_Sector *next = linedef.next;

            if (next == NULL) {
                // render whole wall

                // init clipping list
                clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = def.floor_height;
                clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = def.ceiling_height;
                clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = def.ceiling_height;
                clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = def.floor_height;
                unsigned clipped_len = 4;

                // clip
                lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                // transform to screen coords
                for (unsigned i = 0; i < clipped_len; ++i) {
                    lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                    lw_mat4MulVec4(cam.vp_mat, a, transformed_points[i]);

                    float inv_w;
                    if (transformed_points[i][3] > 0.0f) {
                        inv_w = 1.0f / transformed_points[i][3];
                    } else {
                        inv_w = 0.0f;
                    }

                    ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                    ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                    screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (framebuffer->width - 1);
                    screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (framebuffer->height - 1);
                }

                // render
                lw_drawPoly(framebuffer, screen_points, clipped_len, LW_COLOR_WHITE);
            } else {
                // render steps

                // step through each sub sector of next sector
                // if next sub sector's ceiling < current floor, continue
                // if next sub sector's floor > current ceiling, continue
                // do normal stuff

                float max_ceiling = def.floor_height;
                float step_bottom = def.floor_height;
                for (unsigned ssid = 0; ssid < next->num_sub_sectors; ++ssid) {
                    LW_Subsector next_def = next->sub_sectors[ssid];
                    if (next_def.ceiling_height <= def.floor_height ||
                        next_def.floor_height >= def.ceiling_height) continue;

                    max_ceiling = max(max_ceiling, next_def.ceiling_height);

                    if (step_bottom < next_def.floor_height) {
                        // bottom step

                        // init clipping list
                        clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = step_bottom;
                        clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = next_def.floor_height;
                        clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = next_def.floor_height;
                        clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = step_bottom;
                        unsigned clipped_len = 4;

                        // clip
                        lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                        // transform to screen coords
                        for (unsigned i = 0; i < clipped_len; ++i) {
                            lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                            lw_mat4MulVec4(cam.vp_mat, a, transformed_points[i]);

                            float inv_w;
                            if (transformed_points[i][3] > 0.0f) {
                                inv_w = 1.0f / transformed_points[i][3];
                            } else {
                                inv_w = 0.0f;
                            }

                            ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                            ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                            screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (framebuffer->width - 1);
                            screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (framebuffer->height - 1);
                        }

                        // render
                        lw_drawPoly(framebuffer, screen_points, clipped_len, LW_COLOR_WHITE);
                    }
                    step_bottom = next_def.ceiling_height;

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = max(def.floor_height, next_def.floor_height);
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = max(def.floor_height, next_def.floor_height);
                    unsigned clipped_len = 4;

                    // clip
                    lw_vec3 *clipped_poly = _clipPolygon(frustum, true, clipping_list0, clipping_list1, &clipped_len);

                    if (clipped_len > 2) {
                        LW_Frustum next_frustum = lw_calcFrustumFromPoly(clipped_poly, clipped_len, cam.pos); // TODO add far plane

                        // draw next sector
                        sector_queue[sector_queue_end]     = next;
                        sub_sector_queue[sector_queue_end] = ssid;
                        frustum_queue[sector_queue_end]    = next_frustum;
                        sector_queue_end                   = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;
                    }
                }
                if (def.ceiling_height > max_ceiling) {
                    // top step

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = def.ceiling_height;
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = max_ceiling;
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = max_ceiling;
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = def.ceiling_height;
                    unsigned clipped_len = 4;

                    // clip
                    lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                    // transform to screen coords
                    for (unsigned i = 0; i < clipped_len; ++i) {

                        lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                        lw_mat4MulVec4(cam.vp_mat, a, transformed_points[i]);

                        float inv_w;
                        if (transformed_points[i][3] > 0.0f) {
                            inv_w = 1.0f / transformed_points[i][3];
                        } else {
                            inv_w = 0.0f;
                        }

                        ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                        ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                        screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (framebuffer->width - 1);
                        screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (framebuffer->height - 1);
                    }

                    // render
                    lw_drawPoly(framebuffer, screen_points, clipped_len, LW_COLOR_WHITE);
                }
            }
        }

        // render floor
        if (cam.pos[2] > def.floor_height) {
            // init clipping list
            for (unsigned i = 0; i < sector->num_walls; ++i) {
                clipping_list0[i][0] = sector->walls[i].start[0], clipping_list0[i][1] = sector->walls[i].start[1], clipping_list0[i][2] = def.floor_height;
            }

            unsigned clipped_len = sector->num_walls;

            // clip
            lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

            // transform to screen coords
            for (unsigned i = 0; i < clipped_len; ++i) {
                lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                lw_mat4MulVec4(cam.vp_mat, a, transformed_points[i]);

                float inv_w;
                if (transformed_points[i][3] > 0.0f) {
                    inv_w = 1.0f / transformed_points[i][3];
                } else {
                    inv_w = 0.0f;
                }

                ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (framebuffer->width - 1);
                screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (framebuffer->height - 1);
            }

            // render
            lw_drawPoly(framebuffer, screen_points, clipped_len, LW_COLOR_RED);
        }

        // render ceiling
        if (cam.pos[2] < def.ceiling_height) {
            // init clipping list
            for (unsigned i = 0; i < sector->num_walls; ++i) {
                unsigned j           = sector->num_walls - i - 1;
                clipping_list0[j][0] = sector->walls[i].start[0], clipping_list0[j][1] = sector->walls[i].start[1], clipping_list0[j][2] = def.ceiling_height;
            }

            unsigned clipped_len = sector->num_walls;

            // clip
            lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

            // transform to screen coords
            for (unsigned i = 0; i < clipped_len; ++i) {
                lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                lw_mat4MulVec4(cam.vp_mat, a, transformed_points[i]);

                float inv_w;
                if (transformed_points[i][3] > 0.0f) {
                    inv_w = 1.0f / transformed_points[i][3];
                } else {
                    inv_w = 0.0f;
                }

                ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (framebuffer->width - 1);
                screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (framebuffer->height - 1);
            }

            // render
            lw_drawPoly(framebuffer, screen_points, clipped_len, LW_COLOR_YELLOW);
        }

        free(frustum.planes);
    }
}

lw_vec3 *_clipPolygon(LW_Frustum frustum, bool ignore_near, lw_vec3 *point_list0, lw_vec3 *point_list1, unsigned *io_len) {
    // https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    assert(point_list0 != NULL);
    assert(point_list1 != NULL);
    assert(io_len != NULL);

    unsigned clipping_in_len;
    unsigned clipping_out_len = *io_len;
    lw_vec3 *clipping_in      = point_list1;
    lw_vec3 *clipping_out     = point_list0;

    for (unsigned plane_index = ignore_near ? 1 : 0; plane_index < frustum.num_planes; ++plane_index) {
        swap(lw_vec3 *, clipping_in, clipping_out);
        clipping_in_len  = clipping_out_len;
        clipping_out_len = 0;

        for (unsigned edge_curr = 0; edge_curr < clipping_in_len; ++edge_curr) {
            unsigned edge_prev = (edge_curr - 1 + clipping_in_len) % clipping_in_len;

            bool in_curr;
            bool in_prev;

            {
                float d0 = lw_dot3d(frustum.planes[plane_index], clipping_in[edge_curr]) - frustum.planes[plane_index][3];
                float d1 = lw_dot3d(frustum.planes[plane_index], clipping_in[edge_prev]) - frustum.planes[plane_index][3];

                in_curr = d0 >= 0.0f;
                in_prev = d1 >= 0.0f;
            }

            lw_vec3 line[2] = {
                { clipping_in[edge_prev][0], clipping_in[edge_prev][1], clipping_in[edge_prev][2] },
                { clipping_in[edge_curr][0], clipping_in[edge_curr][1], clipping_in[edge_curr][2] },
            };
            float t         = 0.0f;
            bool intersects = lw_intersectSegmentPlane(line, frustum.planes[plane_index], &t);

            lw_vec3 intersection_point = {
                intersection_point[0] = lerp(clipping_in[edge_prev][0], clipping_in[edge_curr][0], t),
                intersection_point[1] = lerp(clipping_in[edge_prev][1], clipping_in[edge_curr][1], t),
                intersection_point[2] = lerp(clipping_in[edge_prev][2], clipping_in[edge_curr][2], t),
            };

            if (in_curr) {
                if (intersects && !in_prev) {
                    // add intersection point
                    for (unsigned _x = 0; _x < 3; ++_x) {
                        clipping_out[clipping_out_len][_x] = intersection_point[_x];
                    }
                    ++clipping_out_len;
                    assert(clipping_out_len < CLIP_BUFFER_SIZE);
                }
                // add current point
                for (unsigned _x = 0; _x < 3; ++_x) {
                    clipping_out[clipping_out_len][_x] = clipping_in[edge_curr][_x];
                }
                ++clipping_out_len;
                assert(clipping_out_len < CLIP_BUFFER_SIZE);

            } else if (intersects && in_prev) {
                // add intersection point
                for (unsigned _x = 0; _x < 3; ++_x) {
                    clipping_out[clipping_out_len][_x] = intersection_point[_x];
                }
                ++clipping_out_len;
                assert(clipping_out_len < CLIP_BUFFER_SIZE);
            }
        }
        if (clipping_out_len == 0) break;
    }

    *io_len = clipping_out_len;

    return clipping_out;
}