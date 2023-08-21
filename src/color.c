#include "color.h"

Color mixColor(Color c0, Color c1) {
    return (Color){
        .r = (uint8_t)(((uint16_t)c0.r * (uint16_t)c1.r) >> 8),
        .g = (uint8_t)(((uint16_t)c0.g * (uint16_t)c1.g) >> 8),
        .b = (uint8_t)(((uint16_t)c0.b * (uint16_t)c1.b) >> 8),
        .a = (uint8_t)(((uint16_t)c0.a * (uint16_t)c1.a) >> 8),
    };
}