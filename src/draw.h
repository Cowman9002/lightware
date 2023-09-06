#pragma once

#include "mathlib.h"
#include "color.h"
#include <stdbool.h>

#define RESOLUTION_DIVISOR 1
#define SCREEN_WIDTH (640 / RESOLUTION_DIVISOR)
#define SCREEN_HEIGHT (480 / RESOLUTION_DIVISOR)
#define PIXEL_SIZE (2 * RESOLUTION_DIVISOR)
// #define PIXEL_SIZE (1)

#define SCREEN_WIDTH_HALF (SCREEN_WIDTH * 0.5)
#define SCREEN_HEIGHT_HALF (SCREEN_HEIGHT * 0.5)

#define ASPECT_RATIO ((float)SCREEN_WIDTH / SCREEN_HEIGHT)
#define INV_ASPECT_RATIO ((float)SCREEN_HEIGHT / SCREEN_WIDTH)

#define FAR_PLANE 100.0f
#define NEAR_PLANE 0.1f

// #define FOV (70.0 * TO_RADS)
#define FOV (90.0 * TO_RADS)

typedef struct Image {
    Color *data;
    int width, height;
} Image;

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, unsigned x, unsigned y);

Color **getPixelBufferPtr();

void setPixel(unsigned x, unsigned y, Color color);
void setPixelI(unsigned i, Color color);
Color getPixel(unsigned x, unsigned y);

void drawLine(int x0, int y0, int x1, int y1, Color color);
