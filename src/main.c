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

typedef struct Image {
    Color *data;
    int width, height;
} Image;

bool solveClipping(vec2 plane0, vec2 plane1, vec2 p0, vec2 p1, float *o_t, int *o_clip_index);

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, unsigned x, unsigned y);

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
    cam.pos[1] = 4.0f;
    cam.pos[2] = 0.5f;
    cam.rot    = 0.0f;

    mat3 view_mat;

    PortalWorld pod = {
        .sectors = (SectorDef[]){
            (SectorDef){ 0, 4, 1.0f, 0.0f },
        },
        .num_sectors = 1,
        .wall_lines  = (Line[]){
            { { { -5.0f, -5.0f }, { 5.0f, -5.0f } } },
            { { { 5.0f, -5.0f }, { 5.0f, 5.0f } } },
            { { { 5.0f, 5.0f }, { -5.0f, 5.0f } } },
            { { { -5.0f, 5.0f }, { -5.0f, -5.0f } } },
        },
        .wall_nexts = (unsigned[]){ INVALID_SECTOR_INDEX, INVALID_SECTOR_INDEX, INVALID_SECTOR_INDEX, INVALID_SECTOR_INDEX },
        .num_walls  = 4,
    };

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

            cam.rot += input_r * delta * 2.0f;
            cam.rot_cos = cosf(cam.rot);
            cam.rot_sin = sinf(cam.rot);

            vec2 movement = { input_h * delta * 10.0f, input_v * delta * 10.0f };

            cam.pos[0] += cam.rot_cos * movement[0] + -cam.rot_sin * movement[1];
            cam.pos[1] += cam.rot_sin * movement[0] + cam.rot_cos * movement[1];
            // cam.pos[2] += input_z * delta * 5.0f;

            // printf("%f, %f\n", cam.pos[0], cam.pos[1]);

            cam.sector = getCurrentSector(pod, cam.pos, cam.sector);

            if (cam.sector != INVALID_SECTOR_INDEX) {
                cam.pos[2] = pod.sectors[cam.sector].floor_height + 0.5f;

                pod.sectors[cam.sector].ceiling_height += input_z * delta;
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

        { // Render map overlay
            for (unsigned i = 0; i < pod.num_sectors; ++i) {
                SectorDef sector = pod.sectors[i];

                for (unsigned k = 0; k < sector.length; ++k) {
                    vec2 wall_points[2];
                    wall_points[0][0] = pod.wall_lines[sector.start + k].points[0][0];
                    wall_points[0][1] = pod.wall_lines[sector.start + k].points[0][1];
                    wall_points[1][0] = pod.wall_lines[sector.start + k].points[1][0];
                    wall_points[1][1] = pod.wall_lines[sector.start + k].points[1][1];

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

                    drawLine(screen_pos[0][0], screen_pos[0][1], screen_pos[1][0], screen_pos[1][1], COLOR_WHITE);
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
