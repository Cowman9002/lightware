#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include "portals.h"
#include "color.h"
#include "draw.h"
#include "util.h"

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define WORLD_SCALE 5.0f

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

uint16_t *g_depth_buffer;

Image g_image_array[3];
Image g_sky_image_array[1];

bool g_render_occlusion = false;

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
    g_depth_buffer = depth_buffer;

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

    bool render_depth   = false;
    bool render_map     = false;
    bool render_overlay = false;

    Image main_font;
    const unsigned MAIN_FONT_CHAR_WIDTH = 16;
    if (!readPng("res/fonts/vhs.png", &main_font)) return -1;

    if (!readPng("res/textures/wall.png", &g_image_array[0])) return -1;
    if (!readPng("res/textures/floor.png", &g_image_array[1])) return -1;
    if (!readPng("res/textures/ceiling.png", &g_image_array[2])) return -1;
    if (!readPng("res/textures/MUNSKY01.png", &g_sky_image_array[0])) return -1;

    char print_buffer[128];

    Camera cam;
    cam.sector = -1;
    cam.pos[0] = 0.0f;
    cam.pos[1] = 0.0f;
    cam.pos[2] = 1.65f;
    cam.rot    = 0.0f;
    cam.pitch  = 0.0f;
    cam.fov    = 90.0f * TO_RADS;

    mat3 view_mat;

    PortalWorld pod;
    if (!loadWorld("res/maps/map0.map", &pod, WORLD_SCALE)) return -3;

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
            if (keys[SDL_SCANCODE_L] && !last_keys[SDL_SCANCODE_L]) {
                PortalWorld pod2;
                if (loadWorld("res/maps/map0.map", &pod2, WORLD_SCALE)) {
                    freeWorld(pod);
                    pod = pod2;
                    printf("Reloaded world\n");
                } else {
                    printf("Failed to reload world\n");
                }
            }

            if (keys[SDL_SCANCODE_P] && !last_keys[SDL_SCANCODE_P]) {
                render_depth = !render_depth;
            }

            if (keys[SDL_SCANCODE_O] && !last_keys[SDL_SCANCODE_O]) {
                g_render_occlusion = !g_render_occlusion;
            }

            if (keys[SDL_SCANCODE_M] && !last_keys[SDL_SCANCODE_M]) {
                render_map = !render_map;
            }

            if (keys[SDL_SCANCODE_X] && !last_keys[SDL_SCANCODE_X]) {
                render_overlay = !render_overlay;
            }

            float input_h   = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
            float input_v   = keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W];
            float input_z   = keys[SDL_SCANCODE_SPACE] - keys[SDL_SCANCODE_LSHIFT];
            float input_fov = keys[SDL_SCANCODE_Q] - keys[SDL_SCANCODE_Z];
            float input_p   = keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN];
            float input_r   = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];

            cam.rot += input_r * delta * 2.0f;
            cam.rot_cos = cosf(cam.rot);
            cam.rot_sin = sinf(cam.rot);

            cam.pitch += input_p * delta;
            cam.pitch = clamp(cam.pitch, -1.0f, 1.0f);

            cam.fov += input_fov * delta * 0.5f;
            cam.fov = clamp(cam.fov, 30.0f * TO_RADS, 120.0f * TO_RADS);

            cam.forward[0] = cam.rot_sin;
            cam.forward[1] = -cam.rot_cos;
            cam.forward[2] = atanf(cam.pitch);
            normalize3d(cam.forward);

            vec2 movement = { cam.rot_cos * input_h + -cam.rot_sin * input_v, cam.rot_sin * input_h + cam.rot_cos * input_v };

            vec2 new_pos = {
                cam.pos[0] + movement[0] * delta * 10.0f,
                cam.pos[1] + movement[1] * delta * 10.0f,
            };

            // player collision
            if (cam.sector < pod.num_sectors) {
                SectorDef sector = pod.sectors[cam.sector];
                float t;

                for (unsigned i = 0; i < sector.length; ++i) {
                    Line wall_line     = pod.wall_lines[sector.start + i];
                    unsigned wall_next = pod.wall_nexts[sector.start + i];
                    if (wall_next < pod.num_sectors) continue; // TODO: step height

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

                    Line movement_line = {
                        .points = {
                            { cam.pos[0], cam.pos[1] },
                            { new_pos[0] - wall_norm[0], new_pos[1] - wall_norm[1] },
                        },
                    };

                    if (intersectSegmentSegment(movement_line.points, wall_line.points, &t)) {
                        vec2 intersect_point = {
                            lerp(movement_line.points[0][0], movement_line.points[1][0], t),
                            lerp(movement_line.points[0][1], movement_line.points[1][1], t),
                        };
                        new_pos[0] = intersect_point[0] + wall_norm[0];
                        new_pos[1] = intersect_point[1] + wall_norm[1];
                    }
                }
            }

            cam.pos[0] = new_pos[0];
            cam.pos[1] = new_pos[1];
            cam.pos[2] += input_z * delta * 10.0f;

            // printf("%f, %f\n", cam.pos[0], cam.pos[1]);

            cam.sector = getCurrentSector(pod, cam.pos, cam.sector);
            cam.tier   = getSectorTier(pod, cam.pos[2], cam.sector);

            if (cam.sector < pod.num_sectors) {
                if (cam.tier >= pod.sectors[cam.sector].num_tiers) cam.tier = 0;

                // cam.pos[2] = pod.sectors[cam.sector].floor_heights[0] + 1.65f;
                // cam.pos[2] = pod.sectors[cam.sector].floor_height + 0.5f;

                float input_ceil  = keys[SDL_SCANCODE_R] - keys[SDL_SCANCODE_E];
                float input_floor = keys[SDL_SCANCODE_Y] - keys[SDL_SCANCODE_T];

                pod.sectors[cam.sector].ceiling_heights[cam.tier] += input_ceil * delta;
                pod.sectors[cam.sector].floor_heights[cam.tier] += input_floor * delta;
            } else {
                cam.sector = 0;
                cam.tier   = 0;
            }
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
            setPixelI(i, RGB(0, 0, 0));
            depth_buffer[i] = ~0;
        }

        renderPortalWorld(pod, cam);

        if (render_depth) {
            for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
                float z = depth_buffer[i] * 255.0f / (uint16_t)~0;
                setPixelI(i, RGB(z, z, z));
            }
        }

        if (render_map) { // Render map overlay
            for (unsigned i = 0; i < pod.num_sectors; ++i) {
                SectorDef sector = pod.sectors[i];

                for (unsigned k = 0; k < sector.length; ++k) {
                    vec2 wall_points[2];
                    wall_points[0][0]  = pod.wall_lines[sector.start + k].points[0][0];
                    wall_points[0][1]  = pod.wall_lines[sector.start + k].points[0][1];
                    wall_points[1][0]  = pod.wall_lines[sector.start + k].points[1][0];
                    wall_points[1][1]  = pod.wall_lines[sector.start + k].points[1][1];
                    unsigned wall_next = pod.wall_nexts[sector.start + k];

                    vec2 world_space[2];
                    vec2 view_tspace[2];
                    vec2 view_space[2];
                    vec2 screen_pos[2];

                    for (unsigned j = 0; j < 2; ++j) {
                        world_space[j][0] = wall_points[j][0];
                        world_space[j][1] = wall_points[j][1];
                    }

                    // mat3MulVec2(view_mat, world_space[0], view_space[0]);
                    // mat3MulVec2(view_mat, world_space[1], view_space[1]);
                    for (unsigned pi = 0; pi < 2; ++pi) {
                        // translate by negative cam_pos
                        view_tspace[pi][0] = world_space[pi][0] - cam.pos[0];
                        view_tspace[pi][1] = world_space[pi][1] - cam.pos[1];

                        // rotate by negative cam_rot
                        // Normal:
                        //  cos -sin
                        //  sin  cos
                        view_space[pi][0] = cam.rot_cos * view_tspace[pi][0] + cam.rot_sin * view_tspace[pi][1];
                        view_space[pi][1] = -cam.rot_sin * view_tspace[pi][0] + cam.rot_cos * view_tspace[pi][1];
                    }

                    for (unsigned j = 0; j < 2; ++j) {
                        screen_pos[j][0] = view_space[j][0] + SCREEN_WIDTH * 0.5f;
                        screen_pos[j][1] = view_space[j][1] + SCREEN_HEIGHT * 0.5f;
                    }

                    drawLine(screen_pos[0][0], screen_pos[0][1], screen_pos[1][0], screen_pos[1][1], wall_next < pod.num_sectors ? COLOR_PURPLE : COLOR_WHITE);
                    setPixel(screen_pos[0][0], screen_pos[0][1], COLOR_BLACK);
                    setPixel(screen_pos[1][0], screen_pos[1][1], COLOR_BLACK);
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

        if (render_overlay && cam.sector < pod.num_sectors) {
            snprintf(print_buffer, sizeof(print_buffer), "SECTOR: %i", cam.sector);
            renderText(print_buffer, 1, 1, COLOR_BLACK, main_font, MAIN_FONT_CHAR_WIDTH);
            renderText(print_buffer, 0, 0, COLOR_WHITE, main_font, MAIN_FONT_CHAR_WIDTH);

            snprintf(print_buffer, sizeof(print_buffer), "CEIL: %f", pod.sectors[cam.sector].ceiling_heights[cam.tier]);
            renderText(print_buffer, 1, 24 + 1, COLOR_BLACK, main_font, MAIN_FONT_CHAR_WIDTH);
            renderText(print_buffer, 0, 24, COLOR_WHITE, main_font, MAIN_FONT_CHAR_WIDTH);

            snprintf(print_buffer, sizeof(print_buffer), "FLOOR: %f", pod.sectors[cam.sector].floor_heights[cam.tier]);
            renderText(print_buffer, 1, 24 * 2 + 1, COLOR_BLACK, main_font, MAIN_FONT_CHAR_WIDTH);
            renderText(print_buffer, 0, 24 * 2, COLOR_WHITE, main_font, MAIN_FONT_CHAR_WIDTH);

            snprintf(print_buffer, sizeof(print_buffer), "PITCH: %f", cam.pitch);
            renderText(print_buffer, 1, 24 * 3 + 1, COLOR_BLACK, main_font, MAIN_FONT_CHAR_WIDTH);
            renderText(print_buffer, 0, 24 * 3, COLOR_WHITE, main_font, MAIN_FONT_CHAR_WIDTH);
        }

        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:
    freeWorld(pod);

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
