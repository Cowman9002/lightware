#pragma once

#include <stdint.h>

#define RGBA(r, g, b, a) ((((uint32_t)a & 0xff) << 0) | (((uint32_t)b & 0xff) << 8) | (((uint32_t)g & 0xff) << 16) | (((uint32_t)r & 0xff) << 24))
#define RGB(r, g, b) RGBA(r, g, b, 255)

typedef struct Color {
    uint8_t a, b, g, r;
} Color;

#define COLOR_WHITE ((Color){ .r = 255, .g = 255, .b = 255, .a = 255 })
#define COLOR_BLACK ((Color){ .r = 0, .g = 0, .b = 0, .a = 255 })
#define COLOR_RED ((Color){ .r = 255, .g = 0, .b = 0, .a = 255 })
#define COLOR_GREEN ((Color){ .r = 0, .g = 255, .b = 0, .a = 255 })
#define COLOR_BLUE ((Color){ .r = 0, .g = 0, .b = 255, .a = 255 })
#define COLOR_YELLOW ((Color){ .r = 255, .g = 255, .b = 0, .a = 255 })
#define COLOR_CYAN ((Color){ .r = 0, .g = 255, .b = 255, .a = 255 })
#define COLOR_PURPLE ((Color){ .r = 255, .g = 0, .b = 255, .a = 255 })

Color mixColor(Color c0, Color c1);
