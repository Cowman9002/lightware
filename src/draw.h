#pragma once

#include "util.h"
#include "color.h"
#include <stdbool.h>

#define RESOLUTION_DIVISOR 2
#define SCREEN_WIDTH (640 / RESOLUTION_DIVISOR)
#define SCREEN_HEIGHT (480 / RESOLUTION_DIVISOR)
#define PIXEL_SIZE (2 * RESOLUTION_DIVISOR)
// #define PIXEL_SIZE (1)

#define SCREEN_WIDTH_HALF (SCREEN_WIDTH * 0.5)
#define SCREEN_HEIGHT_HALF (SCREEN_HEIGHT * 0.5)

#define ASPECT_RATIO ((float)SCREEN_WIDTH / SCREEN_HEIGHT)
#define INV_ASPECT_RATIO ((float)SCREEN_HEIGHT / SCREEN_WIDTH)

#define FAR_PLANE 100.0f
#define NEAR_PLANE 0.3f

#define FLOAT_TO_DEPTH(z) ((z / FAR_PLANE) * (uint16_t)(~0))

typedef struct Image {
    Color *data;
    int width, height;
} Image;

bool readPng(const char *path, Image *out);
Color sampleImage(Image image, unsigned x, unsigned y);

Color **getPixelBufferPtr();

void setPixel(unsigned x, unsigned y, Color color);
void setPixelI(unsigned i, Color color);

void drawLine(int x0, int y0, int x1, int y1, Color color);
