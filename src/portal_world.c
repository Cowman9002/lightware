#include "portal_world.h"

#include "draw.h"
#include "geo.h"

#include <stdio.h>
#include <malloc.h>
#include <assert.h>

#define LIST_TAG SectorList
#define LIST_ITEM_TYPE Sector
#define LIST_ITEM_FREE_FUNC freeSector
#include "list_impl.h"

void freeSectorDef(SectorDef def) {
}

void freeSector(Sector sector) {
    for (unsigned i = 0; i < sector.num_sub_sectors; ++i) {
        freeSectorDef(sector.sub_sectors[i]);
    }

    free(sector.points);
    free(sector.planes);
    free(sector.next_sectors);
    free(sector.sub_sectors);
}

void freePortalWorld(PortalWorld pod) {
    SectorList_free(pod.sectors);
}

bool pointInSector(Sector sector, vec3 point) {
    for (unsigned i = 0; i < sector.num_walls; ++i) {
        float dot = dot3d(sector.planes[i], point);
        if (dot < sector.planes[i][3]) return false;
    }

    return true;
}

Sector *getSector(PortalWorld pod, vec3 point) {
    Sector *sector = NULL;
    for (SectorListNode *node = pod.sectors.head; node != NULL; node = node->next) {
        if (pointInSector(node->item, point)) {
            sector = &node->item;
        }
    }

    return sector;
}

unsigned getSubSector(Sector *sector, vec3 point) {
    for (unsigned i = 0; i < sector->num_sub_sectors; ++i) {
        if (point[2] >= sector->sub_sectors[i].floor_height &&
            point[2] <= sector->sub_sectors[i].ceiling_height)
            return i;
    }

    return 0;
}

void _renderSector(Camera cam, Frustum Frustum, Sector *default_sector);

