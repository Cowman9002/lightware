#pragma once

#include <stdint.h>
#include <stdbool.h>

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

//
//  MATH
//

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TO_RADS (M_PI / 180.0)
#define TO_DEGS (180.0 / M_PI)

#define signum(a) (((a) > 0) - ((a) < 0))
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(a, b, c) ((a) < (b) ? (b) : (a) > (c) ? (c) : (a))
#define lerp(a, b, t) ((a) * (1.0 - (t)) + (b) * (t))

#define swap(T, x, y) \
    do {              \
        T tmp = x;    \
        x     = y;    \
        y     = tmp;  \
    } while (0)

typedef float lw_vec2[2];
typedef float lw_vec3[3];
typedef float lw_vec4[4];
typedef float lw_mat4[16];

typedef int lw_ivec2[2];
typedef unsigned lw_uvec2[2];
typedef int lw_ivec3[3];
typedef unsigned lw_uvec3[3];

LIGHTWARE_API float lw_dot2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_cross2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_dist2d(lw_vec2 a, lw_vec2 b);
LIGHTWARE_API float lw_normalized2d(lw_vec2 a, lw_vec2 o);
LIGHTWARE_API float lw_normalize2d(lw_vec2 a);
LIGHTWARE_API void lw_rot2d(lw_vec2 a, float r, lw_vec2 o);
LIGHTWARE_API float lw_angle2d(lw_vec2 a);
LIGHTWARE_API float lw_angleBetween2d(lw_vec2 a, lw_vec2 b);

LIGHTWARE_API float lw_dot3d(lw_vec3 a, lw_vec3 b);
LIGHTWARE_API void lw_cross3d(lw_vec3 a, lw_vec3 b, lw_vec3 o);
LIGHTWARE_API float lw_dist3d(lw_vec3 a, lw_vec3 b);
LIGHTWARE_API float lw_normalized3d(lw_vec3 a, lw_vec3 o);
LIGHTWARE_API float lw_normalize3d(lw_vec3 a);

