#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include "color.h"
#include "draw.h"
#include "util.h"

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>


#ifdef MEMDEBUG
void *d_malloc(size_t s) {
    void *res = malloc(s);
    printf("[MEM] Malloc %p\n", res);
    return res;
}

void *d_calloc(size_t n, size_t s) {
    void *res = calloc(n, s);
    printf("[MEM] Calloc %p\n", res);
    return res;
}

void d_free(void *mem) {
    printf("[MEM] Free %p\n", mem);
    free(mem);
}

#define malloc(m) d_malloc(m)
#define calloc(m) d_calloc(m)
#define free(m) d_free(m)
#endif

#define FAR_PLANE 100

// https://benedicthenshaw.com/soft_render_sdl2.html

typedef struct Camera {
    int sector;
    vec3 pos;
    float rot;
    float rot_sin, rot_cos;
} Camera;

typedef struct WallDef {
    vec2 points[2];
    int next;
} WallDef;

typedef struct SectorDef {
    unsigned start, length;
} SectorDef;

typedef struct Image {
    Color *data;
    int width, height;
} Image;

bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], float *o_t);
bool intersectSegmentRay(vec2 line[2], vec2 ray[2], float *o_t);
bool solveClipping(vec2 plane0, vec2 plane1, vec2 p0, vec2 p1, float *o_t, int *o_clip_index);

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, unsigned x, unsigned y);

