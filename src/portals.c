#include "portals.h"
#include "geo.h"
#include "draw.h"

#include <SDL2/SDL.h>

#include <math.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

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

#define MIN_WORLD_VERSION 1
#define MAX_WORLD_VERSION 1
bool loadWorld(const char *path, PortalWorld *o_world, float scale) {
    assert(path != NULL);
    assert(o_world != NULL);

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("ERROR: Failed to open %s\n", path);
        return false;
    }
    printf("Opened %s\n", path);

    unsigned wall_index = 0;
    char line[1024];
    char *line_tmp;
    size_t line_size = 1024;

    unsigned version = 0;
    unsigned unsigned_buff0;
    int num_read;
    char directive_name[64];

    unsigned num_sectors_read = 0;
    unsigned num_walls_read   = 0;

    SectorDef tmp_sector;

    memset(o_world, 0, sizeof(*o_world));

    enum state_e {
        state_version,
        state_open,
        state_sectors,
        state_walls,
    } state = state_version;

    while (fgets(line, line_size, file) != NULL) {
        ++wall_index;
        num_read = sscanf(line, "%64s", directive_name);
        if (num_read == EOF) continue;
        if (strcmp(directive_name, "//") == 0) continue;

        // printf("%s: %s", directive_name, line);

        switch (state) {
            case state_version:
                if (strcmp(directive_name, "VERSION") == 0) {
                    num_read = sscanf(line, "%*s %u", &version);
                    if (num_read != 1) {
                        printf("ERROR:%u: 'VERSION' expects one unsigned parameter\n", wall_index);
                        return false;
                    }

                    if (version < MIN_WORLD_VERSION || version > MAX_WORLD_VERSION) {
                        printf("ERROR:%u: Version number of %u is not supported: min %u to max %u.\n", wall_index, version, MIN_WORLD_VERSION, MAX_WORLD_VERSION);
                        return false;
                    }

                    state = state_open;
                } else {
                    printf("ERROR:%u: Expected first directive to be 'VERSION'\n", wall_index);
                    return false;
                }
                break;
                ///////////////////////////////////////////////////////////////////////////////////////////////////
            case state_open:
                if (strcmp(directive_name, "SECTORS") == 0) {
                    num_read = sscanf(line, "%*s %u", &unsigned_buff0);

                    if (num_read != 1) {
                        printf("ERROR:%u: 'SECTORS' expects one unsigned parameter\n", wall_index);
                        return false;
                    }

                    o_world->num_sectors += unsigned_buff0;
                    o_world->sectors = realloc(o_world->sectors, o_world->num_sectors * sizeof(*o_world->sectors));
                    state            = state_sectors;

                } else if (strcmp(directive_name, "WALLS") == 0) {
                    num_read = sscanf(line, "%*s %u", &unsigned_buff0);

                    if (num_read != 1) {
                        printf("ERROR:%u: 'WALLS' expects one unsigned parameter\n", wall_index);
                        return false;
                    }

                    o_world->num_walls += unsigned_buff0;
                    o_world->wall_lines       = realloc(o_world->wall_lines, o_world->num_walls * sizeof(*o_world->wall_lines));
                    o_world->wall_nexts       = realloc(o_world->wall_nexts, o_world->num_walls * sizeof(*o_world->wall_nexts));
                    o_world->wall_is_skys     = realloc(o_world->wall_is_skys, o_world->num_walls * sizeof(*o_world->wall_is_skys));
                    o_world->wall_texture_ids = realloc(o_world->wall_texture_ids, o_world->num_walls * sizeof(*o_world->wall_texture_ids));

                    state = state_walls;
                } else {
                    printf("ERROR:%u: Unknown or unexpected directive: %s\n", wall_index, directive_name);
                    return false;
                }
                break;
                ///////////////////////////////////////////////////////////////////////////////////////////////////
            case state_sectors:
                if (strcmp(directive_name, "END") == 0) {
                    state = state_open;
                    break;
                }
                if (num_sectors_read + 1 > o_world->num_sectors) {
                    o_world->num_sectors += 10;
                    o_world->sectors = realloc(o_world->sectors, o_world->num_sectors * sizeof(*o_world->sectors));
                }

                num_read = sscanf(line, "%u %u %u", &tmp_sector.start, &tmp_sector.length, &tmp_sector.num_tiers);
                if (num_read != 3) {
                    printf("ERROR:%u: Ill-formed sector definition\n", wall_index);
                    return false;
                }

                if (tmp_sector.num_tiers < 1) {
                    printf("ERROR:%u: Sectors require at least one tier\n", wall_index);
                    return false;
                }

                tmp_sector.floor_heights       = malloc(tmp_sector.num_tiers * sizeof(*tmp_sector.floor_heights));
                tmp_sector.ceiling_heights     = malloc(tmp_sector.num_tiers * sizeof(*tmp_sector.ceiling_heights));
                tmp_sector.is_skys             = malloc(tmp_sector.num_tiers * sizeof(*tmp_sector.is_skys));
                tmp_sector.floor_texture_ids   = malloc(tmp_sector.num_tiers * sizeof(*tmp_sector.floor_texture_ids));
                tmp_sector.ceiling_texture_ids = malloc(tmp_sector.num_tiers * sizeof(*tmp_sector.ceiling_texture_ids));

                line_tmp = line;

                for (unsigned i = 0; i < tmp_sector.num_tiers; ++i) {
                    // skip to next tier def
                    line_tmp = strchr(line_tmp, '|') + 1;

                    num_read = sscanf(line_tmp, "%f %f %u %u %u",
                                      &tmp_sector.floor_heights[i],
                                      &tmp_sector.ceiling_heights[i],
                                      &unsigned_buff0,
                                      &tmp_sector.floor_texture_ids[i],
                                      &tmp_sector.ceiling_texture_ids[i]);

                    if (num_read != 5) {
                        printf("ERROR:%u: Ill-formed sector tier definition\n", wall_index);
                        return false;
                    }

                    tmp_sector.is_skys[i] = unsigned_buff0 != 0;
                }

                o_world->sectors[num_sectors_read] = tmp_sector;
                ++num_sectors_read;
                break;
                ///////////////////////////////////////////////////////////////////////////////////////////////////
            case state_walls:
                if (strcmp(directive_name, "END") == 0) {
                    state = state_open;
                    break;
                }
                if (num_walls_read + 1 > o_world->num_walls) {
                    o_world->num_walls += 10;
                    o_world->wall_lines       = realloc(o_world->wall_lines, o_world->num_walls * sizeof(*o_world->wall_lines));
                    o_world->wall_nexts       = realloc(o_world->wall_nexts, o_world->num_walls * sizeof(*o_world->wall_nexts));
                    o_world->wall_is_skys     = realloc(o_world->wall_is_skys, o_world->num_walls * sizeof(*o_world->wall_is_skys));
                    o_world->wall_texture_ids = realloc(o_world->wall_texture_ids, o_world->num_walls * sizeof(*o_world->wall_texture_ids));
                }

                num_read = sscanf(line, "%f %f %f %f %u %u %u",
                                  &o_world->wall_lines[num_walls_read].points[0][0],
                                  &o_world->wall_lines[num_walls_read].points[0][1],
                                  &o_world->wall_lines[num_walls_read].points[1][0],
                                  &o_world->wall_lines[num_walls_read].points[1][1],
                                  &o_world->wall_nexts[num_walls_read],
                                  &unsigned_buff0,
                                  &o_world->wall_texture_ids[num_walls_read]);

                if (num_read != 7) {
                    printf("ERROR:%u: Ill-formed wall definition\n", wall_index);
                    return false;
                }

                o_world->wall_is_skys[num_walls_read] = unsigned_buff0 != 0;

                if (o_world->wall_nexts[num_walls_read] == 0) {
                    o_world->wall_nexts[num_walls_read] = INVALID_SECTOR_INDEX;
                } else {
                    o_world->wall_nexts[num_walls_read] -= 1;
                }

                o_world->wall_lines[num_walls_read].points[0][0] *= scale;
                o_world->wall_lines[num_walls_read].points[0][1] *= scale;
                o_world->wall_lines[num_walls_read].points[1][0] *= scale;
                o_world->wall_lines[num_walls_read].points[1][1] *= scale;

                ++num_walls_read;
                break;
                ///////////////////////////////////////////////////////////////////////////////////////////////////
            default:
                assert(false && "Unhandled state!");
        }
    }

    fclose(file);

    o_world->num_sectors = num_sectors_read;
    o_world->num_walls   = num_walls_read;

    // printf("PARSED\n");
    // printf("VERSION: %u\n", version);
    // printf("SECTORS: %u\n", o_world->num_sectors);
    // for (unsigned i = 0; i < o_world->num_sectors; ++i) {
    //     SectorDef sector = o_world->sectors[i];
    //     printf("start: %u length: %u tiers: %u ", sector.start, sector.length, sector.num_tiers);
    //     for (unsigned j = 0; j < sector.num_tiers; ++j) {
    //         printf("floor height: %f ceiling height: %f ", sector.floor_heights[j], sector.ceiling_heights[j]);
    //     }
    //     printf("\n");
    // }

    // printf("WALLS: %u\n", o_world->num_walls);
    // for (unsigned i = 0; i < o_world->num_walls; ++i) {
    //     Line line = o_world->wall_lines[i];
    //     printf("(%f, %f) (%f, %f) %u\n", line.points[0][0], line.points[0][1], line.points[1][0], line.points[1][1], o_world->wall_nexts[i]);
    // }

    return true;
}

