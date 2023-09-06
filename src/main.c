#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include "color.h"
#include "draw.h"
#include "mathlib.h"
#include "portal_world.h"

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// https://www.flipcode.com/archives/Building_a_3D_Portal_Engine-Issue_01_Introduction.shtml

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

#define _STRINGIFY(v) #v
#define STRINGIFY(v) _STRINGIFY(v)

const char *const TITLE_STRING = "LightWare " STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR);

void renderText(const char *text, int draw_x, int draw_y, Color draw_color, Image font, unsigned char_width);

Image g_image_array[3];
Image g_sky_image_array[1];

int main(int argc, char *argv[]) {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window     = SDL_CreateWindow("Lightware",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          SCREEN_WIDTH * PIXEL_SIZE, SCREEN_HEIGHT * PIXEL_SIZE,
                                          SDL_WINDOW_RESIZABLE);
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0); //SDL_RENDERER_PRESENTVSYNC);

    SDL_SetRenderDrawColor(renderer, 15, 5, 20, 255);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    SDL_RenderSetIntegerScale(renderer, SDL_TRUE);

    SDL_Texture *screen_texture = SDL_CreateTexture(renderer,
                                                    SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
                                                    SCREEN_WIDTH, SCREEN_HEIGHT);

    const int8_t *keys      = (const int8_t *)SDL_GetKeyboardState(NULL);
    int8_t *const last_keys = (int8_t *)malloc(SDL_NUM_SCANCODES * sizeof(*last_keys));
    if (last_keys == NULL) return -2;

    int pitch;
    unsigned int frame = 0;

    uint64_t ticks;
    uint64_t last_ticks = SDL_GetTicks64();
    float delta;
    float seconds;

    uint64_t next_fps_print = 1000;
    unsigned last_fps_frame = 0;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    Image main_font;
    if (!readPng("res/fonts/small.png", &main_font)) return -1;
    unsigned main_font_char_width = main_font.width / 95;

    if (!readPng("res/textures/wall.png", &g_image_array[0])) return -1;
    if (!readPng("res/textures/floor.png", &g_image_array[1])) return -1;
    if (!readPng("res/textures/ceiling.png", &g_image_array[2])) return -1;
    if (!readPng("res/textures/MUNSKY01.png", &g_sky_image_array[0])) return -1;

    char print_buffer[128];

    PortalWorld pod;
    SectorList_init(&pod.sectors);
    {
        Sector sector;
        sector.ceiling_height       = 5.0f;
        sector.floor_height         = 0.0f;
        sector.polygon.num_points   = 4;
        sector.polygon.points       = malloc(sector.polygon.num_points * sizeof(*sector.polygon.points));
        sector.polygon.planes       = malloc(sector.polygon.num_points * sizeof(*sector.polygon.planes));
        sector.polygon.next_sectors = malloc(sector.polygon.num_points * sizeof(*sector.polygon.next_sectors));

        for (unsigned i = 0; i < sector.polygon.num_points; ++i) {
            sector.polygon.next_sectors[i] = NULL;
        }

        sector.polygon.points[0][0] = -10.0f, sector.polygon.points[0][1] = -10.0f;
        sector.polygon.points[1][0] = 10.0f, sector.polygon.points[1][1] = -10.0f;
        sector.polygon.points[2][0] = 20.0f, sector.polygon.points[2][1] = 20.0f;
        sector.polygon.points[3][0] = -10.0f, sector.polygon.points[3][1] = 10.0f;

        // precalc the planes
        for (unsigned i = 0; i < sector.polygon.num_points; ++i) {
            unsigned j = (i + 1) % sector.polygon.num_points;
            vec3 p0 = { 0 }, p1 = { 0 };
            p0[0] = sector.polygon.points[i][0];
            p0[1] = sector.polygon.points[i][1];
            p1[0] = sector.polygon.points[j][0];
            p1[1] = sector.polygon.points[j][1];

            vec2 normal;
            normal[0] = -(p1[1] - p0[1]);
            normal[1] = (p1[0] - p0[0]);
            normalize2d(normal);

            float d = dot2d(normal, p0);

            sector.polygon.planes[i][0] = normal[0];
            sector.polygon.planes[i][1] = normal[1];
            sector.polygon.planes[i][2] = 0.0f;
            sector.polygon.planes[i][3] = d;
        }

        SectorList_push_back(&pod.sectors, sector);
    }

    {
        Sector sector;
        sector.ceiling_height       = 10.0f;
        sector.floor_height         = 1.0f;
        sector.polygon.num_points   = 4;
        sector.polygon.points       = malloc(sector.polygon.num_points * sizeof(*sector.polygon.points));
        sector.polygon.planes       = malloc(sector.polygon.num_points * sizeof(*sector.polygon.planes));
        sector.polygon.next_sectors = malloc(sector.polygon.num_points * sizeof(*sector.polygon.next_sectors));

        for (unsigned i = 0; i < sector.polygon.num_points; ++i) {
            sector.polygon.next_sectors[i] = NULL;
        }

        // clang-format off
        sector.polygon.points[0][0] = -10.0f, sector.polygon.points[0][1] = 10.0f;
        sector.polygon.points[1][0] = 20.0f, sector.polygon.points[1][1] = 20.0f;
        sector.polygon.points[2][0] = 20.0f,  sector.polygon.points[2][1] = 40.0f;
        sector.polygon.points[3][0] = -20.0f,  sector.polygon.points[3][1] = 50.0f;
        // clang-format on

        // precalc the planes
        for (unsigned i = 0; i < sector.polygon.num_points; ++i) {
            unsigned j = (i + 1) % sector.polygon.num_points;
            vec3 p0 = { 0 }, p1 = { 0 };
            p0[0] = sector.polygon.points[i][0];
            p0[1] = sector.polygon.points[i][1];
            p1[0] = sector.polygon.points[j][0];
            p1[1] = sector.polygon.points[j][1];

            vec2 normal;
            normal[0] = -(p1[1] - p0[1]);
            normal[1] = (p1[0] - p0[0]);
            normalize2d(normal);

            float d = dot2d(normal, p0);

            sector.polygon.planes[i][0] = normal[0];
            sector.polygon.planes[i][1] = normal[1];
            sector.polygon.planes[i][2] = 0.0f;
            sector.polygon.planes[i][3] = d;
        }

        SectorList_push_back(&pod.sectors, sector);
    }

    pod.sectors.head->item.polygon.next_sectors[2]       = &pod.sectors.head->next->item;
    pod.sectors.head->next->item.polygon.next_sectors[0] = &pod.sectors.head->item;

    mat4 tmp_mat[16];

    vec3 cam_pos  = { 0.0f, 0.0f, 1.65f };
    float cam_yaw = 0.0f, cam_pitch = 0.0f;

    mat4 proj_mat;
    mat4Perspective(FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE, proj_mat);

    mat4 view_matrix;
    mat4 view_rotation;
    mat4 vp_mat;

    float map_scale = 1.0f;
    mat4 map_projection;
    mat4 map_mat;
    mat4Translate((vec3){ SCREEN_WIDTH_HALF, SCREEN_HEIGHT_HALF, 0.0f }, tmp_mat[0]);
    mat4Scale((vec3){ 1.0f, -1.0f, 1.0f }, tmp_mat[1]);
    mat4Mul(tmp_mat[0], tmp_mat[1], map_projection);

    mat4 frustum_matrix;
    Frustum view_frustum;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    while (1) {
        ticks      = SDL_GetTicks64();
        seconds    = ticks / 1000.0f;
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

        ////////////////////////////////////////////////
        //      UPDATE
        ////////////////////////////////////////////////

        {
            float input_h = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
            float input_v = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
            float input_z = keys[SDL_SCANCODE_SPACE] - keys[SDL_SCANCODE_LSHIFT];
            float input_r = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
            float input_p = keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN];

            map_scale += (keys[SDL_SCANCODE_Q] - keys[SDL_SCANCODE_Z]) * delta;
            map_scale = max(map_scale, 0.1f);

            cam_yaw += input_r * delta * 2.0f;
            cam_pitch -= input_p * delta * 1.0f;
            cam_pitch = clamp(cam_pitch, -M_PI * 0.5f, M_PI * 0.5f);

            vec2 movement_input;
            rot2d((vec2){ input_h, input_v }, cam_yaw, movement_input);
            normalize2d(movement_input);

            cam_pos[0] += movement_input[0] * delta * 10.0f;
            cam_pos[1] += movement_input[1] * delta * 10.0f;
            cam_pos[2] += input_z * delta * 10.0f;
        }

        mat4Translate((vec3){ -cam_pos[0], -cam_pos[1], -cam_pos[2] }, tmp_mat[0]);
        mat4RotateZ(-cam_yaw, tmp_mat[1]);
        mat4RotateX(-cam_pitch, tmp_mat[2]);
        mat4Scale((vec3){ map_scale, map_scale, 1.0f }, tmp_mat[3]);

        mat4Mul(tmp_mat[1], tmp_mat[0], tmp_mat[4]);
        mat4Mul(tmp_mat[3], tmp_mat[4], tmp_mat[5]);
        mat4Mul(map_projection, tmp_mat[5], map_mat);

        mat4Mul(tmp_mat[2], tmp_mat[1], view_rotation);
        mat4Mul(view_rotation, tmp_mat[0], view_matrix);
        mat4Mul(proj_mat, view_matrix, vp_mat);

        mat4RotateZ(cam_yaw, tmp_mat[1]);
        mat4RotateX(cam_pitch, tmp_mat[2]);
        mat4Mul(tmp_mat[1], tmp_mat[2], frustum_matrix);

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)getPixelBufferPtr(), &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
            setPixelI(i, RGB(0, 0, 0));
        }

        {
            const float half_v = FAR_PLANE * tanf(FOV * .5f);
            const float half_h = half_v * ASPECT_RATIO;
            vec3 cam_front, cam_right, cam_up, cam_front_far;
            mat4MulVec3(frustum_matrix, (vec3){ 1.0f, 0.0f, 0.0f }, cam_right);
            mat4MulVec3(frustum_matrix, (vec3){ 0.0f, 1.0f, 0.0f }, cam_front);
            mat4MulVec3(frustum_matrix, (vec3){ 0.0f, 0.0f, 1.0f }, cam_up);
            for (unsigned _x = 0; _x < 3; ++_x)
                cam_front_far[_x] = cam_front[_x] * FAR_PLANE;

            vec3 tmp_vec[2];

            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[0][_x] = cam_right[_x] * half_h;
            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
            cross3d(tmp_vec[1], cam_up, view_frustum.planes[0]);

            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[0][_x] = cam_right[_x] * half_h;
            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
            cross3d(cam_up, tmp_vec[1], view_frustum.planes[1]);

            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[0][_x] = cam_up[_x] * half_v;
            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[1][_x] = cam_front_far[_x] - tmp_vec[0][_x];
            cross3d(cam_right, tmp_vec[1], view_frustum.planes[2]);

            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[0][_x] = cam_up[_x] * half_v;
            for (unsigned _x = 0; _x < 3; ++_x)
                tmp_vec[1][_x] = cam_front_far[_x] + tmp_vec[0][_x];
            cross3d(tmp_vec[1], cam_right, view_frustum.planes[3]);

            for (unsigned _x = 0; _x < 3; ++_x)
                view_frustum.planes[4][_x] = cam_front[_x];
            for (unsigned _x = 0; _x < 3; ++_x)
                view_frustum.planes[5][_x] = -cam_front[_x];

            for (unsigned i = 0; i < 6; ++i) {
                normalize3d(view_frustum.planes[i]);
                view_frustum.planes[i][3] = dot3d(view_frustum.planes[i], cam_pos);
            }

            view_frustum.planes[4][3] += NEAR_PLANE;
            view_frustum.planes[5][3] += -FAR_PLANE;
        }

        portalWorldRender(pod, vp_mat, cam_pos, view_frustum);

        // Render map view
        {
            SectorListNode *node = pod.sectors.head;
            while (node != NULL) {
                for (unsigned i = 0; i < node->item.polygon.num_points; ++i) {
                    unsigned j = (i + 1) % node->item.polygon.num_points;
                    vec3 p0 = { 0 }, p1 = { 0 };
                    p0[0] = node->item.polygon.points[i][0];
                    p0[1] = node->item.polygon.points[i][1];
                    p1[0] = node->item.polygon.points[j][0];
                    p1[1] = node->item.polygon.points[j][1];

                    vec2 normal;
                    rot2d((vec2){ node->item.polygon.planes[i][0], -node->item.polygon.planes[i][1] }, cam_yaw, normal);

                    vec3 t0, t1;
                    mat4MulVec3(map_mat, p0, t0);
                    mat4MulVec3(map_mat, p1, t1);

                    vec2 avg = { (t0[0] + t1[0]) * 0.5f, (t0[1] + t1[1]) * 0.5f };

                    Color highlight, shadow;
                    if (node->item.polygon.next_sectors[i] == NULL) {
                        highlight = COLOR_WHITE;
                        shadow    = RGB(50, 50, 60);
                    } else {
                        highlight = RGB(255, 150, 255);
                        shadow    = RGB(160, 50, 170);
                    }

                    // main wall
                    drawLine(t0[0] + 1, t0[1] + 1, t1[0] + 1, t1[1] + 1, shadow);
                    drawLine(t0[0], t0[1], t1[0], t1[1], highlight);

                    if (node->item.polygon.next_sectors[i] != NULL) {
                        Sector *next = node->item.polygon.next_sectors[i];
                        vec3 center  = { 0.0f };
                        for (unsigned n = 0; n < next->polygon.num_points; ++n) {
                            center[0] += next->polygon.points[n][0];
                            center[1] += next->polygon.points[n][1];
                        }
                        center[0] /= next->polygon.num_points;
                        center[1] /= next->polygon.num_points;

                        vec3 c;
                        mat4MulVec3(map_mat, center, c);

                        drawLine(avg[0], avg[1], c[0], c[1], highlight);
                    }

                    // normal
                    drawLine(avg[0], avg[1], avg[0] + normal[0] * map_scale * 2.0f, avg[1] + normal[1] * map_scale * 2.0f, COLOR_BLUE);
                }
                node = node->next;
            }
            setPixel(SCREEN_WIDTH_HALF, SCREEN_HEIGHT_HALF, COLOR_WHITE);
            setPixel(SCREEN_WIDTH_HALF + 1, SCREEN_HEIGHT_HALF + 1, COLOR_GREEN);
        }

        snprintf(print_buffer, sizeof(print_buffer), "%6.3f %6.3f %6.3f", cam_pos[0], cam_pos[1], cam_pos[2]);
        renderText(print_buffer, 0, 0, COLOR_WHITE, main_font, main_font_char_width);

        renderText(TITLE_STRING, 0, SCREEN_HEIGHT - main_font.height + 1, RGB(255, 200, 10), main_font, main_font_char_width);
        renderText(TITLE_STRING, 0, SCREEN_HEIGHT - main_font.height, RGB(20, 60, 120), main_font, main_font_char_width);
        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:

    freePortalWorld(pod);

    free(last_keys);
    SDL_DestroyTexture(screen_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Exit successful\n");

    return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

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
