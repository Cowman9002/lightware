#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include "color.h"
#include "draw.h"
#include "mathlib.h"
#include "portal_world.h"
#include "camera.h"

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

    // uint64_t next_fps_print = 1000;
    const unsigned FPS_PRINT_INTERVAL = 100;
    uint64_t next_fps_print = FPS_PRINT_INTERVAL;
    unsigned last_fps_frame = 0;
    unsigned fps_print_val  = 0;

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

    const char *const map_path = "res/maps/map0.map";
    PortalWorld pod;
    if(!loadPortalWorld(map_path, 5.0f, &pod)) return -2;

    mat4 tmp_mat[16];

    Camera cam = {
        .pos   = { 0.0f, 0.0f, 1.65f },
        .pitch = 0.0f,
        .yaw   = 0.0f,
    };

    mat4Perspective(FOV, ASPECT_RATIO, NEAR_PLANE, FAR_PLANE, cam.proj_mat);

    float map_scale = 1.0f;
    mat4 map_projection;
    mat4 map_mat;
    mat4Translate((vec3){ SCREEN_WIDTH_HALF, SCREEN_HEIGHT_HALF, 0.0f }, tmp_mat[0]);
    mat4Scale((vec3){ 1.0f, -1.0f, 1.0f }, tmp_mat[1]);
    mat4Mul(tmp_mat[0], tmp_mat[1], map_projection);

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    while (1) {
        ticks      = SDL_GetTicks64();
        seconds    = ticks / 1000.0f;
        delta      = (float)(ticks - last_ticks) / 1000.0f;
        last_ticks = ticks;

        // if (ticks >= next_fps_print) {
        //     next_fps_print += 1000;
        //     printf("FPS: %4u   MS: %f\n", frame - last_fps_frame, 1.0 / (frame - last_fps_frame));
        //     last_fps_frame = frame;
        // }

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

        if(keys[SDL_SCANCODE_L] && !last_keys[SDL_SCANCODE_L]) {
            PortalWorld tmp;
            if(!loadPortalWorld(map_path, 5.0f, &tmp)) {
                printf("Failed to reload %s\n", map_path);
            } else {
                freePortalWorld(pod);
                pod = tmp;
                printf("Reloaded map %s\n", map_path);
            }
        }

        {
            float input_h = keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A];
            float input_v = keys[SDL_SCANCODE_W] - keys[SDL_SCANCODE_S];
            float input_z = keys[SDL_SCANCODE_SPACE] - keys[SDL_SCANCODE_LSHIFT];
            float input_r = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
            float input_p = keys[SDL_SCANCODE_UP] - keys[SDL_SCANCODE_DOWN];

            map_scale += (keys[SDL_SCANCODE_Q] - keys[SDL_SCANCODE_Z]) * delta;
            map_scale = max(map_scale, 0.1f);

            cam.yaw += input_r * delta * 2.0f;
            cam.pitch -= input_p * delta * 1.0f;
            cam.pitch = clamp(cam.pitch, -M_PI * 0.5f, M_PI * 0.5f);

            vec2 movement_input;
            rot2d((vec2){ input_h, input_v }, cam.yaw, movement_input);
            normalize2d(movement_input);

            cam.pos[0] += movement_input[0] * delta * 10.0f;
            cam.pos[1] += movement_input[1] * delta * 10.0f;
            cam.pos[2] += input_z * delta * 10.0f;

            cam.sector = getSector(pod, cam.pos);

            if (cam.sector != NULL) {
                cam.sub_sector = getSubSector(cam.sector, cam.pos);

                cam.sector->sub_sectors[cam.sub_sector].floor_height += (keys[SDL_SCANCODE_R] - keys[SDL_SCANCODE_E]) * delta;
                cam.sector->sub_sectors[cam.sub_sector].ceiling_height += (keys[SDL_SCANCODE_Y] - keys[SDL_SCANCODE_T]) * delta;
            }
        }

        mat4Translate((vec3){ -cam.pos[0], -cam.pos[1], -cam.pos[2] }, tmp_mat[0]);
        mat4RotateZ(-cam.yaw, tmp_mat[1]);
        mat4RotateX(-cam.pitch, tmp_mat[2]);
        mat4Scale((vec3){ map_scale, map_scale, 1.0f }, tmp_mat[3]);

        mat4Mul(tmp_mat[1], tmp_mat[0], tmp_mat[4]);
        mat4Mul(tmp_mat[3], tmp_mat[4], tmp_mat[5]);
        mat4Mul(map_projection, tmp_mat[5], map_mat);

        mat4Mul(tmp_mat[2], tmp_mat[1], tmp_mat[4]);
        mat4Mul(tmp_mat[4], tmp_mat[0], cam.view_mat);
        mat4Mul(cam.proj_mat, cam.view_mat, cam.vp_mat);

        mat4RotateZ(cam.yaw, tmp_mat[1]);
        mat4RotateX(cam.pitch, tmp_mat[2]);
        mat4Mul(tmp_mat[1], tmp_mat[2], cam.rot_mat);

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)getPixelBufferPtr(), &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
            setPixelI(i, RGB(0, 0, 0));
        }

        portalWorldRender(pod, cam);

        // Render map view
        {
            SectorListNode *node = pod.sectors.head;
            while (node != NULL) {
                for (unsigned i = 0; i < node->item.num_walls; ++i) {
                    unsigned j = (i + 1) % node->item.num_walls;
                    vec3 p0 = { 0 }, p1 = { 0 };
                    p0[0] = node->item.points[i][0];
                    p0[1] = node->item.points[i][1];
                    p1[0] = node->item.points[j][0];
                    p1[1] = node->item.points[j][1];

                    vec2 normal;
                    rot2d((vec2){ node->item.planes[i][0], -node->item.planes[i][1] }, cam.yaw, normal);

                    vec3 t0, t1;
                    mat4MulVec3(map_mat, p0, t0);
                    mat4MulVec3(map_mat, p1, t1);

                    vec2 avg = { (t0[0] + t1[0]) * 0.5f, (t0[1] + t1[1]) * 0.5f };

                    Color highlight, shadow;
                    if (node->item.next_sectors[i] == NULL) {
                        highlight = COLOR_WHITE;
                        shadow    = RGB(50, 50, 60);
                    } else {
                        highlight = RGB(255, 150, 255);
                        shadow    = RGB(160, 50, 170);
                    }

                    // main wall
                    drawLine(t0[0] + 1, t0[1] + 1, t1[0] + 1, t1[1] + 1, shadow);
                    drawLine(t0[0], t0[1], t1[0], t1[1], highlight);

                    // if (node->item.next_sectors[i] != NULL) {
                    //     Sector *next = node->item.next_sectors[i];
                    //     vec3 center  = { 0.0f };
                    //     for (unsigned n = 0; n < next->num_walls; ++n) {
                    //         center[0] += next->points[n][0];
                    //         center[1] += next->points[n][1];
                    //     }
                    //     center[0] /= next->num_walls;
                    //     center[1] /= next->num_walls;

                    //     vec3 c;
                    //     mat4MulVec3(map_mat, center, c);

                    //     drawLine(avg[0], avg[1], c[0], c[1], highlight);
                    // }

                    // normal
                    drawLine(avg[0], avg[1], avg[0] + normal[0] * map_scale * 2.0f, avg[1] + normal[1] * map_scale * 2.0f, COLOR_BLUE);
                }
                node = node->next;
            }
            setPixel(SCREEN_WIDTH_HALF, SCREEN_HEIGHT_HALF, COLOR_WHITE);
            setPixel(SCREEN_WIDTH_HALF + 1, SCREEN_HEIGHT_HALF + 1, COLOR_GREEN);
        }

        snprintf(print_buffer, sizeof(print_buffer), "%6.3f %6.3f %6.3f", cam.pos[0], cam.pos[1], cam.pos[2]);
        renderText(print_buffer, 9, 0, COLOR_WHITE, main_font, main_font_char_width);

        if (cam.sector != NULL) {
            SectorDef def = cam.sector->sub_sectors[cam.sub_sector];
            snprintf(print_buffer, sizeof(print_buffer), "%6.3f %6.3f", def.floor_height, def.ceiling_height);
            renderText(print_buffer, 9, 16 * 1, COLOR_WHITE, main_font, main_font_char_width);

            snprintf(print_buffer, sizeof(print_buffer), "SSID: %u", cam.sub_sector);
            renderText(print_buffer, 9, 16 * 2, COLOR_WHITE, main_font, main_font_char_width);
        }

        if (ticks >= next_fps_print) {
            next_fps_print += FPS_PRINT_INTERVAL;
            fps_print_val  = frame - last_fps_frame;
            last_fps_frame = frame;
        }

        snprintf(print_buffer, sizeof(print_buffer), "%5.1f FPS", fps_print_val / (FPS_PRINT_INTERVAL / 1000.0f));
        renderText(print_buffer, SCREEN_WIDTH - 170, SCREEN_HEIGHT - main_font.height + 1, RGB(100, 255, 10), main_font, main_font_char_width);

        snprintf(print_buffer, sizeof(print_buffer), "%5.3fms", ((float)FPS_PRINT_INTERVAL) / fps_print_val);
        renderText(print_buffer, SCREEN_WIDTH - 70, SCREEN_HEIGHT - main_font.height + 1, RGB(100, 255, 10), main_font, main_font_char_width);

        renderText(TITLE_STRING, 9, SCREEN_HEIGHT - main_font.height + 1, RGB(255, 200, 10), main_font, main_font_char_width);
        renderText(TITLE_STRING, 9, SCREEN_HEIGHT - main_font.height, RGB(20, 60, 120), main_font, main_font_char_width);
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