void freeWorld(PortalWorld world) {
    free(world.wall_lines);
    free(world.wall_nexts);
    free(world.wall_is_skys);
    free(world.wall_texture_ids);

    for (unsigned i = 0; i < world.num_sectors; ++i) {
        free(world.sectors[i].floor_heights);
        free(world.sectors[i].ceiling_heights);
        free(world.sectors[i].is_skys);
        free(world.sectors[i].floor_texture_ids);
        free(world.sectors[i].ceiling_texture_ids);
    }
}

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
        float sector_world_floor   = sector.floor_heights[i];
        float sector_world_ceiling = sector.ceiling_heights[i];

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
extern Image g_sky_image_array[1];

extern bool g_render_occlusion;

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
    Image img             = g_sky_image_array[texid];

    float sky_ar = (float)img.height / (img.width * SKY_SCALE);

    float u = modff(screen_x / (float)SCREEN_WIDTH * sky_ar * ASPECT_RATIO + cam.rot / (2 * M_PI), NULL);

    // place base on horizon line
    float v = (screen_y / (float)SCREEN_HEIGHT + 1.0f - cam.pitch) / SKY_SCALE;
    // float v   = (screen_y / (float)SCREEN_HEIGHT + 1.0f - cam.pitch);
    if (u < 0) u += 1;
    if (v < 0) return false;

    unsigned tex_x = (u * (img.width - 1));
    unsigned tex_y = (v * (img.height - 1));
    *o_color       = sampleImage(img, tex_x, tex_y);
    return true;
}

