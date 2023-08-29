#include "portals.h"
#include "geo.h"
#include "draw.h"

#include <stdio.h>

typedef struct WallAttribute {
    vec2 uv;
}WallAttribute;

bool clipWall(vec2 clip_plane[2], Line *wall, WallAttribute attr[2]);

unsigned getCurrentSector(PortalWorld pod, vec2 point, unsigned last_sector) {
    if (last_sector < pod.num_sectors) {
        // look at current sector
        SectorDef current_sector = pod.sectors[last_sector];
        Line *test_walls         = &pod.wall_lines[current_sector.start];
        if (pointInPoly(test_walls, current_sector.length, point)) {
            return last_sector;
        }

        // look at neighbors
        for (unsigned i = 0; i < current_sector.length; ++i) {
            unsigned wall_index = current_sector.start + i;
            unsigned wall_next  = pod.wall_nexts[wall_index];
            if (wall_next != INVALID_SECTOR_INDEX) {
                SectorDef next_sector = pod.sectors[wall_next];
                Line *test_walls      = &pod.wall_lines[next_sector.start];

                if (pointInPoly(test_walls, next_sector.length, point)) {
                    return wall_next;
                }
            }
        }
    }

    // linear lookup
    for (unsigned s = 0; s < pod.num_sectors; ++s) {
        SectorDef sector = pod.sectors[s];
        Line *test_walls = &pod.wall_lines[sector.start];
        if (pointInPoly(test_walls, sector.length, point)) {
            return s;
        }
    }

    return INVALID_SECTOR_INDEX;
}

