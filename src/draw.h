#pragma once

#include "util.h"
#include "color.h"

#define RESOLUTION_DIVISOR 4
#define SCREEN_WIDTH (640 / RESOLUTION_DIVISOR)
#define SCREEN_HEIGHT (480 / RESOLUTION_DIVISOR)
#define PIXEL_SIZE (2 * RESOLUTION_DIVISOR)

Color **getPixelBufferPtr();

void setPixel(unsigned x, unsigned y, Color color);
void setPixelI(unsigned i, Color color);

void drawLine(int x0, int y0, int x1, int y1, Color color);