typedef struct TrapezoidPortal {
    int min_x, max_x;
    int low_y[2];
    int high_y[2];
} TrapezoidPortal;

bool clipTrapPortal(TrapezoidPortal base, TrapezoidPortal *test) {
    assert(test != NULL);

    // check if outside x range
    if (test->max_x < base.min_x || base.max_x < test->min_x) return false;

    float base_inv_range = 1.0f / (base.max_x - base.min_x);

    // zero to one range indicating how far along base test.min_x is
    float test_min_x_u = (test->min_x - base.min_x) * base_inv_range;
    // zero to one range indicating how far along base test.max_x is
    float test_max_x_u = (test->max_x - base.min_x) * base_inv_range;

    int base_low_at_test[2] = {
        max(lerp(base.low_y[0], base.low_y[1], test_min_x_u), base.low_y[0]),
        max(lerp(base.low_y[0], base.low_y[1], test_max_x_u), base.low_y[1])
    };
    int base_high_at_test[2] = {
        min(lerp(base.high_y[0], base.high_y[1], test_min_x_u), base.high_y[0]),
        min(lerp(base.high_y[0], base.high_y[1], test_max_x_u), base.high_y[1])
    };

    // check if outside y range
    if (test->high_y[0] < base_low_at_test[0] && test->high_y[1] < base_low_at_test[1]) return false;
    if (base_high_at_test[0] < test->low_y[0] && base_high_at_test[1] < test->low_y[1]) return false;

    // now for the fun part.

    float test_inv_range = 1.0f / (test->max_x - test->min_x);
    // zero to one range indicating how far along test base.min_x is
    float base_min_x_u = (base.min_x - test->min_x) * test_inv_range;
    // zero to one range indicating how far along test base.max_x is
    float base_max_x_u = (base.max_x - test->min_x) * test_inv_range;

    // precalc intersection points
    int test_low_at_base[2]  = { lerp(test->low_y[0], test->low_y[1], base_min_x_u), lerp(test->low_y[0], test->low_y[1], base_max_x_u) };
    int test_high_at_base[2] = { lerp(test->high_y[0], test->high_y[1], base_min_x_u), lerp(test->high_y[0], test->high_y[1], base_max_x_u) };

    // each corner needs to be clipped to fit inside the base trapezoid.
    // assuming that the points are in the correct order along with the above clipping, we should only need to check the corresponding sides
    // the top left vertex of test should only need to be checked against the top, and the left side of base.

    // if the point is outside both, simply set the point to the corresponding vertex on base. This will also lose some accuracy, but it's fine
    // if the point is outside the y point but not the x point, simply move the y down to meet. This is not correct, but only ever loses space, so only minor artifacts are visible
    // if the point is outside the x point but not the y point, set test's x to base's x and set test's y to the intersected y point
    // if the point is inside both corresponding sides, no clipping needed

    // precalc outside
    bool outside_min = test->min_x < base.min_x;
    bool outside_max = test->max_x > base.max_x;

    bool outside_low[2]  = { test->low_y[0] < base_low_at_test[0], test->low_y[1] < base_low_at_test[1] };
    bool outside_high[2] = { test->high_y[0] > base_high_at_test[0], test->high_y[1] > base_high_at_test[1] };

    // top left vertex
    if (outside_min && outside_low[0]) {
        test->min_x    = base.min_x;
        test->low_y[0] = base.low_y[0];
    } else if (!outside_min && outside_low[0]) {
        test->low_y[0] = base.low_y[0];
    } else if (outside_min && !outside_low[0]) {
        test->min_x    = base.min_x;
        test->low_y[0] = test_low_at_base[0];
    }

    // top right vertex
    if (outside_max && outside_low[1]) {
        test->max_x    = base.max_x;
        test->low_y[1] = base.low_y[1];
    } else if (!outside_max && outside_low[1]) {
        test->low_y[1] = base.low_y[1];
    } else if (outside_max && !outside_low[1]) {
        test->max_x    = base.max_x;
        test->low_y[1] = test_low_at_base[1];
    }

    // bottom left vertex
    if (outside_min && outside_high[0]) {
        test->min_x     = base.min_x;
        test->high_y[0] = base.high_y[0];
    } else if (!outside_min && outside_high[0]) {
        test->high_y[0] = base.high_y[0];
    } else if (outside_min && !outside_high[0]) {
        test->min_x     = base.min_x;
        test->high_y[0] = test_high_at_base[0];
    }

    // bottom right vertex
    if (outside_max && outside_high[1]) {
        test->max_x     = base.max_x;
        test->high_y[1] = base.high_y[1];
    } else if (!outside_max && outside_high[1]) {
        test->high_y[1] = base.high_y[1];
    } else if (outside_max && !outside_high[1]) {
        test->max_x     = base.max_x;
        test->high_y[1] = test_high_at_base[1];
    }

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

    TrapezoidPortal portal_queue[SECTOR_QUEUE_SIZE] = {
        { .min_x = 0, .max_x = SCREEN_WIDTH, .low_y = { 0, 0 }, .high_y = { SCREEN_HEIGHT, SCREEN_HEIGHT } }
    };

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

    // loop variables
    Line view_tspace;
    Line view_space;
    Line ndc_space;
    WallAttribute attr[2];
    bool last_tier = true;

    // bredth first traversal of sectors
    while (sector_queue_start != sector_queue_end) {
        // pop
        unsigned sector_index  = sector_queue[sector_queue_start];
        unsigned tier_index    = tier_queue[sector_queue_start];
        TrapezoidPortal portal = portal_queue[sector_queue_start];

        sector_queue_start = (sector_queue_start + 1) % SECTOR_QUEUE_SIZE;

        if (sector_index >= pod.num_sectors) continue; // avoid invalid sectors
        SectorDef sector = pod.sectors[sector_index];
        if (tier_index >= sector.num_tiers) continue; // avoid invalid tiers

        float sector_world_floor   = sector.floor_heights[tier_index];
        float sector_world_ceiling = sector.ceiling_heights[tier_index];

        float dist_to_floor   = (cam.pos[2] - sector_world_floor);
        float dist_to_ceiling = (sector_world_ceiling - cam.pos[2]);

        // calculate occlusion buffer
        {
            for (int x = 0; x < portal.min_x; ++x) {
                window_low[x]  = 0;
                window_high[x] = 0;
            }

            for (int x = portal.max_x; x < SCREEN_WIDTH; ++x) {
                window_low[x]  = 0;
                window_high[x] = 0;
            }

            float inv_range = 1.0f / (portal.max_x - portal.min_x);
            for (int x = portal.min_x; x < portal.max_x; ++x) {
                float tx = (float)(x - portal.min_x) * inv_range;
                int low  = lerp(portal.low_y[0], portal.low_y[1], tx);
                int high = lerp(portal.high_y[0], portal.high_y[1], tx);

                window_low[x]  = clamp(low, 0, SCREEN_HEIGHT - 1);
                window_high[x] = clamp(high, 0, SCREEN_HEIGHT);
            }
        }

        // render every wall
        for (unsigned i = 0; i < sector.length; ++i) {
            Line wall_line      = pod.wall_lines[sector.start + i];
            unsigned wall_next  = pod.wall_nexts[sector.start + i];
            bool wall_is_sky    = pod.wall_is_skys[sector.start + i];
            unsigned wall_texid = pod.wall_texture_ids[sector.start + i];

            bool is_portal = wall_next != INVALID_SECTOR_INDEX && wall_next != sector_index;

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


#ifndef CURRENT_SECTOR_ONLY
// BUG: Wall gets clipped prior to this, so when close to a portal, the next sector will not be rendered
            if (is_portal) {
                if (wall_next < pod.num_sectors) {
                    SectorDef nsector   = pod.sectors[wall_next];
                    unsigned start_tier = getSectorTier(pod, cam.pos[2], wall_next);
                    unsigned num_tiers  = nsector.num_tiers;

                    if (start_tier >= num_tiers) start_tier = 0;

                    TrapezoidPortal tmp_portal;

                    {
                        tmp_portal.min_x = start_x;
                        tmp_portal.max_x = end_x;

                        float dist_to_nfloor   = (cam.pos[2] - nsector.floor_heights[start_tier]);
                        float dist_to_nceiling = (nsector.ceiling_heights[start_tier] - cam.pos[2]);

                        float top_of_nwall[2] = {
                            (0.5 - dist_to_nceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                            (0.5 - dist_to_nceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                        };

                        float bottom_of_nwall[2] = {
                            (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                            (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                        };

                        tmp_portal.low_y[0]  = max(top_of_nwall[0], top_of_wall[0]);
                        tmp_portal.low_y[1]  = max(top_of_nwall[1], top_of_wall[1]);
                        tmp_portal.high_y[0] = min(bottom_of_nwall[0], bottom_of_wall[0]);
                        tmp_portal.high_y[1] = min(bottom_of_nwall[1], bottom_of_wall[1]);

                        if (clipTrapPortal(portal, &tmp_portal)) {
                            sector_queue[sector_queue_end] = wall_next;
                            tier_queue[sector_queue_end]   = start_tier;
                            portal_queue[sector_queue_end] = tmp_portal;

                            sector_queue_end = (sector_queue_end + 1) % 128;
                        }
                    }

                    // go down
                    for (unsigned i = start_tier; i > 0;) {
                        --i;

                        {
                            tmp_portal.min_x = start_x;
                            tmp_portal.max_x = end_x;

                            float dist_to_nfloor   = (cam.pos[2] - nsector.floor_heights[i]);
                            float dist_to_nceiling = (nsector.ceiling_heights[i] - cam.pos[2]);

                            float top_of_nwall[2] = {
                                (0.5 - dist_to_nceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                                (0.5 - dist_to_nceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                            };

                            float bottom_of_nwall[2] = {
                                (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                                (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                            };

                            tmp_portal.low_y[0]  = max(top_of_nwall[0], top_of_wall[0]);
                            tmp_portal.low_y[1]  = max(top_of_nwall[1], top_of_wall[1]);
                            tmp_portal.high_y[0] = min(bottom_of_nwall[0], bottom_of_wall[0]);
                            tmp_portal.high_y[1] = min(bottom_of_nwall[1], bottom_of_wall[1]);

                            if (clipTrapPortal(portal, &tmp_portal)) {
                                sector_queue[sector_queue_end] = wall_next;
                                tier_queue[sector_queue_end]   = i;
                                portal_queue[sector_queue_end] = tmp_portal;

                                sector_queue_end = (sector_queue_end + 1) % 128;
                            }
                        }
                    }

                    // go up
                    for (unsigned i = start_tier + 1; i < num_tiers; ++i) {
                        {
                            tmp_portal.min_x = start_x;
                            tmp_portal.max_x = end_x;

                            float dist_to_nfloor   = (cam.pos[2] - nsector.floor_heights[i]);
                            float dist_to_nceiling = (nsector.ceiling_heights[i] - cam.pos[2]);

                            float top_of_nwall[2] = {
                                (0.5 - dist_to_nceiling * ndc_space.points[0][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                                (0.5 - dist_to_nceiling * ndc_space.points[1][1] * INV_TAN_FOV_HALF + cam.pitch) * SCREEN_HEIGHT,
                            };

                            float bottom_of_nwall[2] = {
                                (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                                (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT,
                            };

                            tmp_portal.low_y[0]  = max(top_of_nwall[0], top_of_wall[0]);
                            tmp_portal.low_y[1]  = max(top_of_nwall[1], top_of_wall[1]);
                            tmp_portal.high_y[0] = min(bottom_of_nwall[0], bottom_of_wall[0]);
                            tmp_portal.high_y[1] = min(bottom_of_nwall[1], bottom_of_wall[1]);

                            if (clipTrapPortal(portal, &tmp_portal)) {
                                sector_queue[sector_queue_end] = wall_next;
                                tier_queue[sector_queue_end]   = i;
                                portal_queue[sector_queue_end] = tmp_portal;

                                sector_queue_end = (sector_queue_end + 1) % 128;
                            }
                        }
                    }
                }
            }
#endif


#ifndef NO_CEILINGS

            // render ceiling
            if (dist_to_ceiling > 0.0) {
                int ceiling_top = max(top_of_wall[0], top_of_wall[1]);

                float scale = 2.0f * dist_to_ceiling;
                // int horizon = (0.5 + cam.pitch) * SCREEN_HEIGHT;
                for (int y = 0; y < ceiling_top; ++y) {
                    float wy = -1.0f; // half_view_plane . tan(fov / 2)
                    float wz = (1.0f - (y - cam.pitch * SCREEN_HEIGHT) / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                    float z     = clamp(wz == 0 ? 0 : (scale / wz), NEAR_PLANE, FAR_PLANE);
                    float depth = FLOAT_TO_DEPTH(z);

                    for (int x = start_x; x <= end_x; ++x) {
                        if (y < window_low[x] ||
                            y >= window_high[x]) continue;

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        if (!g_render_occlusion) {
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

                            bool draw;
                            unsigned ti = sector.ceiling_texture_ids[tier_index];
                            if (sector.is_skys[tier_index]) {
                                draw = skyPixelProgram(attr, cam, ti, x, y, &color);
                            } else {
                                draw = pixelProgram(attr, cam, ti, x, y, &color);
                            }

                            if (draw) {
                                setPixel(x, y, color);
                            }
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
                // int horizon = (0.5 + cam.pitch) * SCREEN_HEIGHT;

                for (int y = SCREEN_HEIGHT - 1; y >= floor_top; --y) {
                    float wy = -1.0f;
                    float wz = (1.0f - ((SCREEN_HEIGHT_HALF - y + SCREEN_HEIGHT_HALF) + cam.pitch * SCREEN_HEIGHT) / SCREEN_HEIGHT_HALF) * TAN_FOV_HALF;

                    float z     = clamp(wz == 0 ? 0 : (scale / wz), NEAR_PLANE, FAR_PLANE);
                    float depth = FLOAT_TO_DEPTH(z);

                    for (int x = start_x; x <= end_x; ++x) {
                        if (y < window_low[x] ||
                            y >= window_high[x]) continue;

                        int depth_index             = x + y * SCREEN_WIDTH;
                        g_depth_buffer[depth_index] = depth;

                        if (!g_render_occlusion) {
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

                            bool draw;
                            unsigned ti = sector.floor_texture_ids[tier_index];
                            draw        = pixelProgram(attr, cam, ti, x, y, &color);

                            if (draw) {
                                setPixel(x, y, color);
                            }
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

                    start_y = clamp(start_y, window_low[x], window_high[x]);
                    end_y   = clamp(end_y, window_low[x], window_high[x]);

                    // wall
                    for (int y = start_y; y < end_y; ++y) {
                        int depth_index = x + y * SCREEN_WIDTH;

                        g_depth_buffer[depth_index] = depth;

                        if (!g_render_occlusion) {

                            float ty = (float)(y - start_y_real) / (end_y_real - start_y_real);

                            float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                            world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                            WallAttribute attr = {
                                .uv        = { u, v },
                                .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                                .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                            };
                            Color color;

                            bool draw;
                            if (wall_is_sky) {
                                draw = skyPixelProgram(attr, cam, wall_texid, x, y, &color);
                            } else {
                                draw = pixelProgram(attr, cam, wall_texid, x, y, &color);
                            }

                            if (draw) {
                                setPixel(x, y, color);
                            }
                        }
                    }
                }
            } else {
#ifndef NO_STEPS
                // draw steps
                SectorDef nsector = pod.sectors[wall_next];

                for (unsigned ntier_index = 0; ntier_index < nsector.num_tiers; ++ntier_index) {
                    float nsector_world_floor   = nsector.floor_heights[ntier_index];
                    float nsector_world_ceiling = nsector.ceiling_heights[ntier_index];

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

                    if (ntier_index >= nsector.num_tiers - 1) {
                        // this is the very top step
                        top_of_step[0] = top_of_wall[0];
                        top_of_step[1] = top_of_wall[1];
                    } else {
                        // need to use the floor of above tier
                        float nsector_world_floor = nsector.floor_heights[ntier_index + 1];
                        float dist_to_nfloor      = (cam.pos[2] - nsector_world_floor);
                        top_of_step[0]            = (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[0][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT;
                        top_of_step[1]            = (1.0 - (0.5 - dist_to_nfloor * ndc_space.points[1][1] * INV_TAN_FOV_HALF) + cam.pitch) * SCREEN_HEIGHT;
                    }

                    if (ntier_index == 0) {
                        // this is the very first step
                        bottom_of_step[0] = bottom_of_wall[0];
                        bottom_of_step[1] = bottom_of_wall[1];
                    } else {
                        // dont draw bottom steps because they are covered by top step
                        bottom_of_step[0] = bottom_of_nwall[0];
                        bottom_of_step[1] = bottom_of_nwall[1];
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

                        int start_y_real = lerp(top_of_wall[0], top_of_wall[1], tx);
                        int end_y_real   = lerp(bottom_of_wall[0], bottom_of_wall[1], tx);

                        start_y = clamp(start_y, window_low[x], window_high[x]);
                        end_y   = clamp(end_y, window_low[x], window_high[x]);

                        start_ny = clamp(min(start_ny, end_y), window_low[x], window_high[x]);
                        end_ny   = clamp(max(end_ny, start_y), window_low[x], window_high[x]);

                        // top step
                        for (int y = start_y; y < start_ny; ++y) {
                            int depth_index = x + y * SCREEN_WIDTH;

                            g_depth_buffer[depth_index] = depth;

                            if (!g_render_occlusion) {
                                float ty = (float)(y - start_y_real) / (end_y_real - start_y_real);

                                float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                                world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);

                                WallAttribute attr = {
                                    .uv        = { u, v },
                                    .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                                    .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                                };
                                Color color;

                                bool draw;
                                if (wall_is_sky) {
                                    draw = skyPixelProgram(attr, cam, wall_texid, x, y, &color);
                                } else {
                                    draw = pixelProgram(attr, cam, wall_texid, x, y, &color);
                                }

                                if (draw) {
                                    setPixel(x, y, color);
                                }
                            }
                        }

                        // bottom step
                        for (int y = end_ny; y < end_y; ++y) {
                            int depth_index = x + y * SCREEN_WIDTH;

                            g_depth_buffer[depth_index] = depth;
                            if (!g_render_occlusion) {
                                float ty     = (float)(y - start_y_real) / (end_y_real - start_y_real);
                                float v      = lerp(attr[1].uv[1], attr[0].uv[1], ty);
                                world_pos[2] = lerp(attr[1].world_pos[2], attr[0].world_pos[2], ty);


                                WallAttribute attr = {
                                    .uv        = { u, v },
                                    .world_pos = { world_pos[0], world_pos[1], world_pos[2] },
                                    .normal    = { wall_norm[0], wall_norm[1], 0.0f },
                                };
                                Color color;

                                bool draw;
                                if (wall_is_sky) {
                                    draw = skyPixelProgram(attr, cam, wall_texid, x, y, &color);
                                } else {
                                    draw = pixelProgram(attr, cam, wall_texid, x, y, &color);
                                }

                                if (draw) {
                                    setPixel(x, y, color);
                                }
                            }
                        }
                    }
                }
#endif
            }
        }
        last_tier = false;


        if (g_render_occlusion) {

            for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
                // for (unsigned y = 0; y < window_low[x]; ++y) {
                //     setPixel(x, y, COLOR_BLACK);
                // }

                for (unsigned y = window_low[x]; y < window_high[x]; ++y) {
                    Color c = getPixel(x, y);
                    setPixel(x, y, RGB(c.r + 128, c.g + 64, c.b + 32));
                }

                // for (unsigned y = window_high[x]; y < SCREEN_HEIGHT; ++y) {
                //     setPixel(x, y, COLOR_BLACK);
                // }
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

    attr[clip_index].uv[0]        = lerp(attr[0].uv[0], attr[1].uv[0], t);
    attr[clip_index].world_pos[0] = lerp(attr[0].world_pos[0], attr[1].world_pos[0], t);
    attr[clip_index].world_pos[1] = lerp(attr[0].world_pos[1], attr[1].world_pos[1], t);

    // wall->uv_coords[clip_index][0] = lerp(wall->uv_coords[0][0], wall->uv_coords[1][0], t);

    return true;
}