void portalWorldRender(PortalWorld pod, Camera cam) {
    if (pod.sectors.head == NULL) return;

    Frustum frustum = calcFrustumFromCamera(cam);
    _renderSector(cam, frustum, pod.sectors.head);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIP_BUFFER_SIZE 16
#define SECTOR_QUEUE_SIZE 64

vec3 *_clipPolygon(Frustum frustum, bool ignore_near, vec3 *point_list0, vec3 *point_list1, unsigned *io_len);

void _renderSector(Camera cam, Frustum start_frustum, Sector *default_sector) {
    vec3 clipping_list0[CLIP_BUFFER_SIZE];
    vec3 clipping_list1[CLIP_BUFFER_SIZE];

    vec4 transformed_points[CLIP_BUFFER_SIZE];
    vec2 ndc_points[CLIP_BUFFER_SIZE];
    vec2 screen_points[CLIP_BUFFER_SIZE];

    Sector *sector_queue[SECTOR_QUEUE_SIZE];
    unsigned sub_sector_queue[SECTOR_QUEUE_SIZE];
    Frustum frustum_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    sector_queue[sector_queue_end]     = cam.sector != NULL ? cam.sector : default_sector;
    sub_sector_queue[sector_queue_end] = cam.sector != NULL ? cam.sub_sector : 0;
    frustum_queue[sector_queue_end]    = start_frustum;
    sector_queue_end                   = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    unsigned sector_id = 0;

    while (sector_queue_start != sector_queue_end) {
        ++sector_id;
        Sector *sector      = sector_queue[sector_queue_start];
        unsigned sub_sector = sub_sector_queue[sector_queue_start];
        Frustum frustum     = frustum_queue[sector_queue_start];
        sector_queue_start  = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        SectorDef def = sector->sub_sectors[sub_sector];

        for (unsigned index0 = 0; index0 < sector->num_walls; ++index0) {
            unsigned index1 = (index0 + 1) % sector->num_walls;
            vec3 p0 = { 0 }, p1 = { 0 };
            p0[0] = sector->points[index0][0];
            p0[1] = sector->points[index0][1];
            p1[0] = sector->points[index1][0];
            p1[1] = sector->points[index1][1];

            // back face culling
            float back_face_test = dot3d(sector->planes[index0], cam.pos);
            if (back_face_test < sector->planes[index0][3]) continue;

            Sector *next = sector->next_sectors[index0];

            if (next == NULL) {
                // render whole wall

                // init clipping list
                clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = def.floor_height;
                clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = def.ceiling_height;
                clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = def.ceiling_height;
                clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = def.floor_height;
                unsigned clipped_len = 4;

                // clip
                vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                // transform to screen coords
                for (unsigned i = 0; i < clipped_len; ++i) {
                    transformed_points[i][3] = mat4MulVec3(cam.vp_mat, clipped_poly[i], transformed_points[i]);

                    float inv_w;
                    if (transformed_points[i][3] > 0.0f) {
                        inv_w = 1.0f / transformed_points[i][3];
                    } else {
                        inv_w = 0.0f;
                    }

                    ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                    ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                    screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (SCREEN_WIDTH - 1);
                    screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (SCREEN_HEIGHT - 1);
                }

                // render
                for (unsigned i = 0; i < clipped_len; ++i) {
                    unsigned j = (i + 1) % clipped_len;
                    drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_WHITE);
                }
            } else {
                // render steps

                // step through each sub sector of next sector
                // if next sub sector's ceiling < current floor, continue
                // if next sub sector's floor > current ceiling, continue
                // do normal stuff

                float max_ceiling = def.floor_height;
                float step_bottom = def.floor_height;
                for (unsigned ssid = 0; ssid < next->num_sub_sectors; ++ssid) {
                    SectorDef next_def = next->sub_sectors[ssid];
                    if (next_def.ceiling_height < def.floor_height ||
                        next_def.floor_height > def.ceiling_height) continue;

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
                        vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                        // transform to screen coords
                        for (unsigned i = 0; i < clipped_len; ++i) {
                            transformed_points[i][3] = mat4MulVec3(cam.vp_mat, clipped_poly[i], transformed_points[i]);

                            float inv_w;
                            if (transformed_points[i][3] > 0.0f) {
                                inv_w = 1.0f / transformed_points[i][3];
                            } else {
                                inv_w = 0.0f;
                            }

                            ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                            ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                            screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (SCREEN_WIDTH - 1);
                            screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (SCREEN_HEIGHT - 1);
                        }

                        // render
                        for (unsigned i = 0; i < clipped_len; ++i) {
                            unsigned j = (i + 1) % clipped_len;
                            drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_RED);
                        }

                        step_bottom = next_def.ceiling_height;
                    }

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = max(def.floor_height, next_def.floor_height);
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = min(def.ceiling_height, next_def.ceiling_height);
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = max(def.floor_height, next_def.floor_height);
                    unsigned clipped_len = 4;

                    // clip
                    vec3 *clipped_poly = _clipPolygon(frustum, true, clipping_list0, clipping_list1, &clipped_len);

                    if (clipped_len > 2) {
                        Frustum next_frustum = calcFrustumFromPoly(clipped_poly, clipped_len, cam);

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
                    vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

                    // transform to screen coords
                    for (unsigned i = 0; i < clipped_len; ++i) {
                        transformed_points[i][3] = mat4MulVec3(cam.vp_mat, clipped_poly[i], transformed_points[i]);

                        float inv_w;
                        if (transformed_points[i][3] > 0.0f) {
                            inv_w = 1.0f / transformed_points[i][3];
                        } else {
                            inv_w = 0.0f;
                        }

                        ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                        ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                        screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (SCREEN_WIDTH - 1);
                        screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (SCREEN_HEIGHT - 1);
                    }

                    // render
                    for (unsigned i = 0; i < clipped_len; ++i) {
                        unsigned j = (i + 1) % clipped_len;
                        drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_GREEN);
                    }
                }
            }
        }

        // render floor
        {
            // init clipping list
            for (unsigned i = 0; i < sector->num_walls; ++i) {
                clipping_list0[i][0] = sector->points[i][0], clipping_list0[i][1] = sector->points[i][1], clipping_list0[i][2] = def.floor_height;
            }

            unsigned clipped_len = sector->num_walls;

            // clip
            vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

            // transform to screen coords
            for (unsigned i = 0; i < clipped_len; ++i) {
                transformed_points[i][3] = mat4MulVec3(cam.vp_mat, clipped_poly[i], transformed_points[i]);

                float inv_w;
                if (transformed_points[i][3] > 0.0f) {
                    inv_w = 1.0f / transformed_points[i][3];
                } else {
                    inv_w = 0.0f;
                }

                ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (SCREEN_WIDTH - 1);
                screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (SCREEN_HEIGHT - 1);
            }

            // render
            for (unsigned i = 0; i < clipped_len; ++i) {
                unsigned j = (i + 1) % clipped_len;
                drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_BLUE);
            }
        }

        // render ceiling
        {
            // init clipping list
            for (unsigned i = 0; i < sector->num_walls; ++i) {
                unsigned j           = sector->num_walls - i - 1;
                clipping_list0[j][0] = sector->points[i][0], clipping_list0[j][1] = sector->points[i][1], clipping_list0[j][2] = def.ceiling_height;
            }

            unsigned clipped_len = sector->num_walls;

            // clip
            vec3 *clipped_poly = _clipPolygon(frustum, false, clipping_list0, clipping_list1, &clipped_len);

            // transform to screen coords
            for (unsigned i = 0; i < clipped_len; ++i) {
                transformed_points[i][3] = mat4MulVec3(cam.vp_mat, clipped_poly[i], transformed_points[i]);

                float inv_w;
                if (transformed_points[i][3] > 0.0f) {
                    inv_w = 1.0f / transformed_points[i][3];
                } else {
                    inv_w = 0.0f;
                }

                ndc_points[i][0]    = transformed_points[i][0] * inv_w;
                ndc_points[i][1]    = transformed_points[i][2] * inv_w;
                screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * (SCREEN_WIDTH - 1);
                screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * (SCREEN_HEIGHT - 1);
            }

            // render
            for (unsigned i = 0; i < clipped_len; ++i) {
                unsigned j = (i + 1) % clipped_len;
                drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_YELLOW);
            }
        }

        free(frustum.planes);
    }
}

