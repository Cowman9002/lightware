#pragma once

#include "lightware.h"

#include <stdint.h>
#include <SDL2/SDL.h>

typedef struct LW_Context {
    // rendering
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *screen_texture;
    unsigned logical_width, logical_height;

    // input
    const int8_t *keys;
    int8_t *last_keys;
    int mouse_button_bitset;
    int last_mouse_button_bitset;
    
    int mouse_x, mouse_y;
    int last_mouse_x, last_mouse_y;

    // hooks
    void *user_data;
    LW_UpdateFn update_fn;
    LW_RenderFn render_fn;

    // time
    float seconds;

} LW_Context;

typedef struct LW_Framebuffer {
    LW_Color *pixels;
    unsigned width, height;
} LW_Framebuffer;