void renderPortalWorld(PortalWorld pod, Camera cam) {
#define SECTOR_QUEUE_SIZE 128
    int sector_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    // sector_queue[sector_queue_end] = cam.sector;
    sector_queue[sector_queue_end] = 0;
    sector_queue_end               = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    // loop variables
    Line view_tspace;
    Line view_space;
    Line ndc_space;
    WallAttribute attr[2];

    // bredth first traversal of sectors
    while (sector_queue_start != sector_queue_end) {
        unsigned sector_index = sector_queue[sector_queue_start];
        if (sector_index >= pod.num_sectors) break; // avoid invalid sectors

        // pop sector from queue
        SectorDef sector   = pod.sectors[sector_index];
        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        // render every wall
        for (unsigned i = 0; i < sector.length; ++i) {
            Line wall_line     = pod.wall_lines[sector.start + i];
            unsigned wall_next = pod.wall_nexts[sector.start + i];
            bool is_portal     = wall_next != INVALID_SECTOR_INDEX && wall_next != sector_index;

            // Back face culling
            {
                float cross_val = cross2d(
                    VEC2(wall_line.points[0][0] - wall_line.points[1][0], wall_line.points[0][1] - wall_line.points[1][1]),
                    VEC2(wall_line.points[0][0] - cam.pos[0], wall_line.points[0][1] - cam.pos[1]));

                if (cross_val < 0.0) {
                    continue; // this wall cannot be seen
                }
            }

            float length;
            {
                vec2 diff = { wall_line.points[0][0] - wall_line.points[1][0], wall_line.points[0][1] - wall_line.points[1][1] };
                length    = sqrtf(dot2d(diff, diff));
            }

            attr[0].uv[0] = 0.0f;
            attr[1].uv[0] = length;

            // convert to view space
            {
                for (unsigned pi = 0; pi < 2; ++pi) {
                    // translate by negative cam_pos
                    view_tspace.points[pi][0] = wall_line.points[pi][0] - cam.pos[0];
                    view_tspace.points[pi][1] = wall_line.points[pi][1] - cam.pos[1];

                    // rotate by negative cam_rot
                    //  cos -sin
                    //  sin  cos
                    view_space.points[pi][0] = cam.rot_cos * view_tspace.points[pi][0] + cam.rot_sin * view_tspace.points[pi][1];
                    view_space.points[pi][1] = -cam.rot_sin * view_tspace.points[pi][0] + cam.rot_cos * view_tspace.points[pi][1];
                }
            }

            // Clip
            {
                vec2 clip_planes[4][2] = {
                    { { 0.0f, 0.0f }, { -1.0f, 1.0f } },
                    { { 1.0f, 1.0f }, { 0.0f, 0.0f } },
                    { { 1.0f, -NEAR_PLANE }, { -1.0f, -NEAR_PLANE } },
                    { { -1.0f, -FAR_PLANE }, { 1.0f, -FAR_PLANE } },
                };

                if (!clipWall(clip_planes[0], &view_space, attr)) continue;
                if (!clipWall(clip_planes[1], &view_space, attr)) continue;

                // if (is_portal) {
                //     sector_queue[sector_queue_end] = wall_next;
                //     sector_queue_end               = (sector_queue_end + 1) % 128;
                // }

                if (!clipWall(clip_planes[2], &view_space, attr)) continue;
                if (!clipWall(clip_planes[3], &view_space, attr)) continue;
            }

            // Project
            {
                for (unsigned pi = 0; pi < 2; ++pi) {
                    ndc_space.points[pi][1] = -1.0f / view_space.points[pi][1];
                    ndc_space.points[pi][0] = view_space.points[pi][0] * ndc_space.points[pi][1];

                    // perpective correct mapping
                    attr[pi].uv[0] *= ndc_space.points[pi][1];
                }
            }

            float start_x = (ndc_space.points[0][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
            float end_x   = (ndc_space.points[1][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
            if (start_x > end_x) {
                swap(float, start_x, end_x);
                swap(float, ndc_space.points[0][1], ndc_space.points[1][1]);
                swap(WallAttribute, attr[0], attr[1]);
            }

            float dist_to_ceiling = (sector.ceiling_height - cam.pos[2]);
            float dist_to_floor   = (cam.pos[2] - sector.floor_height);

            float top_of_wall[2] = {
                (0.5 - dist_to_ceiling * ndc_space.points[0][1]) * SCREEN_HEIGHT,
                (0.5 - dist_to_ceiling * ndc_space.points[1][1]) * SCREEN_HEIGHT,
            };

            float bottom_of_wall[2] = {
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[0][1])) * SCREEN_HEIGHT,
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[1][1])) * SCREEN_HEIGHT,
            };

            // draw wall
            for (float x = start_x; x <= end_x; ++x) {
                float t       = (x - start_x) / (end_x - start_x);
                float start_y = lerp(top_of_wall[0], top_of_wall[1], t);
                float end_y   = lerp(bottom_of_wall[0], bottom_of_wall[1], t);

                float z     = 1.0f / lerp(ndc_space.points[0][1], ndc_space.points[1][1], t);
                // float depth = z * (float)((uint16_t)~0) / FAR_PLANE;

                float u = lerp(attr[0].uv[0], attr[1].uv[0], t);
                // // perspective correct mapping
                u *= z;

                start_y = clamp(start_y, 0.0f, SCREEN_HEIGHT);
                end_y   = clamp(end_y, 0.0f, SCREEN_HEIGHT);

                // wall
                for (float y = start_y; y < end_y; ++y) {
                    int modu = (int)u % 2;
                    setPixel(x, y, modu == 0 ? COLOR_BLUE : RGB(0, 0, 128));
                    // int depth_index = (int)x + (int)y * SCREEN_WIDTH;
                    // if (depth < depth_buffer[depth_index]) {
                    //     int modu = (int)u % 2;
                    //     setPixel(x, y, modu == 0 ? COLOR_BLUE : RGB(0, 0, 128));
                    //     depth_buffer[depth_index] = depth;
                    // }
                }
            }
        }
    }
}

//
// INTERNAL
//

bool clipWall(vec2 clip_plane[2], Line *wall, WallAttribute attr[2]) {
    int clip_index = 0;
    float t;

    bool inside[2];

    vec2 plane  = { clip_plane[1][0] - clip_plane[0][0], clip_plane[1][1] - clip_plane[0][1] };
    vec2 p0_vec = { wall->points[0][0] - clip_plane[0][0], wall->points[0][1] - clip_plane[0][1] };
    vec2 p1_vec = { wall->points[1][0] - clip_plane[0][0], wall->points[1][1] - clip_plane[0][1] };

    inside[0] = cross2d(plane, p0_vec) >= 0.0;
    inside[1] = cross2d(plane, p1_vec) >= 0.0;

    t = 0.0;

    if (!inside[0] && !inside[1]) {
        return false; // entire thing is clipped
    } else if (inside[0] && inside[1]) {
        return true; // nothing is clipped
    }

    // find intersection t
    intersectSegmentLine(wall->points, clip_plane, &t);
    clip_index = inside[0] ? 1 : 0;

    wall->points[clip_index][0] = lerp(wall->points[0][0], wall->points[1][0], t);
    wall->points[clip_index][1] = lerp(wall->points[0][1], wall->points[1][1], t);

    attr[clip_index].uv[0] = lerp(attr[0].uv[0], attr[1].uv[0], t);

    // wall->uv_coords[clip_index][0] = lerp(wall->uv_coords[0][0], wall->uv_coords[1][0], t);

    return true;
}