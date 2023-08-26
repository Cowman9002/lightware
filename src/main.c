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

#define PIXELS_PER_UNIT 8.0

// https://benedicthenshaw.com/soft_render_sdl2.html

typedef struct Camera {
    vec3 pos;
    float rot;
    float rot_sin, rot_cos;
} Camera;

bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], vec2 o_point);
bool clipWall(vec2 plane0, vec2 plane1, vec2 p0, vec2 p1);

int main(int argc, char *argv[]) {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("",
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

    const int8_t *keys = (const int8_t *)SDL_GetKeyboardState(NULL);
    int8_t *last_keys  = malloc(SDL_NUM_SCANCODES);

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

    Camera cam;
    cam.pos[0] = 0.0f;
    cam.pos[0] = 0.0f;
    cam.pos[2] = 10.0f;
    cam.rot    = 0.0f;

    mat3 view_mat;

    vec2 points[] = {
        { 10.0f, -10.0f },
        { 10.0f, 10.0f },
        { -10.0f, 10.0f },
        { -10.0f, -10.0f },

        { -20.0f, -20.0f },
        { -20.0f, -30.0f },
        { 5.0f, -50.0f },
        { 40.0f, -20.0f },
    };

    // clang-format off
    unsigned walls[] = {
        0, 1,
        1, 2,
        2, 3,
        3, 4,
        4, 5,
        5, 6,
        6, 7,
        7, 0,
    };
    // clang-format on
    unsigned num_walls = sizeof(walls) / sizeof(*walls) / 2;

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
            // setPixelI(i, RGB(80, 10, 60));
            setPixelI(i, RGB(0, 0, 0));
        }

        { // Render walls
            for(unsigned i = 0; i < num_walls; ++i) {
                vec2 world_space[2];
                vec2 view_space[2];
                vec2 proj_space[2];

                // World space
                for(unsigned j = 0; j < 2; ++j) {
                    world_space[j][0] = points[walls[i * 2 + j]][0];
                    world_space[j][1] = points[walls[i * 2 + j]][1];
                }

                // View space
                mat3MulVec2(view_mat, world_space[0], view_space[0]);
                mat3MulVec2(view_mat, world_space[1], view_space[1]);

                // Clip
                if(!clipWall(VEC2(0.0f, 0.0f), VEC2(-1.0f, 1.0f), view_space[0], view_space[1])) continue;
                if(!clipWall(VEC2(1.0f, 1.0f), VEC2(0.0f, 0.0f), view_space[0], view_space[1])) continue;
                if(!clipWall(VEC2(1.0f, -0.3f), VEC2(-1.0f, -0.3f), view_space[0], view_space[1])) continue;

                // Project
                for(unsigned j = 0; j < 2; ++j) {
                    proj_space[j][1] = 1.0f / fabsf(view_space[j][1]);
                    proj_space[j][0] = view_space[j][0] * proj_space[j][1];
                }

                float start_wall_size = proj_space[0][1] * SCREEN_HEIGHT;
                float end_wall_size = proj_space[1][1] * SCREEN_HEIGHT;

                float start_x = (proj_space[0][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
                float end_x = (proj_space[1][0] * 0.5f + 0.5f) * SCREEN_WIDTH;
                if(start_x > end_x) {
                    float t = start_x;
                    start_x = end_x;
                    end_x = t;

                    t = start_wall_size;
                    start_wall_size = end_wall_size;
                    end_wall_size = t;
                }

                for(float x = start_x; x < end_x; ++x) {
                    float t = (x - start_x) / (end_x - start_x);
                    float wall_height = lerp(start_wall_size, end_wall_size, t);
                    float start_y = (SCREEN_HEIGHT - wall_height) * 0.5f;
                    float end_y = SCREEN_HEIGHT - start_y;

                    for(float y = max(start_y, 0.0); y < min(end_y, SCREEN_HEIGHT); ++y) {
                        setPixel(x, y, COLOR_BLUE);
                    }
                }
            }
        }

        { // Render map overlay

            for(unsigned i = 0; i < num_walls; ++i) {
                vec2 world_space[2];
                vec2 view_space[2];
                vec2 screen_pos[2];

                for(unsigned j = 0; j < 2; ++j) {
                    world_space[j][0] = points[walls[i * 2 + j]][0];
                    world_space[j][1] = points[walls[i * 2 + j]][1];
                }

                mat3MulVec2(view_mat, world_space[0], view_space[0]);
                mat3MulVec2(view_mat, world_space[1], view_space[1]);

                // for(unsigned j = 0; j < 2; ++j) {
                //     screen_pos[j][0] = points[walls[i * 2 + j]][0] + SCREEN_WIDTH * 0.5f;
                //     screen_pos[j][1] = points[walls[i * 2 + j]][1] + SCREEN_HEIGHT * 0.5f;
                // }
                for(unsigned j = 0; j < 2; ++j) {
                    screen_pos[j][0] = view_space[j][0] + SCREEN_WIDTH * 0.5f;
                    screen_pos[j][1] = view_space[j][1] + SCREEN_HEIGHT * 0.5f;
                }

                drawLine(screen_pos[0][0], screen_pos[0][1], screen_pos[1][0], screen_pos[1][1], COLOR_WHITE);
                setPixel(screen_pos[0][0], screen_pos[0][1], COLOR_GREEN);
                setPixel(screen_pos[1][0], screen_pos[1][1], COLOR_GREEN);
            }

            // vec2 screen_cam_pos = { cam.pos[0], cam.pos[1] };
            vec2 screen_cam_pos = { 0.0f, 0.0f };
            screen_cam_pos[0] += SCREEN_WIDTH * 0.5f;
            screen_cam_pos[1] += SCREEN_HEIGHT * 0.5f;

            // drawLine(screen_cam_pos[0], screen_cam_pos[1], screen_cam_pos[0] + cam.rot_sin * 4.0f, screen_cam_pos[1] + -cam.rot_cos * 4.0f, COLOR_YELLOW);
            drawLine(screen_cam_pos[0], screen_cam_pos[1], screen_cam_pos[0] + 0.0f, screen_cam_pos[1] + -4.0f, COLOR_YELLOW);
            setPixel(screen_cam_pos[0], screen_cam_pos[1], COLOR_RED);
        }

        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:
    free(last_keys);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Exit successful\n");

    return EXIT_SUCCESS;
}

// returns false if entire wall is clipped
bool clipWall(vec2 plane0, vec2 plane1, vec2 p0, vec2 p1) {
    bool inside[2];

    vec2 plane  = { plane1[0] - plane0[0], plane1[1] - plane0[1] };
    vec2 p0_vec = { p0[0] - plane0[0], p0[1] - plane0[1] };
    vec2 p1_vec = { p1[0] - plane0[0], p1[1] - plane0[1] };

    inside[0] = cross2d(plane, p0_vec) >= 0.0;
    inside[1] = cross2d(plane, p1_vec) >= 0.0;

    if (!inside[0] && !inside[1])
        return false;
    else if (inside[0] && inside[1])
        return true;
    else {
        vec2 clip_point;
        if (intersectSegmentLine((vec2[2]){ { p0[0], p0[1] }, { p1[0], p1[1] } }, (vec2[2]){ { plane0[0], plane0[1] }, { plane1[0], plane1[1] } }, clip_point)) {
            if (!inside[0]) {
                memcpy(p0, clip_point, sizeof(vec2));
            } else {
                memcpy(p1, clip_point, sizeof(vec2));
            }
        }
        return true;
    }
}

bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], vec2 o_point) {
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

    o_point[0] = x1 + t * (x2 - x1);
    o_point[1] = y1 + t * (y2 - y1);

    return true;
}