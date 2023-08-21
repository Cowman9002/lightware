#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#include "color.h"
#include "draw.h"

// https://benedicthenshaw.com/soft_render_sdl2.html

#define min(a, b) ((a < b) ? a : b)
#define max(a, b) ((a > b) ? a : b)

typedef float vec2[2];

typedef struct Image {
    Color *data;
    int width, height;
} Image;

typedef struct Camera {
    vec2 pos;
    float rot;
    float rot_cos;
    float rot_sin;
} Camera;

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, int x, int y);

bool intersectSegmentLine(vec2 line0[2], vec2 line1[2], vec2 o_point);

float cross2d(vec2 a, vec2 b) {
    return a[0] * b[1] - a[1] * b[0];
}

void renderWall(vec2 point_buffer[2], Camera cam);

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

uint32_t *g_pixels = NULL;

int main(int argc, char *argv[]) {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH * PIXEL_SIZE, SCREEN_HEIGHT * PIXEL_SIZE,
                                          SDL_WINDOW_RESIZABLE);

    // Create a renderer with V-Sync enabled.
    SDL_Renderer *renderer = SDL_CreateRenderer(window,
                                                -1, SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawColor(renderer, 15, 5, 20, 255);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture *screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                                    SCREEN_WIDTH, SCREEN_HEIGHT);

    const int8_t *keys = (const int8_t *)SDL_GetKeyboardState(NULL);

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
    cam.pos[1] = -20.0f;
    cam.rot = 0.0f;

    float points[][2] = {
        { -10.0, -10.0 },
        { -10.0, 10.0 },
        { 10.0, 10.0 },
        { 10.0, -10.0 },
    };

    unsigned walls[][2] = {
        { 0, 1 },
        { 1, 2 },
        { 2, 3 },
        { 3, 0 },
    };
    unsigned num_walls = sizeof(walls) / sizeof(walls[0]);

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    while (1) {

        ticks      = SDL_GetTicks64();
        delta      = (float)(ticks - last_ticks) / 1000.0f;
        last_ticks = ticks;

        if (ticks >= next_fps_print) {
            next_fps_print += 1000;
            printf("FPS: %u\n", frame - last_fps_frame);
            last_fps_frame = frame;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) goto _success_exit;
        }

        {
            int input_x   = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
            int input_y   = keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W];
            int input_rot = keys[SDL_SCANCODE_LEFT] - keys[SDL_SCANCODE_RIGHT];

            cam.rot += input_rot * 0.05;
            cam.rot_sin = sinf(cam.rot);
            cam.rot_cos = cosf(cam.rot);

            cam.pos[0] += cam.rot_sin * input_y * 0.5;
            cam.pos[1] += -cam.rot_cos * input_y * 0.5;
            cam.pos[0] += cam.rot_cos * input_x * 0.5;
            cam.pos[1] += cam.rot_sin * input_x * 0.5;
        }

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)&g_pixels, &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
            g_pixels[i] = RGB(80, 10, 60);


        for (unsigned i = 0; i < num_walls; ++i) {
            float point_buffer[2][2] = {
                { points[walls[i][0]][0], points[walls[i][0]][1] },
                { points[walls[i][1]][0], points[walls[i][1]][1] },
            };

            renderWall(point_buffer, cam);
        }

        // Map
        {
            for (unsigned i = 0; i < num_walls; ++i) {
                float point_buffer[2][2] = {
                    { points[walls[i][0]][0], points[walls[i][0]][1] },
                    { points[walls[i][1]][0], points[walls[i][1]][1] },
                };

                vec2 view_buffer[2];
                view_buffer[0][0] = point_buffer[0][0] - cam.pos[0];
                view_buffer[0][1] = point_buffer[0][1] - cam.pos[1];
                view_buffer[1][0] = point_buffer[1][0] - cam.pos[0];
                view_buffer[1][1] = point_buffer[1][1] - cam.pos[1];

                float tmp         = view_buffer[0][0];
                view_buffer[0][0] = cam.rot_cos * view_buffer[0][0] + cam.rot_sin * view_buffer[0][1];
                view_buffer[0][1] = -cam.rot_sin * tmp + cam.rot_cos * view_buffer[0][1];

                tmp               = view_buffer[1][0];
                view_buffer[1][0] = cam.rot_cos * view_buffer[1][0] + cam.rot_sin * view_buffer[1][1];
                view_buffer[1][1] = -cam.rot_sin * tmp + cam.rot_cos * view_buffer[1][1];

                point_buffer[0][0] = view_buffer[0][0] + SCREEN_WIDTH / 2;
                point_buffer[0][1] = -view_buffer[0][1] + SCREEN_HEIGHT / 2;
                point_buffer[1][0] = view_buffer[1][0] + SCREEN_WIDTH / 2;
                point_buffer[1][1] = -view_buffer[1][1] + SCREEN_HEIGHT / 2;

                // point_buffer[0][0] = point_buffer[0][0] + SCREEN_WIDTH / 2;
                // point_buffer[0][1] = -point_buffer[0][1] + SCREEN_HEIGHT / 2;
                // point_buffer[1][0] = point_buffer[1][0] + SCREEN_WIDTH / 2;
                // point_buffer[1][1] = -point_buffer[1][1] + SCREEN_HEIGHT / 2;

                drawLine(point_buffer[0][0], point_buffer[0][1], point_buffer[1][0], point_buffer[1][1], COLOR_WHITE);
            }

            // float ppos_x      = cam.pos[0] + SCREEN_WIDTH / 2;
            // float ppos_y      = -cam.pos[1] + SCREEN_HEIGHT / 2;
            // float view_line_x = -cam.rot_sin;
            // float view_line_y = -cam.rot_cos;

            // float fov_line_x = -cam.rot_sin + cam.rot_cos;
            // float fov_line_y = -cam.rot_cos - cam.rot_sin;

            // drawLine(ppos_x, ppos_y, ppos_x + view_line_x * 10.0f, ppos_y + view_line_y * 10.0f, COLOR_GREEN);

            // drawLine(ppos_x, ppos_y, ppos_x + fov_line_x * 10.0f, ppos_y + fov_line_y * 10.0f, COLOR_BLUE);
            // drawLine(ppos_x, ppos_y, ppos_x + fov_line_y * 10.0f, ppos_y - fov_line_x * 10.0f, COLOR_BLUE);

            // drawPoint(ppos_x, ppos_y, COLOR_RED);

            float ppos_x      = SCREEN_WIDTH / 2;
            float ppos_y      = SCREEN_HEIGHT / 2;
            float view_line_x = 0.0f;
            float view_line_y = -1.0f;
            drawLine(ppos_x, ppos_y, ppos_x + view_line_x * 10.0f, ppos_y + view_line_y * 10.0f, COLOR_GREEN);
            drawPoint(ppos_x, ppos_y, COLOR_RED);
        }

        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:
    // free(player_image.data);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Exit successful\n");
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

