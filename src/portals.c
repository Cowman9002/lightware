#include "portals.h"
#include "geo.h"
#include "draw.h"

#include <SDL2/SDL.h>

#include <stdio.h>

typedef struct WallAttribute {
    vec2 uv;
    vec3 world_pos;
} WallAttribute;

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
            if (wall_next < pod.num_sectors) {
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

extern uint16_t *g_depth_buffer;

#define FLASHLIGHT_CUTOFF 0.97f
#define FLASHLIGHT_OUTER_CUTOFF 0.93f
#define FLASHLIGHT_POWER 10.0f

void renderPortalWorld(PortalWorld pod, Camera cam) {
#define SECTOR_QUEUE_SIZE 128
    const float TAN_FOV_HALF = tanf(cam.fov * 0.5f);
    const float INV_TAN_FOV_HALF = 1.0f / TAN_FOV_HALF;

    int sector_queue[SECTOR_QUEUE_SIZE];
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    if (cam.sector < pod.num_sectors) {
        sector_queue[sector_queue_end] = cam.sector;
    } else {
        sector_queue[sector_queue_end] = 0;
    }
    sector_queue_end = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    int occlusion_top[SCREEN_WIDTH];    // inclusive
    int occlusion_bottom[SCREEN_WIDTH]; // exclusive
    for (unsigned i = 0; i < SCREEN_WIDTH; ++i) {
        occlusion_top[i]    = 0;
        occlusion_bottom[i] = SCREEN_HEIGHT;
    }

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

        float dist_to_ceiling = (sector.ceiling_height - cam.pos[2]);
        float dist_to_floor   = (cam.pos[2] - sector.floor_height);

        // render ceiling
        if (dist_to_ceiling > 0.0) {
            float scale = 2.0f * dist_to_ceiling;
            for (int y = 0; y < SCREEN_HEIGHT_HALF; ++y) {
                float wy = -1.0f; // half_view_plane . tan(fov / 2)
                float wz = (1.0f - y / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                for (int x = 0; x < SCREEN_WIDTH; ++x) {
                    if (y < occlusion_top[x] || y >= occlusion_bottom[x]) continue;

                    float wx = (x - SCREEN_WIDTH_HALF) / SCREEN_WIDTH * ASPECT_RATIO * TAN_FOV_HALF;

                    float rx = cam.rot_cos * wx + -cam.rot_sin * wy;
                    float ry = cam.rot_sin * wx + cam.rot_cos * wy;

                    float px = (rx * scale / wz + cam.pos[0]);
                    float py = (ry * scale / wz + cam.pos[1]);

                    vec3 to_cam       = { cam.pos[0] - px, cam.pos[1] - py, cam.pos[2] - sector.ceiling_height };
                    float to_cam_dist = sqrtf(dot3d(to_cam, to_cam));

                    vec3 light = { to_cam[0] / to_cam_dist, to_cam[1] / to_cam_dist, to_cam[2] / to_cam_dist };

                    float attenuation    = clamp(FLASHLIGHT_POWER / to_cam_dist, 0.0f, 1.0f);
                    float ndotl          = clamp(dot3d(light, VEC3(0.0f, 0.0f, -1.0f)), 0.0f, 1.0f);
                    
                    float spot_theta     = -dot3d(light, VEC3(cam.rot_sin, -cam.rot_cos, 0.0f));
                    float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
                    float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

                    float lighting = spot_intensity * attenuation * ndotl;

                    int checker = (int)(floorf(px) + floorf(py)) % 2;
                    Color color = checker ? COLOR_RED : RGB(128, 0, 0);
                    // color       = mulColor(color, lighting * 255);
                    setPixel(x, y, color);
                }
            }
        }

        // render floor
        if (dist_to_floor > 0.0) {
            float scale = 2.0f * dist_to_floor;
            for (int y = 0; y < SCREEN_HEIGHT_HALF; ++y) {
                int pixel_y = SCREEN_HEIGHT - y - 1;
                float wy    = -1.0f;
                float wz    = (1.0f - y / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                for (int x = 0; x < SCREEN_WIDTH; ++x) {
                    if (pixel_y < occlusion_top[x] || pixel_y >= occlusion_bottom[x]) continue;

                    float wx = (x - SCREEN_WIDTH_HALF) / SCREEN_WIDTH * ASPECT_RATIO * TAN_FOV_HALF;

                    float rx = cam.rot_cos * wx + -cam.rot_sin * wy;
                    float ry = cam.rot_sin * wx + cam.rot_cos * wy;

                    float px = (rx * scale / wz + cam.pos[0]);
                    float py = (ry * scale / wz + cam.pos[1]);

                    vec3 to_cam       = { cam.pos[0] - px, cam.pos[1] - py, cam.pos[2] - sector.floor_height };
                    float to_cam_dist = sqrtf(dot3d(to_cam, to_cam));

                    vec3 light = { to_cam[0] / to_cam_dist, to_cam[1] / to_cam_dist, to_cam[2] / to_cam_dist };

                    float attenuation = clamp(FLASHLIGHT_POWER / to_cam_dist, 0.0f, 1.0f);
                    float ndotl       = clamp(dot3d(light, VEC3(0.0f, 0.0f, 1.0f)), 0.0f, 1.0f);

                    float spot_theta     = -dot3d(light, VEC3(cam.rot_sin, -cam.rot_cos, 0.0f));
                    float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
                    float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

                    float lighting = spot_intensity * attenuation * ndotl;

                    int checker = (int)(floorf(px) + floorf(py)) % 2;
                    Color color = checker ? COLOR_GREEN : RGB(0, 128, 0);
                    // color       = mulColor(color, lighting * 255);
                    setPixel(x, pixel_y, color);
                }
            }
        }

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

            attr[0].uv[1] = 0.0f;
            attr[1].uv[1] = sector.ceiling_height - sector.floor_height;

            attr[0].world_pos[0] = wall_line.points[0][0];
            attr[1].world_pos[0] = wall_line.points[1][0];
            attr[0].world_pos[1] = wall_line.points[0][1];
            attr[1].world_pos[1] = wall_line.points[1][1];
            attr[0].world_pos[2] = sector.floor_height;
            attr[1].world_pos[2] = sector.ceiling_height;

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
                    { { 0.0f, 0.0f }, { -ASPECT_RATIO * 0.5f * TAN_FOV_HALF, 1.0f } },
                    { { ASPECT_RATIO * 0.5f * TAN_FOV_HALF, 1.0f }, { 0.0f, 0.0f } },
                    { { 1.0f, -NEAR_PLANE }, { -1.0f, -NEAR_PLANE } },
                    { { -1.0f, -FAR_PLANE }, { 1.0f, -FAR_PLANE } },
                };

                if (!clipWall(clip_planes[0], &view_space, attr)) continue;
                if (!clipWall(clip_planes[1], &view_space, attr)) continue;

                if (is_portal) {
                    sector_queue[sector_queue_end] = wall_next;
                    sector_queue_end               = (sector_queue_end + 1) % 128;
                }

                if (!clipWall(clip_planes[2], &view_space, attr)) continue;
                if (!clipWall(clip_planes[3], &view_space, attr)) continue;
            }

            // Project
            {
                for (unsigned pi = 0; pi < 2; ++pi) {
                    ndc_space.points[pi][1] = -1.0f / view_space.points[pi][1];
                    ndc_space.points[pi][0] = view_space.points[pi][0] * ndc_space.points[pi][1] * 2.0f * INV_ASPECT_RATIO * INV_TAN_FOV_HALF;

                    // perpective correct mapping
                    attr[pi].uv[0] *= ndc_space.points[pi][1];
                    attr[pi].world_pos[0] *= ndc_space.points[pi][1];
                    attr[pi].world_pos[1] *= ndc_space.points[pi][1];
                }
            }

            int start_x = (ndc_space.points[0][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
            int end_x   = (ndc_space.points[1][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
            if (start_x > end_x) {
                swap(int, start_x, end_x);
                swap(float, ndc_space.points[0][1], ndc_space.points[1][1]);
                swap(WallAttribute, attr[0], attr[1]);
            }

            float top_of_wall[2] = {
                (0.5 - dist_to_ceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF) * SCREEN_HEIGHT,
                (0.5 - dist_to_ceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF) * SCREEN_HEIGHT,
            };

            float bottom_of_wall[2] = {
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[0][1] * INV_TAN_FOV_HALF)) * SCREEN_HEIGHT,
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[1][1] * INV_TAN_FOV_HALF)) * SCREEN_HEIGHT,
            };

            start_x = clamp(start_x, 0, SCREEN_WIDTH);
            end_x   = clamp(end_x, 0, SCREEN_WIDTH);

            vec2 wall_norm;
            {
                vec2 d    = { wall_line.points[1][0] - wall_line.points[0][0], wall_line.points[1][1] - wall_line.points[0][1] };
                float len = dot2d(d, d);
                if (len != 0) {
                    len = sqrtf(len);
                    d[0] /= len;
                    d[1] /= len;
                    wall_norm[0] = -d[1];
                    wall_norm[1] = d[0];
                }
            }

            if (!is_portal) {
                // draw wall
                for (int x = start_x; x <= end_x; ++x) {
                    float tx    = (float)(x - start_x) / (end_x - start_x);
                    int start_y = lerp(top_of_wall[0], top_of_wall[1], tx);
                    int end_y   = lerp(bottom_of_wall[0], bottom_of_wall[1], tx);

                    float z   = 1.0f / lerp(ndc_space.points[0][1], ndc_space.points[1][1], tx);
                    int depth = (z / FAR_PLANE) * (uint16_t)(~0);

                    float u = lerp(attr[0].uv[0], attr[1].uv[0], tx);
                    // // perspective correct mapping
                    u *= z;

                    vec3 world_pos = {
                        lerp(attr[0].world_pos[0], attr[1].world_pos[0], tx) * z,
                        lerp(attr[0].world_pos[1], attr[1].world_pos[1], tx) * z,
                        0.0f,
                    };

                    int start_y_real = start_y;
                    int end_y_real   = end_y;

                    start_y = clamp(start_y, occlusion_top[x], occlusion_bottom[x]);
                    end_y   = clamp(end_y, occlusion_top[x], occlusion_bottom[x]);

                    occlusion_bottom[x] = 0;

                    // wall
                    for (int y = start_y; y < end_y; ++y) {
                        float ty = (float)(y - start_y_real) / (end_y_real - start_y_real);

                        float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                        world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        vec3 to_cam       = { cam.pos[0] - world_pos[0], cam.pos[1] - world_pos[1], cam.pos[2] - world_pos[2] };
                        float to_cam_dist = sqrtf(dot3d(to_cam, to_cam));

                        vec3 light = { to_cam[0] / to_cam_dist, to_cam[1] / to_cam_dist, to_cam[2] / to_cam_dist };

                        float attenuation = clamp(FLASHLIGHT_POWER / to_cam_dist, 0.0f, 1.0f);
                        float ndotl       = clamp(dot3d(light, VEC3(wall_norm[0], wall_norm[1], 0.0f)), 0.0f, 1.0f);

                        float spot_theta     = -dot3d(light, VEC3(cam.rot_sin, -cam.rot_cos, 0.0f));
                        float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
                        float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

                        float lighting = spot_intensity * attenuation * ndotl;

                        int checker = (int)(floorf(u) + floorf(v)) % 2;
                        Color color = checker ? COLOR_BLUE : RGB(0, 0, 128);
                        // color       = mulColor(color, lighting * 255);
                        setPixel(x, y, color);
                    }
                }
            } else {
                // draw steps
                float dist_to_nceiling = (pod.sectors[wall_next].ceiling_height - cam.pos[2]);
                float dist_to_nfloor   = (cam.pos[2] - pod.sectors[wall_next].floor_height);

                float top_of_nwall[2] = {
                    (0.5 - dist_to_nceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF) * SCREEN_HEIGHT,
                    (0.5 - dist_to_nceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF) * SCREEN_HEIGHT,
                };

                float bottom_of_nwall[2] = {
                    (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF)) * SCREEN_HEIGHT,
                    (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF)) * SCREEN_HEIGHT,
                };

                for (int x = start_x; x <= end_x; ++x) {
                    float tx    = (float)(x - start_x) / (end_x - start_x);
                    int start_y = lerp(top_of_wall[0], top_of_wall[1], tx);
                    int end_y   = lerp(bottom_of_wall[0], bottom_of_wall[1], tx);

                    int start_ny = lerp(top_of_nwall[0], top_of_nwall[1], tx);
                    int end_ny   = lerp(bottom_of_nwall[0], bottom_of_nwall[1], tx);

                    float z   = 1.0f / lerp(ndc_space.points[0][1], ndc_space.points[1][1], tx);
                    int depth = FLOAT_TO_DEPTH(z);

                    float u = lerp(attr[0].uv[0], attr[1].uv[0], tx);
                    // // perspective correct mapping
                    u *= z;

                    vec3 world_pos = {
                        lerp(attr[0].world_pos[0], attr[1].world_pos[0], tx) * z,
                        lerp(attr[0].world_pos[1], attr[1].world_pos[1], tx) * z,
                        0.0f,
                    };

                    int start_y_real = start_y;
                    int end_y_real   = end_y;

                    start_y  = clamp(start_y, occlusion_top[x], occlusion_bottom[x]);
                    end_y    = clamp(end_y, occlusion_top[x], occlusion_bottom[x]);

                    start_ny = min(clamp(start_ny, occlusion_top[x], occlusion_bottom[x]), end_y);
                    end_ny   = max(clamp(end_ny, occlusion_top[x], occlusion_bottom[x]), start_y);

                    occlusion_top[x]    = max(occlusion_top[x], max(start_y, start_ny));
                    occlusion_bottom[x] = min(occlusion_bottom[x], min(end_y, end_ny));

                    float attenuation = clamp(FLASHLIGHT_POWER / z, 0.0f, 1.0f);

                    // top step
                    for (int y = start_y; y < start_ny; ++y) {
                        float ty     = (float)(y - start_y_real) / (end_y_real - start_y_real);
                        float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                        world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        vec3 to_cam       = { cam.pos[0] - world_pos[0], cam.pos[1] - world_pos[1], cam.pos[2] - world_pos[2] };
                        float to_cam_dist = sqrtf(dot3d(to_cam, to_cam));

                        vec3 light = { to_cam[0] / to_cam_dist, to_cam[1] / to_cam_dist, to_cam[2] / to_cam_dist };

                        float attenuation = clamp(FLASHLIGHT_POWER / to_cam_dist, 0.0f, 1.0f);
                        float ndotl       = clamp(dot3d(light, VEC3(wall_norm[0], wall_norm[1], 0.0f)), 0.0f, 1.0f);

                        float spot_theta     = -dot3d(light, VEC3(cam.rot_sin, -cam.rot_cos, 0.0f));
                        float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
                        float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

                        float lighting = spot_intensity * attenuation * ndotl;

                        int checker = (int)(floorf(u) + floorf(v)) % 2;
                        Color color = checker ? RGB(200, 0, 255) : RGB(100, 0, 128);
                        // color       = mulColor(color, lighting * 255);
                        setPixel(x, y, color);
                    }

                    // bottom step
                    for (int y = end_ny; y < end_y; ++y) {

                        float ty     = (float)(y - start_y_real) / (end_y_real - start_y_real);
                        float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                        world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        vec3 to_cam       = { cam.pos[0] - world_pos[0], cam.pos[1] - world_pos[1], cam.pos[2] - world_pos[2] };
                        float to_cam_dist = sqrtf(dot3d(to_cam, to_cam));

                        vec3 light = { to_cam[0] / to_cam_dist, to_cam[1] / to_cam_dist, to_cam[2] / to_cam_dist };

                        float attenuation = clamp(FLASHLIGHT_POWER / to_cam_dist, 0.0f, 1.0f);
                        float ndotl       = clamp(dot3d(light, VEC3(wall_norm[0], wall_norm[1], 0.0f)), 0.0f, 1.0f);

                        float spot_theta     = -dot3d(light, VEC3(cam.rot_sin, -cam.rot_cos, 0.0f));
                        float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
                        float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

                        float lighting = spot_intensity * attenuation * ndotl;

                        int checker = (int)(floorf(u) + floorf(v)) % 2;
                        Color color = checker ? RGB(0, 200, 255) : RGB(0, 100, 128);
                        // color       = mulColor(color, lighting * 255);
                        setPixel(x, y, color);
                    }
                }
            }
        }
    }

    // for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
    //     for (unsigned y = 0; y < occlusion_top[x]; ++y) {
    //         setPixel(x, y, COLOR_BLACK);
    //     }

    //     for (unsigned y = occlusion_top[x]; y < occlusion_bottom[x]; ++y) {
    //         setPixel(x, y, COLOR_WHITE);
    //     }

    //     for (unsigned y = occlusion_bottom[x]; y < SCREEN_HEIGHT; ++y) {
    //         setPixel(x, y, COLOR_BLACK);
    //     }
    // }
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

    attr[clip_index].uv[0]        = lerp(attr[0].uv[0], attr[1].uv[0], t);
    attr[clip_index].world_pos[0] = lerp(attr[0].world_pos[0], attr[1].world_pos[0], t);
    attr[clip_index].world_pos[1] = lerp(attr[0].world_pos[1], attr[1].world_pos[1], t);

    // wall->uv_coords[clip_index][0] = lerp(wall->uv_coords[0][0], wall->uv_coords[1][0], t);

    return true;
}