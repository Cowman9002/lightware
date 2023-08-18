#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <lodepng.h>

#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

// https://benedicthenshaw.com/soft_render_sdl2.html

#define SCREEN_WIDTH (640 / 2)
#define SCREEN_HEIGHT (480 / 2)
#define PIXEL_SIZE 4

#define RGBA(r, g, b, a) ((((uint32_t)a & 0xff) << 0) | (((uint32_t)b & 0xff) << 8) | (((uint32_t)g & 0xff) << 16) | (((uint32_t)r & 0xff) << 24))
#define RGB(r, g, b) RGBA(r, g, b, 255)

#define min(a, b) ((a < b) ? a : b)
#define max(a, b) ((a > b) ? a : b)

typedef struct Color {
    uint8_t a, b, g, r;
} Color;

typedef struct Image {
    unsigned char *data;
    int width, height;
} Image;

typedef struct Rect {
    int x0, y0, x1, y1;
} Rect;

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, int x, int y);
Color sampleImageRegion(Image image, int x, int y, Rect region);
Color sampleImage3d(Image image, int layer_height, int depth, int x, int y, int z);

void drawPoint(int x, int y, int32_t color);
void drawLine(int x0, int y0, int x1, int y1, int32_t color);

uint32_t *g_pixels = NULL;

int main(int argc, char *argv[]) {

    // Image player_image;
    // if (!readPng("res/sprites/player.png", &player_image)) return -1;

    Image chest_image;
    if (!readPng("res/sprites/chest_open.png", &chest_image)) return -1;
    int chest_depth              = 16; // 10
    int chest_image_layer_height = chest_image.height / chest_depth;

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

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    float player_pos[] = { 0, 0 };
    float player_rot   = 0;

    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////
    /////////////////////////////////////////////////////////////

    while (1) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) goto _success_exit;
        }


        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(renderer);
        SDL_LockTexture(screen_texture, NULL, (void **)&g_pixels, &pitch);

        // clear screen
        for (unsigned i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; ++i)
            g_pixels[i] = RGB(80, 10, 60);

        {
            float rcos = cosf((float)frame / 30.0);
            float rsin = sinf((float)frame / 30.0);

            for (int z = chest_depth; z > 0;) {
                --z;
                for (int y = 0; y < SCREEN_HEIGHT; ++y) {
                    for (int x = 0; x < SCREEN_WIDTH; ++x) {
                        float sample_x, sample_y;
                        float tx, ty;

                        tx = x - 40;
                        ty = y - 40;

                        sample_x = rcos * tx + rsin * ty;
                        sample_y = -rsin * tx + rcos * ty;

                        Color c = sampleImage3d(chest_image, chest_image_layer_height, chest_depth, sample_x, sample_y, z);
                        if (c.a > 128) {
                            drawPoint(x, y + z, *(int32_t *)(&c));
                        }
                    }
                }
            }
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

    out->data   = data;
    out->width  = width;
    out->height = height;

    return true;
}

Color sampleImage(Image image, int x, int y) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return (Color){};
    Color res;
    unsigned i = x + y * image.width;
    res.r      = image.data[i * 4 + 0];
    res.g      = image.data[i * 4 + 1];
    res.b      = image.data[i * 4 + 2];
    res.a      = image.data[i * 4 + 3];
    return res;
}

Color sampleImageRegion(Image image, int x, int y, Rect region) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return (Color){};
    if (x < region.x0 || y < region.y0 || x >= region.x1 || y >= region.y1) return (Color){};
    Color res;
    unsigned i = x + y * image.width;
    res.r      = image.data[i * 4 + 0];
    res.g      = image.data[i * 4 + 1];
    res.b      = image.data[i * 4 + 2];
    res.a      = image.data[i * 4 + 3];
    return res;
}

Color sampleImage3d(Image image, int layer_height, int depth, int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= image.width || y >= layer_height || z >= depth) {
        return (Color){};
    }

    Color res;
    unsigned i = x + y * image.width + z * image.width * layer_height;
    res.r      = image.data[i * 4 + 0];
    res.g      = image.data[i * 4 + 1];
    res.b      = image.data[i * 4 + 2];
    res.a      = image.data[i * 4 + 3];
    return res;
}

void drawPoint(int x, int y, int32_t color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    g_pixels[x + y * SCREEN_WIDTH] = color;
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

void _plotLineHigh(int x0, int y0, int x1, int y1, int32_t color);
void _plotLineLow(int x0, int y0, int x1, int y1, int32_t color);
void drawLine(int x0, int y0, int x1, int y1, int32_t color) {
    if (abs(y1 - y0) < abs(x1 - x0)) {
        if (x0 > x1)
            _plotLineLow(x1, y1, x0, y0, color);
        else
            _plotLineLow(x0, y0, x1, y1, color);
    } else {
        if (y0 > y1)
            _plotLineHigh(x1, y1, x0, y0, color);
        else
            _plotLineHigh(x0, y0, x1, y1, color);
    }
}

void _plotLineHigh(int x0, int y0, int x1, int y1, int32_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int xi = 1;
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int D = (2 * dx) - dy;
    int x = x0;

    for (int y = y0; y <= y1; ++y) {
        drawPoint(x, y, color);
        if (D > 0) {
            x = x + xi;
            D = D + (2 * (dx - dy));
        } else {
            D = D + 2 * dx;
        }
    }
}

void _plotLineLow(int x0, int y0, int x1, int y1, int32_t color) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }

    int D = (2 * dy) - dx;
    int y = y0;

    for (int x = x0; x <= x1; ++x) {
        drawPoint(x, y, color);
        if (D > 0) {
            y = y + yi;
            D = D + (2 * (dy - dx));
        } else {
            D = D + 2 * dy;
        }
    }
}

// typedef int32_t fixed32;
// fixed32 floatToFixed(float f) {
//     float whole, frac;
//     frac = modff(f, &whole);
//     return ((fixed32)whole << 16) | (fixed32)(frac * (1 << 16));
// }

// fixed32 intToFixed(int v) {
//     return (fixed32)v << 16;
// }