Color sampleImage(Image image, int x, int y) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return (Color){};
    Color res;
    unsigned i = x + y * image.width;
    res        = image.data[i];
    return res;
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

void renderWall(vec2 point_buffer[2], Camera cam) {
    // apply view
    vec2 view_buffer[2];
    view_buffer[0][0] = point_buffer[0][0] - cam.pos[0];
    view_buffer[0][1] = point_buffer[0][1] - cam.pos[1];
    view_buffer[1][0] = point_buffer[1][0] - cam.pos[0];
    view_buffer[1][1] = point_buffer[1][1] - cam.pos[1];

    float tmp         = view_buffer[0][0];
    view_buffer[0][0] = cam.rot_cos * view_buffer[0][0] + cam.rot_sin * view_buffer[0][1];
    view_buffer[0][1] = -cam.rot_sin * tmp + cam.rot_cos * view_buffer[0][1];

    tmp               = view_buffer[1][0];
    view_buffer[1][0] = cam.rot_cos * view_buffer[1][0] + cam.rot_sin * view_buffer[1][1];
    view_buffer[1][1] = -cam.rot_sin * tmp + cam.rot_cos * view_buffer[1][1];

    // clip right view plane
    if (!clipWall((vec2){ 0.0f, 0.0f }, (vec2){ 1.0f, 1.0f }, view_buffer[0], view_buffer[1])) return;
    // clip left view plane
    if (!clipWall((vec2){ -1.0f, 1.0f }, (vec2){ 0.0f, 0.0f }, view_buffer[0], view_buffer[1])) return;
    // clip near view plane
    if (!clipWall((vec2){ -1.0f, 1.0f }, (vec2){ 1.0f, 1.0f }, view_buffer[0], view_buffer[1])) return;

    // apply projection (-1 to 1 space)
    float proj_buffer[2][2];
    proj_buffer[0][1] = 1.0f / view_buffer[0][1];
    proj_buffer[1][1] = 1.0f / view_buffer[1][1];
    proj_buffer[0][0] = view_buffer[0][0] * proj_buffer[0][1];
    proj_buffer[1][0] = view_buffer[1][0] * proj_buffer[1][1];

    // convert to screen coordinates
    float screen_buffer[2][2];
    screen_buffer[0][0] = (proj_buffer[0][0] * 0.5 + 0.5) * SCREEN_WIDTH;
    screen_buffer[1][0] = (proj_buffer[1][0] * 0.5 + 0.5) * SCREEN_WIDTH;

    screen_buffer[0][1] = proj_buffer[0][1] * SCREEN_HEIGHT;
    screen_buffer[1][1] = proj_buffer[1][1] * SCREEN_HEIGHT;

    float start_y0 = (SCREEN_HEIGHT - screen_buffer[0][1]) / 2;
    float start_y1 = (SCREEN_HEIGHT - screen_buffer[1][1]) / 2;

    drawLine(screen_buffer[0][0], start_y0, screen_buffer[1][0], start_y1, COLOR_YELLOW);
    drawLine(screen_buffer[0][0], SCREEN_HEIGHT - start_y0, screen_buffer[1][0], SCREEN_HEIGHT - start_y1, COLOR_YELLOW);
}
