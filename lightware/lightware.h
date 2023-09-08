#pragma once

#include <stdint.h>

#if defined(LIGHTWARE_EXPORT)
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllexport)
#elif defined(__ELF__)
#define LIGHTWARE_API __attribute__((visibility("default")))
#else
#define LIGHTWARE_API
#endif
#else
#if defined(_WIN32)
#define LIGHTWARE_API __declspec(dllimport)
#else
#define LIGHTWARE_API
#endif
#endif

typedef struct LW_Context LW_Context;
typedef struct LW_Framebuffer LW_Framebuffer;

typedef int (*LW_UpdateFn)(LW_Context *const context, float delta);
typedef int (*LW_RenderFn)(LW_Context *const context, LW_Framebuffer const *const main_frame_buffer);

typedef struct LW_ContextInit {
    const char *title;
    unsigned logical_width, logical_height;
    unsigned scale;

    void *user_data;
    LW_UpdateFn update_fn;
    LW_RenderFn render_fn;
} LW_ContextInit;

typedef struct LW_Color {
    uint8_t a, b, g, r;
} LW_Color;

typedef struct LW_Framebuffer {
    LW_Color *pixels;
    unsigned width, height;
} LW_Framebuffer;

LIGHTWARE_API LW_Context *lw_init(LW_ContextInit init);
LIGHTWARE_API void lw_deinit(LW_Context *const context);

LIGHTWARE_API void *lw_getUserData(LW_Context *const context);

LIGHTWARE_API int lw_start(LW_Context *const context);
