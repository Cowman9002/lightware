#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include "color.h"
#include "draw.h"
#include "mathlib.h"

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


void renderText(const char *text, int draw_x, int draw_y, Color draw_color, Image font, unsigned char_width);

Image g_image_array[3];
Image g_sky_image_array[1];

int main(int argc, char *argv[]) {

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("Lightware",
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

    uint64_t next_fps_print = 1000;
    unsigned last_fps_frame = 0;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    Image main_font;
    if (!readPng("res/fonts/small.png", &main_font)) return -1;
    unsigned main_font_char_width =  main_font.width / 95;

    if (!readPng("res/textures/wall.png", &g_image_array[0])) return -1;
    if (!readPng("res/textures/floor.png", &g_image_array[1])) return -1;
    if (!readPng("res/textures/ceiling.png", &g_image_array[2])) return -1;
    if (!readPng("res/textures/MUNSKY01.png", &g_sky_image_array[0])) return -1;

    char print_buffer[128];

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

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)getPixelBufferPtr(), &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i) {
            setPixelI(i, RGB(0, 0, 0));
        }

        renderText("LightWare 0.1", 0, SCREEN_HEIGHT - main_font.height, COLOR_WHITE, main_font, main_font_char_width);

        SDL_UnlockTexture(screen_texture);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frame += 1;
    }

_success_exit:

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