LIGHTWARE_API void lw_mat4Identity(lw_mat4 out);
LIGHTWARE_API void lw_mat4Translate(lw_vec3 t, lw_mat4 out);
LIGHTWARE_API void lw_mat4Scale(lw_vec3 s, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateX(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateY(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4RotateZ(float r, lw_mat4 out);
LIGHTWARE_API void lw_mat4Perspective(float fov, float aspect_ratio, float near, float far, lw_mat4 out);
LIGHTWARE_API void lw_mat4Mul(lw_mat4 a, lw_mat4 b, lw_mat4 out);
LIGHTWARE_API void lw_mat4MulVec4(lw_mat4 a, lw_vec4 b, lw_vec4 out);

//
// CONTEXT
//

typedef struct LW_Context LW_Context;
typedef struct LW_Framebuffer LW_Framebuffer;

#define LW_EXIT_OK 0

typedef int (*LW_UpdateFn)(LW_Context *const context, float delta);
typedef int (*LW_RenderFn)(LW_Context *const context, LW_Framebuffer *const main_frame_buffer);

typedef struct LW_ContextInit {
    const char *title;
    unsigned logical_width, logical_height;
    unsigned scale;

    void *user_data;
    LW_UpdateFn update_fn;
    LW_RenderFn render_fn;
} LW_ContextInit;

LIGHTWARE_API LW_Context *lw_init(LW_ContextInit init);
LIGHTWARE_API void lw_deinit(LW_Context *const context);

LIGHTWARE_API int lw_start(LW_Context *const context);

LIGHTWARE_API void *lw_getUserData(LW_Context *const context);

//
//  INPUT
//
//  Most input functions are simple wrappers around SDL 2.0 functions
//

typedef enum LW_Key {

    // Based on the SDL 2.0 scancode enums

    LW_KeyUnknown = 0,

    LW_KeyA = 4,
    LW_KeyB = 5,
    LW_KeyC = 6,
    LW_KeyD = 7,
    LW_KeyE = 8,
    LW_KeyF = 9,
    LW_KeyG = 10,
    LW_KeyH = 11,
    LW_KeyI = 12,
    LW_KeyJ = 13,
    LW_KeyK = 14,
    LW_KeyL = 15,
    LW_KeyM = 16,
    LW_KeyN = 17,
    LW_KeyO = 18,
    LW_KeyP = 19,
    LW_KeyQ = 20,
    LW_KeyR = 21,
    LW_KeyS = 22,
    LW_KeyT = 23,
    LW_KeyU = 24,
    LW_KeyV = 25,
    LW_KeyW = 26,
    LW_KeyX = 27,
    LW_KeyY = 28,
    LW_KeyZ = 29,

    LW_Key1 = 30,
    LW_Key2 = 31,
    LW_Key3 = 32,
    LW_Key4 = 33,
    LW_Key5 = 34,
    LW_Key6 = 35,
    LW_Key7 = 36,
    LW_Key8 = 37,
    LW_Key9 = 38,
    LW_Key0 = 39,

    LW_KeyReturn    = 40,
    LW_KeyEscape    = 41,
    LW_KeyBackspace = 42,
    LW_KeyTab       = 43,
    LW_KeySpace     = 44,

    LW_KeyMinus        = 45,
    LW_KeyEquals       = 46,
    LW_KeyLeftBracket  = 47,
    LW_KeyRightBracket = 48,
    LW_KeyBackslash    = 49,
    LW_KeyNonUsSlash   = 50,
    LW_KeySemicolon    = 51,
    LW_KeyApostrophe   = 52,
    LW_KeyGravee       = 53,
    LW_KeyComma        = 54,
    LW_KeyPeriod       = 55,
    LW_KeySlash        = 56,

    LW_KeyCapsLock = 57,

    LW_KeyF1  = 58,
    LW_KeyF2  = 59,
    LW_KeyF3  = 60,
    LW_KeyF4  = 61,
    LW_KeyF5  = 62,
    LW_KeyF6  = 63,
    LW_KeyF7  = 64,
    LW_KeyF8  = 65,
    LW_KeyF9  = 66,
    LW_KeyF10 = 67,
    LW_KeyF11 = 68,
    LW_KeyF12 = 69,

    LW_KeyPrintScreen = 70,
    LW_KeyScrollLock  = 71,
    LW_KeyPause       = 72,
    LW_KeyInsert      = 73,

    LW_KeyHome     = 74,
    LW_KeyPageUp   = 75,
    LW_KeyDelete   = 76,
    LW_KeyEnd      = 77,
    LW_KeyPageDown = 78,
    LW_KeyRight    = 79,
    LW_KeyLeft     = 80,
    LW_KeyDown     = 81,
    LW_KeyUp       = 82,

    LW_KeyNumLockClear = 83,

    LW_KeyKpDivide   = 84,
    LW_KeyKpMultiply = 85,
    LW_KeyKpMinus    = 86,
    LW_KeyKpPlus     = 87,
    LW_KeyKpEnter    = 88,
    LW_KeyKp1        = 89,
    LW_KeyKp2        = 90,
    LW_KeyKp3        = 91,
    LW_KeyKp4        = 92,
    LW_KeyKp5        = 93,
    LW_KeyKp6        = 94,
    LW_KeyKp7        = 95,
    LW_KeyKp8        = 96,
    LW_KeyKp9        = 97,
    LW_KeyKp0        = 98,
    LW_KeyKpPeriod   = 99,

    LW_KeyNonUsBackslash = 100,

    LW_KeyApplication = 101,
    LW_KeyPower       = 102,

    LW_KeyKpEquals   = 103,
    LW_KeyF13        = 104,
    LW_KeyF14        = 105,
    LW_KeyF15        = 106,
    LW_KeyF16        = 107,
    LW_KeyF17        = 108,
    LW_KeyF18        = 109,
    LW_KeyF19        = 110,
    LW_KeyF20        = 111,
    LW_KeyF21        = 112,
    LW_KeyF22        = 113,
    LW_KeyF23        = 114,
    LW_KeyF24        = 115,
    LW_KeyExecute    = 116,
    LW_KeyHelp       = 117, /**< AL Integrated Help Center */
    LW_KeyMenu       = 118, /**< Menu (show menu) */
    LW_KeySelect     = 119,
    LW_KeyStop       = 120, /**< AC Stop */
    LW_KeyAgain      = 121, /**< AC Redo/Repeat */
    LW_KeyUndo       = 122, /**< AC Undo */
    LW_KeyCut        = 123, /**< AC Cut */
    LW_KeyCopy       = 124, /**< AC Copy */
    LW_KeyPaste      = 125, /**< AC Paste */
    LW_KeyFind       = 126, /**< AC Find */
    LW_KeyMute       = 127,
    LW_KeyVolumeUp   = 128,
    LW_KeyVolumeDown = 129,
    /* not sure whether there's a reason to enable these */
    /*     LW_KeyLOCKINGCAPSLOCK = 130,  */
    /*     LW_KeyLOCKINGNUMLOCK = 131, */
    /*     LW_KeyLOCKINGSCROLLLOCK = 132, */
    LW_KeyKpComma       = 133,
    LW_KeyKpEqualsAs400 = 134,

    LW_KeyInternational1 = 135, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    LW_KeyInternational2 = 136,
    LW_KeyInternational3 = 137, /**< Yen */
    LW_KeyInternational4 = 138,
    LW_KeyInternational5 = 139,
    LW_KeyInternational6 = 140,
    LW_KeyInternational7 = 141,
    LW_KeyInternational8 = 142,
    LW_KeyInternational9 = 143,
    LW_KeyLang1          = 144, /**< Hangul/English toggle */
    LW_KeyLang2          = 145, /**< Hanja conversion */
    LW_KeyLang3          = 146, /**< Katakana */
    LW_KeyLang4          = 147, /**< Hiragana */
    LW_KeyLang5          = 148, /**< Zenkaku/Hankaku */
    LW_KeyLang6          = 149, /**< reserved */
    LW_KeyLang7          = 150, /**< reserved */
    LW_KeyLang8          = 151, /**< reserved */
    LW_KeyLang9          = 152, /**< reserved */

    LW_KeyLCtrl  = 224,
    LW_KeyLShift = 225,
    LW_KeyLAlt   = 226, /**< alt, option */
    LW_KeyLGui   = 227, /**< windows, command (apple), meta */
    LW_KeyRCtrl  = 228,
    LW_KeyRShift = 229,
    LW_KeyRAlt   = 230, /**< alt gr, option */
    LW_KeyRGui   = 231, /**< windows, command (apple), meta */

    LW_KeycodeSize
} LW_Keycode;

LIGHTWARE_API bool lw_isKey(LW_Context *const context, LW_Keycode key);
LIGHTWARE_API bool lw_isKeyDown(LW_Context *const context, LW_Keycode key);
LIGHTWARE_API bool lw_isKeyUp(LW_Context *const context, LW_Keycode key);

LIGHTWARE_API bool lw_isMouseButton(LW_Context *const context, unsigned button);
LIGHTWARE_API bool lw_isMouseButtonDown(LW_Context *const context, unsigned button);
LIGHTWARE_API bool lw_isMouseButtonUp(LW_Context *const context, unsigned button);

LIGHTWARE_API void lw_getMousePos(LW_Context *const context, lw_ivec2 o_pos);
LIGHTWARE_API void lw_getMouseDelta(LW_Context *const context, lw_ivec2 o_delta);
LIGHTWARE_API float lw_getMouseScroll(LW_Context *const context);

//
//  GEOMETRY
//

typedef struct LW_Rect {
    lw_vec2 pos, size;
} LW_Rect;

typedef struct LW_Recti {
    lw_ivec2 pos, size;
} LW_Recti;

typedef struct LW_Circle {
    lw_vec2 pos;
    float radius;
} LW_Circle;

typedef struct LW_Circlei {
    lw_ivec2 pos;
    int radius;
} LW_Circlei;

/// @brief Returns true if point is inside of polygon. Polygon can be concave and have holes, but should be closed for predictable results.
/// @param vertices array of points stored {line_start0, line_end0, line_start1, line_end1, ...}
/// @param num_verts number of vertices in the vertices array
/// @param point point to test
/// @return true if point is inside of the polygon
LIGHTWARE_API bool lw_pointInPoly(lw_vec2 *vertices, unsigned num_verts, lw_vec2 point);

/// @brief Returns true if point is inside of closed, convex polygon. Unlike pointInPoly, the polygon must be convex and cannot contain holes for predictable results.
/// @param vertices array of points stored and connected one after the other, wrapping from the last to the beginning
/// @param num_vertices  number of vertices in vertices array
/// @param point point to test
/// @return true if point is inside of the polygon
LIGHTWARE_API bool lw_pointInConvexPoly(lw_vec2 *vertices, unsigned num_vertices, lw_vec2 point);

LIGHTWARE_API bool lw_intersectSegmentPlane(lw_vec3 line[2], lw_vec4 plane, float *o_t);

LIGHTWARE_API bool lw_intersectSegmentSegment(lw_vec2 seg0[2], lw_vec2 seg1[2], float *o_t);
LIGHTWARE_API bool lw_intersectSegmentLine(lw_vec2 seg[2], lw_vec2 line[2], float *o_t);
LIGHTWARE_API bool lw_intersectSegmentRay(lw_vec2 line[2], lw_vec2 ray[2], float *o_t);

LIGHTWARE_API void lw_calcPlaneFromPoints(lw_vec3 p0, lw_vec3 p1, lw_vec3 p2, lw_vec4 o_plane);

LIGHTWARE_API void lw_closestPointOnSegment(lw_vec2 seg[2], lw_vec2 point, lw_vec2 o_point);

//
//  TEXTURE
//

typedef struct LW_Color {
    uint8_t a, b, g, r;
} LW_Color;
typedef struct LW_Color LW_Colour;

typedef struct LW_Texture {
    LW_Color *data;
    unsigned width, height;
} LW_Texture;

LIGHTWARE_API bool lw_loadTexture(const char *path, LW_Texture *o_texture);
LIGHTWARE_API void lw_freeTexture(LW_Texture texture);

LIGHTWARE_API LW_Color lw_sampleTextureRaw(LW_Texture texture, lw_uvec2 coords);

//
//  DRAWING
//

typedef struct LW_FontTexture {
    LW_Texture texture;
    unsigned char_width;
} LW_FontTexture;

#define RGBA(r, g, b, a) ((LW_Color){ (a), (b), (g), (r) })
#define RGB(r, g, b) RGBA(r, g, b, 255)
#define RGBv(v) RGB(v, v, v)

#define LW_COLOR_CLEAR ((LW_Color){ .r = 0, .g = 0, .b = 0, .a = 0 })
#define LW_COLOR_WHITE ((LW_Color){ .r = 255, .g = 255, .b = 255, .a = 255 })
#define LW_COLOR_BLACK ((LW_Color){ .r = 0, .g = 0, .b = 0, .a = 255 })
#define LW_COLOR_GREY ((LW_Color){ .r = 128, .g = 128, .b = 128, .a = 255 })
#define LW_COLOR_GRAY LW_COLOR_GREY

#define LW_COLOR_RED ((LW_Color){ .r = 255, .g = 0, .b = 0, .a = 255 })
#define LW_COLOR_GREEN ((LW_Color){ .r = 0, .g = 255, .b = 0, .a = 255 })
#define LW_COLOR_BLUE ((LW_Color){ .r = 0, .g = 0, .b = 255, .a = 255 })
#define LW_COLOR_YELLOW ((LW_Color){ .r = 255, .g = 255, .b = 0, .a = 255 })
#define LW_COLOR_CYAN ((LW_Color){ .r = 0, .g = 255, .b = 255, .a = 255 })
#define LW_COLOR_PURPLE ((LW_Color){ .r = 255, .g = 0, .b = 255, .a = 255 })

LIGHTWARE_API LW_Color lw_lerpColor(LW_Color c0, LW_Color c1, uint8_t t);
LIGHTWARE_API LW_Color lw_mulColor(LW_Color c0, LW_Color c1);
LIGHTWARE_API LW_Color lw_shadeColor(LW_Color c0, uint8_t m);

LIGHTWARE_API void lw_getFramebufferDimentions(LW_Framebuffer *const frame_buffer, lw_ivec2 o_dims);

LIGHTWARE_API void lw_setPixel(LW_Framebuffer *const framebuffer, lw_uvec2 pos, LW_Color color);
LIGHTWARE_API void lw_fillBuffer(LW_Framebuffer *const framebuffer, LW_Color color);

LIGHTWARE_API void lw_fillRect(LW_Framebuffer *const framebuffer, LW_Recti rect, LW_Color color);
LIGHTWARE_API void lw_drawRect(LW_Framebuffer *const framebuffer, LW_Recti rect, LW_Color color);

LIGHTWARE_API void lw_fillCircle(LW_Framebuffer *const framebuffer, LW_Circlei circle, LW_Color color);
LIGHTWARE_API void lw_drawCircle(LW_Framebuffer *const framebuffer, LW_Circlei circle, LW_Color color);

LIGHTWARE_API void lw_drawLine(LW_Framebuffer *const framebuffer, lw_ivec2 v0, lw_ivec2 v1, LW_Color color);
LIGHTWARE_API void lw_drawPoly(LW_Framebuffer *const framebuffer, lw_vec2 *vertices, unsigned num_vertices, LW_Color color);

LIGHTWARE_API void lw_drawString(LW_Framebuffer *const framebuffer, lw_ivec2 pos, LW_Color draw_color, LW_FontTexture font, const char *text);

//
//  PORTAL WORLD
//

typedef struct LW_Subsector {
    float floor_height, ceiling_height;
} LW_Subsector;

typedef struct LW_Sector LW_Sector;
typedef struct LW_LineDef {
    lw_vec2 start; // position
    unsigned next;  // index to other linedef
    unsigned prev;  // index to last linedef
    LW_Sector *sector;

    lw_vec4 plane;
    LW_Sector *portal_sector;
    struct LW_LineDef *portal_wall;
} LW_LineDef;

typedef struct LW_Sector {
    unsigned num_walls;
    LW_LineDef *walls;               // list

    // definitions of sub sectors for sector over sector
    unsigned num_sub_sectors;
    LW_Subsector *sub_sectors; // list
} LW_Sector;

#define LIST_TAG LW_SectorList
#define LIST_ITEM_TYPE LW_Sector
#include "list_header.h"

typedef struct LW_PortalWorld {
    LW_SectorList sectors;
} LW_PortalWorld;

typedef struct LW_Frustum {
    // First should always be near plane
    lw_vec4 *planes;
    unsigned num_planes;
} LW_Frustum;

typedef struct LW_Camera {
    LW_Sector *sector;
    unsigned sub_sector;

    lw_vec3 pos;
    float yaw, pitch;

    float fov, aspect_ratio, far_plane, near_plane;

    lw_mat4 view_mat;
    lw_mat4 proj_mat;
    lw_mat4 vp_mat;
    lw_mat4 rot_mat;
    LW_Frustum view_frustum;
} LW_Camera;

/// @brief Assumes cam->view_frustum already has a preallocated buffer for at least 6 planes
/// @param cam
/// @return
LIGHTWARE_API void lw_calcCameraFrustum(LW_Camera *const cam);
LIGHTWARE_API void lw_calcCameraProjection(LW_Camera *const cam);
LIGHTWARE_API LW_Frustum lw_calcFrustumFromPoly(lw_vec3 *polygon, unsigned num_verts, lw_vec3 view_point);

LIGHTWARE_API bool lw_loadPortalWorld(const char *path, float scale, LW_PortalWorld *o_pod);
LIGHTWARE_API void lw_recalcLinePlane(LW_LineDef *const linedef);

LIGHTWARE_API void lw_freeSubsector(LW_Subsector subsector);
LIGHTWARE_API void lw_freeSector(LW_Sector sector);
LIGHTWARE_API void lw_freePortalWorld(LW_PortalWorld pod);

LIGHTWARE_API bool lw_pointInSector(LW_Sector sector, lw_vec2 point);
LIGHTWARE_API LW_Sector *lw_getSector(LW_PortalWorld pod, lw_vec2 point);
LIGHTWARE_API unsigned lw_getSubSector(LW_Sector *sector, lw_vec3 point);

LIGHTWARE_API void lw_renderPortalWorld(LW_Framebuffer *const framebuffer, LW_PortalWorld pod, LW_Camera camera);


//
//  COLLISION
//

typedef struct LW_Aabb {
    lw_vec2 low, high;
} LW_Aabb;

LIGHTWARE_API bool lw_pointInAabb(LW_Aabb aabb, lw_vec2 point);
LIGHTWARE_API bool lw_pointInCircle(LW_Circle circle, lw_vec2 point);