bool pointInPoly(WallDef *walls, unsigned num_walls, vec2 point) {
    vec2 ray[2] = { { point[0], point[1] }, { point[0] - 1.0f, point[1] } };
    vec2 line[2];
    float t;

    unsigned num_intersections = 0;

    // Point in polygon casts ray to left and counts number of intersections, odd inside, even out

    for (unsigned i = 0; i < num_walls; ++i) {
        line[0][0] = walls[i].points[0][0];
        line[0][1] = walls[i].points[0][1];
        line[1][0] = walls[i].points[1][0];
        line[1][1] = walls[i].points[1][1];

        if (intersectSegmentRay(line, ray, &t)) {
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

void renderText(const char *text, int draw_x, int draw_y, Color draw_color, Image font, unsigned char_width) {
    int x_offset = 0;

    for (const char *c = text; *c != '\0'; ++c) {
        if (*c >= ' ' && *c <= '~') {
            unsigned index = *c - ' ';

            for (unsigned y = 0; y < font.height; ++y) {
                for (unsigned x = 0; x < char_width; ++x) {
                    Color color = sampleImage(font, x + index * char_width, y);

                    if (color.r != 0) {
                        setPixel(draw_x + x + x_offset, draw_y + y, draw_color);
                    }
                }
            }

            x_offset += char_width;
        }
    }
}

typedef struct WallDraw {
    vec2 pos[2];
    vec2 uv_coords[2];
} WallDraw;

bool clipWall(vec2 clip_plane[2], WallDraw *wall) {
    int clip_index = 0;
    float t;

    if (!solveClipping(clip_plane[0], clip_plane[1], wall->pos[0], wall->pos[1], &t, &clip_index)) {
        return false;
    } else if (clip_index >= 0) {
        wall->pos[clip_index][0] = lerp(wall->pos[0][0], wall->pos[1][0], t);
        wall->pos[clip_index][1] = lerp(wall->pos[0][1], wall->pos[1][1], t);

        wall->uv_coords[clip_index][0] = lerp(wall->uv_coords[0][0], wall->uv_coords[1][0], t);
    }

    return true;
}

int main(int argc, char *argv[]) {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Lightware",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH * PIXEL_SIZE, SCREEN_HEIGHT * PIXEL_SIZE,
                                          SDL_WINDOW_RESIZABLE);

    // Create a renderer with V-Sync enabled.
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0); //SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawColor(renderer, 15, 5, 20, 255);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture *screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                                    SCREEN_WIDTH, SCREEN_HEIGHT);

    uint16_t *const depth_buffer = (uint16_t *)malloc(SCREEN_HEIGHT * SCREEN_WIDTH * sizeof(*depth_buffer));
    if (depth_buffer == NULL) return -2;
    bool render_depth = false;

    const int8_t *keys      = (const int8_t *)SDL_GetKeyboardState(NULL);
    int8_t *const last_keys = (int8_t *)malloc(SDL_NUM_SCANCODES * sizeof(*last_keys));
    if (last_keys == NULL) return -2;

    int pitch;
    unsigned int frame = 0;

    uint64_t ticks;
    uint64_t last_ticks = SDL_GetTicks64();
    float delta;

    uint64_t next_fps_print = 1000;
    unsigned last_fps_frame = 0;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    Image main_font;
    const unsigned MAIN_FONT_CHAR_WIDTH = 16;
    if (!readPng("res/fonts/vhs.png", &main_font)) return -1;

    char print_buffer[128];

    Camera cam;
    cam.sector = -1;
    cam.pos[0] = 0.0f;
    cam.pos[1] = 0.0f;
    cam.pos[2] = 0.0f;
    cam.rot    = 0.0f;

    mat3 view_mat;

    WallDef walls[] = {
        // Sector 0
        { { { 10.0f, -10.0f }, { 10.0f, 10.0f } }, -1 },
        { { { 10.0f, 10.0f }, { -10.0f, 10.0f } }, -1 },
        { { { -10.0f, 10.0f }, { -10.0f, -10.0f } }, -1 },
        { { { -10.0f, -10.0f }, { 10.0f, -10.0f } }, 1 },

        // Sector 1
        { { { -10.0f, -10.0f }, { -20.0f, -20.0f } }, -1 },
        { { { -20.0f, -20.0f }, { -20.0f, -30.0f } }, -1 },
        { { { -20.0f, -30.0f }, { 5.0f, -50.0f } }, -1 },
        { { { 5.0f, -50.0f }, { 40.0f, -20.0f } }, -1 },
        { { { 40.0f, -20.0f }, { 10.0f, -10.0f } }, -1 },
        { { { 10.0f, -10.0f }, { -10.0f, -10.0f } }, 0 },
    };

    SectorDef sectors[] = {
        { 0, 4 },
        { 4, 6 },
    };
    unsigned num_sectors = sizeof(sectors) / sizeof(*sectors);

    bool *d_sector_was_rendered = (bool *)malloc(num_sectors * sizeof(*d_sector_was_rendered));
    if (d_sector_was_rendered == NULL) return -2;

    int sector_queue[128]       = { -1 };
    unsigned sector_queue_start = 0, sector_queue_end = 0;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    while (1) {

        ticks      = SDL_GetTicks64();
        delta      = (float)(ticks - last_ticks) / 1000.0f;
        last_ticks = ticks;

        if (ticks >= next_fps_print) {
            next_fps_print += 1000;
            printf("FPS: %4u   MS: %f\n", frame - last_fps_frame, 1.0 / (frame - last_fps_frame));
            last_fps_frame = frame;
        }

        for (unsigned i = 0; i < SDL_NUM_SCANCODES; ++i) {
            last_keys[i] = keys[i];
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) goto _success_exit;
        }

        {
            if (keys[SDL_SCANCODE_P] && !last_keys[SDL_SCANCODE_P]) {
                render_depth = !render_depth;
            }

            float input_h = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
            float input_v = keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W];
            float input_z = keys[SDL_SCANCODE_Q] - keys[SDL_SCANCODE_Z];
            float input_r = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];

            cam.rot += input_r * delta * 1.0f;
            cam.rot_cos = cosf(cam.rot);
            cam.rot_sin = sinf(cam.rot);

            vec2 movement = { input_h * delta * 5.0f, input_v * delta * 5.0f };

            cam.pos[0] += cam.rot_cos * movement[0] + -cam.rot_sin * movement[1];
            cam.pos[1] += cam.rot_sin * movement[0] + cam.rot_cos * movement[1];
            cam.pos[2] += input_z * delta * 5.0f;
            cam.pos[2] = clamp(cam.pos[2], 0.1f, 50.0f);

            // printf("%f, %f\n", cam.pos[0], cam.pos[1]);
        }

        mat3 cam_translation, cam_rotation;
        mat3Translate(VEC2(-cam.pos[0], -cam.pos[1]), cam_translation);
        mat3Rotate(-cam.rot, cam_rotation);
        mat3Mul(cam_rotation, cam_translation, view_mat);

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)getPixelBufferPtr(), &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
            // setPixelI(i, RGB(0, 0, 0));
            depth_buffer[i] = ~0;
        }

        { // search for sector
            if (cam.sector >= 0) {
                // look at current sector
                SectorDef sector = sectors[cam.sector];
                {
                    WallDef *test_walls = &walls[sector.start];
                    if (pointInPoly(test_walls, sector.length, cam.pos)) {
                        goto _end_of_sector_search;
                    }
                }

                // neighbors
                for (unsigned i = 0; i < sector.length; ++i) {
                    WallDef wall = walls[sector.start + i];
                    if (wall.next >= 0) {
                        SectorDef n_sector  = sectors[wall.next];
                        WallDef *test_walls = &walls[sector.start];
                        if (pointInPoly(test_walls, n_sector.length, cam.pos)) {
                            cam.sector = wall.next;
                            goto _end_of_sector_search;
                        }
                    }
                }
            }

            // linear lookup
            for (unsigned s = 0; s < num_sectors; ++s) {
                SectorDef sector    = sectors[s];
                WallDef *test_walls = &walls[sector.start];
                if (pointInPoly(test_walls, sector.length, cam.pos)) {
                    cam.sector = s;
                    goto _end_of_sector_search;
                }
            }
        }
    _end_of_sector_search:
        if (cam.sector < 0) {
            printf("Sector is unknown\n");
        }

        sector_queue[sector_queue_end] = cam.sector;
        sector_queue_end               = (sector_queue_end + 1) % 128;

        { // Render walls
            for (unsigned i = 0; i < num_sectors; ++i) {
                d_sector_was_rendered[i] = false;
            }

            while (sector_queue_start != sector_queue_end) {
                unsigned sector_index = sector_queue[sector_queue_start];
                if (sector_index >= num_sectors) break;

                SectorDef sector   = sectors[sector_index];
                sector_queue_start = (sector_queue_start + 1) % 128;

                d_sector_was_rendered[sector_index] = true;

                for (unsigned i = 0; i < sector.length; ++i) {
                    WallDef wall = walls[sector.start + i];

                    vec2 world_space[2];
                    vec2 proj_space[2];

                    WallDraw wall_draw;

                    // World space
                    for (unsigned j = 0; j < 2; ++j) {
                        world_space[j][0] = wall.points[j][0];
                        world_space[j][1] = wall.points[j][1];
                    }

                    // Back face culling
                    {
                        float cross_val = cross2d(
                            VEC2(world_space[0][0] - world_space[1][0], world_space[0][1] - world_space[1][1]),
                            VEC2(world_space[0][0] - cam.pos[0], world_space[0][1] - cam.pos[1]));

                        if (cross_val < 0.0) {
                            goto _end_of_wall_draw;
                        }
                    }

                    float length;
                    {
                        vec2 diff = { world_space[0][0] - world_space[1][0], world_space[0][1] - world_space[1][1] };
                        length    = sqrtf(dot2d(diff, diff));
                    }

                    wall_draw.uv_coords[0][0] = 0.0f;
                    wall_draw.uv_coords[1][0] = length;

                    // View space
                    mat3MulVec2(view_mat, world_space[0], wall_draw.pos[0]);
                    mat3MulVec2(view_mat, world_space[1], wall_draw.pos[1]);

                    // Clip
                    vec2 clip_planes[4][2] = {
                        { { 0.0f, 0.0f }, { -1.0f, 1.0f } },
                        { { 1.0f, 1.0f }, { 0.0f, 0.0f } },
                        { { 1.0f, -0.3f }, { -1.0f, -0.3f } },
                        { { -1.0f, -FAR_PLANE }, { 1.0f, -FAR_PLANE } },
                    };

                    if (!clipWall(clip_planes[0], &wall_draw)) goto _end_of_wall_draw;
                    if (!clipWall(clip_planes[1], &wall_draw)) goto _end_of_wall_draw;

                    if (wall.next >= 0 && wall.next != sector_index) {
                        sector_queue[sector_queue_end] = wall.next;
                        sector_queue_end               = (sector_queue_end + 1) % 128;
                        // TODO: Remove for steps
                        goto _end_of_wall_draw;
                    }

                    if (!clipWall(clip_planes[2], &wall_draw)) goto _end_of_wall_draw;
                    if (!clipWall(clip_planes[3], &wall_draw)) goto _end_of_wall_draw;

                    // Project
                    for (unsigned j = 0; j < 2; ++j) {
                        proj_space[j][1] = -1.0f / wall_draw.pos[j][1];
                        proj_space[j][0] = wall_draw.pos[j][0] * proj_space[j][1];

                        // perpective correct mapping
                        wall_draw.uv_coords[j][0] *= proj_space[j][1];
                    }

                    float start_x = (proj_space[0][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
                    float end_x   = (proj_space[1][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
                    if (start_x > end_x) {
                        swap(float, start_x, end_x);
                        swap(float, proj_space[0][1], proj_space[1][1]);
                        swap(float, wall_draw.uv_coords[0][0], wall_draw.uv_coords[1][0]);
                    }

                    float start_wall_size = proj_space[0][1] * SCREEN_HEIGHT;
                    float end_wall_size   = proj_space[1][1] * SCREEN_HEIGHT;

                    for (float x = start_x; x < end_x; ++x) {
                        float t           = (x - start_x) / (end_x - start_x);
                        float wall_height = lerp(start_wall_size, end_wall_size, t);
                        float start_y     = (SCREEN_HEIGHT - wall_height) * 0.5f;
                        float end_y       = SCREEN_HEIGHT - start_y;

                        float z     = 1.0f / lerp(proj_space[0][1], proj_space[1][1], t);
                        float depth = z * (float)((uint16_t)~0) / FAR_PLANE;

                        // perspective correct mapping
                        float u = lerp(wall_draw.uv_coords[0][0], wall_draw.uv_coords[1][0], t);
                        u *= z;

                        start_y = max(start_y, 0.0);
                        end_y   = min(end_y, SCREEN_HEIGHT);

                        // ceiling
                        for (float y = 0; y < start_y; ++y) {
                            int depth_index = (int)x + (int)y * SCREEN_WIDTH;
                            if (depth < depth_buffer[depth_index]) {
                                setPixel(x, y, RGB(100, 128, 255));
                                depth_buffer[depth_index] = depth;
                            }
                        }

                        // wall
                        for (float y = start_y; y < end_y; ++y) {
                            int depth_index = (int)x + (int)y * SCREEN_WIDTH;
                            if (depth < depth_buffer[depth_index]) {
                                int modu = (int)u % 2;
                                setPixel(x, y, modu == 0 ? COLOR_BLUE : RGB(0, 0, 128));
                                depth_buffer[depth_index] = depth;
                            }
                        }

                        // floor
                        for (float y = end_y; y < SCREEN_HEIGHT; ++y) {
                            int depth_index = (int)x + (int)y * SCREEN_WIDTH;
                            if (depth < depth_buffer[depth_index]) {
                                setPixel(x, y, RGB(100, 200, 60));
                                depth_buffer[depth_index] = depth;
                            }
                        }
                    }

                _end_of_wall_draw:
                }
            }
        }

        { // Render map overlay
            for (unsigned i = 0; i < num_sectors; ++i) {
                SectorDef sector = sectors[i];

                for (unsigned k = 0; k < sector.length; ++k) {
                    WallDef wall = walls[sector.start + k];

                    vec2 world_space[2];
                    vec2 view_space[2];
                    vec2 screen_pos[2];

                    for (unsigned j = 0; j < 2; ++j) {
                        world_space[j][0] = wall.points[j][0];
                        world_space[j][1] = wall.points[j][1];
                    }

                    mat3MulVec2(view_mat, world_space[0], view_space[0]);
                    mat3MulVec2(view_mat, world_space[1], view_space[1]);

                    for (unsigned j = 0; j < 2; ++j) {
                        screen_pos[j][0] = view_space[j][0] + SCREEN_WIDTH * 0.5f;
                        screen_pos[j][1] = view_space[j][1] + SCREEN_HEIGHT * 0.5f;
                    }

                    drawLine(screen_pos[0][0], screen_pos[0][1], screen_pos[1][0], screen_pos[1][1], d_sector_was_rendered[i] ? COLOR_CYAN : COLOR_WHITE);
                    setPixel(screen_pos[0][0], screen_pos[0][1], COLOR_GREEN);
                    setPixel(screen_pos[1][0], screen_pos[1][1], COLOR_GREEN);
                }
            }

            // vec2 screen_cam_pos = { cam.pos[0], cam.pos[1] };
            vec2 screen_cam_pos = { 0.0f, 0.0f };
            screen_cam_pos[0] += SCREEN_WIDTH * 0.5f;
            screen_cam_pos[1] += SCREEN_HEIGHT * 0.5f;

            // drawLine(screen_cam_pos[0], screen_cam_pos[1], screen_cam_pos[0] + cam.rot_sin * 4.0f, screen_cam_pos[1] + -cam.rot_cos * 4.0f, COLOR_YELLOW);
            drawLine(screen_cam_pos[0], screen_cam_pos[1], screen_cam_pos[0] + 0.0f, screen_cam_pos[1] + -4.0f, COLOR_YELLOW);
            setPixel(screen_cam_pos[0], screen_cam_pos[1], COLOR_RED);
        }

        if (render_depth) {
            for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
                float z = depth_buffer[i] * 255.0f / (uint16_t)~0;
                setPixelI(i, RGB(z, z, z));
            }
        }

        {
            snprintf(print_buffer, sizeof(print_buffer), "SECTOR: %i", cam.sector);
            renderText(print_buffer, 0, 0, COLOR_WHITE, main_font, MAIN_FONT_CHAR_WIDTH);
        }

        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:
    free(d_sector_was_rendered);
    free(last_keys);
    free(depth_buffer);
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Exit successful\n");

    return EXIT_SUCCESS;
}

bool readPng(const char *path, Image *out) {
    assert(path != NULL);
    assert(out != NULL);

    unsigned error;
    unsigned char *data = 0;
    unsigned width, height;

    error = lodepng_decode32_file(&data, &width, &height, path);
    if (error) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
        return false;
    }

    out->data   = (Color *)data;
    out->width  = width;
    out->height = height;

    // lodepng reads as rgba, but we need abgr
    Color tmp;
    for (unsigned i = 0; i < width * height; ++i) {
        tmp            = out->data[i];
        out->data[i].r = tmp.a;
        out->data[i].g = tmp.b;
        out->data[i].b = tmp.g;
        out->data[i].a = tmp.r;
    }

    return true;
}

Color sampleImage(Image image, unsigned x, unsigned y) {
    if (x >= image.width || y >= image.height) return (Color){};
    Color res;
    unsigned i = x + y * image.width;
    res        = image.data[i];
    return res;
}


// returns false if entire wall is clipped
// clipping is not done in function
bool solveClipping(vec2 plane0, vec2 plane1, vec2 p0, vec2 p1, float *o_t, int *o_clip_index) {
    assert(o_t != NULL);
    assert(o_clip_index != NULL);

    bool inside[2];

    vec2 plane  = { plane1[0] - plane0[0], plane1[1] - plane0[1] };
    vec2 p0_vec = { p0[0] - plane0[0], p0[1] - plane0[1] };
    vec2 p1_vec = { p1[0] - plane0[0], p1[1] - plane0[1] };

    inside[0] = cross2d(plane, p0_vec) >= 0.0;
    inside[1] = cross2d(plane, p1_vec) >= 0.0;

    *o_t = 0.0;

    if (!inside[0] && !inside[1])
        return false;
    else if (inside[0] && inside[1]) {
        *o_clip_index = -1;
        return true;
    } else {
        if (intersectSegmentLine((vec2[2]){ { p0[0], p0[1] }, { p1[0], p1[1] } }, (vec2[2]){ { plane0[0], plane0[1] }, { plane1[0], plane1[1] } }, o_t)) {
            *o_clip_index = inside[0] ? 1 : 0;
        }
        return true;
    }
}

bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], float *o_t) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = line0[0][0];
    y1 = line0[0][1];
    x2 = line0[1][0];
    y2 = line0[1][1];
    x3 = line1[0][0];
    y3 = line1[0][1];
    x4 = line1[1][0];
    y4 = line1[1][1];

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float td = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);

    if (td == 0) return false;

    float t = tn / td;

    if (t < 0 || t > 1) return false;

    if (o_t != NULL) {
        // TODO Remove division when o_t is not needed
        *o_t = t;
    }

    return true;
}

bool intersectSegmentRay(vec2 line[2], vec2 ray[2], float *o_t) {
    float x1, x2, x3, x4;
    float y1, y2, y3, y4;
    x1 = line[0][0];
    y1 = line[0][1];
    x2 = line[1][0];
    y2 = line[1][1];
    x3 = ray[0][0];
    y3 = ray[0][1];
    x4 = ray[1][0];
    y4 = ray[1][1];

    float denom = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
    if (denom == 0) return false;

    float tn = (x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4);
    float un = (x1 - x3) * (y1 - y2) - (y1 - y3) * (x1 - x2);

    float t = tn / denom;
    float u = un / denom;

    // t is segment, u is ray.
    if (t < 0.0f || t > 1.0f || u < 0.0f) return false;

    if (o_t != NULL) {
        // TODO Remove division when o_t is not needed
        *o_t = t;
    }

    return true;
}