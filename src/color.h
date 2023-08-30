#pragma once

#include <stdint.h>

typedef struct Color {
    uint8_t a, b, g, r;
} Color;

#define RGBA(r, g, b, a) ((Color){(a), (b), (g), (r)})
#define RGB(r, g, b) RGBA(r, g, b, 255)
#define RGBV(v) RGB(v, v, v)

#define COLOR_WHITE ((Color){ .r = 255, .g = 255, .b = 255, .a = 255 })
#define COLOR_BLACK ((Color){ .r = 0, .g = 0, .b = 0, .a = 255 })
#define COLOR_RED ((Color){ .r = 255, .g = 0, .b = 0, .a = 255 })
#define COLOR_GREEN ((Color){ .r = 0, .g = 255, .b = 0, .a = 255 })
#define COLOR_BLUE ((Color){ .r = 0, .g = 0, .b = 255, .a = 255 })
#define COLOR_YELLOW ((Color){ .r = 255, .g = 255, .b = 0, .a = 255 })
#define COLOR_CYAN ((Color){ .r = 0, .g = 255, .b = 255, .a = 255 })
#define COLOR_PURPLE ((Color){ .r = 255, .g = 0, .b = 255, .a = 255 })

Color lerpColor(Color c0, Color c1, uint8_t t);
Color mixColor(Color c0, Color c1);
Color mulColor(Color c0, uint8_t m);
