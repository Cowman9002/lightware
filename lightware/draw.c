#include "internal.h"

#include <lodepng.h>
#include <stdint.h>
#include <stdio.h>
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

//////////////////////////////////////////////////////////////////////////////////////////////////

bool lw_loadTexture(const char *path, LW_Texture *o_texture) {
    if (path == NULL) return false;
    if (o_texture == NULL) return false;

    unsigned error;
    unsigned char *data = 0;
    unsigned width, height;

    error = lodepng_decode32_file(&data, &width, &height, path);
    if (error) {
        printf("error %u: %s\n", error, lodepng_error_text(error));
        return false;
    }

    o_texture->data   = (LW_Color *)data;
    o_texture->width  = width;
    o_texture->height = height;

    // lodepng reads as rgba, but we need abgr
    LW_Color tmp;
    for (unsigned i = 0; i < width * height; ++i) {
        tmp                  = o_texture->data[i];
        o_texture->data[i].r = tmp.a;
        o_texture->data[i].g = tmp.b;
        o_texture->data[i].b = tmp.g;
        o_texture->data[i].a = tmp.r;
    }

    return true;
}

void lw_freeTexture(LW_Texture texture) {
    free(texture.data);
}

LW_Color lw_sampleTextureRaw(LW_Texture texture, lw_uvec2 coords) {
    if (coords[0] >= texture.width || coords[1] >= texture.height) return LW_COLOR_CLEAR;
    return texture.data[coords[0] + coords[1] * texture.width];
}


//////////////////////////////////////////////////////////////////////////////////////////////////

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

void lw_fillRect(LW_Framebuffer *const framebuffer, LW_Recti rect, LW_Color color) {
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

void lw_drawRect(LW_Framebuffer *const framebuffer, LW_Recti rect, LW_Color color) {
    LW_Recti bounds = {
        .pos = {
            rect.pos[0],
            rect.pos[1],
        },
        .size = {
            rect.pos[0] + rect.size[0],
            rect.pos[1] + rect.size[1],
        },
    };

    bool left_clipped   = bounds.pos[0] < 0 || bounds.pos[0] >= framebuffer->width;
    bool right_clipped  = (bounds.size[0] - 1) < 0 || (bounds.size[0] - 1) >= framebuffer->width;
    bool top_clipped    = bounds.pos[1] < 0 || bounds.pos[1] >= framebuffer->height;
    bool bottom_clipped = (bounds.size[1] - 1) < 0 || (bounds.size[1] - 1) >= framebuffer->height;

    bounds.pos[0]  = clamp(bounds.pos[0], 0, framebuffer->width);
    bounds.size[0] = clamp(bounds.size[0], 0, framebuffer->width);
    bounds.pos[1]  = clamp(bounds.pos[1], 0, framebuffer->height);
    bounds.size[1] = clamp(bounds.size[1], 0, framebuffer->height);

    // left and right
    for (unsigned y = bounds.pos[1]; y < bounds.size[1]; ++y) {
        if (!left_clipped)
            framebuffer->pixels[bounds.pos[0] + y * framebuffer->width] = color;
        if (!right_clipped)
            framebuffer->pixels[(bounds.size[0] - 1) + y * framebuffer->width] = color;
    }

    // top and bottom
    for (unsigned x = bounds.pos[0]; x < bounds.size[0]; ++x) {
        if (!top_clipped)
            framebuffer->pixels[x + bounds.pos[1] * framebuffer->width] = color;
        if (!bottom_clipped)
            framebuffer->pixels[x + (bounds.size[1] - 1) * framebuffer->width] = color;
    }
}

// http://fredericgoset.ovh/mathematiques/courbes/en/filled_circle.html
void lw_fillCircle(LW_Framebuffer *const framebuffer, LW_Circlei circle, LW_Color color) {
    if (circle.pos[0] + circle.radius < 0) return;
    if (circle.pos[0] - circle.radius >= (int)framebuffer->width) return;
    if (circle.pos[1] + circle.radius < 0) return;
    if (circle.pos[1] - circle.radius >= (int)framebuffer->height) return;

    int x = 0;
    int y = circle.radius;
    int m = 5 - 4 * circle.radius;

    while (x <= y) {
        int start_x_raw = circle.pos[0] - y;
        int end_x_raw   = circle.pos[0] + y;
        int start_y_raw = circle.pos[1] - x;
        int end_y_raw   = circle.pos[1] + x;

        int start_x = clamp(start_x_raw, 0, framebuffer->width - 1);
        int end_x   = clamp(end_x_raw, 0, framebuffer->width - 1);
        int start_y = clamp(start_y_raw, 0, framebuffer->height - 1);
        int end_y   = clamp(end_y_raw, 0, framebuffer->height - 1);

        if (start_x <= end_x_raw && end_x >= start_x_raw && start_y <= end_y_raw && end_y >= start_y_raw) {
            int pos1 = (start_x) + (start_y)*framebuffer->width;
            int pos2 = (start_x) + (end_y)*framebuffer->width;

            for (int xx = start_x; xx <= end_x; xx++) {
                framebuffer->pixels[pos1++] = color;
                framebuffer->pixels[pos2++] = color;
            }
        }

        if (m > 0) {
            int start_x_raw = circle.pos[0] - x;
            int end_x_raw   = circle.pos[0] + x;
            int start_y_raw = circle.pos[1] - y;
            int end_y_raw   = circle.pos[1] + y;

            int start_x = clamp(start_x_raw, 0, framebuffer->width - 1);
            int end_x   = clamp(end_x_raw, 0, framebuffer->width - 1);
            int start_y = clamp(start_y_raw, 0, framebuffer->height - 1);
            int end_y   = clamp(end_y_raw, 0, framebuffer->height - 1);

            if (start_x <= end_x_raw && end_x >= start_x_raw && start_y <= end_y_raw && end_y >= start_y_raw) {
                int pos1 = (start_x) + (start_y)*framebuffer->width;
                int pos2 = (start_x) + (end_y)*framebuffer->width;

                for (int xx = start_x; xx <= end_x; xx++) {
                    framebuffer->pixels[pos1++] = color;
                    framebuffer->pixels[pos2++] = color;
                }
            }

            y--;
            m -= 8 * y;
        }

        x++;
        m += 8 * x + 4;
    }
}

// https://www.geeksforgeeks.org/bresenhams-circle-drawing-algorithm/#
// http://fredericgoset.ovh/mathematiques/courbes/en/bresenham_circle.html
void _setCirclePixel(LW_Framebuffer *const framebuffer, lw_ivec2 pos, lw_ivec2 offset, LW_Color color) {
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] + offset[0], pos[1] + offset[1] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] - offset[0], pos[1] + offset[1] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] + offset[0], pos[1] - offset[1] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] - offset[0], pos[1] - offset[1] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] + offset[1], pos[1] + offset[0] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] - offset[1], pos[1] + offset[0] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] + offset[1], pos[1] - offset[0] }, color);
    lw_setPixel(framebuffer, (lw_uvec2){ pos[0] - offset[1], pos[1] - offset[0] }, color);
}

