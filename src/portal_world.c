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

void freeSectorPoly(SectorPoly poly) {
    free(poly.points);
    free(poly.planes);
    free(poly.next_sectors);
}

void freeSector(Sector sector) {
    freeSectorPoly(sector.polygon);
}

void freePortalWorld(PortalWorld pod) {
    SectorList_free(pod.sectors);
}

bool pointInSector(Sector sector, vec3 point) {
    for (unsigned i = 0; i < sector.polygon.num_points; ++i) {
        float dot = dot3d(sector.polygon.planes[i], point);
        if (dot < sector.polygon.planes[i][3]) return false;
    }

    return true;
}

void _renderSector(Sector *start_sector, mat4 vp_matrix, vec3 cam_pos, Frustum Frustum);

void portalWorldRender(PortalWorld pod, mat4 vp_matrix, vec3 cam_pos, Frustum frustum) {
    if (pod.sectors.head == NULL) return;

    Sector *start_sector = &pod.sectors.head->item;
    SectorListNode *node = pod.sectors.head;
    while (node != NULL) {
        if (pointInSector(node->item, cam_pos)) {
            start_sector = &node->item;
        }
        node = node->next;
    }

    _renderSector(start_sector, vp_matrix, cam_pos, frustum);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////

#define CLIP_BUFFER_SIZE 16
#define SECTOR_QUEUE_SIZE 64

vec3 *_clipPolygon(Frustum frustum, vec3 *point_list0, vec3 *point_list1, unsigned *io_len);

void _renderSector(Sector *start_sector, mat4 vp_matrix, vec3 cam_pos, Frustum frustum) {
    vec3 clipping_list0[CLIP_BUFFER_SIZE];
    vec3 clipping_list1[CLIP_BUFFER_SIZE];

    vec4 transformed_points[CLIP_BUFFER_SIZE];
    vec2 ndc_points[CLIP_BUFFER_SIZE];
    vec2 screen_points[CLIP_BUFFER_SIZE];

    Sector *sector_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end= 0;

    sector_queue[sector_queue_end] = start_sector;
    sector_queue_end = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    while(sector_queue_start != sector_queue_end) {
        Sector *sector      = sector_queue[sector_queue_start];
        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;
        SectorPoly polygon = sector->polygon;

        for (unsigned index0 = 0; index0 < polygon.num_points; ++index0) {
            unsigned index1 = (index0 + 1) % polygon.num_points;
            vec3 p0 = { 0 }, p1 = { 0 };
            p0[0] = polygon.points[index0][0];
            p0[1] = polygon.points[index0][1];
            p1[0] = polygon.points[index1][0];
            p1[1] = polygon.points[index1][1];

            // back face culling
            float back_face_test = dot3d(polygon.planes[index0], cam_pos);
            if (back_face_test < polygon.planes[index0][3]) continue;

            Sector *next = polygon.next_sectors[index0];

            if(next == NULL) {
                // render whole wall

                // init clipping list
                clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = sector->floor_height;
                clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = sector->ceiling_height;
                clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = sector->ceiling_height;
                clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = sector->floor_height;
                unsigned clipped_len = 4;

                // clip
                vec3 *clipped_poly = _clipPolygon(frustum, clipping_list0, clipping_list1, &clipped_len);

                // transform to screen coords
                for (unsigned i = 0; i < clipped_len; ++i) {
                    transformed_points[i][3] = mat4MulVec3(vp_matrix, clipped_poly[i], transformed_points[i]);

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

                if(sector->floor_height < next->floor_height) {
                    // bottom step

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = sector->floor_height;
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = next->floor_height;
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = next->floor_height;
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = sector->floor_height;
                    unsigned clipped_len = 4;

                    // clip
                    vec3 *clipped_poly = _clipPolygon(frustum, clipping_list0, clipping_list1, &clipped_len);

                    // transform to screen coords
                    for (unsigned i = 0; i < clipped_len; ++i) {
                        transformed_points[i][3] = mat4MulVec3(vp_matrix, clipped_poly[i], transformed_points[i]);

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
                }

                if(sector->ceiling_height > next->ceiling_height) {
                    // top step

                    // init clipping list
                    clipping_list0[0][0] = p0[0], clipping_list0[0][1] = p0[1], clipping_list0[0][2] = next->ceiling_height;
                    clipping_list0[1][0] = p0[0], clipping_list0[1][1] = p0[1], clipping_list0[1][2] = sector->ceiling_height;
                    clipping_list0[2][0] = p1[0], clipping_list0[2][1] = p1[1], clipping_list0[2][2] = sector->ceiling_height;
                    clipping_list0[3][0] = p1[0], clipping_list0[3][1] = p1[1], clipping_list0[3][2] = next->ceiling_height;
                    unsigned clipped_len = 4;

                    // clip
                    vec3 *clipped_poly = _clipPolygon(frustum, clipping_list0, clipping_list1, &clipped_len);

                    // transform to screen coords
                    for (unsigned i = 0; i < clipped_len; ++i) {
                        transformed_points[i][3] = mat4MulVec3(vp_matrix, clipped_poly[i], transformed_points[i]);

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
                }
                
                // draw next sector
                // TODO: Adjust frustum
                sector_queue[sector_queue_end] = next;
                sector_queue_end = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;
            }
        }
    }
}

vec3 *_clipPolygon(Frustum frustum, vec3 *point_list0, vec3 *point_list1, unsigned *io_len) {
    // https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm

    assert(point_list0 != NULL);
    assert(point_list1 != NULL);
    assert(io_len != NULL);

    unsigned clipping_in_len;
    unsigned clipping_out_len = *io_len;
    vec3 *clipping_in         = point_list1;
    vec3 *clipping_out        = point_list0;

    for (unsigned plane_index = 0; plane_index < 6; ++plane_index) {
        swap(vec3 *, clipping_in, clipping_out);
        clipping_in_len  = clipping_out_len;
        clipping_out_len = 0;

        for (unsigned edge_curr = 0; edge_curr < clipping_in_len; ++edge_curr) {
            unsigned edge_prev = (edge_curr - 1 + clipping_in_len) % clipping_in_len;

            bool in_curr = dot3d(frustum.planes[plane_index], clipping_in[edge_curr]) >= frustum.planes[plane_index][3];
            bool in_prev = dot3d(frustum.planes[plane_index], clipping_in[edge_prev]) >= frustum.planes[plane_index][3];

            float t      = 0.0f;
            vec3 line[2] = {
                { clipping_in[edge_prev][0], clipping_in[edge_prev][1], clipping_in[edge_prev][2] },
                { clipping_in[edge_curr][0], clipping_in[edge_curr][1], clipping_in[edge_curr][2] },
            };
            intersectSegmentPlane(line, frustum.planes[plane_index], &t);
            vec3 intersection_point = {
                lerp(clipping_in[edge_prev][0], clipping_in[edge_curr][0], t),
                lerp(clipping_in[edge_prev][1], clipping_in[edge_curr][1], t),
                lerp(clipping_in[edge_prev][2], clipping_in[edge_curr][2], t),
            };

            if (in_curr) {
                if (!in_prev) {
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

            } else if (in_prev) {
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