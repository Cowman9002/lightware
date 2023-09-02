#include "portals.h"
#include "geo.h"
#include "draw.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <stdio.h>

// #define RENDER_OCCLUSION
// #define CURRENT_SECTOR_ONLY
// #define NO_CEILINGS
// #define NO_FLOORS
// #define NO_STEPS

typedef struct WallAttribute {
    vec2 uv;
    vec3 world_pos;
    vec3 normal;
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

unsigned getSectorTier(PortalWorld pod, float z, unsigned sector_id) {
    SectorDef sector = pod.sectors[sector_id];
    for (unsigned i = 0; i < sector.num_tiers; ++i) {
        float sector_world_floor   = i == 0 ? sector.floor_heights[i] : sector.floor_heights[i] + sector.ceiling_heights[0];
        float sector_world_ceiling = i == 0 ? sector.ceiling_heights[i] : sector.ceiling_heights[i] + sector.ceiling_heights[0];

        if (z >= sector_world_floor && z <= sector_world_ceiling) return i;
    }
    return INVALID_SECTOR_INDEX;
}

extern uint16_t *g_depth_buffer;

#define FLASHLIGHT_CUTOFF 0.98f
#define FLASHLIGHT_OUTER_CUTOFF 0.9f
#define AMBIENT 0.05f
static float s_flashlight_power;

extern Image g_image_array[3];
extern Image g_sky_image;

bool pixelProgram(WallAttribute attr, Camera cam, unsigned texid, int screen_x, int screen_y, Color *o_color) {

    float lighting = AMBIENT;

    vec3 to_light    = { cam.pos[0] - attr.world_pos[0], cam.pos[1] - attr.world_pos[1], cam.pos[2] - attr.world_pos[2] };
    float light_dist = normalize3d(to_light);

    // float attenuation = clamp(s_flashlight_power / light_dist, 0.0f, 1.0f);
    float attenuation = clamp(30.0f / light_dist, 0.0f, 1.0f);
    float ndotl       = clamp(dot3d(to_light, attr.normal), 0.0f, 1.0f);

    float spot_theta     = -dot3d(to_light, cam.forward);
    float epsilon        = FLASHLIGHT_CUTOFF - FLASHLIGHT_OUTER_CUTOFF;
    float spot_intensity = clamp((spot_theta - FLASHLIGHT_OUTER_CUTOFF) / epsilon, 0.0, 1.0);

    // lighting = clamp(lighting + spot_intensity * attenuation * ndotl, 0.0f, 1.0f);
    lighting = clamp(lighting + attenuation * ndotl, 0.0f, 1.0f);

    // int checker = (int)(floorf(attr.uv[0]) + floorf(attr.uv[1])) % 2;
    // *o_color    = checker ? color : mulColor(color, 128);
    if (texid >= 3) texid = 0;

    {
        Image img = g_image_array[texid];
        float u   = modff(attr.uv[0], NULL);
        float v   = modff(attr.uv[1], NULL);
        if (u < 0) u += 1;
        if (v < 0) v += 1;

        unsigned tex_x = (u * (img.width - 1));
        unsigned tex_y = (v * (img.height - 1));
        *o_color       = sampleImage(img, tex_x, tex_y);
    }

    *o_color = mulColor(*o_color, lighting * 255);
    return true;
}

bool skyPixelProgram(WallAttribute attr, Camera cam, unsigned texid, int screen_x, int screen_y, Color *o_color) {

    const float SKY_SCALE = 1.5f;

    float sky_ar = (float)g_sky_image.height / (g_sky_image.width * SKY_SCALE);

    float u   = modff(screen_x / (float)SCREEN_WIDTH * sky_ar * ASPECT_RATIO + cam.rot / (2 * M_PI), NULL);

    // place base on horizon line
    float v   = (screen_y / (float)SCREEN_HEIGHT + 1.0f - cam.pitch) / SKY_SCALE;
    // float v   = (screen_y / (float)SCREEN_HEIGHT + 1.0f - cam.pitch);
    if (u < 0) u += 1;
    if (v < 0) return false;

    unsigned tex_x = (u * (g_sky_image.width - 1));
    unsigned tex_y = (v * (g_sky_image.height - 1));
    *o_color       = sampleImage(g_sky_image, tex_x, tex_y);
    return true;
}

void renderPortalWorld(PortalWorld pod, Camera cam) {
#define SECTOR_QUEUE_SIZE 128
    const float TAN_FOV_HALF     = tanf(cam.fov * 0.5f);
    const float INV_TAN_FOV_HALF = 1.0f / TAN_FOV_HALF;

    // TODO: TEMP
    {
        s_flashlight_power = rand() % 100;
        if (s_flashlight_power < 3) {
            s_flashlight_power = 0.0f;
        } else if (s_flashlight_power < 10) {
            s_flashlight_power = 3.0f;
        } else {
            s_flashlight_power = 7.0f;
        }
    }

    unsigned sector_queue_start = 0, sector_queue_end = 0;
    unsigned sector_queue[SECTOR_QUEUE_SIZE];
    unsigned tier_queue[SECTOR_QUEUE_SIZE];
    int x_queue[SECTOR_QUEUE_SIZE][2] = { { INT32_MIN, INT32_MIN } };
    int y_queue[SECTOR_QUEUE_SIZE][4] = { { INT32_MIN, INT32_MIN, INT32_MIN, INT32_MIN } };

    if (cam.sector < pod.num_sectors) {
        sector_queue[sector_queue_end] = cam.sector;
        tier_queue[sector_queue_end]   = cam.tier;
    } else {
        sector_queue[sector_queue_end] = 0;
        tier_queue[sector_queue_end]   = 0;
    }

    sector_queue_end = (sector_queue_end + 1) % SECTOR_QUEUE_SIZE;

    int window_high[SCREEN_WIDTH];
    int window_low[SCREEN_WIDTH];

    int tmp_window_high[SCREEN_WIDTH];
    int tmp_window_low[SCREEN_WIDTH];

    for (unsigned i = 0; i < SCREEN_WIDTH; ++i) {
        window_high[i]     = SCREEN_HEIGHT;
        window_low[i]      = 0;
        tmp_window_high[i] = SCREEN_HEIGHT;
        tmp_window_low[i]  = 0;
    }

    // loop variables
    Line view_tspace;
    Line view_space;
    Line ndc_space;
    WallAttribute attr[2];
    bool last_tier = true;

    // bredth first traversal of sectors
    while (sector_queue_start != sector_queue_end) {
        // pop
        unsigned sector_index = sector_queue[sector_queue_start];
        unsigned tier_index   = tier_queue[sector_queue_start];

        int secondary_window_x[2] = { x_queue[sector_queue_start][0], x_queue[sector_queue_start][1] };
        int secondary_window_y[4] = { y_queue[sector_queue_start][0], y_queue[sector_queue_start][1], y_queue[sector_queue_start][2], y_queue[sector_queue_start][3] };

        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        if (sector_index >= pod.num_sectors) continue; // avoid invalid sectors
        SectorDef sector = pod.sectors[sector_index];
        if (tier_index >= sector.num_tiers) continue; // avoid invalid tiers

        float sector_world_floor   = tier_index == 0 ? sector.floor_heights[tier_index] : sector.floor_heights[tier_index] + sector.ceiling_heights[0];
        float sector_world_ceiling = tier_index == 0 ? sector.ceiling_heights[tier_index] : sector.ceiling_heights[tier_index] + sector.ceiling_heights[0];

        float dist_to_floor   = (cam.pos[2] - sector_world_floor);
        float dist_to_ceiling = (sector_world_ceiling - cam.pos[2]);

        // calculate the secondary occlusion buffer
        if (secondary_window_x[0] != INT32_MIN &&
            secondary_window_x[1] != INT32_MIN &&
            secondary_window_y[0] != INT32_MIN &&
            secondary_window_y[1] != INT32_MIN &&
            secondary_window_y[2] != INT32_MIN &&
            secondary_window_y[3] != INT32_MIN) {

            // TODO: Might not need two buffers???
            // for (int x = secondary_window_x[0]; x < secondary_window_x[1]; ++x) {
            for (int x = 0; x < SCREEN_WIDTH; ++x) {
                float tx = (float)(x - secondary_window_x[0]) / (secondary_window_x[1] - secondary_window_x[0]);
                int low  = lerp(secondary_window_y[0], secondary_window_y[1], tx);
                int high = lerp(secondary_window_y[2], secondary_window_y[3], tx);

                tmp_window_low[x]  = clamp(low, 0, SCREEN_HEIGHT);
                tmp_window_high[x] = clamp(high, 0, SCREEN_HEIGHT);
            }
        } else {
            // clear
            for (unsigned i = 0; i < SCREEN_WIDTH; ++i) {
                tmp_window_high[i] = SCREEN_HEIGHT;
                tmp_window_low[i]  = 0;
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
            attr[1].uv[1] = sector_world_ceiling - sector_world_floor;

            attr[0].world_pos[0] = wall_line.points[0][0];
            attr[1].world_pos[0] = wall_line.points[1][0];
            attr[0].world_pos[1] = wall_line.points[0][1];
            attr[1].world_pos[1] = wall_line.points[1][1];
            attr[0].world_pos[2] = sector_world_floor;
            attr[1].world_pos[2] = sector_world_ceiling;

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

#ifndef CURRENT_SECTOR_ONLY
                if (is_portal) {
                    if (wall_next < pod.num_sectors) {
                        // for (unsigned i = 0; i < pod.sectors[wall_next].num_tiers; ++i) {
                        //     sector_queue[sector_queue_end] = wall_next;
                        //     tier_queue[sector_queue_end]   = i;
                        //     sector_queue_end               = (sector_queue_end + 1) % 128;
                        // }

                        // TODO: Reimplement this
                        unsigned start_tier = getSectorTier(pod, cam.pos[2], wall_next);
                        unsigned num_tiers  = pod.sectors[wall_next].num_tiers;

                        if (start_tier >= num_tiers) start_tier = 0;

                        sector_queue[sector_queue_end] = wall_next;
                        tier_queue[sector_queue_end]   = start_tier;
                        sector_queue_end               = (sector_queue_end + 1) % 128;

                        // go down
                        for (unsigned i = start_tier; i > 0;) {
                            --i;
                            sector_queue[sector_queue_end] = wall_next;
                            tier_queue[sector_queue_end]   = i;
                            sector_queue_end               = (sector_queue_end + 1) % 128;
                        }

                        // go up
                        for (unsigned i = start_tier + 1; i < num_tiers; ++i) {
                            sector_queue[sector_queue_end] = wall_next;
                            tier_queue[sector_queue_end]   = i;
                            sector_queue_end               = (sector_queue_end + 1) % 128;
                        }
                    }
                }
#endif

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

            int start_x = (ndc_space.points[0][0] * 0.5f + 0.5f) * (SCREEN_WIDTH - 1);
            int end_x   = (ndc_space.points[1][0] * 0.5f + 0.5f) * (SCREEN_WIDTH - 1);
            if (start_x > end_x) {
                swap(int, start_x, end_x);
                swap(float, ndc_space.points[0][1], ndc_space.points[1][1]);
                swap(WallAttribute, attr[0], attr[1]);
            }

            float top_of_wall[2] = {
                (0.5 - dist_to_ceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF + cam.pitch) * (SCREEN_HEIGHT - 1),
                (0.5 - dist_to_ceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF + cam.pitch) * (SCREEN_HEIGHT - 1),
            };

            float bottom_of_wall[2] = {
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * (SCREEN_HEIGHT - 1),
                (1.0 - (0.5 - dist_to_floor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * (SCREEN_HEIGHT - 1),
            };

            start_x = clamp(start_x, 0, SCREEN_WIDTH - 1);
            end_x   = clamp(end_x, 0, SCREEN_WIDTH - 1);

            vec2 wall_norm = { 0.0f, 0.0f };
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


#ifndef NO_CEILINGS

            // render ceiling
            if (dist_to_ceiling > 0.0) {
                int ceiling_top = max(top_of_wall[0], top_of_wall[1]);

                float scale = 2.0f * dist_to_ceiling;
                int horizon = (0.5 + cam.pitch) * SCREEN_HEIGHT;
                for (int y = 0; y < ceiling_top; ++y) {
                    float wy = -1.0f; // half_view_plane . tan(fov / 2)
                    float wz = (1.0f - (y - cam.pitch * SCREEN_HEIGHT) / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                    float z     = clamp(wz == 0 ? 0 : (scale / wz), NEAR_PLANE, FAR_PLANE);
                    float depth = FLOAT_TO_DEPTH(z);

                    for (int x = start_x; x <= end_x; ++x) {
                        if (y < window_low[x] ||
                            y >= window_high[x] ||
                            y < tmp_window_low[x] ||
                            y >= tmp_window_high[x]) continue;

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        float wx = (x - SCREEN_WIDTH_HALF) / SCREEN_WIDTH * ASPECT_RATIO * TAN_FOV_HALF;

                        float rx = cam.rot_cos * wx + -cam.rot_sin * wy;
                        float ry = cam.rot_sin * wx + cam.rot_cos * wy;

                        float px = (rx * scale / wz + cam.pos[0]);
                        float py = (ry * scale / wz + cam.pos[1]);

                        WallAttribute attr = {
                            .uv        = { px, py },
                            .world_pos = { px, py, sector_world_ceiling },
                            .normal    = { 0.0f, 0.0f, -1.0f },
                        };
                        Color color;

                        if (pixelProgram(attr, cam, 2, x, y, &color)) {
                        // if (skyPixelProgram(attr, cam, 2, x, y, &color)) {
                            setPixel(x, y, color);
                        }
                    }
                }
            }
#endif

#ifndef NO_FLOORS
            // render floor
            if (dist_to_floor > 0.0) {
                int floor_top = min(bottom_of_wall[0], bottom_of_wall[1]);

                float scale = 2.0f * dist_to_floor;
                int horizon = (0.5 + cam.pitch) * SCREEN_HEIGHT;

                for (int y = SCREEN_HEIGHT - 1; y >= floor_top; --y) {
                    float wy = -1.0f;
                    float wz = (1.0f - ((SCREEN_HEIGHT_HALF - y + SCREEN_HEIGHT_HALF) + cam.pitch * SCREEN_HEIGHT) / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                    float z     = clamp(wz == 0 ? 0 : (scale / wz), NEAR_PLANE, FAR_PLANE);
                    float depth = FLOAT_TO_DEPTH(z);

                    for (int x = start_x; x <= end_x; ++x) {
                        if (y < window_low[x] ||
                            y >= window_high[x] ||
                            y < tmp_window_low[x] ||
                            y >= tmp_window_high[x]) continue;

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        float wx = (x - SCREEN_WIDTH_HALF) / SCREEN_WIDTH * ASPECT_RATIO * TAN_FOV_HALF;

                        float rx = cam.rot_cos * wx + -cam.rot_sin * wy;
                        float ry = cam.rot_sin * wx + cam.rot_cos * wy;

                        float px = (rx * scale / wz + cam.pos[0]);
                        float py = (ry * scale / wz + cam.pos[1]);

                        WallAttribute attr = {
                            .uv        = { px, py },
                            .world_pos = { px, py, sector_world_floor },
                            .normal    = { 0.0f, 0.0f, 1.0f },
                        };
                        Color color;

                        if (pixelProgram(attr, cam, 1, x, y, &color)) {
                            setPixel(x, y, color);
                        }
                    }
                }
            }

#endif
            if (!is_portal) {
                // draw wall
                for (int x = start_x; x <= end_x; ++x) {

                    float tx    = (float)(x - start_x) / (end_x - start_x);
                    int start_y = lerp(top_of_wall[0], top_of_wall[1], tx);
                    int end_y   = lerp(bottom_of_wall[0], bottom_of_wall[1], tx);

                    float z   = 1.0f / lerp(ndc_space.points[0][1], ndc_space.points[1][1], tx);
                    int depth = FLOAT_TO_DEPTH(z);

                    float u = lerp(attr[0].uv[0], attr[1].uv[0], tx) * z;

                    vec3 world_pos = {
                        lerp(attr[0].world_pos[0], attr[1].world_pos[0], tx) * z,
                        lerp(attr[0].world_pos[1], attr[1].world_pos[1], tx) * z,
                        0.0f,
                    };

                    int start_y_real = start_y;
                    int end_y_real   = end_y;

                    start_y = clamp(start_y, max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));
                    end_y   = clamp(end_y, max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));

                    if (last_tier) {
                        window_high[x] = 0;
                    }

                    // wall
                    for (int y = start_y; y < end_y; ++y) {
                        int depth_index = x + y * SCREEN_WIDTH;

                        g_depth_buffer[depth_index] = depth;

                        float ty = (float)(y - start_y_real) / (end_y_real - start_y_real);

                        float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                        world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                        WallAttribute attr = {
                            .uv        = { u, v },
                            .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                            .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                        };
                        Color color;

                        if (pixelProgram(attr, cam, 0, x, y, &color)) {
                        // if (skyPixelProgram(attr, cam, 0, x, y, &color)) {
                            setPixel(x, y, color);
                        }
                    }
                }
            } else {
#ifndef NO_STEPS
                // draw steps
                SectorDef nsector = pod.sectors[wall_next];
                bool single_tier  = nsector.num_tiers == 1;

                unsigned sector_queue_start = (sector_queue_end - nsector.num_tiers + 128) % 128;

                // for (unsigned ntier_index = 0; ntier_index < nsector.num_tiers; ++ntier_index) {
                for (unsigned i = 0; i < nsector.num_tiers; ++i) {
                    unsigned ntier_index = tier_queue[sector_queue_start + i];

                    float nsector_world_floor   = ntier_index == 0 ? nsector.floor_heights[ntier_index] : nsector.floor_heights[ntier_index] + nsector.ceiling_heights[0];
                    float nsector_world_ceiling = ntier_index == 0 ? nsector.ceiling_heights[ntier_index] : nsector.ceiling_heights[ntier_index] + nsector.ceiling_heights[0];

                    float dist_to_nfloor   = (cam.pos[2] - nsector_world_floor);
                    float dist_to_nceiling = (nsector_world_ceiling - cam.pos[2]);

                    float top_of_nwall[2] = {
                        (0.5 - dist_to_nceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                        (0.5 - dist_to_nceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                    };

                    float bottom_of_nwall[2] = {
                        (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                        (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                    };

                    float top_of_step[2];
                    float bottom_of_step[2];

                    if (single_tier || ntier_index >= nsector.num_tiers - 1) {
                        // this is the very top step
                        top_of_step[0] = top_of_wall[0];
                        top_of_step[1] = top_of_wall[1];
                    } else {
                        // need to use the floor of above tier
                        float nsector_world_floor = ntier_index + 1 == 0 ? nsector.floor_heights[ntier_index + 1] : nsector.floor_heights[ntier_index + 1] + nsector.ceiling_heights[0];
                        float dist_to_nfloor      = (cam.pos[2] - nsector_world_floor);
                        top_of_step[0]            = (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT;
                        top_of_step[1]            = (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT;
                    }

                    if (single_tier || ntier_index == 0) {
                        // this is the very first step
                        bottom_of_step[0] = bottom_of_wall[0];
                        bottom_of_step[1] = bottom_of_wall[1];
                    } else {
                        // dont draw bottom steps because they are covered by top step
                        bottom_of_step[0] = bottom_of_nwall[0];
                        bottom_of_step[1] = bottom_of_nwall[1];
                    }

                    if (!single_tier) {
                        x_queue[sector_queue_start + i][0] = start_x;
                        x_queue[sector_queue_start + i][1] = end_x;
                        y_queue[sector_queue_start + i][0] = max(top_of_nwall[0], top_of_step[0]);
                        y_queue[sector_queue_start + i][1] = max(top_of_nwall[1], top_of_step[1]);
                        y_queue[sector_queue_start + i][2] = min(bottom_of_nwall[0] + 1, bottom_of_step[0] + 1);
                        y_queue[sector_queue_start + i][3] = min(bottom_of_nwall[1] + 1, bottom_of_step[1] + 1);
                    } else {
                        x_queue[sector_queue_start + i][0] = INT32_MIN;
                        x_queue[sector_queue_start + i][1] = INT32_MIN;
                        y_queue[sector_queue_start + i][0] = INT32_MIN;
                        y_queue[sector_queue_start + i][1] = INT32_MIN;
                        y_queue[sector_queue_start + i][2] = INT32_MIN;
                        y_queue[sector_queue_start + i][3] = INT32_MIN;
                    }

                    for (int x = start_x; x <= end_x; ++x) {
                        float tx = (float)(x - start_x) / (end_x - start_x);

                        int start_y = lerp(top_of_step[0], top_of_step[1], tx);
                        int end_y   = lerp(bottom_of_step[0], bottom_of_step[1], tx);

                        int start_ny = lerp(top_of_nwall[0], top_of_nwall[1], tx);
                        int end_ny   = lerp(bottom_of_nwall[0], bottom_of_nwall[1], tx);

                        float z   = 1.0f / lerp(ndc_space.points[0][1], ndc_space.points[1][1], tx);
                        int depth = FLOAT_TO_DEPTH(z);

                        float u = lerp(attr[0].uv[0], attr[1].uv[0], tx) * z;

                        vec3 world_pos = {
                            lerp(attr[0].world_pos[0], attr[1].world_pos[0], tx) * z,
                            lerp(attr[0].world_pos[1], attr[1].world_pos[1], tx) * z,
                            0.0f,
                        };

                        int start_y_real = start_y;
                        int end_y_real   = end_y;

                        start_y = clamp(start_y, max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));
                        end_y   = clamp(end_y, max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));

                        start_ny = clamp(min(start_ny, end_y), max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));
                        end_ny   = clamp(max(end_ny, start_y), max(window_low[x], tmp_window_low[x]), min(window_high[x], tmp_window_high[x]));

                        if (single_tier) {
                            window_low[x]  = max(start_ny, start_y);
                            window_high[x] = min(end_ny, end_y);
                        }

                        // top step
                        for (int y = start_y; y < start_ny; ++y) {
                            int depth_index = x + y * SCREEN_WIDTH;

                            g_depth_buffer[depth_index] = depth;
                            float ty                    = (float)(y - start_y_real) / (end_y_real - start_y_real);
                            float v                     = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                            world_pos[2]                = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                            WallAttribute attr = {
                                .uv        = { u, v },
                                .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                                .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                            };
                            Color color;

                            if (pixelProgram(attr, cam, 0, x, y, &color)) {
                                setPixel(x, y, color);
                            }
                        }

                        // bottom step
                        for (int y = end_ny; y < end_y; ++y) {
                            int depth_index = x + y * SCREEN_WIDTH;

                            g_depth_buffer[depth_index] = depth;
                            float ty                    = (float)(y - start_y_real) / (end_y_real - start_y_real);
                            float v                     = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                            world_pos[2]                = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                            WallAttribute attr = {
                                .uv        = { u, v },
                                .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                                .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                            };
                            Color color;

                            if (pixelProgram(attr, cam, 0, x, y, &color)) {
                                setPixel(x, y, color);
                            }
                        }
                    }
                }
#endif
            }
        }
        last_tier = false;
    }


#ifdef RENDER_OCCLUSION
    for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
        for (unsigned y = 0; y < max(window_low[x], tmp_window_low[x]); ++y) {
            setPixel(x, y, COLOR_BLACK);
        }

        for (unsigned y = max(window_low[x], tmp_window_low[x]); y < min(window_high[x], tmp_window_high[x]); ++y) {
            setPixel(x, y, COLOR_WHITE);
        }

        for (unsigned y = min(window_high[x], tmp_window_high[x]); y < SCREEN_HEIGHT; ++y) {
            setPixel(x, y, COLOR_BLACK);
        }
    }
#endif
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