void lw_drawCircle(LW_Framebuffer *const framebuffer, LW_Circlei circle, LW_Color color) {
    int m           = 5 - 4 * circle.radius;
    lw_ivec2 offset = { 0, circle.radius };
    _setCirclePixel(framebuffer, circle.pos, offset, color);
    while (offset[1] >= offset[0]) {
        _setCirclePixel(framebuffer, circle.pos, offset, color);

        if (m > 0) {
            --offset[1];
            m -= 8 * offset[1];
        }
        ++offset[0];
        m += 8 * offset[0] + 4;
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

void lw_drawPoly(LW_Framebuffer *const framebuffer, lw_vec2 *vertices, unsigned num_vertices, LW_Color color) {
    lw_ivec2 a, b;
    a[0] = vertices[0][0], a[1] = vertices[0][1];
    for (unsigned i = 1; i < num_vertices; ++i) {
        b[0] = vertices[i][0], b[1] = vertices[i][1];
        lw_drawLine(framebuffer, a, b, color);
        a[0] = b[0], a[1] = b[1];
    }

    b[0] = vertices[0][0], b[1] = vertices[0][1];
    lw_drawLine(framebuffer, a, b, color);
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

void lw_drawString(LW_Framebuffer *const framebuffer, lw_ivec2 pos, LW_Color draw_color, LW_FontTexture font, const char *text) {
    int x_offset = 0;

    for (const char *c = text; *c != '\0'; ++c) {
        if (*c >= ' ' && *c <= '~') {
            unsigned index = *c - ' ';

            for (unsigned y = 0; y < font.texture.height; ++y) {
                for (unsigned x = 0; x < font.char_width; ++x) {
                    LW_Color color = lw_sampleTextureRaw(font.texture, (lw_uvec2){ x + index * font.char_width, y });

                    if (color.r != 0) {
                        lw_setPixel(framebuffer, (lw_uvec2){ pos[0] + x + x_offset, pos[1] + y }, draw_color);
                    }
                }
            }

            x_offset += font.char_width;
        }
    }
}