vec3 *_clipPolygon(Frustum frustum, bool ignore_near, vec3 *point_list0, vec3 *point_list1, unsigned *io_len) {
    // https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    assert(point_list0 != NULL);
    assert(point_list1 != NULL);
    assert(io_len != NULL);

    unsigned clipping_in_len;
    unsigned clipping_out_len = *io_len;
    vec3 *clipping_in         = point_list1;
    vec3 *clipping_out        = point_list0;

    for (unsigned plane_index = ignore_near ? FRUSTUM_NEAR_PLANE_INDEX + 1 : 0; plane_index < frustum.num_planes; ++plane_index) {
        swap(vec3 *, clipping_in, clipping_out);
        clipping_in_len  = clipping_out_len;
        clipping_out_len = 0;

        for (unsigned edge_curr = 0; edge_curr < clipping_in_len; ++edge_curr) {
            unsigned edge_prev = (edge_curr - 1 + clipping_in_len) % clipping_in_len;

            bool in_curr;
            bool in_prev;

            {
                float d0 = dot3d(frustum.planes[plane_index], clipping_in[edge_curr]);
                float d1 = dot3d(frustum.planes[plane_index], clipping_in[edge_prev]);

                in_curr = d0 - frustum.planes[plane_index][3] >= -0.003f;
                in_prev = d1 - frustum.planes[plane_index][3] >= -0.003f;
            }

            float t      = 0.5f;
            vec3 line[2] = {
                { clipping_in[edge_prev][0], clipping_in[edge_prev][1], clipping_in[edge_prev][2] },
                { clipping_in[edge_curr][0], clipping_in[edge_curr][1], clipping_in[edge_curr][2] },
            };
            bool intersects         = intersectSegmentPlane(line, frustum.planes[plane_index], &t);
            vec3 intersection_point = {
                lerp(clipping_in[edge_prev][0], clipping_in[edge_curr][0], t),
                lerp(clipping_in[edge_prev][1], clipping_in[edge_curr][1], t),
                lerp(clipping_in[edge_prev][2], clipping_in[edge_curr][2], t),
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
