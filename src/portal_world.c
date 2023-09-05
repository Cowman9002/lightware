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

#define VERTEX_BUFFER_SIZE 128
void portalWorldRender(PortalWorld pod, mat4 vp_matrix, vec3 cam_pos, Frustum frustum) {
    if (pod.sectors.head == NULL) return;

    Sector sector = pod.sectors.head->item;

    vec3 clipping_list0[VERTEX_BUFFER_SIZE];
    vec3 clipping_list1[VERTEX_BUFFER_SIZE];

    unsigned clipping_in_len;
    unsigned clipping_out_len;
    vec3 *clipping_in;
    vec3 *clipping_out;

    vec4 transformed_points[VERTEX_BUFFER_SIZE];
    vec2 ndc_points[VERTEX_BUFFER_SIZE];
    vec2 screen_points[VERTEX_BUFFER_SIZE];

    for (unsigned index0 = 0; index0 < sector.polygon.num_points; ++index0) {
        unsigned index1 = (index0 + 1) % sector.polygon.num_points;
        vec3 p0 = { 0 }, p1 = { 0 };
        p0[0] = sector.polygon.points[index0][0];
        p0[1] = sector.polygon.points[index0][1];
        p1[0] = sector.polygon.points[index1][0];
        p1[1] = sector.polygon.points[index1][1];

        // back face culling
        float back_face_test = dot3d(sector.polygon.planes[index0], cam_pos);
        if (back_face_test < sector.polygon.planes[index0][3]) continue;

        // clip
        clipping_in  = clipping_list0;
        clipping_out = clipping_list1;

        // init clipping lists
        clipping_out[0][0] = p0[0], clipping_out[0][1] = p0[1], clipping_out[0][2] = sector.floor_height;
        clipping_out[1][0] = p0[0], clipping_out[1][1] = p0[1], clipping_out[1][2] = sector.ceiling_height;
        clipping_out[2][0] = p1[0], clipping_out[2][1] = p1[1], clipping_out[2][2] = sector.ceiling_height;
        clipping_out[3][0] = p1[0], clipping_out[3][1] = p1[1], clipping_out[3][2] = sector.floor_height;
        clipping_out_len = 4;

        // https://en.wikipedia.org/wiki/Sutherland%E2%80%93Hodgman_algorithm
        for (unsigned plane_index = 0; plane_index < 6; ++plane_index) {
            swap(vec3 *, clipping_in, clipping_out);
            clipping_in_len  = clipping_out_len;
            clipping_out_len = 0;

            for (unsigned edge_curr = 0; edge_curr < clipping_in_len; ++edge_curr) {
                unsigned edge_prev = (edge_curr - 1 + clipping_in_len) % clipping_in_len;

                bool in_curr = dot3d(frustum.planes[plane_index], clipping_in[edge_curr]) >= frustum.planes[plane_index][3];
                bool in_prev = dot3d(frustum.planes[plane_index], clipping_in[edge_prev]) >= frustum.planes[plane_index][3];

                float t = 0.0f;
                vec3 line[2] = {
                    { clipping_in[edge_prev][0], clipping_in[edge_prev][1], clipping_in[edge_prev][2] },
                    { clipping_in[edge_curr][0], clipping_in[edge_curr][1], clipping_in[edge_curr][2] },
                };
                bool intersects = intersectSegmentPlane(line, frustum.planes[plane_index], &t);
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
                        assert(clipping_out_len < VERTEX_BUFFER_SIZE);
                    }
                    // add current point
                    for (unsigned _x = 0; _x < 3; ++_x) {
                        clipping_out[clipping_out_len][_x] = clipping_in[edge_curr][_x];
                    }
                    ++clipping_out_len;
                    assert(clipping_out_len < VERTEX_BUFFER_SIZE);

                } else if (in_prev) {
                    // add intersection point
                        for (unsigned _x = 0; _x < 3; ++_x) {
                            clipping_out[clipping_out_len][_x] = intersection_point[_x];
                        }
                        ++clipping_out_len;
                        assert(clipping_out_len < VERTEX_BUFFER_SIZE);

                        printf("%f\n", t);
                }
            }
            if (clipping_out_len == 0) break;
        }

        // render wall

        for (unsigned i = 0; i < clipping_out_len; ++i) {
            transformed_points[i][3] = mat4MulVec3(vp_matrix, clipping_out[i], transformed_points[i]);

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

        for (unsigned i = 0; i < clipping_out_len; ++i) {
            unsigned j = (i + 1) % clipping_out_len;
            drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_WHITE);
        }
    }
}