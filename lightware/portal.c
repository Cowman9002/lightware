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

void lw_screenPointToRay(LW_Camera cam, int width, int height, lw_vec2 point, lw_vec3 o_pos, lw_vec3 o_dir) {
    // https://gamedev.stackexchange.com/questions/194575/what-is-the-logic-behind-of-screenpointtoray

    // Remap so (0, 0) is the center of the window,
    // and the edges are at -0.5 and +0.5.
    lw_vec2 relative;
    relative[0] = point[0] / width - 0.5f;
    relative[1] = -(point[1] / height - 0.5f);

    // Angle in radians from the view axis
    // to the top plane of the view pyramid.
    float angle = 0.5f * cam.fov;

    // World space height of the view pyramid
    // measured at 1 m depth from the camera.
    float world_height = 2.0f * tanf(angle);

    // Convert relative position to world units.
    lw_vec4 world_units;
    world_units[0] = relative[0] * world_height * cam.aspect_ratio;
    world_units[1] = 1.0f;
    world_units[2] = relative[1] * world_height;
    world_units[1] = 1.0f;

    // Rotate to match camera orientation.
    lw_vec4 direction;
    lw_mat4MulVec4(cam.rot_mat, world_units, direction);

    o_pos[0] = cam.pos[0], o_pos[1] = cam.pos[1], o_pos[2] = cam.pos[2];
    o_dir[0] = direction[0], o_dir[1] = direction[1], o_dir[2] = direction[2];
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void lw_recalcLinePlane(LW_LineDef *const linedef) {
    LW_Sector *const sector = linedef->sector;
    if (linedef->next >= sector->num_walls) return;

    lw_vec2 p0 = { 0 }, p1 = { 0 };

    p0[0] = linedef->start[0];
    p0[1] = linedef->start[1];
    p1[0] = sector->walls[linedef->next].start[0];
    p1[1] = sector->walls[linedef->next].start[1];

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
    for (unsigned i = 0; i < sector.num_subsectors; ++i) {
        lw_freeSubsector(sector.subsectors[i]);
    }

    free(sector.walls);
    free(sector.subsectors);
}

void lw_freePortalWorld(LW_PortalWorld pod) {
    LW_SectorList_free(pod.sectors);
}

bool lw_pointInSector(LW_Sector sector, lw_vec2 point, bool count_edges) {
    lw_vec2 ray[2] = { { point[0], point[1] }, { -1.0f, 0.0f } };
    lw_vec2 line[2];
    float t, u;

    unsigned num_intersections = 0;

    // Point in polygon casts ray to left and counts number of intersections, odd inside, even out

    for (unsigned i = 0; i < sector.num_walls; ++i) {
        line[0][0] = sector.walls[i].start[0];
        line[0][1] = sector.walls[i].start[1];
        line[1][0] = sector.walls[sector.walls[i].next].start[0];
        line[1][1] = sector.walls[sector.walls[i].next].start[1];

        if (lw_pointOnSegment(line, point)) {
            return count_edges;
        }

        if (lw_intersectSegmentRay(line, ray, &t, &u)) {
            // If hitting a vertex exactly, only count if other vertex is above the ray
            if (t != 0.0f && t != 1.0f) {
                ++num_intersections;
            } else if (t == 1.0f && line[0][1] > ray[0][1]) {
                ++num_intersections;
            } else if (t == 0.0f && line[1][1] > ray[0][1]) {
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
        if (lw_pointInSector(node->item, point, 0.0f)) {
            sector = &node->item;
            break;
        }
    }

    return sector;
}

unsigned lw_getSubSector(LW_Sector *sector, lw_vec3 point) {
    if (sector == NULL) return 0;
    unsigned res = 0;

    for (unsigned i = 0; i < sector->num_subsectors; ++i) {
        if (point[2] < sector->subsectors[i].floor_height) {
            return res;
        } else {
            res = i;
        }
    }

    return res;
}

void lw_resetPortalWorldTimers(LW_PortalWorld pod) {
    LW_SectorListNode *node = pod.sectors.head;
    for (; node != NULL; node = node->next) {
        for (unsigned i = 0; i < node->item.num_walls; ++i) {
            node->item.walls[i].last_look_frame = 0;
        }
    }
}

void _renderSector(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum Frustum, LW_Sector *default_sector, uint64_t frame);
void lw_renderPortalWorld(LW_Framebuffer *const framebuffer, LW_PortalWorld pod, LW_Camera camera, uint64_t frame) {
    if (pod.sectors.head == NULL) return;

    // duplicate frustum
    LW_Frustum frustum;
    frustum.num_planes = camera.view_frustum.num_planes;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));
    memcpy(frustum.planes, camera.view_frustum.planes, frustum.num_planes * sizeof(*frustum.planes));
    _renderSector(framebuffer, camera, frustum, &pod.sectors.head->item, frame);
}

void _renderSectorWireframe(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum Frustum, LW_Sector *default_sector);
void lw_renderPortalWorldWireframe(LW_Framebuffer *const framebuffer, LW_PortalWorld pod, LW_Camera camera) {
    if (pod.sectors.head == NULL) return;

    // duplicate frustum
    LW_Frustum frustum;
    frustum.num_planes = camera.view_frustum.num_planes;
    frustum.planes     = malloc(frustum.num_planes * sizeof(*frustum.planes));
    memcpy(frustum.planes, camera.view_frustum.planes, frustum.num_planes * sizeof(*frustum.planes));
    _renderSectorWireframe(framebuffer, camera, frustum, &pod.sectors.head->item);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIP_BUFFER_SIZE 64
#define SECTOR_QUEUE_SIZE 128

lw_vec3 *_clipPolygon(LW_Frustum frustum, bool ignore_near, lw_vec3 *point_list0, lw_vec3 *point_list1, unsigned *io_len);

void _renderConvexPoly(LW_Framebuffer *const framebuffer, lw_ivec2 *points, unsigned num_points, int *poly_x_start, int *poly_x_end, LW_Color base_color) {
    // using the screen bounds instead of some arbitrary large and small values
    //  should prevent going out of bounds when actually setting pixels

    // find min and max
    int y_start = framebuffer->height - 1, y_end = 0;
    for (int i = 0; i < num_points; ++i) {
        if (points[i][1] < y_start) y_start = points[i][1];
        if (points[i][1] > y_end) y_end = points[i][1];
    }

    y_start = max(y_start, 0);
    y_end   = min(y_end, framebuffer->height);

    // preset arrays to values
    for (int i = y_start; i < y_end; ++i) {
        poly_x_start[i] = framebuffer->width - 1;
        poly_x_end[i]   = 0;
    }

    // fill edge arrays
    for (int i = 0; i < num_points; ++i) {
        lw_vec2 a = { points[i][0], points[i][1] };
        lw_vec2 b = { points[(i + 1) % num_points][0], points[(i + 1) % num_points][1] };

        if (b[1] == a[1]) continue;

        // go from top to bottom
        if (b[1] < a[1]) {
            swap(float, a[0], b[0]);
            swap(float, a[1], b[1]);
        }

        float x      = a[0];
        float deltax = (b[0] - a[0]) / (b[1] - a[1]);
        for (int y = a[1]; y < b[1]; ++y) {
            if (y < y_start || y >= y_end) {
                continue; // this is the result of poor clipping
            }

            if ((int)x < poly_x_start[y]) poly_x_start[y] = (int)x;
            if ((int)x > poly_x_end[y]) poly_x_end[y] = (int)x;
            x += deltax;
        }
    }

    // draw from edge arrays
    for (int y = y_start; y < y_end; ++y) {
        int x_start = max(poly_x_start[y], 0);
        int x_end   = min(poly_x_end[y], framebuffer->width);

        for (int x = x_start; x < x_end; ++x) {
            framebuffer->pixels[x + y * framebuffer->width] = base_color;
        }
    }
}

// TODO: ? add span buffer https://www.flipcode.com/archives/The_Coverage_Buffer_C-Buffer.shtml
void _renderSector(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum start_frustum, LW_Sector *default_sector, uint64_t frame) {
    // allocate both arrays at once
    int *poly_x_start = malloc(framebuffer->height * 2 * sizeof(*poly_x_start));
    int *poly_x_end   = &poly_x_start[framebuffer->height];

    lw_vec3 clipping_list0[CLIP_BUFFER_SIZE];
    lw_vec3 clipping_list1[CLIP_BUFFER_SIZE];

    LW_Sector *sector_queue[SECTOR_QUEUE_SIZE];
    unsigned subsector_queue[SECTOR_QUEUE_SIZE];
    LW_Frustum frustum_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    sector_queue[sector_queue_end]    = cam.sector != NULL ? cam.sector : default_sector;
    subsector_queue[sector_queue_end] = cam.sector != NULL ? cam.subsector : 0;
    frustum_queue[sector_queue_end]   = start_frustum;
    sector_queue_end                  = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;
    bool first                        = true;

    struct Poly {
        lw_vec4 *points;
        unsigned len;
        unsigned index;
        LW_LineDef *linedef;
    };

    // sorted nearest to farthest
    unsigned poly_capacity = 32;
    unsigned poly_count    = 0;
    struct Poly *polys     = calloc(poly_capacity, sizeof(*polys));

    // generate polygons
    while (sector_queue_start != sector_queue_end) {
        LW_Sector *sector  = sector_queue[sector_queue_start];
        unsigned subsector = subsector_queue[sector_queue_start];
        LW_Frustum frustum = frustum_queue[sector_queue_start];
        LW_Subsector def   = sector->subsectors[subsector];

        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        unsigned sector_id = (intptr_t)sector & 0xffff;

        for (unsigned wall_i = 0; wall_i < sector->num_walls; ++wall_i) {
            LW_LineDef *linedef = &sector->walls[wall_i];
            lw_vec2 p0 = { 0 }, p1 = { 0 };

            p0[0] = linedef->start[0];
            p0[1] = linedef->start[1];
            p1[0] = sector->walls[linedef->next].start[0];
            p1[1] = sector->walls[linedef->next].start[1];

            // back face culling
            float back_face_test = lw_dot3d(linedef->plane, cam.pos);
            if (back_face_test < linedef->plane[3]) continue;

            LW_Sector *portal_sector = linedef->portal_sector;

            if (portal_sector == NULL) {
                // init clipping list
                clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = def.floor_height;
                clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = def.ceiling_height;
                clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = def.ceiling_height;
                clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = def.floor_height;
                unsigned clipped_len = 4;

                // clip
                lw_vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);
                if (clipped_len < 3) continue;

                lw_vec4 *ndc_points = calloc(clipped_len, sizeof(*ndc_points));

                // transform to normalized device coords
                for (unsigned i = 0; i < clipped_len; ++i) {
                    lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                    lw_vec4 t;
                    lw_mat4MulVec4(cam.vp_mat, a, t);

                    float inv_w;
                    if (t[3] > 0.0f) {
                        inv_w = 1.0f / t[3];
                    } else {
                        inv_w = 0.0f;
                    }

                    ndc_points[i][0] = t[0] * inv_w;
                    ndc_points[i][1] = t[2] * inv_w;
                    ndc_points[i][2] = t[1] * inv_w;
                    ndc_points[i][3] = t[3] * inv_w;
                }

                // add to list
                struct Poly p = { .points = ndc_points, .len = clipped_len, .index = wall_i + sector_id * 0xffff, .linedef = linedef };
                if (poly_count + 1 >= poly_capacity) {
                    poly_capacity = poly_count + 32;
                    polys         = realloc(polys, poly_capacity * sizeof(*polys));
                }
                polys[poly_count++] = p;
            } else if (linedef->last_look_frame < frame) {
                linedef->last_look_frame = frame;

                // render steps
                // step through each sub sector of next sector
                // if next sub sector's ceiling < current floor, continue
                // if next sub sector's floor > current ceiling, continue
                // do normal stuff

                float max_ceiling = def.floor_height;
                float step_bottom = def.floor_height;
                for (unsigned ssid = 0; ssid < portal_sector->num_subsectors; ++ssid) {
                    LW_Subsector next_def = portal_sector->subsectors[ssid];

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
                        if (clipped_len >= 3) {
                            lw_vec4 *ndc_points = calloc(clipped_len, sizeof(*ndc_points));

                            // transform to normalized device coords
                            for (unsigned i = 0; i < clipped_len; ++i) {
                                lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                                lw_vec4 t;
                                lw_mat4MulVec4(cam.vp_mat, a, t);

                                float inv_w;
                                if (t[3] > 0.0f) {
                                    inv_w = 1.0f / t[3];
                                } else {
                                    inv_w = 0.0f;
                                }

                                ndc_points[i][0] = t[0] * inv_w;
                                ndc_points[i][1] = t[2] * inv_w;
                                ndc_points[i][2] = t[1] * inv_w;
                                ndc_points[i][3] = t[3] * inv_w;
                            }

                            // add to list
                            struct Poly p = { .points = ndc_points, .len = clipped_len, .index = wall_i + sector_id * 0xffff, .linedef = linedef };
                            if (poly_count + 1 >= poly_capacity) {
                                poly_capacity = poly_count + 32;
                                polys         = realloc(polys, poly_capacity * sizeof(*polys));
                            }
                            polys[poly_count++] = p;
                        }
                    }
                    step_bottom = next_def.ceiling_height;

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = max(def.floor_height, next_def.floor_height);
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = max(def.floor_height, next_def.floor_height);
                    unsigned clipped_len = 4;

                    // clip
                    lw_vec3 *clipped_poly = _clipPolygon(frustum, first, clipping_list0, clipping_list1, &clipped_len);

                    if (clipped_len > 2) {
                        LW_Frustum next_frustum = lw_calcFrustumFromPoly(clipped_poly, clipped_len, cam.pos); // TODO add far plane

                        // draw next sector
                        sector_queue[sector_queue_end]    = portal_sector;
                        subsector_queue[sector_queue_end] = ssid;
                        frustum_queue[sector_queue_end]   = next_frustum;
                        sector_queue_end                  = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;
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
                    if (clipped_len >= 3) {

                        lw_vec4 *ndc_points = calloc(clipped_len, sizeof(*ndc_points));

                        // transform to normalized device coords
                        for (unsigned i = 0; i < clipped_len; ++i) {
                            lw_vec4 a = { clipped_poly[i][0], clipped_poly[i][1], clipped_poly[i][2], 1.0f };
                            lw_vec4 t;
                            lw_mat4MulVec4(cam.vp_mat, a, t);

                            float inv_w;
                            if (t[3] > 0.0f) {
                                inv_w = 1.0f / t[3];
                            } else {
                                inv_w = 0.0f;
                            }

                            ndc_points[i][0] = t[0] * inv_w;
                            ndc_points[i][1] = t[2] * inv_w;
                            ndc_points[i][2] = t[1] * inv_w;
                            ndc_points[i][3] = t[3] * inv_w;
                        }

                        // add to list
                        struct Poly p = { .points = ndc_points, .len = clipped_len, .index = wall_i + sector_id * 0xffff, .linedef = linedef };
                        if (poly_count + 1 >= poly_capacity) {
                            poly_capacity = poly_count + 32;
                            polys         = realloc(polys, poly_capacity * sizeof(*polys));
                        }
                        polys[poly_count++] = p;
                    }
                }
            }
        }

        free(frustum.planes);
        first = false;
    }

    {
        // sort walls nearest to farthest

        int i, j;
        struct Poly key;
        for (i = 1; i < poly_count; i++) {
            key = polys[i];
            j   = i - 1;

            while (j >= 0) {

                // polys[j] <= key: break

                float d0 = lw_dot2d(polys[j].linedef->plane, key.linedef->start) - polys[j].linedef->plane[3];
                float d1 = lw_dot2d(polys[j].linedef->plane, key.linedef->sector->walls[key.linedef->next].start) - polys[j].linedef->plane[3];
                float d2 = lw_dot2d(key.linedef->plane, polys[j].linedef->start) - key.linedef->plane[3];
                // float d3 = lw_dot2d(key.linedef->plane, sector->walls[polys[j].linedef->next].start);

                if ((d0 >= 0 && d1 >= 0) || (d0 <= 0 && d1 <= 0)) {
                    // both points of key are on the same side of j, so j is the infinite plane to test against
                    float dp = lw_dot2d(polys[j].linedef->plane, cam.pos) - polys[j].linedef->plane[3];

                    if ((dp >= 0 && d0 >= 0) || (dp <= 0 && d0 <= 0)) {
                        // key and cam are on the same side of the wall, therefore key is in front of j
                    } else {
                        // key and cam are on opposite sides of the wall, therefore j is in front of key
                        break;
                    }

                } else {
                    // both points of j are on the same side of key, so key is the infinite plane to test against
                    float dp = lw_dot2d(key.linedef->plane, cam.pos) - key.linedef->plane[3];

                    if ((dp >= 0 && d2 >= 0) || (dp <= 0 && d2 <= 0)) {
                        // j and cam are on the same side of the wall, therefore j is in front of key
                        break;
                    } else {
                        // j and cam are on opposite sides of the wall, therefore key is in front of j
                    }
                }

                polys[j + 1] = polys[j];
                j            = j - 1;
            }
            polys[j + 1] = key;
        }
    }

    // render

    lw_ivec2 point_buffer[64];
    // for (unsigned poly_i = 0; poly_i < poly_count; ++poly_i) {
    for (unsigned poly_i = poly_count; poly_i > 0;) {
        --poly_i;

        struct Poly poly = polys[poly_i];

        unsigned n = min(poly.len, 64);
        for (unsigned i = 0; i < n; ++i) {
            point_buffer[i][0] = (poly.points[i][0] * 0.5f + 0.5f) * (framebuffer->width - 1);
            point_buffer[i][1] = (-poly.points[i][1] * 0.5f + 0.5f) * (framebuffer->height - 1);
        }

        unsigned ci    = poly.index * 16 + 10;
        LW_Color color = LW_COLOR_BLACK;
        color.r        = lw_xorShift32(&ci);
        color.g        = lw_xorShift32(&ci);
        color.b        = lw_xorShift32(&ci);

        _renderConvexPoly(framebuffer, point_buffer, n, poly_x_start, poly_x_end, color);
    }

    free(polys);
    free(poly_x_start);
    // poly_x_end is just a pointer into this array, so don't free it
}

void _renderSectorWireframe(LW_Framebuffer *const framebuffer, LW_Camera cam, LW_Frustum start_frustum, LW_Sector *default_sector) {
    lw_vec3 clipping_list0[CLIP_BUFFER_SIZE];
    lw_vec3 clipping_list1[CLIP_BUFFER_SIZE];

    lw_vec4 transformed_points[CLIP_BUFFER_SIZE];
    lw_vec2 ndc_points[CLIP_BUFFER_SIZE];
    lw_vec2 screen_points[CLIP_BUFFER_SIZE];

    LW_Sector *sector_queue[SECTOR_QUEUE_SIZE];
    unsigned subsector_queue[SECTOR_QUEUE_SIZE];
    LW_Frustum frustum_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    sector_queue[sector_queue_end]    = cam.sector != NULL ? cam.sector : default_sector;
    subsector_queue[sector_queue_end] = cam.sector != NULL ? cam.subsector : 0;
    frustum_queue[sector_queue_end]   = start_frustum;
    sector_queue_end                  = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    unsigned sector_id = 0;
    bool first         = true;

    while (sector_queue_start != sector_queue_end) {
        ++sector_id;
        LW_Sector *sector  = sector_queue[sector_queue_start];
        unsigned subsector = subsector_queue[sector_queue_start];
        LW_Frustum frustum = frustum_queue[sector_queue_start];
        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        LW_Subsector def = sector->subsectors[subsector];

        for (unsigned index0 = 0; index0 < sector->num_walls; ++index0) {
            lw_vec2 p0 = { 0 }, p1 = { 0 };
            LW_LineDef linedef = sector->walls[index0];

            p0[0] = linedef.start[0];
            p0[1] = linedef.start[1];
            p1[0] = sector->walls[linedef.next].start[0];
            p1[1] = sector->walls[linedef.next].start[1];

            // back face culling
            float back_face_test = lw_dot3d(linedef.plane, cam.pos);
            if (back_face_test < linedef.plane[3]) continue;

            LW_Sector *portal_sector = linedef.portal_sector;

            if (portal_sector == NULL) {
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
                for (unsigned ssid = 0; ssid < portal_sector->num_subsectors; ++ssid) {
                    LW_Subsector next_def = portal_sector->subsectors[ssid];
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
                    lw_vec3 *clipped_poly = _clipPolygon(frustum, first, clipping_list0, clipping_list1, &clipped_len);

                    if (clipped_len > 2) {
                        LW_Frustum next_frustum = lw_calcFrustumFromPoly(clipped_poly, clipped_len, cam.pos); // TODO add far plane

                        // draw next sector
                        sector_queue[sector_queue_end]    = portal_sector;
                        subsector_queue[sector_queue_end] = ssid;
                        frustum_queue[sector_queue_end]   = next_frustum;
                        sector_queue_end                  = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;
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

        free(frustum.planes);
        first = false;
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
