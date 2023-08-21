#include "draw.h"

#include <stdlib.h>

extern uint32_t *g_pixels;

void drawPoint(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    g_pixels[x + y * SCREEN_WIDTH] = *(uint32_t*)&color;
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm

void _plotLineHigh(int x0, int y0, int x1, int y1, Color color);
void _plotLineLow(int x0, int y0, int x1, int y1, Color color);
void drawLine(int x0, int y0, int x1, int y1, Color color) {
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

void _plotLineHigh(int x0, int y0, int x1, int y1, Color color) {
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

void _plotLineLow(int x0, int y0, int x1, int y1, Color color) {
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