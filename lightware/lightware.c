#include "internal.h"

#include <SDL2/SDL.h>
#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

LW_Context *lw_init(LW_ContextInit init) {
    if (init.update_fn == NULL ||
        init.render_fn == NULL) {
        return NULL;
    }

    LW_Context *context = (LW_Context *)malloc(sizeof(*context));
    if (context == NULL) return NULL;

    memset(context, 0, sizeof(*context));

    // Init sdl
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        goto _error_return;
    }

    // Create window
    context->window = SDL_CreateWindow(
        init.title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        init.logical_width * init.scale, init.logical_height * init.scale,
        SDL_WINDOW_RESIZABLE);

    if (context->window == NULL) {
        goto _error_return;
    }

    // Create renderer
    context->renderer = SDL_CreateRenderer(context->window, -1, 0); //SDL_RENDERER_PcontextENTVSYNC);
    if (context->renderer == NULL) {
        goto _error_return;
    }

    SDL_SetRenderDrawColor(context->renderer, 0, 0, 0, 255);
    SDL_RenderSetLogicalSize(context->renderer, init.logical_width, init.logical_height);
    SDL_RenderSetIntegerScale(context->renderer, SDL_TRUE);

    // Create main render texture
    context->screen_texture = SDL_CreateTexture(
        context->renderer,
        SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING,
        init.logical_width, init.logical_height);
    if (context->screen_texture == NULL) {
        goto _error_return;
    }

    // Initialize input
    context->keys      = (const int8_t *)SDL_GetKeyboardState(NULL);
    context->last_keys = (int8_t *)malloc(SDL_NUM_SCANCODES * sizeof(*context->last_keys));
    if (context->last_keys == NULL) {
        goto _error_return;
    }

    context->logical_width  = init.logical_width;
    context->logical_height = init.logical_height;
    context->user_data      = init.user_data;
    context->update_fn      = init.update_fn;
    context->render_fn      = init.render_fn;

    return context;

_error_return:
    lw_deinit(context);
    return NULL;
}

void lw_deinit(LW_Context *const context) {
    if (context == NULL) return;
    free(context->last_keys);
    SDL_DestroyTexture(context->screen_texture);
    SDL_DestroyRenderer(context->renderer);
    SDL_DestroyWindow(context->window);
    free(context);
}

int lw_start(LW_Context *const context) {
    if (context == NULL) return -1;

    int function_result;
    int pitch;

    LW_Framebuffer main_frame_buffer;
    main_frame_buffer.width  = context->logical_width;
    main_frame_buffer.height = context->logical_height;

    SDL_Event event;

    uint64_t ticks;
    uint64_t last_ticks = SDL_GetTicks64();
    float delta;
    // float seconds;

    while (1) {
        ticks            = SDL_GetTicks64();
        delta            = (float)(ticks - last_ticks) / 1000.0f;
        last_ticks       = ticks;
        context->seconds = ticks / 1000.0f;

        memcpy(context->last_keys, context->keys, sizeof(*context->last_keys) * SDL_NUM_SCANCODES);
        context->last_mouse_button_bitset = context->mouse_button_bitset;
        context->last_mouse_x             = context->mouse_x;
        context->last_mouse_y             = context->mouse_y;
        context->scroll = 0;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    return 0;
                case SDL_MOUSEWHEEL:
                    context->scroll = event.wheel.preciseY;
                case SDL_MOUSEMOTION:
                    context->mouse_x = event.motion.x;
                    context->mouse_y = event.motion.y;
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    context->mouse_button_bitset |= SDL_BUTTON(event.button.button);
                    break;
                case SDL_MOUSEBUTTONUP:
                    context->mouse_button_bitset &= ~SDL_BUTTON(event.button.button);
                    break;
            }
        }

        ////////////////////////////////////////////////
        //      UPDATE
        ////////////////////////////////////////////////

        function_result = context->update_fn(context, delta);
        if (function_result != 0) return function_result;

        ////////////////////////////////////////////////
        //      RENDER
        ////////////////////////////////////////////////

        SDL_RenderClear(context->renderer);
        SDL_LockTexture(context->screen_texture, NULL, (void **)&main_frame_buffer.pixels, &pitch);

        function_result = context->render_fn(context, &main_frame_buffer);
        if (function_result != 0) return function_result;

        SDL_UnlockTexture(context->screen_texture);
        SDL_RenderCopy(context->renderer, context->screen_texture, NULL, NULL);
        SDL_RenderPresent(context->renderer);
    }

    return 0;
}

void *lw_getUserData(LW_Context *const context) {
    if (context == NULL) return NULL;
    return context->user_data;
}

void lw_getFramebufferDimentions(LW_Framebuffer *const frame_buffer, lw_ivec2 o_dims) {
    o_dims[0] = frame_buffer->width;
    o_dims[1] = frame_buffer->height;
}

bool lw_isKey(LW_Context *const context, LW_Keycode key) {
    return context->keys[key];
}

bool lw_isKeyDown(LW_Context *const context, LW_Keycode key) {
    return context->keys[key] && !context->last_keys[key];
}

bool lw_isKeyUp(LW_Context *const context, LW_Keycode key) {
    return !context->keys[key] && context->last_keys[key];
}

bool lw_isMouseButton(LW_Context *const context, unsigned button) {
    return (context->mouse_button_bitset & (1 << button)) != 0;
}

bool lw_isMouseButtonDown(LW_Context *const context, unsigned button) {
    return (context->mouse_button_bitset & (1 << button)) != 0 && (context->last_mouse_button_bitset & (1 << button)) == 0;
}

bool lw_isMouseButtonUp(LW_Context *const context, unsigned button) {
    return (context->mouse_button_bitset & (1 << button)) == 0 && (context->last_mouse_button_bitset & (1 << button)) != 0;
}

void lw_getMousePos(LW_Context *const context, lw_ivec2 o_pos) {
    o_pos[0] = context->mouse_x;
    o_pos[1] = context->mouse_y;
}

void lw_getMouseDelta(LW_Context *const context, lw_ivec2 o_delta) {
    o_delta[0] = context->mouse_x - context->last_mouse_x;
    o_delta[1] = context->mouse_y - context->last_mouse_y;
}

float lw_getMouseScroll(LW_Context *const context) {
    return context->scroll;
}