#include "portal_world.h"

#include "draw.h"

#include <stdio.h>
#include <malloc.h>

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

void portalWorldRender(PortalWorld pod, mat4 vp_matrix, vec3 cam_pos) {
    if (pod.sectors.head == NULL) return;

    Sector sector = pod.sectors.head->item;

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

        // render wall
        vec3 raw_points[4] = {
            { p0[0], p0[1], sector.floor_height },
            { p1[0], p1[1], sector.floor_height },
            { p1[0], p1[1], sector.ceiling_height },
            { p0[0], p0[1], sector.ceiling_height },
        };

        vec4 transformed_points[4];
        vec2 ndc_points[4];
        vec2 screen_points[4];

        for (unsigned i = 0; i < 4; ++i) {
            transformed_points[i][3] = mat4MulVec3(vp_matrix, raw_points[i], transformed_points[i]);

            float inv_w;
            if (transformed_points[i][3] > 0.0f) {
                inv_w = 1.0f / transformed_points[i][3];
            } else {
                inv_w = 1.0f / 0.01f;
            }

            ndc_points[i][0]    = transformed_points[i][0] * inv_w;
            ndc_points[i][1]    = transformed_points[i][2] * inv_w;
            screen_points[i][0] = (ndc_points[i][0] * 0.5 + 0.5) * SCREEN_WIDTH;
            screen_points[i][1] = (-ndc_points[i][1] * 0.5 + 0.5) * SCREEN_HEIGHT;
        }

        for (unsigned i = 0; i < 4; ++i) {
            unsigned j = (i + 1) % 4;
            drawLine(screen_points[i][0], screen_points[i][1], screen_points[j][0], screen_points[j][1], COLOR_WHITE);
        }
    }
}