#include "internal.h"

#include <stdint.h>
#include <math.h>

LW_Color lw_lerpColor(LW_Color c0, LW_Color c1, uint8_t t) {
    uint8_t s = 255 - t;

    LW_Color a = lw_shadeColor(c0, s);
    LW_Color b = lw_shadeColor(c1, t);

    return (LW_Color){
        .r = a.r + b.r,
        .g = a.g + b.g,
        .b = a.b + b.b,
        .a = a.a + b.a,
    };
}

LW_Color lw_mulColor(LW_Color c0, LW_Color c1) {
    return (LW_Color){
        .r = (uint8_t)(((uint16_t)c0.r * (uint16_t)c1.r) >> 8),
        .g = (uint8_t)(((uint16_t)c0.g * (uint16_t)c1.g) >> 8),
        .b = (uint8_t)(((uint16_t)c0.b * (uint16_t)c1.b) >> 8),
        .a = (uint8_t)(((uint16_t)c0.a * (uint16_t)c1.a) >> 8),
    };
}

LW_Color lw_shadeColor(LW_Color c0, uint8_t m) {
    return (LW_Color){
        .r = (uint8_t)(((uint16_t)c0.r * (uint16_t)m) >> 8),
        .g = (uint8_t)(((uint16_t)c0.g * (uint16_t)m) >> 8),
        .b = (uint8_t)(((uint16_t)c0.b * (uint16_t)m) >> 8),
        .a = (uint8_t)(((uint16_t)c0.a * (uint16_t)m) >> 8),
    };
}


void lw_setPixel(LW_Framebuffer *const framebuffer, lw_uvec2 pos, LW_Color color) {
    if (pos[0] >= framebuffer->width || pos[1] >= framebuffer->height) return;
    framebuffer->pixels[pos[0] + pos[1] * framebuffer->width] = color;
}

void lw_fillBuffer(LW_Framebuffer *const framebuffer, LW_Color color) {
    unsigned j = framebuffer->width * framebuffer->height;
    for (unsigned i = 0; i < j; ++i) {
        framebuffer->pixels[i] = color;
    }
}

void lw_fillRect(LW_Framebuffer *const framebuffer, LW_Rect rect, LW_Color color) {
    LW_Rect clipped = {
        .pos = {
            clamp(rect.pos[0], 0, framebuffer->width),
            clamp(rect.pos[1], 0, framebuffer->height),
        },
        .size = {
            clamp(rect.pos[0] + rect.size[0], 0, framebuffer->width),
            clamp(rect.pos[1] + rect.size[1], 0, framebuffer->height),
        },
    };

    for (unsigned y = clipped.pos[1]; y < clipped.size[1]; ++y) {
        for (unsigned x = clipped.pos[0]; x < clipped.size[0]; ++x) {
            framebuffer->pixels[x + y * framebuffer->width] = color;
        }
    }
}

void lw_drawRect(LW_Framebuffer *const framebuffer, LW_Rect rect, LW_Color color) {
    LW_Rect clipped = {
        .pos = {
            clamp(rect.pos[0], 0, framebuffer->width),
            clamp(rect.pos[1], 0, framebuffer->height),
        },
        .size = {
            clamp(rect.pos[0] + rect.size[0], 0, framebuffer->width),
            clamp(rect.pos[1] + rect.size[1], 0, framebuffer->height),
        },
    };

    for (unsigned y = clipped.pos[1]; y < clipped.size[1]; ++y) {
        framebuffer->pixels[clipped.pos[0] + y * framebuffer->width]  = color;
        framebuffer->pixels[clipped.size[0] + y * framebuffer->width] = color;
    }

    for (unsigned x = clipped.pos[0]; x < clipped.size[0]; ++x) {
        framebuffer->pixels[x + clipped.pos[1] * framebuffer->width]  = color;
        framebuffer->pixels[x + clipped.size[1] * framebuffer->width] = color;
    }
}

// https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm
void _plotLineHigh(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color);
void _plotLineLow(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color);
void lw_drawLine(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color) {
    if (abs(v1[1] - v0[1]) < abs(v1[0] - v0[0])) {
        if (v0[0] > v1[0])
            _plotLineLow(framebuffer, v1, v0, color);
        else
            _plotLineLow(framebuffer, v0, v1, color);
    } else {
        if (v0[1] > v1[1])
            _plotLineHigh(framebuffer, v1, v0, color);
        else
            _plotLineHigh(framebuffer, v0, v1, color);
    }
}

void lw_drawPoly(LW_Framebuffer *const framebuffer, lw_ivec2 *vertices, unsigned num_vertices, LW_Color color) {
    for (unsigned i = 1; i < num_vertices; ++i) {
        lw_drawLine(framebuffer, vertices[i - 1], vertices[i], color);
    }
    lw_drawLine(framebuffer, vertices[num_vertices - 1], vertices[0], color);
}

//
//      INTERNAL
//

void _plotLineHigh(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color) {
    int dx = v1[0] - v0[0];
    int dy = v1[1] - v0[1];
    int xi = 1;
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int D = (2 * dx) - dy;
    int x = v0[0];

    for (int y = v0[1]; y <= v1[1]; ++y) {
        lw_setPixel(framebuffer, (lw_uvec2){ x, y }, color);
        if (D > 0) {
            x = x + xi;
            D = D + (2 * (dx - dy));
        } else {
            D = D + 2 * dx;
        }
    }
}

void _plotLineLow(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color) {
    int dx = v1[0] - v0[0];
    int dy = v1[1] - v0[1];
    int yi = 1;
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }

    int D = (2 * dy) - dx;
    int y = v0[1];

    for (int x = v0[0]; x <= v1[0]; ++x) {
        lw_setPixel(framebuffer, (lw_uvec2){ x, y }, color);
        if (D > 0) {
            y = y + yi;
            D = D + (2 * (dy - dx));
        } else {
            D = D + 2 * dy;
        }
    }
}