#include "color.h"

Color mixColor(Color c0, Color c1) {
    return (Color){
        .r = (uint8_t)(((uint16_t)c0.r * (uint16_t)c1.r) >> 8),
        .g = (uint8_t)(((uint16_t)c0.g * (uint16_t)c1.g) >> 8),
        .b = (uint8_t)(((uint16_t)c0.b * (uint16_t)c1.b) >> 8),
        .a = (uint8_t)(((uint16_t)c0.a * (uint16_t)c1.a) >> 8),
    };
}

Color mulColor(Color c0, uint8_t m) {
        return (Color){
        .r = (uint8_t)(((uint16_t)c0.r * (uint16_t)m) >> 8),
        .g = (uint8_t)(((uint16_t)c0.g * (uint16_t)m) >> 8),
        .b = (uint8_t)(((uint16_t)c0.b * (uint16_t)m) >> 8),
        .a = (uint8_t)(((uint16_t)c0.a * (uint16_t)m) >> 8),
    };
}

Color lerpColor(Color c0, Color c1, uint8_t t) {
    uint8_t s = 255 - t;

    Color a = mulColor(c0, s);
    Color b = mulColor(c1, t);

    return (Color){
        .r = a.r + b.r,
        .g = a.g + b.g,
        .b = a.b + b.b,
        .a = a.a + b.a,
    };
}