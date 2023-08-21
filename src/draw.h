#pragma once

#include "color.h"

#define SCREEN_WIDTH (640 / 2)
#define SCREEN_HEIGHT (480 / 2)
#define PIXEL_SIZE 4

void drawPoint(int x, int y, Color color);
void drawLine(int x0, int y0, int x1, int y1, Color color);